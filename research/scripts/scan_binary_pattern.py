from __future__ import annotations

import sys
from pathlib import Path


def main() -> None:
    if len(sys.argv) != 3:
        raise SystemExit("usage: scan_binary_pattern.py ROOT HEX_BYTES")
    root = Path(sys.argv[1])
    needle = bytes.fromhex(sys.argv[2])
    for path in root.rglob("*"):
        if not path.is_file():
            continue
        try:
            data = path.read_bytes()
        except OSError:
            continue
        offset = data.find(needle)
        if offset >= 0:
            print(f"{path}\t0x{offset:x}")


if __name__ == "__main__":
    main()
