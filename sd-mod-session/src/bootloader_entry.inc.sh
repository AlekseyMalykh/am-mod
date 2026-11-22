# SampleBoot and HS Boot honour sampleboot.inplace, but hand off with ac50
# stopped. Keep the verified controller's original stock/runtime checks.
# This fragment runs after its hash check, before the optional power audit.
if systemctl is-active --quiet mpc-clone-trial-v1-supervisor.service 2>/dev/null &&
   systemctl is-active --quiet mpc-clone-trial-v1-clone.service 2>/dev/null; then
    entry_line BOOTLOADER_ALREADY_RUNNING
    exit 0
fi



# D.S.B Boot (sampleboot.inplace): its "ok" write goes to this folder's own hs-boot.state, because the bind of this
# folder over autorun/ also shows in the menu's view of autorun/. The card's real state keeps "pending=" and counts
# a crash on every start, until two make Stock the default. Once the menu has written its "ok" here, carry that
# state to the real file through a plain bind of the card root (its submounts are not copied). Everything is logged.
# The menu's journal (RAM only) is copied beside it for the same reason.
if [ -r /run/hs-boot/boot.log ]; then
    setsid sh -c '
D="$1"; S="${D%/autorun}"; L="$D/dsb-state-fix.log"
for t in 20 20 30; do sleep $t; cp /run/hs-boot/boot.log "$D/dsb-boot.log" 2>/dev/null; done
[ -f "$D/hs-boot.state" ] || { echo "$(date -u +%FT%TZ) no own state" >> "$L"; exit 0; }
grep -q "^pending=$" "$D/hs-boot.state" || { echo "$(date -u +%FT%TZ) own state not ok: $(tr "\n" " " < "$D/hs-boot.state")" >> "$L"; exit 0; }
r=$(mktemp -d /run/am-sdroot-XXXXXX) || exit 0
if mount --bind "$S" "$r" 2>/dev/null; then
    if [ -f "$r/autorun/hs-boot.state" ] && grep -q "^pending=." "$r/autorun/hs-boot.state"; then
        echo "$(date -u +%FT%TZ) real state before: $(tr "\n" " " < "$r/autorun/hs-boot.state")" >> "$L"
        cp "$D/hs-boot.state" "$r/autorun/hs-boot.state" && echo "$(date -u +%FT%TZ) real state replaced by: $(tr "\n" " " < "$D/hs-boot.state")" >> "$L"
    else echo "$(date -u +%FT%TZ) real state not pending or missing" >> "$L"; fi
    umount "$r" 2>/dev/null
else echo "$(date -u +%FT%TZ) bind of $S failed" >> "$L"; fi
rmdir "$r" 2>/dev/null' sh "$DIR" </dev/null >/dev/null 2>&1 &
fi

am_entry_lock=/run/am-mod-entry-bootstrap
if ! mkdir "$am_entry_lock" 2>/dev/null; then
    # Starting clean stock invokes its SD runner again. The first entry still
    # owns the handoff: a second entry must not start another controller.
    entry_line BOOTLOADER_ENTRY_BUSY
    exit 0
fi
am_entry_finish() {
    am_entry_status=$1
    rm -f "$am_entry_lock/pid"
    rmdir "$am_entry_lock" 2>/dev/null || true
    finish_entry "$am_entry_status"
}
trap 'am_entry_finish $?' EXIT
printf '%s\n' "$$" > "$am_entry_lock/pid" || exit 1

if ! systemctl is-active --quiet ac50.service 2>/dev/null; then
    # Do not start an unknown replacement or a service with injected options.
    # The controller repeats these checks after stock has started.
    [ -f "$DIR/sampleboot.inplace" ] && [ ! -L "$DIR/sampleboot.inplace" ] || {
        entry_line ABORT_BOOTLOADER_MARKER_MISSING; exit 1;
    }
    [ "$(sha256sum '/usr/bin/MPC Sample' | cut -d ' ' -f 1)" = 'b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508' ] || {
        entry_line ABORT_BOOTLOADER_STOCK_HASH; exit 1;
    }
    [ "$(sha256sum /usr/lib/systemd/system/ac50.service | cut -d ' ' -f 1)" = '6b2dc52826263daf19c6f1a945c94bd7ba69b15eb7843df68eebdd63ea311e5d' ] || {
        entry_line ABORT_BOOTLOADER_STOCK_UNIT; exit 1;
    }
    [ "$(systemctl show ac50.service -p FragmentPath --value)" = /usr/lib/systemd/system/ac50.service ] &&
    [ -z "$(systemctl show ac50.service -p DropInPaths --value)" ] || {
        entry_line ABORT_BOOTLOADER_STOCK_OVERRIDE; exit 1;
    }
    entry_line BOOTLOADER_START_CLEAN_STOCK
    systemctl start ac50.service || { entry_line ABORT_BOOTLOADER_STOCK_START; exit 1; }
    systemctl is-active --quiet ac50.service || { entry_line ABORT_BOOTLOADER_STOCK_INACTIVE; exit 1; }
fi

# Type=simple may become active while launch-ac50 is still its shell/loader.
# Wait for the exact stock executable; never relax the controller's inode
# identity check to accept that transitional process.
am_stock_identity=$(stat -Lc '%d:%i' '/usr/bin/MPC Sample') || exit 1
am_stock_wait=0
while :; do
    am_stock_pid=$(systemctl show ac50.service -p MainPID --value)
    case "$am_stock_pid" in ''|*[!0-9]*) am_stock_pid=0 ;; esac
    if [ "$am_stock_pid" -gt 1 ] &&
       [ "$(stat -Lc '%d:%i' "/proc/$am_stock_pid/exe" 2>/dev/null)" = "$am_stock_identity" ]; then
        entry_line BOOTLOADER_STOCK_READY
        break
    fi
    am_stock_wait=$((am_stock_wait + 1))
    [ "$am_stock_wait" -lt 20 ] || { entry_line ABORT_BOOTLOADER_STOCK_EXEC_TIMEOUT; exit 1; }
    sleep 1
done
