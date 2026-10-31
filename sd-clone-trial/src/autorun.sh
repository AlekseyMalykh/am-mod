#!/bin/sh
# MPC Sample 1.3.0.12: one-shot, unmodified clone trial. No firmware tool.
set -eu
PATH=/usr/sbin:/usr/bin:/sbin:/bin
export PATH
umask 077
R=/run/mpc-clone-trial-v1
U=mpc-clone-trial-v1
STOCK='/usr/bin/MPC Sample'
STOCK_UNIT=ac50.service
STOCK_SHA=b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508
UNIT_SHA=6b2dc52826263daf19c6f1a945c94bd7ba69b15eb7843df68eebdd63ea311e5d
DROP=/run/systemd/system/ac50.service.d/90-mpc-clone-trial.conf
APP=/run/mpc-trial-app
POLICY_SHA=bf11ff3862e252bd78254ef7b64c6293740df95e6cc1f837eb7a0337a93f0562
PROXY_SHA=751a39dd6c4ce06a11839f454607319ea6d90eb334b481233fa6cf6968fc2689
CLONE_SECONDS=180
CLONE_RUNTIME_EXPECTED=3min
SUPERVISOR_SECONDS=210
GUARD_SECONDS=240
PROXY_SECONDS=330

hash() { sha256sum "$1" | cut -d ' ' -f 1; }
prop() { systemctl show "$1" -p "$2" --value; }
log() { printf '%s %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$*" >> "$R/events.txt"; }
fail() { log "ABORT: $*"; exit 1; }
# Silent WAV names are visible in the stock Sample Browser. No display, audio,
# pad LED, or scripting IPC is used for notification. This is a manual status view.
status() {
    [ -s "$R/status.wav" ] || return 1
    status_report=$(cat "$R/report-path") || return 1
    status_sd=${status_report%/autorun/MPC_CLONE_TRIAL.txt}
    [ -d "$status_sd/autorun" ] || return 1
    [ "$(stat -c %d "$status_sd/autorun" 2>/dev/null)" = "$(cat "$R/sd-device")" ] || return 1
    status_dir="$status_sd/MPC_TRIAL_STATUS"
    mkdir -p "$status_dir" || return 1
    cp "$R/status.wav" "$status_dir/.next" || return 1
    # Only our known markers; never delete user samples with a wildcard.
    for status_name in READY RUNNING RETURNING 'TEST ENDED' 'STOPPED BEFORE TEST' 'RECOVERY ERROR'; do
        rm -f "$status_dir/$status_name.wav" || return 1
    done
    mv -f "$status_dir/.next" "$status_dir/$1.wav" || return 1
    log "SD_STATUS: $1"
}
on_exit() {
    if [ "$1" -ne 0 ]; then
        log "EXIT_ERROR: mode=${exit_mode:-main} code=$1"
        if [ "${exit_mode:-main}" = recover ]; then
            status 'RECOVERY ERROR' || true
        elif [ -f "$R/switch-intent" ]; then
            status RETURNING || true
        else
            status 'STOPPED BEFORE TEST' || true
        fi
    fi
    if [ "${exit_mode:-main}" = main ] && [ "$1" -ne 0 ] && [ ! -f "$R/switch-intent" ]; then
        systemctl stop "$U-bus-proxy.service" 2>/dev/null || true
    fi
    export_report || true
}
export_report() {
    # A path remembered from a validated SD mount; never recreate it if removed.
    [ -f "$R/report-path" ] || return 0
    dst=$(cat "$R/report-path")
    dir=${dst%/*}
    [ -d "$dir" ] || return 0
    expected=$(cat "$R/sd-device")
    current=$(stat -c %d "$dir" 2>/dev/null || true)
    [ "$current" = "$expected" ] || return 0
    {
        echo MPC_CLONE_TRIAL_V1
        cat "$R/events.txt"
        for f in "$R/app/probe.txt" "$R/app/clone.txt" "$R/proxy/proxy.txt"; do
            if [ -f "$f" ]; then echo "--- ${f##*/} ---"; cat "$f"; fi
        done
        for service in "$U-probe" "$U-clone" "$U-supervisor" "$U-recover" "$U-bus-proxy"; do
            systemctl show "$service.service" -p ActiveState -p SubState -p Result -p ExecMainStatus 2>&1 || true
        done
        echo '--- Probe/application/recovery journal (last 80 lines) ---'
        journalctl -u "$U-probe.service" -u "$U-clone.service" -u "$U-recover.service" -u "$U-bus-proxy.service" -n 80 --no-pager 2>&1 || true
    } > "$dst.tmp" && mv -f "$dst.tmp" "$dst"
}

masked_nodes_check() {
    [ -s "$APP/mask-identities" ] || { echo 'CHECK_FAILED: missing_mask_identities'; return 1; }
    while IFS= read -r node; do
        actual_mask=$(stat -Lc '%d:%i:%f:%t:%T:%u:%g' "$node") || {
            echo "CHECK_FAILED: stat_mask $node"; return 1;
        }
        grep -Fxq "$actual_mask" "$APP/mask-identities" || {
            echo "CHECK_FAILED: not_systemd_inaccessible_node $node actual=$actual_mask"; return 1;
        }
        awk -v p="$node" '$5==p && $6 ~ /(^|,)ro(,|$)/ {ok=1} END {exit !ok}' /proc/self/mountinfo || {
            echo "CHECK_FAILED: mask_not_mounted_read_only $node"; return 1;
        }
    done < "$APP/blocked-nodes"
    echo RAW_DEVICE_MASK_IDENTITIES_VERIFIED
}

bus_call() {
    /usr/bin/dbus-send --system --print-reply --reply-timeout=3000 \
        --dest=org.freedesktop.DBus / "org.freedesktop.DBus.$1"
}

proxy_check() {
    DBUS_SYSTEM_BUS_ADDRESS=unix:path=/run/dbus/system_bus_socket
    export DBUS_SYSTEM_BUS_ADDRESS
    actual_socket=$(stat -Lc '%d:%i' /run/dbus/system_bus_socket) || return 1
    [ "$actual_socket" = "$(cat "$APP/proxy-socket-id")" ] || { echo 'PROXY_FAILED: wrong_socket'; return 1; }
    connected_bus_id=$(bus_call GetId | awk '$1=="string" {gsub(/"/, "", $2); print $2}')
    [ -n "$connected_bus_id" ] && [ "$connected_bus_id" = "$(cat "$APP/host-bus-id")" ] || {
        echo 'PROXY_FAILED: wrong_upstream'; return 1;
    }
    # Harmless read-only method: must be blocked because systemd is not allowed.
    if /usr/bin/dbus-send --system --print-reply --reply-timeout=3000 \
        --dest=org.freedesktop.systemd1 /org/freedesktop/systemd1 \
        org.freedesktop.systemd1.Manager.ListUnits > "$APP/blocked-bus-check.txt" 2>&1; then
        echo 'PROXY_FAILED: systemd_access_allowed'; return 1
    fi
    grep -Eq 'org.freedesktop.DBus.Error.(AccessDenied|ServiceUnknown|NameHasNoOwner)' "$APP/blocked-bus-check.txt" || {
        cat "$APP/blocked-bus-check.txt"; echo 'PROXY_FAILED: unexpected_denial'; return 1;
    }
    echo 'FILTERED_STOCK_DBUS_READY (systemd access rejected)'
}

sandbox_check() {
    echo 'SANDBOX_CHECK_BEGIN'
    # All disk-backed mounts must be read-only in this namespace.
    awk '
      $3 ~ /^(ext[234]|exfat|vfat|f2fs|btrfs|xfs|ntfs|ntfs3|fuseblk|overlay)$/ {
        if ($4 !~ /(^|,)ro(,|$)/) { print "WRITABLE_DISK: " $2; bad=1 }
      }
      END { exit bad }
    ' /proc/mounts || { echo 'CHECK_FAILED: disk_mounts_read_only'; return 1; }
    here=$(readlink /proc/self/ns/mnt) || { echo 'CHECK_FAILED: read_namespace'; return 1; }
    host_ns=$(cat "$APP/host-mnt-ns") || { echo 'CHECK_FAILED: read_host_namespace'; return 1; }
    echo "NAMESPACE_ACTUAL=$here HOST=$host_ns"
    [ "$here" != "$host_ns" ] || { echo 'CHECK_FAILED: namespace_not_isolated'; return 1; }
    [ ! -S /run/systemd/private ] || { echo 'CHECK_FAILED: host_systemd_socket_visible'; return 1; }
    [ -S /run/dbus/system_bus_socket ] || { echo 'CHECK_FAILED: filtered_bus_socket_missing'; return 1; }
    for tmp_path in /run /tmp /var/tmp; do
        tmp_type=$(stat -f -c %T "$tmp_path") || { echo "CHECK_FAILED: stat $tmp_path"; return 1; }
        echo "FILESYSTEM $tmp_path=$tmp_type"
        [ "$tmp_type" = tmpfs ] || { echo "CHECK_FAILED: not_tmpfs $tmp_path"; return 1; }
    done
    # BusyBox 1.36 test -r/-w returns true for root even without DAC capabilities.
    # Verify exact identities of systemd's mode-000, device-0:0 placeholders and
    # read-only mounts instead. No actual storage node is opened for this check.
    masked_nodes_check || return 1
    echo "NAMESPACE=$here"
    echo 'DISK_MOUNTS_READ_ONLY=yes'
    echo 'RAW_DEVICE_NODES_INACCESSIBLE=yes'
}

case "${1:-main}" in
  probe)
    exec > "$APP/probe.txt" 2>&1
    sandbox_check || { echo PROBE_FAILED; exit 1; }
    proxy_check || { echo FILTERED_DBUS_PROBE_FAILED; exit 1; }
    "$APP/busybox" true
    printf 'probe write in RAM\n' > "$APP/ram-write-check"
    echo PROBE_OK
    exit 0
    ;;
  clone)
    exec >> "$APP/clone.txt" 2>&1
    sandbox_check || { echo CLONE_SANDBOX_FAILED; exit 1; }
    proxy_check || { echo FILTERED_DBUS_START_FAILED; exit 1; }
    [ "$(hash "$STOCK")" = "$STOCK_SHA" ] || exit 1
    echo '--- Display launch context ---'
    awk '/^(CapInh|CapPrm|CapEff|CapBnd|NoNewPrivs):/ {print}' /proc/self/status
    for display_node in /dev/tty0 /dev/dri/card* /dev/rga2 /dev/v4l/by-path/platform-*.rga-video-index0; do
        [ -c "$display_node" ] || continue
        stat -Lc '%n mode=%a uid=%u gid=%g device=%t:%T' "$display_node" || true
    done
    echo 'STARTING_UNMODIFIED_COPY (process startup is not UI readiness)'
    ulimit -S -s 1024
    export MALLOC_ARENA_MAX=1
    exec setarch -R -- "$STOCK"
    ;;
  proxy)
    exec >> "$R/proxy/proxy.txt" 2>&1
    [ "$(hash "$R/dbus-proxy")" = "$PROXY_SHA" ] || exit 1
    [ "$(hash "$R/proxy-policy.args")" = "$POLICY_SHA" ] || exit 1
    set -- unix:path=/run/dbus/system_bus_socket "$R/proxy/system_bus_socket"
    while IFS= read -r rule; do
        [ -n "$rule" ] || continue
        set -- "$@" "$rule"
    done < "$R/proxy-policy.args"
    exec "$R/dbus-proxy" "$@"
    ;;
  stock)
    # Used by a RAM-only ac50 drop-in. Deliberately skips firmware-tool.
    [ "$(hash "$STOCK")" = "$STOCK_SHA" ] || exit 1
    ulimit -S -s 1024
    export MALLOC_ARENA_MAX=1
    exec setarch -R -- "$STOCK"
    ;;
  recover)
    exit_mode=recover
    trap 'on_exit $?' EXIT
    [ ! -f "$R/recovered" ] || exit 0
    log RECOVERY_BEGIN
    status RETURNING || log STATUS_WRITE_FAILED
    systemctl --no-block stop "$U-supervisor.service" || true
    systemctl stop "$U-clone.service" || true
    # Never start a second hardware owner while the clone may still be alive.
    [ "$(prop "$U-clone.service" MainPID)" = 0 ] || fail CLONE_STILL_RUNNING
    if [ -f "$R/switch-intent" ]; then
        [ -f "$DROP" ] || fail MISSING_STOCK_RETURN_OVERRIDE
        systemctl daemon-reload
        systemctl reset-failed "$STOCK_UNIT" || true
        systemctl start "$STOCK_UNIT" || fail STOCK_START_FAILED
        tries=0
        matched=0
        while [ "$tries" -lt 15 ]; do
            pid=$(prop "$STOCK_UNIT" MainPID)
            if [ "$pid" -gt 1 ] && [ "$(stat -Lc '%d:%i' "/proc/$pid/exe" 2>/dev/null || true)" = "$(stat -Lc '%d:%i' "$STOCK")" ]; then
                matched=1
                break
            fi
            tries=$((tries+1))
            sleep 1
        done
        [ "$matched" = 1 ] || fail STOCK_EXECUTABLE_NOT_OBSERVED
        sleep 2
        systemctl is-active --quiet "$STOCK_UNIT" || fail STOCK_NOT_ACTIVE
        [ "$(prop "$STOCK_UNIT" MainPID)" = "$pid" ] || fail STOCK_RESTARTED_DURING_CHECK
        log "STOCK_PROCESS_RESTORED pid=$pid (confirm the screen yourself)"
    else
        log NO_STOCK_STOP_REQUESTED
    fi
    systemctl stop "$U-bus-proxy.service" || true
    touch "$R/recovered"
    systemctl stop "$U-guard.timer" || true
    log 'DONE: runtime files and stock launch override disappear on reboot'
    if [ -f "$R/switch-intent" ]; then
        status 'TEST ENDED' || log STATUS_WRITE_FAILED
    else
        status 'STOPPED BEFORE TEST' || log STATUS_WRITE_FAILED
    fi
    export_report
    exit 0
    ;;
  supervise)
    log 'SUPERVISOR_BEGIN; waiting 5s for autorun confirmation to close'
    sleep 5
    systemctl is-active --quiet "$U-guard.timer" || fail GUARD_NOT_ARMED
    systemctl is-active --quiet "$U-bus-proxy.service" || fail PROXY_NOT_RUNNING
    systemctl is-active --quiet "$STOCK_UNIT" || fail STOCK_NOT_RUNNING
    # Intent precedes the stop; independent recovery can handle interruption.
    touch "$R/switch-intent"
    log STOCK_STOP_REQUESTED
    systemctl stop "$STOCK_UNIT"
    [ "$(prop "$STOCK_UNIT" MainPID)" = 0 ] || fail STOCK_STILL_RUNNING
    log CLONE_START_REQUESTED
    systemctl start "$U-clone.service"
    n=0
    verified=0
    while [ "$n" -lt "$((CLONE_SECONDS+5))" ] && systemctl is-active --quiet "$U-clone.service"; do
        pid=$(prop "$U-clone.service" MainPID)
        if [ "$verified" = 0 ] && [ "$pid" -gt 1 ]; then
            actual=$(stat -Lc '%d:%i' "/proc/$pid/exe" 2>/dev/null || true)
            copy=$(stat -Lc '%d:%i' "$R/MPC-Sample")
            if [ "$actual" = "$copy" ]; then
                log "CLONE_PROCESS_VERIFIED pid=$pid inode=$actual"
                verified=1
                export_report
            fi
        fi
        n=$((n+1))
        sleep 1
    done
    [ "$verified" = 1 ] || log CLONE_EXEC_NOT_OBSERVED
    log SUPERVISOR_END
    exit 0
    ;;
  main) ;;
  *) exit 2 ;;
esac

# No daemon or stock state changes until identity and sandbox checks have passed.
[ "$(id -u)" = 0 ] || exit 1
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
case "$DIR" in /media/*/autorun) ;; *) exit 1 ;; esac
case "$DIR" in *' '*|*'%'*|*'\'*) exit 1 ;; esac
SD=${DIR%/autorun}
awk -v p="$SD" '$2==p && $3=="exfat" && $4 ~ /(^|,)rw(,|$)/ {ok=1} END {exit !ok}' /proc/mounts || exit 1
[ "$(stat -f -c %T /run)" = tmpfs ] || exit 1
if ! mkdir "$R"; then
    printf 'Already attempted this boot. Reboot before another attempt.\n' > "$DIR/MPC_CLONE_ALREADY_ATTEMPTED.txt"
    exit 1
fi
mkdir "$R/app" "$R/proxy"
printf '%s\n' "$DIR/MPC_CLONE_TRIAL.txt" > "$R/report-path"
stat -c %d "$DIR" > "$R/sd-device"
touch "$R/events.txt"
exit_mode=main
trap 'on_exit $?' EXIT
log PRECHECK_BEGIN
STATUS_WAV_OCTAL='\0122\0111\0106\0106\0304\0000\0000\0000\0127\0101\0126\0105\0146\0155\0164\0040\0020\0000\0000\0000\0001\0000\0001\0000\0100\0037\0000\0000\0200\0076\0000\0000\0002\0000\0020\0000\0144\0141\0164\0141\0240\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000\0000'
printf '%b' "$STATUS_WAV_OCTAL" > "$R/status.wav" || fail STATUS_TEMPLATE_FAILED
[ "$(stat -c %s "$R/status.wav")" = 204 ] || fail BAD_STATUS_TEMPLATE
status RUNNING || fail STATUS_UNAVAILABLE
[ "$(uname -m)" = aarch64 ] || fail WRONG_ARCHITECTURE
for cmd in systemctl journalctl sha256sum stat readlink setarch awk dbus-send; do
    command -v "$cmd" >/dev/null 2>&1 || fail "MISSING_COMMAND $cmd"
done
[ "$(hash "$STOCK")" = "$STOCK_SHA" ] || fail WRONG_STOCK_HASH
[ "$(hash /usr/lib/systemd/system/ac50.service)" = "$UNIT_SHA" ] || fail CHANGED_AC50_UNIT
[ -z "$(prop "$STOCK_UNIT" DropInPaths)" ] || fail EXISTING_AC50_DROPINS
[ "$(prop "$STOCK_UNIT" FragmentPath)" = /usr/lib/systemd/system/ac50.service ] || fail CHANGED_UNIT_SOURCE
[ ! -e "$DROP" ] || fail EXISTING_OVERRIDE
systemctl is-active --quiet "$STOCK_UNIT" || fail STOCK_NOT_ACTIVE
pid=$(prop "$STOCK_UNIT" MainPID)
[ "$pid" -gt 1 ] || fail STOCK_PID_MISSING
[ "$(stat -Lc '%d:%i' "/proc/$pid/exe")" = "$(stat -Lc '%d:%i' "$STOCK")" ] || fail UNKNOWN_RUNNING_APP
awk '/^MemAvailable:/ {ok=($2>=524288)} END {exit !ok}' /proc/meminfo || fail LESS_THAN_512M_AVAILABLE
readlink /proc/self/ns/mnt > "$R/app/host-mnt-ns"
bus_call GetId > "$R/host-bus-reply" || fail HOST_BUS_ID_UNAVAILABLE
awk '$1=="string" {gsub(/"/, "", $2); print $2}' "$R/host-bus-reply" > "$R/app/host-bus-id"
[ -s "$R/app/host-bus-id" ] || fail HOST_BUS_ID_EMPTY
touch "$R/app/mask-identities"
for kind in blk chr sock; do
    mask_source="/run/systemd/inaccessible/$kind"
    [ -e "$mask_source" ] || continue
    [ "$(stat -Lc '%a:%t:%T:%u:%g' "$mask_source")" = '0:0:0:0:0' ] || fail BAD_SYSTEMD_MASK
    stat -Lc '%d:%i:%f:%t:%T:%u:%g' "$mask_source" >> "$R/app/mask-identities"
done
[ -s "$R/app/mask-identities" ] || fail NO_SYSTEMD_MASK_IDENTITIES
cp "$0" "$R/launcher.sh"
cp /usr/bin/busybox.nosuid "$R/app/busybox"
chmod 500 "$R/app/busybox"
cp "$STOCK" "$R/MPC-Sample"
chmod 500 "$R/MPC-Sample"
[ "$(hash "$R/MPC-Sample")" = "$STOCK_SHA" ] || fail COPY_HASH_MISMATCH
log COPY_HASH_VERIFIED
[ "$(hash "$DIR/xdg-dbus-proxy")" = "$PROXY_SHA" ] || fail WRONG_PROXY_HASH
[ "$(hash "$DIR/proxy-policy.args")" = "$POLICY_SHA" ] || fail WRONG_POLICY_HASH
cp "$DIR/xdg-dbus-proxy" "$R/dbus-proxy"
cp "$DIR/proxy-policy.args" "$R/proxy-policy.args"
chmod 500 "$R/dbus-proxy"
[ "$(hash "$R/dbus-proxy")" = "$PROXY_SHA" ] || fail PROXY_COPY_HASH_MISMATCH

# Common sandbox. /run IPC and /tmp are private; persistent disks are read-only.
cat > "$R/sandbox.conf" <<EOF
PrivateMounts=yes
PrivateNetwork=yes
ProtectSystem=strict
ProtectHome=read-only
ProtectKernelTunables=yes
ProtectKernelModules=yes
ProtectControlGroups=yes
NoNewPrivileges=yes
# The stock display initializer uses KDSETMODE on /dev/tty0.
CapabilityBoundingSet=CAP_IPC_LOCK CAP_SYS_NICE CAP_SYS_TTY_CONFIG
TemporaryFileSystem=/run:rw,nosuid,nodev,size=32M /tmp:rw,nosuid,nodev,size=32M /var/tmp:rw,nosuid,nodev,size=16M
BindPaths=$R/app:$APP
BindReadOnlyPaths=$R/proxy:/run/dbus $R/launcher.sh:/usr/bin/launch-ac50 $R/MPC-Sample:"$STOCK"
DevicePolicy=closed
LimitCORE=0
MemoryMax=800M
MemorySwapMax=0
TasksMax=256
Restart=no
KillMode=control-group
TimeoutStopSec=5s
SendSIGKILL=yes
EOF
# Grant only existing audio/display/control character devices, never block devices.
for dev in /dev/snd/* /dev/dri/* /dev/input/event* /dev/tty0 /dev/ttyS[0-9]* /dev/i2c-[0-9]* /dev/gpiochip[0-9]* /dev/spidev* /dev/dma_heap/* /dev/fb[0-9]* /dev/rga2; do
    [ -c "$dev" ] || continue
    case "$dev" in *' '*|*'%'*|*'\'*) fail UNSUPPORTED_DEVICE_PATH ;; esac
    printf 'DeviceAllow=%s rw\n' "$dev" >> "$R/sandbox.conf"
done
# These three RGA paths are referenced by this exact stock executable.
# Resolve only their character-device targets; do not allow all video devices.
for graphics_link in /dev/v4l/by-path/platform-ff920000.rga-video-index0 /dev/v4l/by-path/platform-fdb80000.rga-video-index0 /dev/v4l/by-path/platform-fdeb0000.rga-video-index0; do
    [ -c "$graphics_link" ] || continue
    graphics_node=$(readlink -f "$graphics_link") || fail BAD_RGA_LINK
    case "$graphics_node" in /dev/video[0-9]*) ;; *) fail UNEXPECTED_RGA_TARGET ;; esac
    case "${graphics_node#/dev/video}" in ''|*[!0-9]*) fail UNEXPECTED_RGA_TARGET ;; esac
    [ -c "$graphics_node" ] || fail RGA_NOT_CHARACTER_DEVICE
    printf 'DeviceAllow=%s rw\n' "$graphics_node" >> "$R/sandbox.conf"
done
# Hide raw storage and memory nodes independently of cgroup BPF support.
find /dev -type b > "$R/block-nodes"
touch "$R/app/blocked-nodes"
for dev in $(cat "$R/block-nodes") /dev/mem /dev/kmem /dev/port /dev/mtd[0-9]* /dev/sg[0-9]* /dev/nvme[0-9] /dev/mmc*; do
    [ -e "$dev" ] || continue
    case "$dev" in *' '*|*'%'*|*'\'*) fail UNSUPPORTED_BLOCK_PATH ;; esac
    printf 'InaccessiblePaths=%s\n' "$dev" >> "$R/sandbox.conf"
    printf '%s\n' "$dev" >> "$R/app/blocked-nodes"
done
for mode in probe clone; do
    {
        echo '[Unit]'
        echo 'Description=MPC unmodified clone trial'
        echo '[Service]'
        if [ "$mode" = probe ]; then echo Type=oneshot; echo TimeoutStartSec=20s;
        else echo Type=exec; echo "RuntimeMaxSec=${CLONE_SECONDS}s"; fi
        echo "ExecStart=/bin/sh /usr/bin/launch-ac50 $mode"
        cat "$R/sandbox.conf"
    } > "/run/systemd/system/$U-$mode.service"
done
cat > "/run/systemd/system/$U-bus-proxy.service" <<EOF
[Unit]
Description=Filtered access to stock MPC system bus
[Service]
Type=exec
ExecStart=/bin/sh $R/launcher.sh proxy
PrivateMounts=yes
PrivateNetwork=yes
PrivateDevices=yes
ProtectSystem=strict
ProtectHome=read-only
ProtectKernelTunables=yes
ProtectKernelModules=yes
ProtectControlGroups=yes
NoNewPrivileges=yes
CapabilityBoundingSet=
InaccessiblePaths=/run/systemd/private
ReadWritePaths=$R/proxy
RestrictAddressFamilies=AF_UNIX
MemoryMax=64M
TasksMax=32
KillMode=control-group
TimeoutStopSec=5s
RuntimeMaxSec=${PROXY_SECONDS}s
Restart=no
EOF
cat > "/run/systemd/system/$U-recover.service" <<EOF
[Unit]
Description=MPC trial independent stock recovery
[Service]
Type=oneshot
ExecStart=/bin/sh $R/launcher.sh recover
TimeoutStartSec=40s
RemainAfterExit=yes
EOF
cat > "/run/systemd/system/$U-supervisor.service" <<EOF
[Unit]
Description=MPC trial supervisor
[Service]
Type=exec
ExecStart=/bin/sh $R/launcher.sh supervise
ExecStopPost=/usr/bin/systemctl --no-block start $U-recover.service
RuntimeMaxSec=${SUPERVISOR_SECONDS}s
TimeoutStartSec=10s
TimeoutStopSec=5s
Restart=no
EOF
cat > "/run/systemd/system/$U-guard.timer" <<EOF
[Unit]
Description=MPC trial independent deadline
[Timer]
OnActiveSec=${GUARD_SECONDS}s
AccuracySec=1s
Unit=$U-recover.service
EOF
systemctl daemon-reload
# Embedded firmware has no systemd-analyze. Require manager acceptance of the
# essential directives, then exercise the actual sandbox before stopping stock.
for mode in probe clone; do
    service="$U-$mode.service"
    [ "$(prop "$service" LoadState)" = loaded ] || fail UNIT_NOT_LOADED
    [ "$(prop "$service" ProtectSystem)" = strict ] || fail NO_PROTECT_SYSTEM
    [ "$(prop "$service" PrivateMounts)" = yes ] || fail NO_PRIVATE_MOUNTS
    [ "$(prop "$service" PrivateNetwork)" = yes ] || fail NO_PRIVATE_NETWORK
    [ "$(prop "$service" NoNewPrivileges)" = yes ] || fail NO_NEW_PRIVILEGES_GUARD
    [ "$(prop "$service" DevicePolicy)" = closed ] || fail NO_DEVICE_POLICY
    [ "$(prop "$service" KillMode)" = control-group ] || fail BAD_KILL_MODE
done
[ "$(prop "$U-clone.service" RuntimeMaxUSec)" = "$CLONE_RUNTIME_EXPECTED" ] || fail NO_CLONE_DEADLINE
systemctl start "$U-bus-proxy.service" || fail PROXY_START_FAILED
proxy_wait=0
while [ ! -S "$R/proxy/system_bus_socket" ] && [ "$proxy_wait" -lt 5 ]; do
    sleep 1
    proxy_wait=$((proxy_wait+1))
done
[ -S "$R/proxy/system_bus_socket" ] || fail PROXY_SOCKET_MISSING
stat -Lc '%d:%i' "$R/proxy/system_bus_socket" > "$R/app/proxy-socket-id"
systemctl start "$U-probe.service" || fail SANDBOX_PROBE_FAILED
grep -qx PROBE_OK "$R/app/probe.txt" || fail PROBE_MARKER_MISSING
log SANDBOX_PROBE_PASSED
export_report

# Arm recovery before changing ac50's runtime configuration.
systemctl start "$U-guard.timer" || fail GUARD_START_FAILED
systemctl is-active --quiet "$U-guard.timer" || fail GUARD_NOT_ACTIVE
mkdir -p /run/systemd/system/ac50.service.d
cat > "$DROP.tmp" <<EOF
[Service]
ExecStart=
ExecStart=/bin/sh $R/launcher.sh stock
TimeoutStopSec=10s
EOF
mv "$DROP.tmp" "$DROP"
systemctl daemon-reload
log "RECOVERY_ARMED; ${CLONE_SECONDS}s clone limit, ${GUARD_SECONDS}s independent recovery deadline"
systemctl start "$U-supervisor.service" || fail SUPERVISOR_START_FAILED
log HANDED_TO_SYSTEMD
exit 0
