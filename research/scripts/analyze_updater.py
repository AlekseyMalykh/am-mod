from __future__ import annotations

import hashlib
import math
import re
import struct
import sys
from collections import Counter
from pathlib import Path


MAGICS = {
    b"PK\x03\x04": "ZIP local header",
    b"7z\xbc\xaf\x27\x1c": "7-Zip",
    b"\x1f\x8b\x08": "gzip",
    b"\xfd7zXZ\x00": "XZ",
    b"hsqs": "SquashFS little-endian",
    b"sqsh": "SquashFS big-endian",
    b"\x7fELF": "ELF",
    b"CrAU": "Android update payload",
    b"UBI#": "UBI erase-count header",
    b"ubifs": "UBIFS marker",
}


def entropy(data: bytes) -> float:
    if not data:
        return 0.0
    counts = Counter(data)
    size = len(data)
    return -sum((n / size) * math.log2(n / size) for n in counts.values())


def occurrences(data: bytes, needle: bytes, limit: int = 30) -> list[int]:
    result: list[int] = []
    start = 0
    while len(result) < limit:
        pos = data.find(needle, start)
        if pos < 0:
            break
        result.append(pos)
        start = pos + 1
    return result


def main(path: Path) -> None:
    data = path.read_bytes()
    print(f"file={path.name}")
    print(f"size={len(data)}")
    print(f"sha256={hashlib.sha256(data).hexdigest()}")

    if data[:2] != b"MZ":
        print("not a PE file")
        return

    peoff = struct.unpack_from("<I", data, 0x3C)[0]
    if data[peoff : peoff + 4] != b"PE\0\0":
        print("invalid PE signature")
        return

    machine, section_count, _, _, _, opt_size, _ = struct.unpack_from(
        "<HHIIIHH", data, peoff + 4
    )
    machine_name = {0x14C: "x86", 0x8664: "x86-64", 0xAA64: "ARM64"}.get(
        machine, hex(machine)
    )
    print(f"pe_offset=0x{peoff:x} machine={machine_name} sections={section_count}")

    section_table = peoff + 24 + opt_size
    raw_end = 0
    for index in range(section_count):
        off = section_table + index * 40
        name = data[off : off + 8].rstrip(b"\0").decode("ascii", "replace")
        virtual_size, virtual_addr, raw_size, raw_ptr = struct.unpack_from(
            "<IIII", data, off + 8
        )
        raw_end = max(raw_end, raw_ptr + raw_size)
        sample = data[raw_ptr : raw_ptr + raw_size]
        print(
            f"section[{index}]={name} va=0x{virtual_addr:x} "
            f"vsize=0x{virtual_size:x} raw=0x{raw_ptr:x}+0x{raw_size:x} "
            f"entropy={entropy(sample):.3f}"
        )

    overlay = data[raw_end:]
    print(f"pe_raw_end=0x{raw_end:x} overlay_size={len(overlay)} overlay_entropy={entropy(overlay):.3f}")

    print("magic_hits:")
    for magic, label in MAGICS.items():
        hits = occurrences(data, magic)
        if hits:
            print(f"  {label}: " + ", ".join(f"0x{x:x}" for x in hits))

    strings = [m.group().decode("ascii", "replace") for m in re.finditer(rb"[ -~]{8,}", data)]
    pattern = re.compile(
        r"(?i)(firmware|update|payload|\.bin|\.img|\.zip|\.tar|\.squash|usb|dfu|mpc.sample|akai|electron|nsis|qt|http)"
    )
    interesting: list[str] = []
    seen: set[str] = set()
    for value in strings:
        if pattern.search(value) and value not in seen:
            seen.add(value)
            interesting.append(value[:500])
    print(f"interesting_strings={len(interesting)}")
    for value in interesting[:250]:
        print("  " + value)


if __name__ == "__main__":
    target = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("MPC Sample 1.3.0 Updater.exe")
    main(target)
