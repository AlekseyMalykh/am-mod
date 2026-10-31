from __future__ import annotations

import struct
import sys
import argparse
from pathlib import Path

sys.path.insert(0, str(Path(__file__).with_name(".tools") / "python"))
from capstone import CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN, Cs  # noqa: E402


def c_string(data: bytes, offset: int) -> str:
    end = data.find(b"\0", offset)
    return data[offset : end if end >= 0 else None].decode("ascii", "replace")


def main(path: Path, start: int | None = None, end: int | None = None) -> None:
    data = path.read_bytes()
    if data[:4] != b"\x7fELF" or data[4:6] != b"\x02\x01":
        raise SystemExit("Expected a 64-bit little-endian ELF")
    shoff = struct.unpack_from("<Q", data, 0x28)[0]
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", data, 0x3A)

    sections: list[dict[str, int | str]] = []
    for index in range(shnum):
        off = shoff + index * shentsize
        name, kind, flags, addr, file_off, size, link, info, align, entsize = struct.unpack_from(
            "<IIQQQQIIQQ", data, off
        )
        sections.append(
            dict(name_off=name, kind=kind, flags=flags, addr=addr, offset=file_off, size=size,
                 link=link, info=info, align=align, entsize=entsize)
        )
    names = sections[shstrndx]
    names_data = data[int(names["offset"]) : int(names["offset"]) + int(names["size"])]
    for section in sections:
        section["name"] = c_string(names_data, int(section["name_off"]))

    for section in sections:
        print(
            f"section {section['name']}: addr=0x{int(section['addr']):x} "
            f"offset=0x{int(section['offset']):x} size=0x{int(section['size']):x}"
        )

    if "--sections-only" in sys.argv:
        return

    text = next(section for section in sections if section["name"] == ".text")
    text_addr = int(text["addr"])
    selected_start = max(text_addr, start if start is not None else text_addr)
    selected_end = min(text_addr + int(text["size"]), end if end is not None else text_addr + int(text["size"]))
    file_start = int(text["offset"]) + selected_start - text_addr
    file_end = int(text["offset"]) + selected_end - text_addr
    code = data[file_start:file_end]
    md = Cs(CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN)
    print("disassembly:")
    for insn in md.disasm(code, selected_start):
        print(f"0x{insn.address:08x}: {insn.mnemonic:8} {insn.op_str}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("path", type=Path)
    parser.add_argument("--sections-only", action="store_true")
    parser.add_argument("--start", type=lambda value: int(value, 0))
    parser.add_argument("--end", type=lambda value: int(value, 0))
    parsed = parser.parse_args()
    if parsed.sections_only:
        sys.argv.append("--sections-only")
    main(parsed.path, parsed.start, parsed.end)
