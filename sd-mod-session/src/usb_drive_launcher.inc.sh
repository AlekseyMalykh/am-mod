# Sourced into the RAM launcher by the package builder, not by the app.
usb_drive_repin() {
    save_path=$(cat "$R/storage-meta/save-path") || return 1
    sd_path=${save_path%/MPC-Sample}
    [ "$(stat -c %d "$sd_path")" = "$(cat "$R/sd-device")" ] || return 1
    for dir in "$sd_path" "$save_path" "$save_path/Projects" "$save_path/Samples" "$save_path/Samples/Export" "$sd_path/autorun"; do
        [ -d "$dir" ] && [ ! -L "$dir" ] && [ "$(readlink -f "$dir")" = "$dir" ] || return 1
        [ "$(stat -c %d "$dir")" = "$(cat "$R/sd-device")" ] || return 1
    done
    stat -c '%d:%i' "$sd_path" > "$R/storage-meta/sd-identity"
    stat -c '%d:%i' "$save_path" > "$R/storage-meta/save-identity"
    for child in Projects Samples; do stat -c '%d:%i' "$save_path/$child" > "$R/storage-meta/save-$child-identity"; done
    stat -c '%d:%i' "$save_path/Samples/Export" > "$R/storage-meta/song-export-identity"
    stat -c '%d:%i' "$sd_path/autorun" > "$R/storage-meta/journal-identity"
    work=$(cat "$R/storage-meta/temp-workspace-path") || return 1
    [ "$work" = "$save_path/Samples/.am-mod-workspace/temp-projects-v1" ] || return 1
    [ -d "$work" ] && [ "$(readlink -f "$work")" = "$work" ] || return 1
    [ "$(stat -c %d "$work")" = "$(cat "$R/sd-device")" ] || return 1
    stat -c '%d:%i' "$work" > "$R/storage-meta/temp-workspace-identity"
    # The return target is a specific saved project, not the newest file the
    # computer may have uploaded. Missing/deleted copies do not silently load
    # another project.
    resume=$(cat "$R/app/usb-resume-project") || return 1
    [ "$resume" = "$save_path/Projects/Mod-Exit-Last.xpj" ] || return 1
    [ -s "$resume" ] && [ ! -L "$resume" ] || return 1
    storage_host_check
}
usb_drive_proxy_start() {
    [ "$(prop "$U-bus-proxy.service" MainPID)" = 0 ] || return 1
    rm -f "$R/proxy/system_bus_socket"
    systemctl start "$U-bus-proxy.service" || return 1
    retry=0
    while [ ! -S "$R/proxy/system_bus_socket" ]; do
        retry=$((retry+1));[ "$retry" -lt 50 ] || return 1;sleep 0.1
    done
    stat -Lc '%d:%i' "$R/proxy/system_bus_socket" > "$R/app/proxy-socket-id"
}
usb_drive_audio_prepare() {
    "$R/usb-mode.sh" prepare "$(cat "$R/storage-meta/save-path")" || return 1
    # SD mode recreates the audio gadget even when the selected mode is stereo.
    # Refresh its device allow-list on every return.
        udevadm settle --timeout=5 || return 1
        mkdir -p "/run/systemd/system/$U-clone.service.d"
        usb_drop="/run/systemd/system/$U-clone.service.d/usb-audio.conf"
        printf '[Service]\n' > "$usb_drop"
        for usb_node in /dev/snd/*; do
            [ -c "$usb_node" ] || continue
            printf 'DeviceAllow=%s rw\n' "$usb_node" >> "$usb_drop"
        done
        systemctl daemon-reload
}
usb_drive_cycle() {
    touch "$R/usb-drive-active" || return 1
    rm -f "$R/app/usb-drive.request" "$R/app/usb-files.on" "$R/app/screen-mirror.on" || return 1
    # Capture MIDI subscriptions while the app still owns its ALSA ports.
    {
        date -u +%Y-%m-%dT%H:%M:%SZ
        for midi_proc in /proc/asound/cards /proc/asound/seq/clients /proc/asound/seq/ports /proc/asound/card*/midi*; do
            [ -f "$midi_proc" ] || continue
            echo "--- $midi_proc ---"; cat "$midi_proc" || true
        done
    } > "$R/app/midi-transport.txt" 2>&1 || true
    # Every SD namespace owned by Am MOD must go away before an unmount.
    for service in "$U-usb-files" "$U-screen-mirror" "$U-clone" "$U-bus-proxy"; do
        systemctl stop "$service.service" || { [ "$(prop "$service.service" LoadState 2>/dev/null || true)" = not-found ] || return 1; }
        [ "$(prop "$service.service" MainPID 2>/dev/null || echo 0)" = 0 ] || return 1
    done
    "$R/stage-data" --flush-sd >> "$R/events.txt" 2>&1 || return 1
    export_report || true
    "$R/usb-mode.sh" restore || return 1
    transfer_ok=0
    screen_pid=
    screen_start=
    if "$R/usb-drive.sh" prepare "$(cat "$R/storage-meta/save-path")" >> "$R/events.txt" 2>&1; then
        rm -f "$R/usb-drive-screen.ready"
        "$R/usb-drive-screen" "$R/usb-drive/phase" "$R/usb-drive-screen.ready" >> "$R/events.txt" 2>&1 &
        screen_pid=$!
        screen_start=$(awk '{print $22}' "/proc/$screen_pid/stat" 2>/dev/null || true)
        retry=0
        while [ ! -f "$R/usb-drive-screen.ready" ] && kill -0 "$screen_pid" 2>/dev/null; do
            retry=$((retry+1));[ "$retry" -lt 50 ] || break;sleep 0.1
        done
        # A failed display does not strand a card as an invisible USB drive.
        if [ -f "$R/usb-drive-screen.ready" ]; then
            if "$R/usb-drive.sh" share >> "$R/events.txt" 2>&1; then transfer_ok=1; fi
        fi
    fi
    "$R/usb-drive.sh" restore >> "$R/events.txt" 2>&1 || return 1
    if [ -n "$screen_pid" ]; then
        if [ -n "$screen_start" ] && [ "$(awk '{print $22}' "/proc/$screen_pid/stat" 2>/dev/null || true)" = "$screen_start" ]; then kill "$screen_pid" 2>/dev/null || true;fi
        wait "$screen_pid" 2>/dev/null || true
    fi
    usb_drive_repin || return 1
    usb_drive_proxy_start || return 1
    usb_drive_audio_prepare || return 1
    if [ "$transfer_ok" != 1 ]; then
        if [ -s "$R/usb-drive/error" ]; then dd if="$R/usb-drive/error" bs=64 count=1 2>/dev/null > "$R/app/usb-drive-error";
        else printf 'SCREEN_NOT_READY\n' > "$R/app/usb-drive-error";fi
        printf '1\n' > "$R/app/usb-drive.failed"
    fi
    systemctl start "$U-clone.service" || return 1
    systemctl is-active --quiet "$U-clone.service" || return 1
    usb_start_aux
    rm -f "$R/usb-drive-active"
    log "USB_DRIVE_RETURNED transfer_ok=$transfer_ok saved_project=Mod-Exit-Last"
    return 0
}
