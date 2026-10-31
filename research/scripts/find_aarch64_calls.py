from __future__ import annotations

import struct
import sys
from pathlib import Path

TEXT_START = 0x00B55B00
TEXT_END = 0x0457E404


def sign_extend(value: int, bits: int) -> int:
    sign = 1 << (bits - 1)
    return (value & (sign - 1)) - (value & sign)


def main() -> None:
    data = Path(sys.argv[1]).read_bytes()
    targets = {int(value, 0) for value in sys.argv[2:]}
    code = memoryview(data)[TEXT_START:TEXT_END]
    for offset in range(0, len(code) - 4, 4):
        instruction = struct.unpack_from("<I", code, offset)[0]
        if instruction & 0xFC000000 != 0x94000000:
            continue
        pc = TEXT_START + offset
        target = pc + (sign_extend(instruction & 0x03FFFFFF, 26) << 2)
        if target in targets:
            print(f"call=0x{pc:08x} target=0x{target:08x}")


if __name__ == "__main__":
    main()
