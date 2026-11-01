#!/bin/sh
# MPC Sample 1.3.0.12: RAM OverlayFS changes and scoped SD saving.
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
CLONE_RUNTIME_EXPECTED=infinity
STAGE_SHA=STAGE_HELPER_SHA
SAVE_IO_SHA=SAVE_IO_TRACE_SHA
DATA_OVERLAY='run-mpc\x2dclone\x2dtrial\x2dv1-data\x2dimage.mount'
DATA_LOWER='run-mpc\x2dclone\x2dtrial\x2dv1-data\x2dlower.mount'
DATA_CHANGES='run-mpc\x2dclone\x2dtrial\x2dv1-data\x2dchanges.mount'
PATCH_SHA=9f59f39b9dc3dbf41b65d4bb0b96442dd83b9829b38788c8b456ba7b5bbf7068

hash() { sha256sum "$1" | cut -d ' ' -f 1; }
prop() { systemctl show "$1" -p "$2" --value; }
flush_report_sd() {
    if [ -f "$R/journal-flush-ready" ]; then
        "$R/stage-data" --flush-journal >> "$R/journal-flush.txt" 2>&1
    else
        # Bootstrap only, before the helper has passed its SHA check.
        sync
    fi
}
persist_event() {
    event_dir=$(cat "$R/storage-meta/journal-dir") || return 1
    [ "$(readlink -f "$event_dir")" = "$event_dir" ] || return 1
    [ "$(stat -c '%d:%i' "$event_dir")" = "$(cat "$R/storage-meta/journal-identity")" ] || return 1
    [ ! -L "$event_dir/MPC_MOD_EVENTS.txt" ] || return 1
    printf '%s\n' "$1" >> "$event_dir/MPC_MOD_EVENTS.txt" || return 1
    flush_report_sd
}
log() {
    event_line="$(date -u +%Y-%m-%dT%H:%M:%SZ) $*"
    printf '%s\n' "$event_line" >> "$R/events.txt"
    # The guard must never wait for SD I/O before requesting stock recovery.
    # Recovery snapshots are flushed after stock is restored, not per event.
    case "${exit_mode:-main}" in
      guard) return 0 ;;
      recover) [ -f "$R/recovered" ] || return 0 ;;
    esac
    # Losing the SD must not turn an optional log write into a fatal error.
    persist_event "$event_line" || printf '%s SD_EVENT_WRITE_OR_FLUSH_FAILED\n' "$event_line" >> "$R/events.txt"
}
checkpoint() {
    log "$1"
    export_report || return 1
    flush_report_sd
}
resource_snapshot() {
    # Bounded read-only measurements written to RAM. No directory walk or
    # extra observer process, and no SD flush from the timed 20/40s samples.
    {
        echo "--- RESOURCE_SNAPSHOT $1 ---"
        date -u +%Y-%m-%dT%H:%M:%SZ
        cat /proc/uptime
        awk '/^(MemTotal|MemFree|MemAvailable|Cached|SwapTotal|SwapFree|Shmem):/ {print}' /proc/meminfo
        echo 'WORKING_LIMIT_BYTES=536870912 NATIVE_RESERVE_BYTES=419430400'
        if [ -f "$R/data-staged" ]; then
            df -k "$R/data-changes" "$R/data-image" || true
        else
            echo WORKING_LAYER_NOT_MOUNTED
        fi
        systemctl show "$U-clone.service" -p MainPID -p MemoryCurrent -p MemoryPeak -p MemoryMax || true
        snapshot_pid=$(prop "$U-clone.service" MainPID 2>/dev/null || true)
        case "$snapshot_pid" in ''|0|*[!0-9]*) ;; *)
            if [ -r "/proc/$snapshot_pid/status" ]; then
                awk '/^(Name|VmPeak|VmSize|VmLck|VmHWM|VmRSS|RssAnon|RssFile|RssShmem|VmSwap):/ {print}' "/proc/$snapshot_pid/status" || true
            fi
            ;;
        esac
        snapshot_group=$(prop "$U-clone.service" ControlGroup 2>/dev/null || true)
        case "$snapshot_group" in "/system.slice/$U-clone.service")
            for snapshot_metric in memory.current memory.peak memory.max memory.swap.current memory.events; do
                snapshot_path="/sys/fs/cgroup$snapshot_group/$snapshot_metric"
                if [ -r "$snapshot_path" ]; then echo "$snapshot_metric:"; cat "$snapshot_path" || true; fi
            done
            snapshot_path="/sys/fs/cgroup$snapshot_group/memory.stat"
            if [ -r "$snapshot_path" ]; then
                echo 'memory.stat selected:'
                awk '$1 ~ /^(anon|file|kernel|shmem|slab|sock)$/ {print}' "$snapshot_path" || true
            fi
            ;;
        esac
        echo RESOURCE_SNAPSHOT_END
    } >> "$R/resources.txt" 2>&1 || true
    return 0
}
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
native_crash_snapshot() {
    # The application has stopped. Read only the two known RAM settings paths
    # before their tmpfs is unmounted; never follow links or open special files.
    (
        crash_root="$R/data-changes/upper"
        for crash_parent in "$R" "$R/data-changes" "$crash_root" "$crash_root/Settings"; do
            [ -d "$crash_parent" ] && [ ! -L "$crash_parent" ] || {
                echo NATIVE_CRASH_ROOT_UNAVAILABLE; exit 0;
            }
        done
        for crash_relative in 'Akai/MPC Sample/MPC Sample.crashinfo' 'MPC Sample/MPC Sample.crashinfo'; do
            echo "--- NATIVE_CRASH $crash_relative ---"
            crash_path="$crash_root/Settings/$crash_relative"
            crash_dir=${crash_path%/*}
            crash_parent=${crash_dir%/*}
            if [ -L "$crash_parent" ] || [ -L "$crash_dir" ] || [ -L "$crash_path" ]; then
                echo NATIVE_CRASH_LINK_REJECTED
            elif [ ! -f "$crash_path" ]; then
                echo NATIVE_CRASH_NOT_REGULAR_OR_MISSING
            else
                crash_size=$(stat -c %s "$crash_path" 2>/dev/null || true)
                case "$crash_size" in ''|*[!0-9]*) echo NATIVE_CRASH_STAT_FAILED; continue ;; esac
                echo "NATIVE_CRASH_BYTES=$crash_size LIMIT=65536"
                dd if="$crash_path" bs=4096 count=16 2>/dev/null || echo NATIVE_CRASH_READ_FAILED
                echo
                [ "$crash_size" -le 65536 ] || echo NATIVE_CRASH_TRUNCATED
                echo NATIVE_CRASH_END
            fi
        done
    ) > "$R/app/native-crash.txt" 2>&1 || true
    return 0
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
        for f in "$R/app/probe.txt" "$R/app/clone.txt" "$R/app/picker-trace.txt" "$R/proxy/proxy.txt" "$R/journal-flush.txt" "$R/resources.txt" "$R/app/save-io.txt" "$R/app/save-storage.txt" "$R/app/native-crash.txt"; do
            if [ -f "$f" ]; then echo "--- ${f##*/} ---"; cat "$f"; fi
        done
        for service in "$U-probe" "$U-clone" "$U-supervisor" "$U-recover" "$U-bus-proxy"; do
            systemctl show "$service.service" -p ActiveState -p SubState -p Result -p ExecMainStatus 2>&1 || true
        done
        for data_unit in "$DATA_OVERLAY" "$DATA_LOWER" "$DATA_CHANGES"; do
            echo "--- $data_unit ---"
            systemctl show "$data_unit" -p ActiveState -p SubState -p Result 2>&1 || true
        done
        echo '--- Probe/application/recovery/mount journal (last 120 lines) ---'
        journalctl -b -u "$U-probe.service" -u "$U-clone.service" -u "$U-supervisor.service" -u "$U-recover.service" -u "$U-bus-proxy.service" -u "$DATA_OVERLAY" -u "$DATA_LOWER" -u "$DATA_CHANGES" -n 120 --no-pager 2>&1 || true
    } > "$dst.tmp.$$" && mv -f "$dst.tmp.$$" "$dst" && flush_report_sd
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

data_layer_start() {
    log DATA_LAYER_PREPARE
    [ "$(stat -c '%d:%i' /data)" = "$(cat "$R/storage-meta/original-data-identity")" ] || return 1
    # Privileged mounts are made by systemd, only below fixed /run paths.
    # No mount capability is given to the MPC application.
    cat > "/run/systemd/system/$DATA_CHANGES" <<EOF
[Unit]
Description=MPC mod temporary data changes in RAM
DefaultDependencies=no
Conflicts=umount.target
Before=umount.target
[Mount]
What=tmpfs
Where=$R/data-changes
Type=tmpfs
Options=rw,nosuid,nodev,size=512M,mode=0700
TimeoutSec=10s
EOF
    cat > "/run/systemd/system/$DATA_LOWER" <<EOF
[Unit]
Description=MPC mod read-only original data view
DefaultDependencies=no
Conflicts=umount.target
Before=umount.target
[Mount]
What=/data
Where=$R/data-lower
Type=none
Options=bind,ro,nosuid,nodev
TimeoutSec=10s
EOF
    cat > "/run/systemd/system/$DATA_OVERLAY" <<EOF
[Unit]
Description=MPC mod data with changes only in RAM
DefaultDependencies=no
Requires=$DATA_CHANGES $DATA_LOWER
After=$DATA_CHANGES $DATA_LOWER
Conflicts=umount.target
Before=umount.target
[Mount]
What=overlay
Where=$R/data-image
Type=overlay
Options=rw,nosuid,nodev,lowerdir=$R/data-lower,upperdir=$R/data-changes/upper,workdir=$R/data-changes/work,index=off,metacopy=off,redirect_dir=off
TimeoutSec=10s
EOF
    systemctl daemon-reload || return 1
    log DATA_RAM_AND_LOWER_MOUNT_REQUESTED
    systemctl start "$DATA_CHANGES" "$DATA_LOWER" || return 1
    [ "$(stat -f -c %t "$R/data-changes")" = 1021994 ] || return 1
    [ "$(stat -c '%d:%i' "$R/data-lower")" = "$(cat "$R/storage-meta/original-data-identity")" ] || return 1
    awk -v p="$R/data-lower" '$2==p && $4 ~ /(^|,)ro(,|$)/ {n++} END {exit n!=1}' /proc/mounts || return 1
    mkdir "$R/data-changes/upper" "$R/data-changes/work" || return 1
    log DATA_OVERLAY_MOUNT_REQUESTED
    systemctl start "$DATA_OVERLAY" || return 1
    # This firmware BusyBox has no overlayfs name in its stat type table.
    # Check the Linux filesystem magic, not its human-readable label.
    data_overlay_magic=$(stat -f -c %t "$R/data-image") || return 1
    data_overlay_label=$(stat -f -c %T "$R/data-image" 2>/dev/null || true)
    log "DATA_OVERLAY_FILESYSTEM magic=$data_overlay_magic label=$data_overlay_label"
    [ "$data_overlay_magic" = 794c7630 ] || { log DATA_OVERLAY_WRONG_MAGIC; return 1; }
    stat -c '%d:%i' "$R/data-image" > "$R/storage-meta/data-image-identity"
    stat -c '%d:%i' "$R/data-changes" > "$R/storage-meta/data-changes-identity"
    printf '%s\n' overlayfs > "$R/storage-meta/data-image-type"
    touch "$R/data-staged"
    resource_snapshot DATA_LAYER_READY
    log 'DATA_OVERLAY_READY: original read-only, changes tmpfs maximum512MiB; native reserve400MiB; no full-data copy'
}

data_layer_stop() {
    for layer in "$DATA_OVERLAY" "$DATA_LOWER" "$DATA_CHANGES"; do
        if [ -f "/run/systemd/system/$layer" ]; then
            systemctl stop "$layer" || return 1
        fi
    done
    awk -v p="$R/" 'index($5,p)==1 {bad=1} END {exit bad}' /proc/self/mountinfo
}

storage_host_check() {
    save_fs=$(cat "$R/storage-meta/sd-filesystem") || return 1
    case "$save_fs" in exfat|vfat) ;; *) return 1 ;; esac
    save_path=$(cat "$R/storage-meta/save-path") || return 1
    save_sd=${save_path%/MPC-Sample}
    [ "$save_path" = "$save_sd/MPC-Sample" ] || return 1
    [ "$(readlink -f "$save_path")" = "$save_path" ] || return 1
    [ "$(stat -c '%d:%i' "$save_path")" = "$(cat "$R/storage-meta/save-identity")" ] || return 1
    [ "$(stat -c '%d:%i' "$save_sd")" = "$(cat "$R/storage-meta/sd-identity")" ] || return 1
    for save_child in Projects Samples; do
        [ "$(readlink -f "$save_path/$save_child")" = "$save_path/$save_child" ] || return 1
        [ "$(stat -c '%d:%i' "$save_path/$save_child")" = "$(cat "$R/storage-meta/save-$save_child-identity")" ] || return 1
        [ "$(stat -c %d "$save_path/$save_child")" = "$(stat -c %d "$save_sd")" ] || return 1
    done
    awk -v p="$save_sd" -v fs="$save_fs" '$2==p && $3==fs && $4 ~ /(^|,)rw(,|$)/ {n++} END {exit n!=1}' /proc/mounts
}

sandbox_check() {
    echo 'SANDBOX_CHECK_BEGIN'
    save_fs=$(cat "$APP/storage-meta/sd-filesystem") || return 1
    case "$save_fs" in exfat) save_magic=2011bab0 ;; vfat) save_magic=4d44 ;; *) return 1 ;; esac
    save_path=$(cat "$APP/storage-meta/save-path") || return 1
    [ "$(readlink -f "$save_path")" = "$save_path" ] || return 1
    [ "$(stat -c '%d:%i' "$save_path")" = "$(cat "$APP/storage-meta/save-identity")" ] || return 1
    for save_child in Projects Samples; do
        [ "$(readlink -f "$save_path/$save_child")" = "$save_path/$save_child" ] || return 1
        [ "$(stat -c '%d:%i' "$save_path/$save_child")" = "$(cat "$APP/storage-meta/save-$save_child-identity")" ] || return 1
        [ "$(stat -f -c %t "$save_path/$save_child")" = "$save_magic" ] || return 1
    done
    [ "$(stat -c '%d:%i' /data)" = "$(cat "$APP/storage-meta/data-image-identity")" ] || return 1
    data_type=$(cat "$APP/storage-meta/data-image-type") || return 1
    data_magic=$(stat -f -c %t /data) || return 1
    echo "DATA_FILESYSTEM kind=$data_type magic=$data_magic"
    case "$data_type" in
      tmpfs)
        # Before stopping stock: probe only, empty RAM directory.
        [ "$data_magic" = 1021994 ] || { echo 'CHECK_FAILED: probe_data_magic'; return 1; }
        ;;
      overlayfs)
        [ "$data_magic" = 794c7630 ] || { echo 'CHECK_FAILED: overlay_data_magic'; return 1; }
        [ "$(stat -c '%d:%i' "$APP/data-changes")" = "$(cat "$APP/storage-meta/data-changes-identity")" ] || return 1
        [ "$(stat -f -c %t "$APP/data-changes")" = 1021994 ] || return 1
        ;;
      *) return 1 ;;
    esac
    # mountinfo also lists mounts hidden underneath an overmount at /data.
    # Pin the mount actually opened by this process, not the last row/order.
    exec 8< /data || return 1
    data_mount_id=$(awk '$1=="mnt_id:" {print $2}' /proc/$$/fdinfo/8) || { exec 8<&-; return 1; }
    exec 8<&-
    case "$data_mount_id" in ''|*[!0-9]*) echo 'CHECK_FAILED: data_mount_id'; return 1 ;; esac
    echo "DATA_VISIBLE_MOUNT_ID=$data_mount_id TYPE=$data_type"
    # Exactly two accessible disk-backed mounts may be writable: native SD folders.
    # Only hidden rows at the exact /data path are exempt, after the opened
    # mount ID, pinned dev/inode and RAM filesystem type have all been checked.
    awk -v save="$save_path" -v save_fs="$save_fs" -v data_type="$data_type" -v data_id="$data_mount_id" '
      {
        for (s=7;s<=NF && $s!="-";s++) {}
        if (s>NF) {bad=1; next}
        fs=$(s+1); target=$5; opts=$6;
        if (target=="/data") {
          print "DATA_MOUNT id=" $1 " parent=" $2 " device=" $3 " root=" $4 " opts=" opts " fs=" fs;
          if ($1!=data_id) {hidden_data++; next}
          visible_data++;
          expected=(data_type=="overlayfs" ? "overlay" : "tmpfs");
          if (fs!=expected || opts !~ /(^|,)rw(,|$)/) {bad=1; print "CHECK_FAILED: opened_data_mount"}
          next
        }
        if (fs ~ /^(ext[234]|exfat|vfat|f2fs|btrfs|xfs|ntfs|ntfs3|fuseblk|overlay)$/ && opts !~ /(^|,)ro(,|$)/) {
          if (target==(save "/Projects") && fs==save_fs && opts ~ /(^|,)rw(,|$)/) allowed_projects++;
          else if (target==(save "/Samples") && fs==save_fs && opts ~ /(^|,)rw(,|$)/) allowed_samples++;
          else { print "UNEXPECTED_WRITABLE_DISK: " target " id=" $1 " fs=" fs " opts=" opts; bad=1 }
        }
      }
      END {
        print "DATA_MOUNT_SUMMARY visible=" visible_data " hidden=" hidden_data " sd_project_mounts=" allowed_projects " sd_sample_mounts=" allowed_samples;
        exit bad || allowed_projects!=1 || allowed_samples!=1 || visible_data!=1
      }
    ' /proc/self/mountinfo || { echo 'CHECK_FAILED: scoped_sd_write_mount'; return 1; }
    # A private mount tree must not receive subsequent writable host mounts.
    awk '{for(i=7;i<=NF && $i!="-";i++) if($i ~ /^(shared|master):/) bad=1} END {exit bad}' /proc/self/mountinfo || {
        echo 'CHECK_FAILED: mount_propagation_not_private'; return 1;
    }
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
    echo 'INTERNAL_DISK_MOUNTS_READ_ONLY=yes'
    echo "SD_SAVE_ROOT=$save_path; writable=$save_path/Projects,$save_path/Samples"
    echo "WORKING_DATA_VIEW=$data_type; changes in RAM"
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
    [ "$(hash "$STOCK")" = "$PATCH_SHA" ] || exit 1
    echo '--- Display launch context ---'
    awk '/^(CapInh|CapPrm|CapEff|CapBnd|NoNewPrivs):/ {print}' /proc/self/status
    for display_node in /dev/tty0 /dev/dri/card* /dev/rga2 /dev/v4l/by-path/platform-*.rga-video-index0; do
        [ -c "$display_node" ] || continue
        stat -Lc '%n mode=%a uid=%u gid=%g device=%t:%T' "$display_node" || true
    done
    # Read-only drive metadata and namespace snapshot for the saving diagnosis.
    {
        echo SAVE_STORAGE_DIAGNOSTICS_BEGIN
        date -u +%Y-%m-%dT%H:%M:%SZ
        cat /proc/self/mountinfo
        /usr/bin/dbus-send --system --print-reply --reply-timeout=3000 \
            --dest=io.github.inmusicdev.edisksd / \
            org.freedesktop.DBus.ObjectManager.GetManagedObjects || true
        echo SAVE_STORAGE_DIAGNOSTICS_END
    } > "$APP/save-storage.txt" 2>&1 || true
    echo 'STARTING_K_VALUE_PEEK_COPY (experimental UI feature; not hardware validated yet)'
    ulimit -S -s 1024
    export MALLOC_ARENA_MAX=1
    [ "$(hash "$APP/save-io.so")" = "$SAVE_IO_SHA" ] || exit 1
    export LD_PRELOAD="$APP/save-io.so"
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
    systemctl stop "$U-guard.timer" || true
    status RETURNING || log STATUS_WRITE_FAILED
    systemctl --no-block stop "$U-supervisor.service" || true
    supervisor_stop_wait=0
    while [ "$(prop "$U-supervisor.service" MainPID)" != 0 ]; do
        [ "$supervisor_stop_wait" -lt 50 ] || fail SUPERVISOR_STILL_RUNNING
        sleep 0.1
        supervisor_stop_wait=$((supervisor_stop_wait+1))
    done
    resource_snapshot RECOVERY_BEFORE_CLONE_STOP
    if [ -f "$R/data-staged" ]; then
        {
            echo RAM_UPPER_FILES_BEFORE_STOP_MAX128
            find "$R/data-changes/upper" -maxdepth 8 -type f -print | head -n 128
        } >> "$R/app/save-storage.txt" 2>&1 || true
    fi
    systemctl stop "$U-clone.service" || true
    # Never start a second hardware owner while the clone may still be alive.
    [ "$(prop "$U-clone.service" MainPID)" = 0 ] || fail CLONE_STILL_RUNNING
    native_crash_snapshot
    if [ -f "$R/data-staged" ]; then
        if ! "$R/stage-data" --flush-sd >> "$R/events.txt" 2>&1; then
            log SD_SAVE_FLUSH_FAILED
            rm -f "$R/poweroff-confirmed"
        fi
    fi
    data_layer_stop || fail DATA_LAYER_STILL_MOUNTED
    # The clone can request this only after its confirmed, completed SD copy.
    # OS poweroff remains outside its private bus/mount namespace. A normal
    # Mod Off, crash or missing SD must never take this branch.
    if [ -f "$R/poweroff-confirmed" ] && [ ! -L "$R/poweroff-confirmed" ]; then
        systemctl stop "$U-bus-proxy.service" || true
        touch "$R/recovered"
        log POWER_OFF_SD_FLUSHED
        status 'POWERING OFF' || log STATUS_WRITE_FAILED
        export_report || true
        if /usr/bin/systemctl --no-block poweroff; then exit 0; fi
        log POWER_OFF_FAILED_RETURNING_STOCK
        rm -f "$R/poweroff-confirmed" "$R/recovered"
    fi
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
    log SUPERVISOR_BEGIN
    read main_pid main_start < "$R/autorun-id"
    handoff_wait=0
    while [ "$(awk '{print $22}' "/proc/$main_pid/stat" 2>/dev/null || true)" = "$main_start" ]; do
        [ "$handoff_wait" -lt 100 ] || fail AUTORUN_DID_NOT_EXIT
        sleep 0.1
        handoff_wait=$((handoff_wait+1))
    done
    log AUTORUN_EXIT_OBSERVED
    systemctl is-active --quiet "$U-guard.timer" || fail GUARD_NOT_ARMED
    systemctl is-active --quiet "$U-bus-proxy.service" || fail PROXY_NOT_RUNNING
    systemctl is-active --quiet "$STOCK_UNIT" || fail STOCK_NOT_RUNNING
    storage_host_check || fail SD_CHANGED_BEFORE_SWITCH
    # Intent precedes the stop; independent recovery can handle interruption.
    checkpoint BEFORE_STOCK_STOP || fail SD_JOURNAL_CHECKPOINT_FAILED
    touch "$R/switch-intent"
    log STOCK_STOP_REQUESTED
    systemctl stop "$STOCK_UNIT"
    [ "$(prop "$STOCK_UNIT" MainPID)" = 0 ] || fail STOCK_STILL_RUNNING
    checkpoint STOCK_STOPPED || fail SD_JOURNAL_CHECKPOINT_FAILED
    storage_host_check || fail SD_CHANGED_BEFORE_DATA_LAYER
    data_layer_start || fail DATA_LAYER_START_FAILED
    checkpoint BEFORE_CLONE_START || fail SD_JOURNAL_CHECKPOINT_FAILED
    log CLONE_START_REQUESTED
    systemctl start "$U-clone.service"
    n=0
    verified=0
    picker_seen= picker_observed_serial= picker_busy=0
    while systemctl is-active --quiet "$U-clone.service"; do
        storage_host_check || { log SD_SAVE_IDENTITY_LOST; break; }
        if [ -f "$R/app/power-off.request" ] && [ ! -L "$R/app/power-off.request" ] && [ "$(stat -c %s "$R/app/power-off.request")" = 1 ] && [ "$(cat "$R/app/power-off.request")" = 1 ]; then
            log POWER_OFF_REQUESTED
            touch "$R/poweroff-confirmed"
            break
        fi
        if [ -f "$R/app/mod-off.request" ] && [ ! -L "$R/app/mod-off.request" ] && [ "$(stat -c %s "$R/app/mod-off.request")" = 1 ] && [ "$(cat "$R/app/mod-off.request")" = 1 ]; then
            log MOD_OFF_REQUESTED
            break
        fi
        pid=$(prop "$U-clone.service" MainPID)
        if [ "$verified" = 0 ] && [ "$pid" -gt 1 ]; then
            actual=$(stat -Lc '%d:%i' "/proc/$pid/exe" 2>/dev/null || true)
            copy=$(stat -Lc '%d:%i' "$R/MPC-Sample")
            if [ "$actual" = "$copy" ]; then
                log "CLONE_PROCESS_VERIFIED pid=$pid inode=$actual"
                verified=1
                touch "$R/clone-verified"
                resource_snapshot CLONE_VERIFIED
                export_report
            fi
        fi
        # Only a picker pad gesture arms this recovery, never normal use.
        picker_phase= picker_serial=
        if [ -f "$APP/picker-phase.txt" ] && [ ! -L "$APP/picker-phase.txt" ]; then
            read -r picker_phase picker_serial picker_detail < "$APP/picker-phase.txt" || true
            if [ "$picker_serial" != "$picker_observed_serial" ]; then
                picker_busy=0; picker_observed_serial=$picker_serial
            fi
            picker_now="$picker_phase $picker_serial"
            if [ "$picker_now" != "$picker_seen" ] && [ -n "$picker_serial" ]; then
                picker_seen=$picker_now
                log "PICKER_PHASE $picker_now"
                export_report || true
            fi
            case "$picker_phase" in
                PAD_BUSY|PAD_SELECTED|AUDIO_WAIT) picker_busy=$((picker_busy+1)) ;;
                *) picker_busy=0 ;;
            esac
            if [ "$picker_busy" -ge 15 ]; then log PICKER_PAD_CALLBACK_TIMEOUT; break; fi
        fi
        n=$((n+1))
        case "$n" in
            20) resource_snapshot CLONE_TICK_20 ;;
            40) resource_snapshot CLONE_TICK_40 ;;
        esac
        sleep 1
    done
    [ "$verified" = 1 ] || log CLONE_EXEC_NOT_OBSERVED
    log SUPERVISOR_END
    exit 0
    ;;
  guard)
    exit_mode=guard
    [ ! -f "$R/recovered" ] || exit 0
    if [ -f "$R/clone-verified" ]; then
        if systemctl is-active --quiet "$U-clone.service" && systemctl is-active --quiet "$U-supervisor.service"; then exit 0; fi
    else
        # Sample verification once. The supervisor can create the marker
        # while this guard reads the deadline; that is a successful startup,
        # not a reason to fall through into stock recovery.
        now=$(cut -d . -f 1 /proc/uptime)
        deadline=$(cat "$R/startup-deadline")
        if [ "$now" -lt "$deadline" ]; then exit 0; fi
    fi
    systemctl --no-block start "$U-recover.service"
    log GUARD_RECOVERY_REQUESTED
    exit 0
    ;;
  main) ;;
  *) exit 2 ;;
esac

# No daemon or stock state changes until identity and sandbox checks have passed.
early_fail() { printf 'EARLY_ABORT: %s\n' "$*" >&2; exit 1; }
[ "$(id -u)" = 0 ] || early_fail NOT_ROOT
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
case "$DIR" in /media/*/autorun) ;; *) early_fail WRONG_AUTORUN_DIRECTORY ;; esac
case "$DIR" in *' '*|*'%'*|*'\'*) early_fail UNSUPPORTED_AUTORUN_PATH ;; esac
SD=${DIR%/autorun}
case "${SD#/media/}" in ''|*[!A-Za-z0-9._-]*) early_fail UNSUPPORTED_SD_NAME ;; esac
[ "$(readlink -f "$SD")" = "$SD" ] || early_fail SD_NOT_CANONICAL
SD_FS=$(awk -v p="$SD" '$2==p && ($3=="exfat" || $3=="vfat") && $4 ~ /(^|,)rw(,|$)/ {n++; fs=$3} END {if(n!=1) exit 1; print fs}' /proc/mounts) || early_fail SD_NOT_RW_EXFAT_OR_FAT32
case "$SD_FS:$(stat -f -c %t "$SD")" in exfat:2011bab0|vfat:4d44) ;; *) early_fail SD_FILESYSTEM_MAGIC_MISMATCH ;; esac
[ "$(stat -f -c %T /run)" = tmpfs ] || early_fail RUN_NOT_TMPFS
relaunch=0
if [ -d "$R" ] && [ -f "$R/recovered" ]; then
    [ ! -L "$R" ] && [ "$(stat -f -c %T "$R")" = tmpfs ] || early_fail OLD_RUNTIME_NOT_TMPFS
    [ "$(hash "$R/launcher.sh")" = "$(hash "$0")" ] || early_fail DIFFERENT_BUILD_REQUIRES_REBOOT
    for old_mode in clone supervisor bus-proxy; do
        [ "$(prop "$U-$old_mode.service" MainPID)" = 0 ] || early_fail "OLD_PROCESS_ACTIVE $old_mode"
    done
    systemctl is-active --quiet "$STOCK_UNIT" || early_fail STOCK_NOT_ACTIVE_ON_RELAUNCH
    old_stock_pid=$(prop "$STOCK_UNIT" MainPID)
    [ "$old_stock_pid" -gt 1 ] || early_fail STOCK_PID_ON_RELAUNCH
    [ "$(stat -Lc '%d:%i' "/proc/$old_stock_pid/exe")" = "$(stat -Lc '%d:%i' "$STOCK")" ] || early_fail WRONG_STOCK_ON_RELAUNCH
    [ "$(hash "$STOCK")" = "$STOCK_SHA" ] || early_fail STOCK_HASH_ON_RELAUNCH
    [ "$(prop "$STOCK_UNIT" DropInPaths)" = "$DROP" ] || early_fail DROPINS_ON_RELAUNCH
    expected_drop=$(printf '[Service]\nExecStart=\nExecStart=/bin/sh %s/launcher.sh stock\nTimeoutStopSec=10s\n' "$R")
    [ "$(cat "$DROP")" = "$expected_drop" ] || early_fail DROPIN_CONTENT_ON_RELAUNCH
    systemctl stop "$U-guard.timer" "$U-guard.service" "$U-recover.service" || early_fail OLD_RECOVERY_STOP_FAILED
    data_layer_stop || early_fail OLD_DATA_LAYER_STILL_MOUNTED
    # Fixed /run directory only; every old hardware-owning process is stopped.
    rm -rf -- "$R"
    [ ! -e "$R" ] || early_fail OLD_RUNTIME_CLEANUP_FAILED
    systemctl reset-failed "$U-clone.service" "$U-supervisor.service" "$U-bus-proxy.service" "$U-probe.service" "$U-recover.service" "$U-guard.service" || true
    relaunch=1
fi
if ! mkdir "$R"; then
    echo 'EARLY_ABORT: ALREADY_ATTEMPTED_THIS_BOOT' >&2
    {
        date -u +%Y-%m-%dT%H:%M:%SZ
        echo 'Already attempted this boot. Fully power off and restart without SD before another attempt.'
        echo "Requested script SHA256=$(hash "$0")"
        echo 'The main trial report belongs to the previous attempt.'
    } > "$DIR/MPC_CLONE_ALREADY_ATTEMPTED.txt"
    exit 1
fi
mkdir "$R/app" "$R/proxy" "$R/data-image" "$R/data-lower" "$R/data-changes" "$R/storage-meta" "$R/app/storage-meta" "$R/app/data-changes"
printf '%s\n' "$DIR/MPC_CLONE_TRIAL.txt" > "$R/report-path"
stat -c %d "$DIR" > "$R/sd-device"
printf '%s\n' "$DIR" > "$R/storage-meta/journal-dir"
stat -c '%d:%i' "$DIR" > "$R/storage-meta/journal-identity"
printf '%s\n' "$SD_FS" > "$R/storage-meta/sd-filesystem"
touch "$R/events.txt"
exit_mode=main
trap 'on_exit $?' EXIT
log PRECHECK_BEGIN
log "LAUNCHER_SHA256=$(hash "$0")"
resource_snapshot PREPARATION_BEGIN
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
if [ "$relaunch" = 0 ]; then
    [ -z "$(prop "$STOCK_UNIT" DropInPaths)" ] || fail EXISTING_AC50_DROPINS
else
    [ "$(prop "$STOCK_UNIT" DropInPaths)" = "$DROP" ] || fail CHANGED_RELAUNCH_DROPIN
fi
[ "$(prop "$STOCK_UNIT" FragmentPath)" = /usr/lib/systemd/system/ac50.service ] || fail CHANGED_UNIT_SOURCE
[ "$relaunch" = 1 ] || [ ! -e "$DROP" ] || fail EXISTING_OVERRIDE
systemctl is-active --quiet "$STOCK_UNIT" || fail STOCK_NOT_ACTIVE
pid=$(prop "$STOCK_UNIT" MainPID)
[ "$pid" -gt 1 ] || fail STOCK_PID_MISSING
[ "$(stat -Lc '%d:%i' "/proc/$pid/exe")" = "$(stat -Lc '%d:%i' "$STOCK")" ] || fail UNKNOWN_RUNNING_APP
awk '/^MemAvailable:/ {ok=($2>=524288)} END {exit !ok}' /proc/meminfo || fail LESS_THAN_512M_AVAILABLE
readlink /proc/self/ns/mnt > "$R/app/host-mnt-ns"
[ "$(hash "$DIR/stage-data")" = "$STAGE_SHA" ] || fail WRONG_DATA_HELPER
cp "$DIR/stage-data" "$R/stage-data"
chmod 500 "$R/stage-data"
[ "$(hash "$R/stage-data")" = "$STAGE_SHA" ] || fail DATA_HELPER_COPY_MISMATCH
touch "$R/journal-flush-ready"
checkpoint JOURNAL_HELPER_READY || fail SD_JOURNAL_CHECKPOINT_FAILED
awk '$NF=="overlay" {ok=1} END {exit !ok}' /proc/filesystems || fail KERNEL_OVERLAYFS_UNAVAILABLE
stat -c '%d:%i' /data > "$R/storage-meta/original-data-identity"
[ "$(stat -c %d /data)" != "$(stat -c %d "$SD")" ] || fail DATA_IS_ON_SELECTED_SD
for save_dir in "$SD/MPC-Sample"; do
    [ ! -L "$save_dir" ] || fail SAVE_DIRECTORY_SYMLINK
    if [ ! -e "$save_dir" ]; then mkdir "$save_dir" || fail CREATE_SAVE_DIRECTORY; fi
    [ -d "$save_dir" ] && [ "$(readlink -f "$save_dir")" = "$save_dir" ] || fail INVALID_SAVE_DIRECTORY
    [ "$(stat -c %d "$save_dir")" = "$(stat -c %d "$SD")" ] || fail SAVE_DIRECTORY_WRONG_DEVICE
done
SAVE="$SD/MPC-Sample"
# Only the two native save directories are persistent write exceptions.
for child in Projects Samples; do
    [ ! -L "$SAVE/$child" ] || fail SAVE_SUBDIRECTORY_SYMLINK
    if [ ! -e "$SAVE/$child" ]; then mkdir "$SAVE/$child" || fail CREATE_SAVE_SUBDIRECTORY; fi
    [ -d "$SAVE/$child" ] && [ "$(readlink -f "$SAVE/$child")" = "$SAVE/$child" ] || fail INVALID_SAVE_SUBDIRECTORY
    [ "$(stat -c %d "$SAVE/$child")" = "$(stat -c %d "$SD")" ] || fail SAVE_SUBDIRECTORY_WRONG_DEVICE
    stat -c '%d:%i' "$SAVE/$child" > "$R/storage-meta/save-$child-identity"
done
printf '%s\n' "$SAVE" > "$R/storage-meta/save-path"
stat -c '%d:%i' "$SAVE" > "$R/storage-meta/save-identity"
stat -c '%d:%i' "$SD" > "$R/storage-meta/sd-identity"
stat -c '%d:%i' "$R/data-image" > "$R/storage-meta/data-image-identity"
printf '%s\n' tmpfs > "$R/storage-meta/data-image-type"
storage_host_check || fail SD_SAVE_DIRECTORY_NOT_PINNED
log "SD_SAVE_SCOPE=$SAVE/Projects,$SAVE/Samples; /data original read-only with RAM changes up to512MiB; native reserve400MiB"
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
# Apply only to the just-verified copy in /run, before stopping stock.
chmod 700 "$R/MPC-Sample"
[ "$(hash "$DIR/apply-peek")" = "2ea8c8a4851cee4fd5278ec6173011ad3fc6bf3673a02c958bcace20c1f58f0a" ] || fail WRONG_PEEK_HELPER
cp "$DIR/apply-peek" "$R/apply-peek"
chmod 500 "$R/apply-peek"
[ "$(hash "$R/apply-peek")" = "2ea8c8a4851cee4fd5278ec6173011ad3fc6bf3673a02c958bcace20c1f58f0a" ] || fail PEEK_HELPER_COPY_MISMATCH
"$R/apply-peek" >> "$R/events.txt" 2>&1 || fail PEEK_PATCH_FAILED
chmod 500 "$R/MPC-Sample"
[ "$(hash "$R/MPC-Sample")" = "$PATCH_SHA" ] || fail PEEK_HASH_MISMATCH
log "K_VALUE_PEEK_PATCH_VERIFIED sha256=$PATCH_SHA"
[ "$(hash "$DIR/xdg-dbus-proxy")" = "$PROXY_SHA" ] || fail WRONG_PROXY_HASH
[ "$(hash "$DIR/proxy-policy.args")" = "$POLICY_SHA" ] || fail WRONG_POLICY_HASH
cp "$DIR/xdg-dbus-proxy" "$R/dbus-proxy"
cp "$DIR/proxy-policy.args" "$R/proxy-policy.args"
chmod 500 "$R/dbus-proxy"
[ "$(hash "$R/dbus-proxy")" = "$PROXY_SHA" ] || fail PROXY_COPY_HASH_MISMATCH

# A bounded diagnostic interceptor is read-only in the clone namespace.
[ "$(hash "$DIR/save-io.so")" = "$SAVE_IO_SHA" ] || fail WRONG_SAVE_IO_LIBRARY
cp "$DIR/save-io.so" "$R/save-io.so"
chmod 500 "$R/save-io.so"
[ "$(hash "$R/save-io.so")" = "$SAVE_IO_SHA" ] || fail SAVE_IO_COPY_MISMATCH
printf 'SAVE_IO_DIAGNOSTICS r9h max_records1024 RAM_only\n' > "$R/app/save-io.txt"
# Common sandbox. Only the two pinned native SD save folders are writable.
cat > "$R/sandbox.conf" <<EOF
PrivateMounts=yes
MountFlags=private
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
BindPaths=$R/app:$APP $R/data-image:/data
ReadWritePaths=$SAVE/Projects $SAVE/Samples
BindReadOnlyPaths=$R/save-io.so:$APP/save-io.so $R/proxy:/run/dbus $R/launcher.sh:/usr/bin/launch-ac50 $R/MPC-Sample:"$STOCK" $R/storage-meta:$APP/storage-meta $R/data-changes:$APP/data-changes
BindReadOnlyPaths=/run/udev/data:/run/udev/data
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
        echo 'Description=MPC K-value peek RAM trial'
        echo '[Service]'
        if [ "$mode" = probe ]; then echo Type=oneshot; echo TimeoutStartSec=20s;
        else echo Type=exec; echo "RuntimeMaxSec=infinity"; fi
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
RuntimeMaxSec=infinity
Restart=no
EOF
cat > "/run/systemd/system/$U-recover.service" <<EOF
[Unit]
Description=MPC trial independent stock recovery
[Service]
Type=oneshot
ExecStart=/bin/sh $R/launcher.sh recover
TimeoutStartSec=75s
RemainAfterExit=yes
EOF
cat > "/run/systemd/system/$U-supervisor.service" <<EOF
[Unit]
Description=MPC trial supervisor
[Service]
Type=exec
ExecStart=/bin/sh $R/launcher.sh supervise
ExecStopPost=/usr/bin/systemctl --no-block start $U-recover.service
RuntimeMaxSec=infinity
TimeoutStartSec=10s
TimeoutStopSec=5s
Restart=no
EOF
cat > "/run/systemd/system/$U-guard.service" <<EOF
[Unit]
Description=MPC independent controller health check
[Service]
Type=oneshot
ExecStart=/bin/sh $R/launcher.sh guard
TimeoutStartSec=8s
EOF
cat > "/run/systemd/system/$U-guard.timer" <<EOF
[Unit]
Description=MPC independent controller health check timer
[Timer]
OnActiveSec=2s
OnUnitInactiveSec=2s
AccuracySec=1s
Unit=$U-guard.service
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
[ "$(prop "$U-clone.service" RuntimeMaxUSec)" = "$CLONE_RUNTIME_EXPECTED" ] || fail UNEXPECTED_CLONE_LIMIT
for unlimited_unit in supervisor bus-proxy; do
    [ "$(prop "$U-$unlimited_unit.service" RuntimeMaxUSec)" = infinity ] || fail UNEXPECTED_RUNTIME_LIMIT
done
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
printf '%s %s\n' "$$" "$(awk '{print $22}' /proc/$$/stat)" > "$R/autorun-id"
now=$(cut -d . -f 1 /proc/uptime)
printf '%s\n' "$((now+60))" > "$R/startup-deadline"
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
log "RECOVERY_ARMED; manual Mod Off, no session time limit; independent controller health check"
systemctl start "$U-supervisor.service" || fail SUPERVISOR_START_FAILED
log HANDED_TO_SYSTEMD
exit 0
