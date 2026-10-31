from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).with_name(".tools") / "python"))

from capstone import CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN, Cs  # noqa: E402
from capstone.arm64 import ARM64_OP_IMM  # noqa: E402

from disassemble_arm64 import section_table  # noqa: E402


def main() -> None:
    if len(sys.argv) < 3:
        raise SystemExit("usage: find_arm64_calls.py ELF TARGET ...")
    path = Path(sys.argv[1])
    targets = {int(value, 0) for value in sys.argv[2:]}
    data = path.read_bytes()
    address, offset, size = section_table(data)[".text"]
    md = Cs(CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN)
    md.detail = True
    for insn in md.disasm(data[offset : offset + size], address):
        if insn.mnemonic not in {"b", "bl"} or not insn.operands:
            continue
        operand = insn.operands[0]
        if operand.type == ARM64_OP_IMM and operand.imm in targets:
            print(f"0x{insn.address:x}: {insn.mnemonic} 0x{operand.imm:x}")


if __name__ == "__main__":
    main()
