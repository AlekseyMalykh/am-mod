from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).with_name(".tools") / "python"))
from capstone import CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN, Cs  # noqa: E402


def main() -> None:
    if len(sys.argv) != 4:
        raise SystemExit("usage: disassemble_range.py ELF START END")
    path, start, end = Path(sys.argv[1]), int(sys.argv[2], 0), int(sys.argv[3], 0)
    data = path.read_bytes()[start:end]
    md = Cs(CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN)
    for insn in md.disasm(data, start):
        print(f"0x{insn.address:08x}: {insn.mnemonic:8} {insn.op_str}")


if __name__ == "__main__":
    main()
