"""Offline, read-only inspection of the pinned AC50 MCU update container.

Prints JSON to stdout. Does not communicate with a device, flash firmware,
send MIDI, or write charger registers. The firmware itself is not distributed.
Run with the extracted stock UpdateImage.rbin as the only argument.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path


PINNED_SHA256 = "73cb081b24dfe85549ffcd92730c19762058b848c77cf45f3dfa0ec0ac01a9f4"


def parse_images(data):
    """Parse concatenated !Rbn images, retaining file offsets and load VAs.

    Stored CRC values are reported, not treated as verified: their algorithm
    is not established. Container lengths and all segment bounds are checked.
    """
    images = []
    start = 0
    while start < len(data):
        if start + 0x44 > len(data) or data[start:start + 4] != b"!Rbn":
            raise ValueError(f"Invalid Rbin header at 0x{start:x}")
        image_tag, version, load, span, crc, size, count = struct.unpack_from(
            "<7I", data, start + 0x28
        )
        table_end = 0x44 + count * 16
        if not span or size < table_end or start + size > len(data):
            raise ValueError("Invalid image size or segment table")
        segments = []
        for index in range(count):
            addr, length, stored_crc, relative = struct.unpack_from(
                "<4I", data, start + 0x44 + 16 * index
            )
            if (not length or relative < table_end or relative + length > size
                    or addr < load or addr + length > load + span):
                raise ValueError("Segment outside image bounds")
            for previous in segments:
                if (addr < previous["address"] + previous["length"]
                        and previous["address"] < addr + length):
                    raise ValueError("Overlapping load segments")
                file_start = start + relative
                if (file_start < previous["file_offset"] + previous["length"]
                        and previous["file_offset"] < file_start + length):
                    raise ValueError("Overlapping file segments")
            segments.append(dict(address=addr, length=length,
                                 file_offset=start + relative,
                                 stored_crc=stored_crc))
        images.append(dict(file_offset=start, size=size, image_tag=image_tag,
                           version=[(version >> shift) & 255
                                    for shift in (24, 16, 8, 0)],
                           load=load, span=span, stored_crc=crc,
                           segments=segments))
        start += size
    return images


def read_va(data, image, address, length):
    for segment in image["segments"]:
        delta = address - segment["address"]
        if 0 <= delta and delta + length <= segment["length"]:
            offset = segment["file_offset"] + delta
            return data[offset:offset + length]
    raise ValueError(f"Unmapped MCU address 0x{address:x}")


def inspect(data):
    digest = hashlib.sha256(data).hexdigest()
    if digest != PINNED_SHA256:
        raise ValueError("Unknown firmware hash; pinned addresses must not be reused")
    images = parse_images(data)
    # This header word distinguishes the two images. Its meaning as a CPU or
    # boot/application selector is not established; do not name it "cpu".
    app = next(image for image in images if image["image_tag"] == 1)
    fields = []
    table = read_va(data, app, 0x0801BD26, 39 * 3)
    for index in range(39):
        register, shift, width = table[index * 3:index * 3 + 3]
        if not 0 < width <= 8 or shift + width > 8:
            raise ValueError("Invalid register field")
        fields.append(dict(index=index, register=register, shift=shift,
                           width=width, mask=((1 << width) - 1) << shift))
    # Names are inferred by matching the manufacturer's register map; an exact
    # BQ24296-versus-BQ24297 chip marking has not been observed on hardware.
    inferred = {0: "EN_HIZ", 5: "OTG_CONFIG", 6: "CHG_CONFIG",
                18: "WATCHDOG", 26: "BATFET_DISABLE", 28: "VBUS_STAT",
                29: "CHRG_STAT"}
    for index, name in inferred.items():
        fields[index]["inferred_name"] = name
    typec_fields = []
    typec_table = read_va(data, app, 0x0801BDA9, 11 * 3)
    for index in range(11):
        register, shift, width = typec_table[index * 3:index * 3 + 3]
        if not 0 < width <= 8 or shift + width > 8:
            raise ValueError("Invalid Type-C register field")
        typec_fields.append(dict(index=index, register=register, shift=shift,
                                  width=width))
    typec_fields[0]["inferred_name"] = "CURRENT_MODE_ADVERTISE"
    typec_fields[1]["inferred_name"] = "CURRENT_MODE_DETECT"
    currents = struct.unpack("<4H", read_va(data, app, 0x0801A9F8, 8))
    parameters = []
    for index in range(10):
        count, kind, flags, default, callback = struct.unpack(
            "<HBBII", read_va(data, app, 0x0801BDD0 + 12 * index, 12)
        )
        parameters.append(dict(index=index, count=count, kind=kind, flags=flags,
                               default=default, callback=callback))
    script = read_va(data, app, 0x0801AA00, 0xCE6)
    gauge_signatures = [b"W: AA 00 01 00", b"C: AA 00 27 04",
                        b"W: AA 00 02 00", b"C: AA 00 02 02"]
    if not all(signature in script for signature in gauge_signatures):
        raise ValueError("Expected BQ27427 identity checks are missing")
    return dict(
        sha256=digest, size=len(data), images=images,
        gauge=dict(identity_check_matches="BQ27427", device_type=0x0427,
                   firmware_version=0x0202, inferred_i2c_7bit=0x55,
                   role="fuel gauge, not a charger control"),
        charger=dict(register_map_matches="BQ24296/BQ24297 family",
                     exact_part_verified=False, inferred_i2c_7bit=0x6B,
                     field_table_va=0x0801BD26, fields=fields,
                     setter_va=0x08011F94,
                     autonomous_policy_va=0x0800FF84,
                     charge_enable_setter_call_va=0x080101CA,
                     independent_linux_control_found=False,
                     supported_host_charge_command_found=False),
        usb_typec=dict(register_map_matches="TUSB320/TUSB320I family",
                       exact_part_verified=False, inferred_i2c_7bit=0x60,
                       field_table_va=0x0801BDA9, fields=typec_fields,
                       source_current_getter_va=0x0800FB04,
                       current_mode_detect_firmware_mA=list(currents),
                       current_mode_detect_meanings=[
                           "default; stock firmware uses 900 mA",
                           "medium current",
                           "charge-through accessory; not generic USB 500 mA",
                           "high current"],
                       detected_current_is_writable=False),
        host_parameters=parameters,
        limits=["Static analysis, not a hardware charging or USB Audio test",
                "No proof that every possible hardware interface is absent",
                "No runtime charge mode implemented or enabled"],
        references=["https://www.ti.com/lit/ug/sluucd5/sluucd5.pdf",
                    "https://www.ti.com/lit/ds/symlink/bq24297.pdf",
                    "https://www.ti.com/lit/ds/symlink/tusb320.pdf"],
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("firmware", type=Path)
    args = parser.parse_args()
    try:
        result = inspect(args.firmware.read_bytes())
    except (OSError, ValueError, struct.error) as error:
        parser.exit(1, f"Inspection refused: {error}\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
