"""Execute pinned AC50 MCU code in Unicorn with mocked hardware inputs.

Offline research fixture, NOT a charger or USB Audio hardware test.
No device endpoints, native firmware tools, or firmware writes are used.
Requires unicorn==2.1.4; firmware binaries are supplied separately by the owner.
"""
import argparse
import json
import struct
import sys
from pathlib import Path

from inspect_power_mcu import inspect


FLASH = 0x08000000
RAM = 0x20000000
GPIO = 0x50000000
RETURN = 0x08020000
CHARGER = 0x200004C0
GAUGE = 0x200003CC
TYPEC = 0x200004F0


def run(data, report):
    try:
        from unicorn import (Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS,
                             UC_HOOK_CODE, UC_HOOK_MEM_WRITE)
        from unicorn.arm_const import (UC_ARM_REG_R0, UC_ARM_REG_R1,
                                       UC_ARM_REG_R2, UC_ARM_REG_R3,
                                       UC_ARM_REG_SP, UC_ARM_REG_LR,
                                       UC_ARM_REG_PC)
    except ImportError as error:
        raise RuntimeError("Install unicorn==2.1.4 for this offline fixture") from error

    argument_registers = (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2,
                          UC_ARM_REG_R3)
    app = next(image for image in report["images"] if image["image_tag"] == 1)

    class Fixture:
        def __init__(self, vbus=True, detected_mode=1, advertised_mode=0, soc=50):
            if detected_mode not in range(4) or advertised_mode not in range(4):
                raise ValueError("Type-C modes must fit the two-bit field")
            self.machine = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
            self.machine.mem_map(FLASH, 0x40000)
            self.machine.mem_map(RAM, 0x10000)
            self.machine.mem_map(GPIO, 0x2000)
            for segment in app["segments"]:
                offset = segment["file_offset"]
                self.machine.mem_write(segment["address"],
                                       data[offset:offset + segment["length"]])
            self.now = 1000000
            self.gpio_writes = []
            self.hooks = {}
            self.machine.hook_add(UC_HOOK_CODE, self.code)
            self.machine.hook_add(UC_HOOK_MEM_WRITE, self.write)
            self.invoke(0x08014E30)  # Native host-parameter defaults.
            # Simulated cached REG08: VBUS present, PG, charging; no fault.
            self.machine.mem_write(CHARGER + 0x1C + 8,
                                   bytes([0x54 if vbus else 0]))
            self.machine.mem_write(0x2000054D, b"\x02")  # Past startup.
            self.machine.mem_write(0x200004A5, b"\x01")  # Gauge grace elapsed.
            # Native gauge getters read these simulated cached values:
            # register 0x04 -> +0xba (mV), 0x1c -> +0xca (SoC).
            self.machine.mem_write(GAUGE + 0x9C, struct.pack("<I", 2))
            self.machine.mem_write(GAUGE + 0xBA, struct.pack("<H", 3800))
            self.machine.mem_write(GAUGE + 0xCA, struct.pack("<H", soc))
            # Native source-current getter reads CURRENT_MODE_DETECT (08[5:4]),
            # not CURRENT_MODE_ADVERTISE (08[7:6]). No fake USB negotiation.
            self.machine.mem_write(TYPEC + 0x1C + 8,
                                   bytes([(advertised_mode << 6) |
                                          (detected_mode << 4)]))
            self.hooks = {
                0x08010F68: lambda *_: self.now,  # Monotonic MCU timer.
                0x08013780: lambda *_: 0,  # USB-source driver pump (no I/O).
                0x08011E52: lambda *_: 0,  # Charger driver pump (no I/O).
                0x0800FD78: lambda *_: 0,  # Battery LED updater only.
                0x0800FA00: lambda *_: 2,  # Powered-on state (nonzero).
                0x08010B4C: lambda *_: 0,  # Physical button matrix released.
                0x080146AC: lambda *_: 0,  # Outgoing MIDI serializer sink.
            }

        def code(self, machine, pc, size, context):
            if pc == RETURN:
                machine.emu_stop()
            elif pc in self.hooks:
                arguments = [machine.reg_read(reg) for reg in argument_registers]
                result = self.hooks[pc](*arguments)
                machine.reg_write(UC_ARM_REG_R0, result or 0)
                machine.reg_write(UC_ARM_REG_PC,
                                  machine.reg_read(UC_ARM_REG_LR))

        def write(self, machine, access, address, size, value, context):
            # Every peripheral write remains inside emulator memory. Reject
            # unexpected addresses so the model cannot silently grow extra I/O.
            if GPIO <= address < GPIO + 0x2000:
                if address not in (0x50000418, 0x50000428) or size != 4:
                    raise RuntimeError(f"Unexpected simulated GPIO: {address:x}")
                self.gpio_writes.append((address, value))

        def invoke(self, address, *arguments):
            for register, value in zip(argument_registers, arguments):
                self.machine.reg_write(register, value)
            self.machine.reg_write(UC_ARM_REG_SP, RAM + 0xF000)
            self.machine.reg_write(UC_ARM_REG_LR, RETURN | 1)
            self.machine.emu_start(address | 1, RETURN, count=100000)
            if self.machine.reg_read(UC_ARM_REG_PC) != RETURN:
                raise RuntimeError("Instruction limit reached; native call did not return")
            return self.machine.reg_read(UC_ARM_REG_R0)

        def charge(self):
            return self.invoke(0x08011F60, CHARGER, 6)

        def source_current(self):
            return self.invoke(0x0800FB04)

        def stage_charge(self, value):
            # Native cache setter in simulated MCU RAM. Never a device write.
            self.invoke(0x08011F94, CHARGER, 6, value)

        def policy(self):
            self.now += 1000000
            self.invoke(0x0800FF84)

        def set_parameter(self, index, value):
            # Execute native SysEx payload dispatcher directly. No MIDI port
            # exists here and this packet is never sent to the real device.
            payload = bytes([index >> 7, index & 127, 0, 0] +
                            [(value >> shift) & 15
                             for shift in range(28, -1, -4)])
            packet = bytes([0, 0x5C, 0x6C, 0, len(payload)]) + payload
            self.machine.mem_write(RAM + 0xE000, packet)
            self.invoke(0x080149E4, RAM + 0x8000, 0x4701,
                        RAM + 0xE000, len(packet))
            return self.invoke(0x08014E94, index, 0)

    outcomes = []
    probe = Fixture()
    assert probe.charge() == 0
    probe.policy()
    assert probe.charge() == 1
    probe.stage_charge(0)
    assert probe.charge() == 0
    probe.policy()
    assert probe.charge() == 1
    outcomes.append("Native policy re-enables a cleared cached CHG_CONFIG")

    parameters = []
    for parameter in report["host_parameters"]:
        probe = Fixture()
        index = parameter["index"]
        value = parameter["default"] + 1
        readback = probe.set_parameter(index, value)
        assert readback == value, (index, value, readback)
        probe.policy()
        assert probe.charge() == 1, index
        parameters.append(dict(index=index, requested=value, readback=readback,
                               cached_charge_after_policy=probe.charge()))
    outcomes.append("All 10 known host parameters accept tested values; none inhibits charging")

    # Negative controls: the fixture must permit native policy to disable the
    # same field when simulated inputs warrant it. CHG_CONFIG is never stubbed.
    for label, configuration in (("no source", dict(vbus=False)),
                                 ("500 mA charge-through accessory mode",
                                  dict(detected_mode=2))):
        probe = Fixture(**configuration)
        probe.stage_charge(1)
        probe.policy()
        assert probe.charge() == 0, label
        outcomes.append(f"Native policy disables cached charging: {label}")

    typec_trials = []
    for mode, current in enumerate((900, 1500, 500, 3000)):
        probe = Fixture(detected_mode=mode)
        assert probe.source_current() == current
        probe.policy()
        charge = probe.charge()
        assert charge == int(mode != 2)
        typec_trials.append(dict(detected_mode=mode, source_current_mA=current,
                                 cached_charge_after_policy=charge))
    for advertised in range(4):
        probe = Fixture(detected_mode=1, advertised_mode=advertised)
        assert probe.source_current() == 1500
        probe.policy()
        assert probe.charge() == 1
    outcomes.append("Native Type-C lookup: 900/1500/500/3000 mA; advertise field does not override detect")
    for soc in (2, 14, 15, 50, 100):
        probe = Fixture(soc=soc)
        probe.policy()
        assert probe.charge() == 1
    outcomes.append("With the stated source, tested SoC values do not implement Low Battery policy")

    return dict(status="PASS", firmware_sha256=report["sha256"],
                checks=outcomes, host_parameter_trials=parameters,
                typec_current_trials=typec_trials,
                device_connected=False, hardware_charging_tested=False,
                usb_audio_tested=False,
                limits=["Timers, sensors, driver pumps and physical keys are mocked",
                        "Only the stated parameter values and input states were tested",
                        "This is not proof that every possible command is absent"])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("firmware", type=Path)
    parser.add_argument("--dependency-path", type=Path,
                        help="Optional local directory containing Unicorn")
    args = parser.parse_args()
    if args.dependency_path:
        sys.path.insert(0, str(args.dependency_path.resolve()))
    data = args.firmware.read_bytes()
    result = run(data, inspect(data))
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
