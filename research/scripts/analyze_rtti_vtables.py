from __future__ import annotations

import argparse
import struct
from pathlib import Path


def c_string(data: bytes, offset: int) -> str:
    end = data.find(b"\0", offset)
    return data[offset : end if end >= 0 else None].decode("ascii", "replace")


def find_all(data: bytes, needle: bytes):
    start = 0
    while True:
        hit = data.find(needle, start)
        if hit < 0:
            return
        yield hit
        start = hit + 1


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("path", type=Path)
    parser.add_argument("targets", nargs="+", type=lambda value: int(value, 0))
    args = parser.parse_args()
    data = args.path.read_bytes()

    shoff = struct.unpack_from("<Q", data, 0x28)[0]
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", data, 0x3A)
    sections = []
    for index in range(shnum):
        off = shoff + index * shentsize
        name, kind, flags, addr, file_off, size, link, info, align, entsize = struct.unpack_from(
            "<IIQQQQIIQQ", data, off
        )
        sections.append(
            dict(name_off=name, kind=kind, flags=flags, addr=addr, offset=file_off, size=size)
        )
    names = sections[shstrndx]
    names_data = data[names["offset"] : names["offset"] + names["size"]]
    for section in sections:
        section["name"] = c_string(names_data, section["name_off"])

    def section_for_file(offset: int):
        return next(
            (
                section
                for section in sections
                if section["offset"] <= offset < section["offset"] + section["size"]
            ),
            None,
        )

    def file_to_va(offset: int) -> int:
        section = section_for_file(offset)
        if section is None:
            return offset
        return section["addr"] + offset - section["offset"]

    text = next(section for section in sections if section["name"] == ".text")
    text_start, text_end = text["addr"], text["addr"] + text["size"]

    for target in args.targets:
        print(f"target string 0x{target:08x}: {c_string(data, target)}")
        name_pointer = struct.pack("<Q", target)
        for name_ref in find_all(data, name_pointer):
            section = section_for_file(name_ref)
            if section is None or not str(section["name"]).startswith(".data.rel.ro"):
                continue
            typeinfo_file = name_ref - 8
            typeinfo_va = file_to_va(typeinfo_file)
            print(
                f"  typeinfo file=0x{typeinfo_file:08x} va=0x{typeinfo_va:08x} "
                f"section={section['name']}"
            )
            for typeinfo_ref in find_all(data, struct.pack("<Q", typeinfo_va)):
                ref_section = section_for_file(typeinfo_ref)
                if ref_section is None or not str(ref_section["name"]).startswith(".data.rel.ro"):
                    continue
                vtable_file = typeinfo_ref - 8
                vtable_va = file_to_va(vtable_file)
                entries = []
                for index in range(1, 13):
                    entry_file = typeinfo_ref + index * 8
                    if entry_file + 8 > len(data):
                        break
                    value = struct.unpack_from("<Q", data, entry_file)[0]
                    if text_start <= value < text_end:
                        entries.append(f"[{index - 1}]=0x{value:08x}")
                    elif entries:
                        break
                print(
                    f"    vtable file=0x{vtable_file:08x} va=0x{vtable_va:08x} "
                    f"functions={' '.join(entries) if entries else '(none found)'}"
                )


if __name__ == "__main__":
    main()
