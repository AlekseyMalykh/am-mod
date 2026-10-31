from __future__ import annotations

import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).with_name(".tools") / "python"))

from capstone import CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN, Cs  # noqa: E402


def u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def u64(data: bytes, offset: int) -> int:
    return struct.unpack_from("<Q", data, offset)[0]


def section_table(data: bytes) -> dict[str, tuple[int, int, int]]:
    shoff = u64(data, 0x28)
    shentsize = u16(data, 0x3A)
    shnum = u16(data, 0x3C)
    shstrndx = u16(data, 0x3E)
    shstr = shoff + shstrndx * shentsize
    names_offset = u64(data, shstr + 0x18)

    def cstring(offset: int) -> str:
        end = data.index(0, offset)
        return data[offset:end].decode("ascii")

    sections: dict[str, tuple[int, int, int]] = {}
    for index in range(shnum):
        item = shoff + index * shentsize
        name = cstring(names_offset + u32(data, item))
        address = u64(data, item + 0x10)
        offset = u64(data, item + 0x18)
        size = u64(data, item + 0x20)
        sections[name] = (address, offset, size)
    return sections


def main() -> None:
    if len(sys.argv) not in {2, 4}:
        raise SystemExit("usage: disassemble_arm64.py ELF [START END]")
    path = Path(sys.argv[1])
    data = path.read_bytes()
    address, offset, size = section_table(data)[".text"]
    start = int(sys.argv[2], 0) if len(sys.argv) == 4 else address
    end = int(sys.argv[3], 0) if len(sys.argv) == 4 else address + size
    start = max(start, address)
    end = min(end, address + size)
    md = Cs(CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN)
    relative_start = start - address
    for insn in md.disasm(data[offset + relative_start : offset + relative_start + end - start], start):
        print(f"{insn.address:08x}:  {insn.bytes.hex(' '):11s}  {insn.mnemonic:8s} {insn.op_str}")


if __name__ == "__main__":
    main()
