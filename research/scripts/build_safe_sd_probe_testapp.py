from __future__ import annotations

from pathlib import Path

import build_marker_testapp as elf_builder


ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / "testapp_probe_safe" / "probe.lua"
OUT_DIR = ROOT / "testapp_probe_safe"
ELF_PATH = OUT_DIR / "mpc-safe-sd-probe-testapp"
ZIP_PATH = ROOT / "mpc-safe-sd-lua-probe-testapp.zip"


SHELL_TEMPLATE = r'''#!/bin/sh
set -eu

# Match the removable exFAT card observed on this MPC, never internal storage.
SD_MOUNT=$(awk '$1 == "/dev/mmcblk1p1" && $2 ~ /^\/media\// && $3 == "exfat" { print $2; exit }' /proc/mounts 2>/dev/null || true)
if [ -z "$SD_MOUNT" ]; then
    echo "Safe SD Lua probe skipped: removable exFAT card is not mounted." >&2
    exec /usr/bin/launch-ac50
fi

SD_OUT="$SD_MOUNT/MPC-Sample/Mods"
case "$SD_OUT" in
    "$SD_MOUNT"/*) ;;
    *) echo "Safe SD Lua probe skipped: invalid output path." >&2; exec /usr/bin/launch-ac50 ;;
esac

if ! mkdir -p "$SD_OUT" 2>/dev/null || [ ! -w "$SD_OUT" ]; then
    echo "Safe SD Lua probe skipped: removable card is not writable." >&2
    exec /usr/bin/launch-ac50
fi

if ! printf '%s\n' 'MPC_SAMPLE_SD_PROBE_ARMED' > "$SD_OUT/.mpc-safe-probe-write-check" 2>/dev/null; then
    echo "Safe SD Lua probe skipped: SD write check failed." >&2
    exec /usr/bin/launch-ac50
fi
if ! rm -f "$SD_OUT/.mpc-safe-probe-write-check"; then
    echo "Safe SD Lua probe skipped: could not clean SD check file." >&2
    exec /usr/bin/launch-ac50
fi

if ! cat > "$SD_OUT/.probe.lua.in" <<'__MPC_PROBE_LUA__'
__LUA_SOURCE__
__MPC_PROBE_LUA__
then
    echo "Safe SD Lua probe skipped: could not write Lua source to SD." >&2
    exec /usr/bin/launch-ac50
fi

if ! sed "s|__SD_OUT__|$SD_OUT|g" "$SD_OUT/.probe.lua.in" > "$SD_OUT/probe.lua"; then
    echo "Safe SD Lua probe skipped: could not prepare Lua file on SD." >&2
    exec /usr/bin/launch-ac50
fi
if ! rm -f "$SD_OUT/.probe.lua.in"; then
    echo "Safe SD Lua probe skipped: could not clean Lua template from SD." >&2
    exec /usr/bin/launch-ac50
fi
if [ ! -s "$SD_OUT/probe.lua" ] || grep -q '__SD_OUT__' "$SD_OUT/probe.lua"; then
    echo "Safe SD Lua probe skipped: prepared Lua file is invalid." >&2
    exec /usr/bin/launch-ac50
fi

# Start the factory MPC binary through its normal launch wrapper.
exec /usr/bin/launch-ac50 \
    --script "$SD_OUT/probe.lua" \
    --scriptOutputPath "$SD_OUT"
'''


lua_source = SOURCE.read_text(encoding="utf-8").rstrip()
if "__LUA_SOURCE__" in lua_source:
    raise SystemExit("Lua source contains reserved embedding marker")

elf_builder.SCRIPT = SHELL_TEMPLATE.replace("__LUA_SOURCE__", lua_source)
elf_builder.OUT_DIR = OUT_DIR
elf_builder.ELF_PATH = ELF_PATH
elf_builder.ZIP_PATH = ZIP_PATH
elf_builder.main()
