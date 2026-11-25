#!/bin/sh
# Host-only D.S.B / SampleBoot in-place binds. Source after context helpers.
# No recursive, force or lazy unmounts. Unknown mounts remain the responsibility
# of their owner and are rejected by the normal exclusive-volume check.
BOOT_VIEW=/run/hs-boot/autorun
boot_row() {
    awk -v p="$1" -v d="$devno" -v f="$fs" -v r="$2" '
        $5==p {n++; if($3!=d || $(NF-2)!=f || $4!=r) bad=1}
        END {exit n!=1 || bad}' "$PROC/self/mountinfo"
}
boot_mounted() { awk -v p="$1" '$5==p {n++} END {exit !n}' "$PROC/self/mountinfo"; }
boot_view_valid() {
    # With shared SD mounts Linux propagates the AM overmount to the saved
    # loader view too. Accept only that exact two-layer parent/peer topology.
    boot_shadow=$(awk -v p="$BOOT_VIEW" -v a="$sd/autorun" -v d="$devno" -v f="$fs" -v r="$boot_root" '
        function peer( i) {for(i=7;i<=NF && $i!="-";i++) if($i~/^shared:/) return $i; return ""}
        $5==a {app_peer=peer()}
        $5==p {
            n++; if($3!=d || $(NF-2)!=f) bad=1
            if($4=="/autorun") {base++; base_id=$1; base_peer=peer()}
            else if($4==r) {shadow++; shadow_parent=$2; shadow_peer=peer()}
            else bad=1
        }
        END {
            if(bad || base!=1) exit 1
            if(n==1) {print 0; exit}
            if(n==2 && shadow==1 && shadow_parent==base_id && base_peer!="" && base_peer==shadow_peer && base_peer==app_peer) {print 1; exit}
            exit 1
        }' "$PROC/self/mountinfo") || return 1
}
boot_os_root_valid() {
    case "$boot_root" in
        /autorun/os/*) boot_kind=dsb; boot_leaf=${boot_root#/autorun/os/} ;;
        /autorun/mods/*) boot_kind=sampleboot; boot_leaf=${boot_root#/autorun/mods/} ;;
        *) return 1 ;;
    esac
    case "$boot_leaf" in ''|.|..|*[!A-Za-z0-9._-]*) return 1 ;; esac
}
sampleboot_loader_valid() {
    for sampleboot_file in "$1/autorun/autorun.sh" "$1/autorun/sampleboot/sampleboot.sh"; do
        [ -f "$sampleboot_file" ] && [ ! -L "$sampleboot_file" ] &&
            [ "$(readlink -f "$sampleboot_file")" = "$sampleboot_file" ] || return 1
    done
    grep -q SAMPLEBOOT "$1/autorun/autorun.sh"
}
sampleboot_view_close() {
    sampleboot_view="$D/sampleboot-view"
    if boot_mounted "$sampleboot_view"; then
        boot_row "$sampleboot_view" / || return 1
        umount "$sampleboot_view" || return 1
    fi
    rm -f "$D/boot-sampleboot-view"
}
sampleboot_binds_detach() {
    boot_step=sampleboot-am-marker
    boot_marker "$sd/autorun" || return 1
    # Non-recursive view of the card root exposes the loader hidden by the
    # module overmount. Validate both the loader and the physical module inode
    # before removing that one known bind. No loader file is modified.
    boot_step=sampleboot-card-view
    sampleboot_view="$D/sampleboot-view"
    [ ! -L "$sampleboot_view" ] && ! boot_mounted "$sampleboot_view" || return 1
    mkdir -p "$sampleboot_view" || return 1
    [ "$(readlink -f "$sampleboot_view")" = "$sampleboot_view" ] || return 1
    touch "$D/boot-sampleboot-view" || return 1
    mount --bind "$sd" "$sampleboot_view" || return 1
    boot_step=sampleboot-identity
    sampleboot_valid=0
    if sampleboot_loader_valid "$sampleboot_view" &&
        boot_marker "$sampleboot_view$boot_root" &&
        [ "$(stat -c '%d:%i' "$sd/autorun")" = "$(stat -c '%d:%i' "$sampleboot_view$boot_root")" ]; then
        sampleboot_valid=1
    fi
    sampleboot_view_close || return 1
    [ "$sampleboot_valid" = 1 ] || return 1
    boot_step=sampleboot-record-state
    printf '%s\n' "$boot_root" > "$D/boot-os-root" || return 1
    cd / || return 1
    boot_step=sampleboot-unmount-am
    umount "$sd/autorun" || return 1
    printf 'USB_DRIVE_SAMPLEBOOT_RELEASED module=%s\n' "$boot_root"
}
sampleboot_binds_restore() {
    if boot_mounted "$sd/autorun"; then
        boot_row "$sd/autorun" "$boot_root" || return 1
    else
        sampleboot_loader_valid "$sd" && boot_marker "$sd$boot_root" || return 1
        [ -d "$sd/autorun" ] && [ ! -L "$sd/autorun" ] || return 1
        mount --bind "$sd$boot_root" "$sd/autorun" || return 1
        boot_row "$sd/autorun" "$boot_root" || return 1
    fi
    boot_marker "$sd/autorun" || return 1
    rm -f "$D/boot-os-root"
    printf 'USB_DRIVE_SAMPLEBOOT_RESTORED module=%s\n' "$boot_root"
}
boot_marker() {
    [ -d "$1" ] && [ ! -L "$1" ] && [ "$(readlink -f "$1")" = "$1" ] || return 1
    [ -f "$1/sampleboot.inplace" ] && [ ! -L "$1/sampleboot.inplace" ] || return 1
    [ "$(stat -c %s "$1/sampleboot.inplace")" = 0 ] || return 1
    [ -f "$1/mod-launcher.sh" ] && [ ! -L "$1/mod-launcher.sh" ]
}
bootloader_binds_detach() {
    boot_step=detect
    if ! boot_mounted "$sd/autorun"; then return 0; fi
    boot_step=os-root
    boot_root=$(awk -v p="$sd/autorun" -v d="$devno" '$5==p && $3==d {n++; r=$4} END {if(n!=1)exit 1;print r}' "$PROC/self/mountinfo") || return 1
    boot_os_root_valid || return 1
    boot_step=am-mount
    boot_row "$sd/autorun" "$boot_root" || return 1
    if [ "$boot_kind" = sampleboot ]; then sampleboot_binds_detach; return $?; fi
    boot_step=loader-view-mount
    boot_view_valid || return 1
    boot_step=loader-view-path
    [ "$(readlink -f "$BOOT_VIEW")" = "$BOOT_VIEW" ] && [ ! -L "$BOOT_VIEW" ] || return 1
    if [ "$boot_shadow" = 0 ]; then
        boot_step=loader-script
        [ -f "$BOOT_VIEW/boot/boot.sh" ] && [ ! -L "$BOOT_VIEW/boot/boot.sh" ] || return 1
        grep -q HSBOOT_DIR "$BOOT_VIEW/boot/boot.sh" || return 1
        boot_alias="$BOOT_VIEW/os/$boot_leaf"
    else
        boot_alias="$BOOT_VIEW"
    fi
    boot_step=am-marker
    boot_marker "$sd/autorun" || return 1
    boot_step=loader-am-marker
    boot_marker "$boot_alias" || return 1
    boot_step=am-identity
    [ "$(stat -c '%d:%i' "$sd/autorun")" = "$(stat -c '%d:%i' "$boot_alias")" ] || return 1
    rm -f "$D/boot-config-wanted"
    if boot_mounted "$sd/autorun/os/mpcqol.conf"; then
        boot_step=config-mount
        boot_row "$sd/autorun/os/mpcqol.conf" /mpcqol.conf || return 1
        touch "$D/boot-config-wanted" || return 1
    fi
    boot_step=record-state
    printf '%s\n' "$boot_root" > "$D/boot-os-root" || return 1
    # The top overmount hides the older config mount: uncover it first.
    cd / || return 1
    boot_step=unmount-am
    umount "$sd/autorun" || return 1
    # Shared propagation removes the matching shadow at the same time.
    boot_step=uncover-loader-view
    boot_row "$BOOT_VIEW" /autorun || return 1
    [ -f "$BOOT_VIEW/boot/boot.sh" ] && [ ! -L "$BOOT_VIEW/boot/boot.sh" ] || return 1
    grep -q HSBOOT_DIR "$BOOT_VIEW/boot/boot.sh" || return 1
    boot_step=unmount-config
    if [ -f "$D/boot-config-wanted" ]; then umount "$sd/autorun/os/mpcqol.conf" || return 1; fi
    boot_step=unmount-loader-view
    umount "$BOOT_VIEW" || return 1
    printf 'USB_DRIVE_BOOTLOADER_RELEASED os=%s\n' "$boot_root"
}
bootloader_binds_prepare() {
    if bootloader_binds_detach; then return 0; fi
    printf 'USB_DRIVE_BOOTLOADER_FAILED step=%s device=%s fs=%s root=%s\n' "$boot_step" "$devno" "$fs" "${boot_root:-unknown}" >&2
    awk -v d="$devno" '$3==d {print "USB_DRIVE_BOOT_MOUNT " $0}' "$PROC/self/mountinfo" >&2
    for boot_path in "$BOOT_VIEW" "$BOOT_VIEW/boot/boot.sh" "$sd/autorun" "$BOOT_VIEW/os/${boot_leaf:-unknown}"; do
        printf 'USB_DRIVE_BOOT_PATH %s canonical=%s\n' "$boot_path" "$(readlink -f "$boot_path" 2>/dev/null || true)" >&2
        stat -c 'USB_DRIVE_BOOT_STAT dev=%d inode=%i size=%s type=%F' "$boot_path" >&2 || true
    done
    return 1
}
bootloader_binds_restore() {
    if [ -f "$D/boot-sampleboot-view" ]; then sampleboot_view_close || return 1; fi
    [ -f "$D/boot-os-root" ] || return 0
    [ ! -L "$D/boot-os-root" ] || return 1
    boot_root=$(cat "$D/boot-os-root") || return 1
    boot_os_root_valid || return 1
    if [ "$boot_kind" = sampleboot ]; then sampleboot_binds_restore; return $?; fi
    if boot_mounted "$sd/autorun"; then
        # A failed first unmount left the original complete mapping intact.
        boot_row "$sd/autorun" "$boot_root" && boot_view_valid || return 1
    else
        boot_marker "$sd$boot_root" || return 1
        [ -d "$sd/autorun" ] && [ ! -L "$sd/autorun" ] || return 1
        [ -f "$sd/autorun/boot/boot.sh" ] && [ ! -L "$sd/autorun/boot/boot.sh" ] || return 1
        if [ -f "$D/boot-config-wanted" ]; then
            if boot_mounted "$sd/autorun/os/mpcqol.conf"; then
                boot_row "$sd/autorun/os/mpcqol.conf" /mpcqol.conf || return 1
            else
                [ -f "$sd/mpcqol.conf" ] && [ ! -L "$sd/mpcqol.conf" ] || return 1
                [ -f "$sd/autorun/os/mpcqol.conf" ] && [ ! -L "$sd/autorun/os/mpcqol.conf" ] || return 1
                mount --bind "$sd/mpcqol.conf" "$sd/autorun/os/mpcqol.conf" || return 1
            fi
        fi
        if boot_mounted "$BOOT_VIEW"; then boot_row "$BOOT_VIEW" /autorun || return 1
        else
            [ -d "$BOOT_VIEW" ] && [ ! -L "$BOOT_VIEW" ] && [ "$(readlink -f "$BOOT_VIEW")" = "$BOOT_VIEW" ] || return 1
            mount --bind "$sd/autorun" "$BOOT_VIEW" || return 1
        fi
        mount --bind "$sd$boot_root" "$sd/autorun" || return 1
        boot_row "$sd/autorun" "$boot_root" && boot_view_valid || return 1
    fi
    if [ -f "$D/boot-config-wanted" ]; then boot_row "$sd/autorun/os/mpcqol.conf" /mpcqol.conf || return 1; fi
    boot_marker "$sd/autorun" || return 1
    rm -f "$D/boot-os-root" "$D/boot-config-wanted"
    printf 'USB_DRIVE_BOOTLOADER_RESTORED os=%s\n' "$boot_root"
}
