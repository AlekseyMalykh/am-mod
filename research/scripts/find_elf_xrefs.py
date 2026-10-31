from __future__ import annotations

import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).with_name(".tools") / "python"))

from capstone import CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN, Cs  # noqa: E402
from capstone.arm64 import ARM64_OP_IMM, ARM64_OP_MEM, ARM64_OP_REG  # noqa: E402


def u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def u64(data: bytes, offset: int) -> int:
    return struct.unpack_from("<Q", data, offset)[0]


def sections(data: bytes) -> dict[str, tuple[int, int, int]]:
    shoff = u64(data, 0x28)
    shentsize = u16(data, 0x3A)
    shnum = u16(data, 0x3C)
    shstrndx = u16(data, 0x3E)
    names_header = shoff + shstrndx * shentsize
    names_offset = u64(data, names_header + 0x18)

    def name_at(offset: int) -> str:
        end = data.index(0, offset)
        return data[offset:end].decode("ascii")

    result: dict[str, tuple[int, int, int]] = {}
    for index in range(shnum):
        item = shoff + index * shentsize
        name = name_at(names_offset + u32(data, item))
        result[name] = (
            u64(data, item + 0x10),
            u64(data, item + 0x18),
            u64(data, item + 0x20),
        )
    return result


def main() -> None:
    if len(sys.argv) < 3:
        raise SystemExit("usage: find_elf_xrefs.py ELF TARGET ...")
    path = Path(sys.argv[1])
    targets = {int(value, 0) for value in sys.argv[2:]}
    blob = path.read_bytes()
    text_address, text_offset, text_size = sections(blob)[".text"]
    md = Cs(CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN)
    md.detail = True
    pages: dict[int, tuple[int, int]] = {}

    for insn in md.disasm(blob[text_offset : text_offset + text_size], text_address):
        operands = insn.operands
        _, writes = insn.regs_access()

        if insn.mnemonic == "adrp" and len(operands) == 2:
            for reg in writes:
                pages.pop(reg, None)
            pages[operands[0].reg] = (operands[1].imm, insn.address)
            continue

        if insn.mnemonic == "adr" and len(operands) == 2 and operands[1].imm in targets:
            print(f"target=0x{operands[1].imm:x} adr=0x{insn.address:x}")

        if (
            insn.mnemonic == "add"
            and len(operands) >= 3
            and operands[0].type == ARM64_OP_REG
            and operands[1].type == ARM64_OP_REG
            and operands[2].type == ARM64_OP_IMM
            and operands[1].reg in pages
        ):
            page, adrp_address = pages[operands[1].reg]
            target = page + operands[2].imm
            if target in targets:
                print(
                    f"target=0x{target:x} adrp=0x{adrp_address:x} "
                    f"add=0x{insn.address:x} dest={insn.reg_name(operands[0].reg)}"
                )

        for operand in operands:
            if operand.type != ARM64_OP_MEM or operand.mem.base not in pages:
                continue
            page, adrp_address = pages[operand.mem.base]
            target = page + operand.mem.disp
            if target in targets:
                print(
                    f"target=0x{target:x} adrp=0x{adrp_address:x} "
                    f"mem=0x{insn.address:x} insn={insn.mnemonic} {insn.op_str}"
                )

        for reg in writes:
            pages.pop(reg, None)
        if insn.mnemonic in {"b", "br", "blr", "ret", "cbz", "cbnz", "tbz", "tbnz"} or insn.mnemonic.startswith("b."):
            pages.clear()


if __name__ == "__main__":
    main()
