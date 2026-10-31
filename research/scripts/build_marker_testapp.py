#!/usr/bin/env python3
"""Build a tiny libc-free AArch64 ELF that execs /bin/sh -c SCRIPT.

The embedded script writes non-destructive launcher diagnostics and then
executes the stock MPC startup wrapper.  No firmware partitions are changed.
"""

from __future__ import annotations

import os
import struct
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parent
OUT_DIR = ROOT / "marker_testapp"
ELF_PATH = OUT_DIR / "mpc-marker-testapp"
ZIP_PATH = ROOT / "mpc-marker-testapp.zip"

SCRIPT = r'''DATA=/data/mpc-testapp-probe
SD=/media/az01-internal-sd/MPC-Sample/Mods
mkdir -p "$DATA"
REPORT="$DATA/launcher-probe.txt"
{
  echo "MPC Sample test-app launcher probe"
  echo "utc=$(date -u 2>/dev/null || true)"
  echo "uid=$(id 2>/dev/null || true)"
  echo "uname=$(uname -a 2>/dev/null || true)"
  echo "pwd=$(pwd 2>/dev/null || true)"
  echo "argv0=$0"
  echo "--- mounts ---"
  mount 2>/dev/null || true
  echo "--- media ---"
  ls -la /media 2>/dev/null || true
  ls -la /media/az01-internal-sd 2>/dev/null || true
  echo "--- storage ---"
  df -h 2>/dev/null || true
} >"$REPORT" 2>&1

if mkdir -p "$SD" 2>/dev/null; then
  cp "$REPORT" "$SD/launcher-probe.txt" 2>/dev/null || true
  {
    echo "TESTAPP_OK"
    echo "The unsigned ARM64 test app was executed."
    echo "No firmware partition was modified."
  } >"$SD/TESTAPP_OK.txt"
  sync
fi

exec /usr/bin/launch-ac50
'''


def movz_x(register: int, value: int) -> int:
    if not 0 <= value <= 0xFFFF:
        raise ValueError(value)
    return 0xD2800000 | (value << 5) | register


def adr_x(register: int, source_address: int, target_address: int) -> int:
    offset = target_address - source_address
    if not -(1 << 20) <= offset < (1 << 20):
        raise ValueError("ADR target is out of range")
    encoded = offset & 0x1FFFFF
    immlo = encoded & 0x3
    immhi = encoded >> 2
    return 0x10000000 | (immlo << 29) | (immhi << 5) | register


def build_elf() -> bytes:
    base = 0x400000
    code_offset = 64 + 56
    entry = base + code_offset

    # Eight AArch64 instructions, followed by immutable strings and argv[].
    code_size = 8 * 4
    data_offset = code_offset + code_size
    path = b"/bin/sh\0"
    arg0 = b"sh\0"
    arg1 = b"-c\0"
    script = SCRIPT.encode("utf-8") + b"\0"

    path_off = data_offset
    arg0_off = path_off + len(path)
    arg1_off = arg0_off + len(arg0)
    script_off = arg1_off + len(arg1)
    argv_off = (script_off + len(script) + 7) & ~7

    insn_addresses = [entry + i * 4 for i in range(8)]
    instructions = [
        adr_x(0, insn_addresses[0], base + path_off),       # x0 = /bin/sh
        adr_x(1, insn_addresses[1], base + argv_off),       # x1 = argv
        0xAA1F03E2,                                         # mov x2, xzr
        movz_x(8, 221),                                     # execve
        0xD4000001,                                         # svc #0
        movz_x(0, 127),                                     # exec failure status
        movz_x(8, 93),                                      # exit
        0xD4000001,                                         # svc #0
    ]
    code = b"".join(struct.pack("<I", item) for item in instructions)

    payload = bytearray(code)
    payload += path + arg0 + arg1 + script
    payload += b"\0" * (argv_off - (code_offset + len(payload)))
    payload += struct.pack(
        "<QQQQ",
        base + arg0_off,
        base + arg1_off,
        base + script_off,
        0,
    )

    file_size = code_offset + len(payload)
    ident = b"\x7fELF" + bytes([2, 1, 1, 0, 0]) + bytes(7)
    elf_header = struct.pack(
        "<16sHHIQQQIHHHHHH",
        ident,
        2,          # ET_EXEC
        183,        # EM_AARCH64
        1,
        entry,
        64,
        0,
        0,
        64,
        56,
        1,
        0,
        0,
        0,
    )
    program_header = struct.pack(
        "<IIQQQQQQ",
        1,          # PT_LOAD
        5,          # PF_R | PF_X
        0,
        base,
        base,
        file_size,
        file_size,
        0x1000,
    )
    return elf_header + program_header + payload


def main() -> None:
    OUT_DIR.mkdir(exist_ok=True)
    ELF_PATH.write_bytes(build_elf())
    os.chmod(ELF_PATH, 0o755)

    with zipfile.ZipFile(ZIP_PATH, "w", zipfile.ZIP_DEFLATED) as archive:
        info = zipfile.ZipInfo(ELF_PATH.name)
        info.create_system = 3
        info.external_attr = (0o100755 & 0xFFFF) << 16
        archive.writestr(info, ELF_PATH.read_bytes())

    print(f"ELF: {ELF_PATH} ({ELF_PATH.stat().st_size} bytes)")
    print(f"ZIP: {ZIP_PATH} ({ZIP_PATH.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
