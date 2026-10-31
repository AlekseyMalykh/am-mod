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
    if len(sys.argv) < 3:
        raise SystemExit("usage: find_aarch64_xrefs.py ELF TARGET [TARGET ...]")
    data = Path(sys.argv[1]).read_bytes()
    targets = {int(value, 0) for value in sys.argv[2:]}
    code = memoryview(data)[TEXT_START:TEXT_END]

    for offset in range(0, len(code) - 20, 4):
        instruction = struct.unpack_from("<I", code, offset)[0]
        pc = TEXT_START + offset
        if instruction & 0x9F000000 == 0x10000000:  # ADR
            immlo = (instruction >> 29) & 0x3
            immhi = (instruction >> 5) & 0x7FFFF
            target = pc + sign_extend((immhi << 2) | immlo, 21)
            if target in targets:
                print(f"target=0x{target:08x} adr=0x{pc:08x}")
        if instruction & 0x9F000000 != 0x90000000:  # ADRP
            continue
        rd = instruction & 0x1F
        immlo = (instruction >> 29) & 0x3
        immhi = (instruction >> 5) & 0x7FFFF
        page = (pc & ~0xFFF) + (sign_extend((immhi << 2) | immlo, 21) << 12)
        for lookahead in range(1, 5):
            add = struct.unpack_from("<I", code, offset + lookahead * 4)[0]
            if add & 0xFF000000 != 0x91000000:  # ADD Xd, Xn, #imm
                continue
            if ((add >> 5) & 0x1F) != rd:
                continue
            immediate = (add >> 10) & 0xFFF
            if (add >> 22) & 1:
                immediate <<= 12
            target = page + immediate
            if target in targets:
                dest = add & 0x1F
                print(f"target=0x{target:08x} adrp=0x{pc:08x} add=0x{pc + lookahead * 4:08x} dest=x{dest}")


if __name__ == "__main__":
    main()
