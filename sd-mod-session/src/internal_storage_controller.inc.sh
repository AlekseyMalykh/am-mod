# Host side only, before creating the application's private namespace.
# Persist user documents/audio, never the complete /data filesystem.
internal_storage_prepare() {
    for internal_dir in /data/UserContent /data/UserContent/Projects /data/UserContent/Samples; do
        [ -d "$internal_dir" ] && [ ! -L "$internal_dir" ] || return 1
        [ "$(readlink -f "$internal_dir")" = "$internal_dir" ] || return 1
        [ "$(stat -f -c %t "$internal_dir")" = ef53 ] || return 1
    done
    for internal_dir in /data/UserContent/Samples/User /data/UserContent/Samples/User/Export; do
        [ ! -L "$internal_dir" ] || return 1
        if [ ! -e "$internal_dir" ]; then mkdir "$internal_dir" || return 1; fi
        [ -d "$internal_dir" ] && [ "$(readlink -f "$internal_dir")" = "$internal_dir" ] || return 1
    done
    [ "$(stat -c %d /data/UserContent/Projects)" = "$(stat -c %d /data/UserContent/Samples/User)" ] || return 1
    printf 'AM_INTERNAL1\n%s\n%s\n' \
        "$(stat -c '%d:%i' /data/UserContent/Projects)" \
        "$(stat -c '%d:%i' /data/UserContent/Samples/User)" > "$R/storage-meta/internal-storage" || return 1
    log 'INTERNAL_USER_STORAGE_PINNED projects=rw samples_user=rw other_user_content=ro system_data=RAM'
}
