# Early handoff: stop stock before copying/checking the large application.
# A detached watcher (systemd fallback) restores stock if preparation exits or times out.
early_watch() {
    read early_pid early_start < "$R/early-main-id"
    early_deadline=$(cat "$R/early-deadline")
    trap '' HUP
    printf '%s %s\n' "$$" "$(awk '{print $22}' /proc/$$/stat)" > "$R/early-watch-ready"
    while [ ! -f "$R/early-handoff" ]; do
        early_now=$(cut -d . -f 1 /proc/uptime)
        early_actual=$(awk '{print $22}' "/proc/$early_pid/stat" 2>/dev/null || true)
        if [ "$early_actual" != "$early_start" ] || [ "$early_now" -ge "$early_deadline" ]; then
            [ ! -f "$R/early-handoff" ] || return
            printf '%s EARLY_PREPARATION_RECOVERY\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$R/events.txt"
            if [ "$early_actual" = "$early_start" ]; then kill -TERM "$early_pid" 2>/dev/null || true; fi
            systemctl --no-block start "$STOCK_UNIT"
            return
        fi
        sleep 1
    done
}
early_begin() {
    [ "$(uname -m)" = aarch64 ] || fail WRONG_ARCHITECTURE
    systemctl is-active --quiet "$STOCK_UNIT" || fail EARLY_STOCK_NOT_ACTIVE
    early_stock_pid=$(prop "$STOCK_UNIT" MainPID)
    [ "$early_stock_pid" -gt 1 ] || fail EARLY_STOCK_PID_MISSING
    [ "$(stat -Lc '%d:%i' "/proc/$early_stock_pid/exe")" = "$(stat -Lc '%d:%i' "$STOCK")" ] || fail EARLY_UNKNOWN_RUNNING_APP
    [ ! -L "$DIR/boot-splash" ] || fail EARLY_SPLASH_SYMLINK
    [ "$(hash "$DIR/boot-splash")" = EARLY_SPLASH_SHA ] || fail WRONG_EARLY_SPLASH
    cp "$DIR/boot-splash" "$R/boot-splash"
    chmod 500 "$R/boot-splash"
    [ "$(hash "$R/boot-splash")" = EARLY_SPLASH_SHA ] || fail EARLY_SPLASH_COPY_MISMATCH
    cp "$0" "$R/launcher.sh"
    printf '%s %s\n' "$$" "$(awk '{print $22}' /proc/$$/stat)" > "$R/early-main-id"
    early_now=$(cut -d . -f 1 /proc/uptime)
    printf '%s\n' "$((early_now+120))" > "$R/early-deadline"
    if command -v setsid >/dev/null 2>&1; then
        # Match the existing firmware's detached autorun pattern. Avoid
        # daemon-reload before stopping stock: it allowed the old project to
        # become visible. The watcher acknowledges readiness before the stop.
        setsid /bin/sh "$R/launcher.sh" early-watch </dev/null >"$R/early-watch.log" 2>&1 &
        early_wait=0
        while [ ! -s "$R/early-watch-ready" ]; do
            [ "$early_wait" -lt 100 ] || fail EARLY_WATCH_NOT_READY
            sleep 0.05
            early_wait=$((early_wait+1))
        done
        read early_watch_pid early_watch_start < "$R/early-watch-ready"
        [ "$(awk '{print $22}' "/proc/$early_watch_pid/stat" 2>/dev/null || true)" = "$early_watch_start" ] || fail EARLY_WATCH_DIED
    else
        # Compatibility fallback; the native firmware includes setsid.
        cat > "/run/systemd/system/$U-early-watch.service" <<EOF
[Unit]
Description=AM Mod early startup recovery
[Service]
Type=exec
ExecStart=/bin/sh $R/launcher.sh early-watch
Restart=no
KillMode=control-group
TimeoutStopSec=2s
MemoryMax=16M
TasksMax=16
EOF
        systemctl daemon-reload
        systemctl start "$U-early-watch.service" || fail EARLY_WATCH_START_FAILED
        systemctl is-active --quiet "$U-early-watch.service" || fail EARLY_WATCH_NOT_ACTIVE
        touch "$R/early-watch-systemd"
    fi
    touch "$R/early-stop-intent"
    systemctl stop "$STOCK_UNIT"
    [ "$(prop "$STOCK_UNIT" MainPID)" = 0 ] || fail EARLY_STOCK_STILL_RUNNING
    touch "$R/early-stopped"
    "$R/boot-splash" --show >> "$R/events.txt" 2>&1 || log EARLY_SPLASH_UNAVAILABLE
    log EARLY_STOCK_STOPPED
}
