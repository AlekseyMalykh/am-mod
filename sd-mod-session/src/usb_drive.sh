#!/bin/sh
# Host-only SD ownership transaction. Called after the clone and its file
# services stopped. Never force/lazy-unmount; never export a mounted volume.
set -eu
R=/run/mpc-clone-trial-v1
G=/sys/kernel/config/usb_gadget/ac50
SYS=/sys/class/block
PROC=/proc
D=$R/usb-drive
DISKS=edisksd.service
GADGET=/usr/bin/az0x-usb-gadget
GADGET_UNIT=az0x-usb-gadget@ac50.service
phase() { printf '%s\n' "$1" > "$D/phase.next"; mv "$D/phase.next" "$D/phase"; }
die() { printf 'USB_DRIVE_ERROR %s\n' "$*" >&2; printf '%s\n' "$1" > "$D/error"; phase error; exit 1; }
read_context() {
    [ -d "$D" ] && [ ! -L "$D" ] || return 1
    sd=$(cat "$D/mount") || return 1
    device=$(cat "$D/device") || return 1
    fs=$(cat "$D/fs") || return 1
    opts=$(cat "$D/options") || return 1
    devno=$(cat "$D/devno") || return 1
    case "$sd" in /media/*) ;; *) return 1 ;; esac
    case "${sd#/media/}" in ''|*[!A-Za-z0-9._-]*) return 1 ;; esac
    case "$device" in /dev/mmcblk[0-9]p[0-9]*) ;; *) return 1 ;; esac
    part=${device#/dev/}; card=${part%p*}
    case "$part" in *[!a-z0-9]*) return 1 ;; esac
    case "$fs" in exfat|vfat) ;; *) return 1 ;; esac
    case "$opts" in ''|*[!A-Za-z0-9,=._:-]*) return 1 ;; esac
    case "$devno" in ''|*[!0-9:]*) return 1 ;; esac
    # The MPC's physical SD slot advertises removable=0. Card bus type,
    # not that block-device hint, distinguishes SD from internal eMMC.
    [ "$(cat "$SYS/$card/device/type" 2>/dev/null)" = SD ] || return 1
    [ "$(cat "$SYS/$part/dev")" = "$devno" ] || return 1
    [ "$(sha256sum "$SYS/$card/device/cid" | cut -d ' ' -f 1)" = "$(cat "$D/cid-sha256")" ] || return 1
}
mounted() { awk -v p="$sd" '$5==p {found=1} END {exit !found}' "$PROC/self/mountinfo"; }
exclusive() {
    # Private namespaces can retain an SD bind even after the host unmount.
    # A remaining reader/writer makes the transfer unavailable, never unsafe.
    for info in "$PROC"/[0-9]*/mountinfo; do
        [ -r "$info" ] || continue
        if awk -v d="$devno" '$3==d {found=1} END {exit !found}' "$info"; then
            printf 'USB_DRIVE_BUSY namespace=%s\n' "$info" >&2
            return 1
        fi
    done
}
restore_disks() {
    if [ -f "$D/mask-owned" ]; then
        systemctl unmask --runtime "$DISKS" || return 1
        rm -f "$D/mask-owned"
    fi
    if [ -f "$D/disks-was-active" ]; then systemctl start "$DISKS" || return 1; fi
}
restore() {
    [ -f "$D/device" ] || return 0
    if [ ! -f "$D/context-ready" ] && [ ! -f "$D/switch-intent" ] && [ ! -f "$D/unmounted" ] && [ ! -f "$D/mask-owned" ]; then phase done;return 0;fi
    phase returning
    if [ -f "$D/switch-intent" ]; then
        # Unbind first. Disable/removal waits for the gadget's I/O workers;
        # do not mount until its backing block device has been closed.
        if [ -f "$G/UDC" ]; then printf '\n' > "$G/UDC" || return 1; fi
        "$GADGET" switch || return 1
        # switch triggers udev asynchronously. Wait for the actual oneshot
        # setup job, not merely for the trigger command to return.
        systemctl start "$GADGET_UNIT" || return 1
        [ ! -d "$G/functions/mass_storage.block" ] || return 1
        rm -f "$D/switch-intent"
    fi
    # Always detach the USB LUN before checking for a removed/replaced card.
    # An identity mismatch must never leave any block device shared.
    if ! read_context; then
        [ ! -f "$D/unmounted" ] && [ ! -f "$D/mask-owned" ] || return 1
        phase done
        return 0
    fi
    sync
    if ! mounted; then
        exclusive || return 1
        [ -d "$sd" ] && [ ! -L "$sd" ] || return 1
        mount -t "$fs" -o "$opts" "$device" "$sd" || return 1
    fi
    [ "$(readlink -f "$sd")" = "$sd" ] || return 1
    awk -v p="$sd" -v d="$devno" '$5==p && $3==d && $6 ~ /(^|,)rw(,|$)/ {ok=1} END {exit !ok}' "$PROC/self/mountinfo" || return 1
    restore_disks || return 1
    rm -f "$D/unmounted"
    phase done
}
case "${1:-}" in
 prepare)
    [ "$#" = 2 ] || exit 2
    sd=${2%/MPC-Sample}
    [ "$2" = "$sd/MPC-Sample" ] || exit 2
    case "$sd" in /media/*) ;; *) exit 2 ;; esac
    case "${sd#/media/}" in ''|*[!A-Za-z0-9._-]*) exit 2 ;; esac
    [ "$(readlink -f "$sd")" = "$sd" ] || exit 1
    [ ! -e "$D" ] || { [ -d "$D" ] && [ ! -L "$D" ] && [ "$(cat "$D/phase")" = done ]; } || exit 1
    mkdir -p "$D"
    phase preparing
    rm -f "$D/context-ready" "$D/error" "$D/exit-request"
    # Source comes only from the kernel mount table, not the application.
    row=$(awk -v p="$sd" '$2==p && ($3=="exfat" || $3=="vfat") && $4 ~ /(^|,)rw(,|$)/ {n++;r=$1" "$3" "$4} END {if(n!=1)exit 1;print r}' "$PROC/mounts") || die NOT_ONE_SD_MOUNT
    read -r device fs opts <<EOF
$row
EOF
    printf '%s\n' "$sd" > "$D/mount"
    printf '%s\n' "$device" > "$D/device"
    printf '%s\n' "$fs" > "$D/fs"
    printf '%s\n' "$opts" > "$D/options"
    case "$device" in /dev/mmcblk[0-9]p[0-9]*) ;; *) die NOT_EXTERNAL_MMC_PARTITION ;; esac
    part=${device#/dev/}; card=${part%p*}
    media_type=$(cat "$SYS/$card/device/type" 2>/dev/null || true)
    [ "$media_type" = SD ] || die NOT_SD_CARD
    printf 'USB_DRIVE_MEDIA type=SD removable_hint=%s\n' "$(cat "$SYS/$card/removable" 2>/dev/null || echo unavailable)"
    cat "$SYS/$part/dev" > "$D/devno"
    sha256sum "$SYS/$card/device/cid" | cut -d ' ' -f 1 > "$D/cid-sha256"
    read_context || die BAD_SD_IDENTITY
    touch "$D/context-ready"
    rm -f "$D/disks-was-active"
    if systemctl is-active --quiet "$DISKS"; then touch "$D/disks-was-active"; fi
    if [ "$(systemctl is-enabled "$DISKS" 2>/dev/null || true)" != masked ]; then
        # Record intent before masking, so interrupted preparation is undoable.
        touch "$D/mask-owned"
        systemctl mask --runtime "$DISKS" || die DISKS_MASK_FAILED
    fi
    systemctl stop "$DISKS" || die DISKS_STOP_FAILED
    sync
    umount "$sd" || die SD_BUSY
    touch "$D/unmounted"
    mounted && die SD_STILL_MOUNTED
    exclusive || die SD_PRIVATE_MOUNT_BUSY
    phase ready
    ;;
 share)
    read_context || exit 1
    [ -f "$D/unmounted" ] && ! mounted && exclusive || die SD_NOT_EXCLUSIVE
    touch "$D/switch-intent"
    "$GADGET" switch sd-access || die GADGET_SWITCH_FAILED
    systemctl start "$GADGET_UNIT" || die GADGET_SETUP_FAILED
    lun="$G/functions/mass_storage.block/lun.0"
    ready_wait=0
    while [ ! -d "$lun" ] || [ ! -f "$G/UDC" ] || [ -z "$(cat "$G/UDC" 2>/dev/null)" ]; do
        ready_wait=$((ready_wait+1))
        [ "$ready_wait" -lt 100 ] || die MASS_STORAGE_SETUP_TIMEOUT
        sleep 0.1
    done
    udc=$(cat "$G/UDC") || die NO_USB_CONTROLLER
    [ -n "$udc" ] || die NO_USB_CONTROLLER
    case "$udc" in *[!A-Za-z0-9._:-]*) die BAD_USB_CONTROLLER ;; esac
    printf '\n' > "$G/UDC" || die USB_UNBIND_FAILED
    [ "$(cat "$lun/removable")" = 1 ] || printf '1\n' > "$lun/removable" || die LUN_REMOVABLE_FAILED
    [ "$(cat "$lun/ro")" = 0 ] || printf '0\n' > "$lun/ro" || die LUN_WRITABLE_FAILED
    printf '%s\n' "$device" > "$lun/file" || die LUN_ATTACH_FAILED
    printf '%s\n' "$udc" > "$G/UDC" || die USB_BIND_FAILED
    phase waiting
    seen=0;waiting=0
    while :; do
        if [ -f "$D/exit-request" ] && [ ! -L "$D/exit-request" ] &&
           [ "$(cat "$D/exit-request")" = 'AM_USB_EXIT 1' ]; then
            printf 'USB_DRIVE_LOCAL_EXIT\n' >&2
            break
        fi
        [ -d "$lun" ] || die GADGET_DISAPPEARED
        # Host Eject closes the backing device. Physical disconnect also ends
        # sharing, but cannot recover files still cached on the computer.
        [ -n "$(cat "$lun/file")" ] || break
        usb_state=$(cat "/sys/class/udc/$udc/state") || die USB_STATE_UNREADABLE
        case "$usb_state" in
          configured) seen=1;phase sharing ;;
          'not attached') [ "$seen" = 0 ] || break ;;
        esac
        if [ "$seen" = 0 ]; then waiting=$((waiting+1));[ "$waiting" -lt 120 ] || break; fi
        sleep 1
    done
    restore || die RESTORE_FAILED
    ;;
 restore) restore ;;
 *) echo 'usage: usb-drive prepare <MPC-Sample root> | share | restore' >&2; exit 2 ;;
esac
