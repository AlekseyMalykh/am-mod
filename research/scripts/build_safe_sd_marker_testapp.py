from __future__ import annotations

from pathlib import Path

import build_marker_testapp as elf_builder


ROOT = Path(__file__).resolve().parent
OUT_DIR = ROOT / "sd_marker_testapp"
ELF_PATH = OUT_DIR / "mpc-safe-sd-marker-testapp"
ZIP_PATH = ROOT / "mpc-safe-sd-marker-testapp.zip"


SCRIPT = r'''#!/bin/sh
set -eu

# This probe writes only one permanent result file to the removable exFAT SD.
SD_MOUNT=$(awk '$1 == "/dev/mmcblk1p1" && $2 ~ /^\/media\// && $3 == "exfat" { print $2; exit }' /proc/mounts 2>/dev/null || true)
if [ -n "$SD_MOUNT" ]; then
    SD_OUT="$SD_MOUNT/MPC-Sample/Mods"
    if mkdir -p "$SD_OUT" 2>/dev/null && [ -w "$SD_OUT" ]; then
        REPORT="$SD_OUT/MPC_SAMPLE_TESTAPP_MARKER.txt"
        TMP="$SD_OUT/.MPC_SAMPLE_TESTAPP_MARKER.tmp"
        if {
            echo 'MPC_SAMPLE_TESTAPP_MARKER_V1'
            echo "utc=$(date -u 2>/dev/null || true)"
            echo "sd_mount=$SD_MOUNT"
        } > "$TMP" 2>/dev/null && mv -f "$TMP" "$REPORT" 2>/dev/null; then
            sync
        else
            rm -f "$TMP" 2>/dev/null || true
        fi
    fi
fi

# Return to the stock MPC startup wrapper. No Lua or user project is touched.
exec /usr/bin/launch-ac50
'''


elf_builder.SCRIPT = SCRIPT
elf_builder.OUT_DIR = OUT_DIR
elf_builder.ELF_PATH = ELF_PATH
elf_builder.ZIP_PATH = ZIP_PATH
elf_builder.main()
