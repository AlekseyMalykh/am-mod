from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).with_name(".tools") / "python"))

from capstone import CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN, Cs  # noqa: E402
from capstone.arm64 import ARM64_OP_IMM, ARM64_OP_MEM, ARM64_OP_REG  # noqa: E402


TEXT_START = 0x96700
TEXT_END = 0x552854


def main() -> None:
    if len(sys.argv) < 3:
        raise SystemExit("usage: find_launcher_xrefs_capstone.py ELF TARGET ...")
    path = Path(sys.argv[1])
    targets = {int(value, 0) for value in sys.argv[2:]}
    data = path.read_bytes()[TEXT_START:TEXT_END]

    md = Cs(CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN)
    md.detail = True
    pages: dict[int, tuple[int, int]] = {}

    for insn in md.disasm(data, TEXT_START):
        ops = insn.operands
        reads, writes = insn.regs_access()

        if insn.mnemonic == "adrp" and len(ops) == 2:
            for reg in writes:
                pages.pop(reg, None)
            pages[ops[0].reg] = (ops[1].imm, insn.address)
            continue

        if insn.mnemonic == "adr" and len(ops) == 2:
            if ops[1].imm in targets:
                print(
                    f"target=0x{ops[1].imm:08x} adr=0x{insn.address:08x} "
                    f"dest={insn.reg_name(ops[0].reg)}"
                )

        if (
            insn.mnemonic == "add"
            and len(ops) >= 3
            and ops[0].type == ARM64_OP_REG
            and ops[1].type == ARM64_OP_REG
            and ops[2].type == ARM64_OP_IMM
            and ops[1].reg in pages
        ):
            page, adrp = pages[ops[1].reg]
            target = page + ops[2].imm
            if target in targets:
                print(
                    f"target=0x{target:08x} adrp=0x{adrp:08x} "
                    f"add=0x{insn.address:08x} dest={insn.reg_name(ops[0].reg)}"
                )

        for operand in ops:
            if operand.type != ARM64_OP_MEM or operand.mem.base not in pages:
                continue
            page, adrp = pages[operand.mem.base]
            target = page + operand.mem.disp
            if target in targets:
                print(
                    f"target=0x{target:08x} adrp=0x{adrp:08x} "
                    f"mem=0x{insn.address:08x} insn={insn.mnemonic} {insn.op_str}"
                )

        for reg in writes:
            pages.pop(reg, None)

        if insn.mnemonic in {
            "b", "br", "blr", "ret", "cbz", "cbnz", "tbz", "tbnz"
        } or insn.mnemonic.startswith("b."):
            pages.clear()


if __name__ == "__main__":
    main()
