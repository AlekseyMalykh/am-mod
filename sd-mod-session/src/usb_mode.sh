#!/bin/sh
# Host-side boot transaction. No writes to the firmware: a RAM file is bound
# over the stock scheme and unmounted before stock recovery. The application
# remains unable to access configfs, block devices or systemd.
set -eu
R=/run/mpc-clone-trial-v1
SCHEME=/usr/lib/az0x-usb-gadget/ac50.scheme
GADGET=/sys/kernel/config/usb_gadget/ac50/functions/uac2.audio
UNIT=az0x-usb-gadget@ac50.service
MODULES=/sys/module
NAMED_KERNEL=6.12.60-imb-2025-12-03-rt13
restore() {
    rm -f "$R/app/usb-audio-18.active"
    if [ -f "$R/usb-scheme-bound" ]; then
        # Remove all function instances before unloading the RAM driver. The
        # kernel refuses an unload if any instance still holds a reference.
        systemctl stop "$UNIT" || return 1
        if [ -f "$R/usb-names-owned" ]; then
            if [ -d "$MODULES/am_usb_names" ]; then rmmod am_usb_names || return 1; fi
            rm -f "$R/usb-names-owned"
        fi
        if awk -v p="$SCHEME" '$5==p {found=1} END {exit !found}' /proc/self/mountinfo; then
            umount "$SCHEME" || return 1
        fi
        systemctl start "$UNIT" || return 1
        rm -f "$R/usb-scheme-bound"
    fi
}
case "${1:-}" in
 restore) restore ;;
 prepare)
    [ "$#" = 2 ] || exit 2
    settings=$2/Projects
    # Every other choice is ordinary stereo. Stock FX mode is stereo too.
    [ -f "$settings/.am-mod-usb-audio" ] && [ ! -L "$settings/.am-mod-usb-audio" ] || exit 0
    [ "$(cat "$settings/.am-mod-usb-audio")" = 'AM_USBAUDIO 1' ] || exit 0
    if [ -f "$settings/.am-mod-fx-mode" ]; then
        [ ! -L "$settings/.am-mod-fx-mode" ] || exit 1
        [ "$(cat "$settings/.am-mod-fx-mode")" != 'AM_FXMODE 0' ] || exit 0
    fi
    [ ! -f "$R/usb-scheme-bound" ] || { restore; }
    [ "$(uname -r)" = "$NAMED_KERNEL" ] || exit 1
    [ -s "$R/am_usb_names.ko" ] && [ ! -L "$R/am_usb_names.ko" ] || exit 1
    [ ! -d "$MODULES/am_usb_names" ] || exit 1
    [ -f "$SCHEME" ] && [ ! -L "$SCHEME" ] || exit 1
    [ "$(grep -c '^[[:space:]]*p_chmask = 3;' "$SCHEME")" = 1 ] || exit 1
    [ "$(grep -c '^[[:space:]]*p_srate = 44100;' "$SCHEME")" = 1 ] || exit 1
    [ "$(grep -c '^[[:space:]]*p_ssize = 4;' "$SCHEME")" = 1 ] || exit 1
    sed 's/^\([[:space:]]*p_chmask = \)3;/\1262143;/' "$SCHEME" > "$R/usb-ac50.scheme"
    chmod 400 "$R/usb-ac50.scheme"
    # Mark intent first, so recovery also handles an interrupted transaction.
    touch "$R/usb-scheme-bound"
    if ! mount --bind "$R/usb-ac50.scheme" "$SCHEME"; then rm -f "$R/usb-scheme-bound"; exit 1; fi
    if ! systemctl stop "$UNIT"; then restore || exit 1;exit 1;fi
    touch "$R/usb-names-owned"
    if ! insmod "$R/am_usb_names.ko" || ! systemctl start "$UNIT" || [ "$(cat "$GADGET/p_chmask" 2>/dev/null)" != 262143 ]; then
        restore || exit 1
        exit 1
    fi
    printf '1\n' > "$R/app/usb-audio-18.active"
    ;;
 *) echo 'usage: usb-mode prepare <MPC-Sample directory> | restore' >&2; exit 2 ;;
esac
