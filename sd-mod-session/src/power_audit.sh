#!/bin/sh
# Read-only, one-shot hardware inventory. No sysfs, I2C, GPIO or USB writes.
set -u
PATH=/usr/sbin:/usr/bin:/sbin:/bin
export PATH
READER=${MPC_POWER_READER:-}
[ -n "$READER" ] && [ -x "$READER" ] || exit 1
umask 077
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd) || exit 1
case "$DIR" in /media/*/autorun) ;; *) exit 1 ;; esac
SD=${DIR%/autorun}
[ "$(readlink -f "$SD")" = "$SD" ] || exit 1
awk -v p="$SD" '$2==p && $3=="exfat" && $4 ~ /(^|,)rw(,|$)/ {n++} END {exit n!=1}' /proc/mounts || exit 1
OUT="$SD/MPC_POWER_AUDIT.txt"
TMP="$SD/.MPC_POWER_AUDIT.next"
[ ! -L "$OUT" ] && [ ! -L "$TMP" ] || exit 1
[ ! -e "$TMP" ] || exit 1
set -C
exec 3> "$TMP" || exit 1
set +C
trap 'rm -f "$TMP"' EXIT
bounded_read() {
    [ -r "$1" ] || return 0
    printf '\n[%s]\n' "$1"
    "$READER" --read "$1" 2>&1 || printf '\nREAD_FAILED_OR_TIMEOUT\n'
    printf '\n'
}
{
 echo 'MPC_POWER_AUDIT_V1 READ_ONLY=1 CHARGER_SETTINGS_CHANGED=0'
 date -u +%Y-%m-%dT%H:%M:%SZ
 uname -a
 bounded_read /proc/uptime
 bounded_read /proc/device-tree/model
 bounded_read /proc/device-tree/compatible
 echo '=== Power supply classes ==='
 count=0
 for device in /sys/class/power_supply/*; do
  [ -d "$device" ] || continue
  count=$((count+1)); [ "$count" -le 16 ] || break
  printf '\nDEVICE=%s REAL=%s DRIVER=%s\n' "$device" "$(readlink -f "$device")" "$(readlink -f "$device/device/driver" 2>/dev/null || true)"
  ls -ln "$device"/ 2>&1
  for attr in uevent type status present online capacity capacity_level health technology voltage_now voltage_min_design voltage_max_design current_now current_avg power_now charge_now charge_full charge_full_design energy_now energy_full temp manufacturer model_name scope usb_type input_current_limit input_voltage_limit current_max voltage_max constant_charge_current constant_charge_current_max constant_charge_voltage constant_charge_voltage_max charge_control_start_threshold charge_control_end_threshold charge_control_limit charge_control_limit_max charge_behaviour charging_enabled charge_enabled input_suspend charge_type; do
   bounded_read "$device/$attr"
  done
 done
 echo '=== I2C device identities (no bus transactions) ==='
 count=0
 for device in /sys/bus/i2c/devices/*; do
  [ -r "$device/name" ] || continue
  count=$((count+1)); [ "$count" -le 64 ] || break
  bounded_read "$device/name"
  printf 'DRIVER=%s\n' "$(readlink -f "$device/driver" 2>/dev/null || true)"
 done
 echo '=== USB/type-C role information ==='
 count=0
 for device in /sys/class/typec/* /sys/class/usb_role/* /sys/class/extcon/*; do
  [ -d "$device" ] || continue
  count=$((count+1)); [ "$count" -le 32 ] || break
  for attr in data_role power_role port_type role state name; do bounded_read "$device/$attr"; done
 done
 echo 'MPC_POWER_AUDIT_END'
} >&3 2>&1
exec 3>&-
mv -f "$TMP" "$OUT" || exit 1
trap - EXIT
exit 0
