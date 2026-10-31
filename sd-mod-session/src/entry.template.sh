#!/bin/sh
# Small native autorun entry point. The full controller is pinned by SHA256.
set -u
PATH=/usr/sbin:/usr/bin:/sbin:/bin
export PATH
umask 077
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd) || exit 1
case "$DIR" in /media/*/autorun) ;; *) exit 1 ;; esac
case "$DIR" in *' '*|*'%'*|*'\'*) exit 1 ;; esac
SD=${DIR%/autorun}
case "${SD#/media/}" in ''|*[!A-Za-z0-9._-]*) exit 1 ;; esac
[ "$(readlink -f "$SD")" = "$SD" ] || exit 1
awk -v p="$SD" '$2==p && $3=="exfat" && $4 ~ /(^|,)rw(,|$)/ {n++} END {exit n!=1}' /proc/mounts || exit 1
ENTRY="$DIR/MPC_MOD_ENTRY.txt"
CONSOLE="$DIR/MPC_MOD_CONSOLE.txt"
for p in "$ENTRY" "$CONSOLE"; do [ ! -L "$p" ] || exit 1; done
entry_line() { printf '%s %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$*" >> "$ENTRY"; }
entry_line 'ENTRY_BEGIN revision=r9h-save-path-diagnostics' || exit 1
printf 'BOOT_ID=%s UPTIME=%s\n' "$(cat /proc/sys/kernel/random/boot_id)" "$(cat /proc/uptime)" >> "$ENTRY" || exit 1
sha256sum "$0" >> "$ENTRY" || exit 1
# Bootstrap flush before a verified RAM helper is available. Later controller
# checkpoints use syncfs on the pinned SD only. No periodic sync loop is used.
sync || exit 1
finish_entry() {
    entry_line "ENTRY_END controller_status=$1; status0 means handoff only, not verified clone"
    sync
}
trap 'finish_entry $?' EXIT
exec >> "$CONSOLE" 2>&1 || exit 1
echo "=== r9h entry $(date -u +%Y-%m-%dT%H:%M:%SZ) ==="
[ "$(id -u)" = 0 ] || { entry_line ABORT_NOT_ROOT; exit 1; }
[ ! -L "$DIR/mod-launcher.sh" ] || { entry_line ABORT_CONTROLLER_SYMLINK; exit 1; }
actual=$(sha256sum "$DIR/mod-launcher.sh" | cut -d ' ' -f 1)
[ "$actual" = 'CONTROLLER_SHA256' ] || { entry_line "ABORT_CONTROLLER_HASH actual=$actual"; exit 1; }
entry_line "CONTROLLER_HASH_VERIFIED sha256=$actual" || exit 1
sync || exit 1
/bin/sh "$DIR/mod-launcher.sh"
exit $?
