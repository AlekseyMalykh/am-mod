from __future__ import annotations

import argparse
import re
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("path", type=Path)
    parser.add_argument("--min", type=int, default=5)
    parser.add_argument("--grep", default="")
    parser.add_argument("--start", type=lambda x: int(x, 0), default=0)
    parser.add_argument("--end", type=lambda x: int(x, 0))
    parser.add_argument("--kind", choices=("all", "ascii", "utf16le"), default="all")
    args = parser.parse_args()
    full_data = args.path.read_bytes()
    end = len(full_data) if args.end is None else min(args.end, len(full_data))
    data = full_data[args.start:end]
    matcher = re.compile(args.grep, re.IGNORECASE) if args.grep else None

    patterns = [
        ("ascii", re.compile(rb"[\x20-\x7e]{%d,}" % args.min)),
        ("utf16le", re.compile(rb"(?:[\x20-\x7e]\x00){%d,}" % args.min)),
    ]
    for encoding, pattern in patterns:
        if args.kind != "all" and args.kind != encoding:
            continue
        for hit in pattern.finditer(data):
            raw = hit.group()
            value = raw.decode("utf-16le" if encoding == "utf16le" else "ascii", "replace")
            if matcher and not matcher.search(value):
                continue
            print(f"0x{args.start + hit.start():08x} {encoding}: {value}")


if __name__ == "__main__":
    main()
