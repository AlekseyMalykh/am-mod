# The DRM child keeps its own buffer alive after releasing master. It exits
# when native scanout replaces it, or when preparation/recovery cancels it.
boot_display_stop() {
    [ ! -d "$R" ] || : > "$R/boot-splash-stop"
}
boot_display_start() {
    # The supervisor calls this again before clone startup. Do not allocate
    # or select a second framebuffer during the same handoff.
    [ ! -s "$R/boot-splash-ready" ] || return 0
    [ ! -e "$R/boot-splash-stop" ] || return 1
    setsid "$R/boot-splash" --hold "$R/boot-splash-ready" "$R/boot-splash-stop" >> "$R/events.txt" 2>&1 &
    display_child=$!
    display_wait=0
    while [ "$display_wait" -lt 100 ]; do
        if [ -s "$R/boot-splash-ready" ]; then
            read display_pid display_fb < "$R/boot-splash-ready"
            log "EARLY_DRM_READY pid=$display_pid framebuffer=$display_fb"
            return 0
        fi
        kill -0 "$display_child" 2>/dev/null || return 1
        sleep 0.05
        display_wait=$((display_wait+1))
    done
    boot_display_stop
    return 1
}
