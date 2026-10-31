from __future__ import annotations

import hashlib
import lzma
import struct
import sys
from pathlib import Path


ENTRY_OFFSETS = (0x90, 0xD0, 0x110, 0x150)
OUTPUT_NAMES = ("bootstrap.bin", "part1.ext4", "part2.ext4", "part3.ext4")


def main(source: Path, output_dir: Path) -> None:
    data = source.read_bytes()
    if data[:4] != b"AZ0x":
        raise SystemExit("Unsupported image: AZ0x magic not found")
    output_dir.mkdir(parents=True, exist_ok=True)

    for entry_offset, output_name in zip(ENTRY_OFFSETS, OUTPUT_NAMES):
        tag = data[entry_offset : entry_offset + 4].decode("ascii", "replace")
        version = struct.unpack_from("<I", data, entry_offset + 4)[0]
        offset, packed_size, flags = struct.unpack_from("<QQQ", data, entry_offset + 8)
        expected_digest = data[entry_offset + 32 : entry_offset + 64]
        packed = data[offset : offset + packed_size]
        actual_digest = hashlib.sha256(packed).digest()
        print(
            f"{tag}: version={version} offset=0x{offset:x} packed={packed_size} "
            f"flags=0x{flags:x} sha256_match={actual_digest == expected_digest}"
        )
        print(f"  expected={expected_digest.hex()}")
        print(f"  packed_sha256={actual_digest.hex()}")

        if tag in {"PARR", "PART"}:
            unpacked = lzma.decompress(packed, format=lzma.FORMAT_XZ)
            destination = output_dir / output_name
            destination.write_bytes(unpacked)
            print(
                f"  unpacked={len(unpacked)} output={destination.name} "
                f"sha256={hashlib.sha256(unpacked).hexdigest()}"
            )
        else:
            destination = output_dir / output_name
            destination.write_bytes(packed)
            print(f"  copied={destination.name}")


if __name__ == "__main__":
    src = Path(sys.argv[1])
    dst = Path(sys.argv[2])
    main(src, dst)
