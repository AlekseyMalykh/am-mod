"""Add a pinned SD TempProjects bind to an existing verified launcher."""
def transform(text,helper_sha):
    assert len(helper_sha)==64
    def replace(old,new,count=1):
        nonlocal text
        assert text.count(old)==count,old
        text=text.replace(old,new)
    replace('storage_host_check || fail SD_SAVE_DIRECTORY_NOT_PINNED\n', '''storage_host_check || fail SD_SAVE_DIRECTORY_NOT_PINNED
[ "$(hash "$DIR/temp-workspace")" = "'''+helper_sha+'''" ] || fail WRONG_TEMP_WORKSPACE_HELPER
cp "$DIR/temp-workspace" "$R/temp-workspace"
chmod 500 "$R/temp-workspace"
[ "$(hash "$R/temp-workspace")" = "'''+helper_sha+'''" ] || fail TEMP_WORKSPACE_HELPER_COPY_MISMATCH
"$R/temp-workspace" > "$R/storage-meta/temp-workspace-path" || fail SD_TEMP_WORKSPACE_PREPARE_FAILED
TEMP_WORK=$(cat "$R/storage-meta/temp-workspace-path")
[ "$TEMP_WORK" = "$SAVE/Samples/.am-mod-workspace/temp-projects-v1" ] || fail WRONG_TEMP_WORKSPACE_PATH
[ "$(readlink -f "$TEMP_WORK")" = "$TEMP_WORK" ] || fail TEMP_WORKSPACE_SYMLINK
[ "$(stat -c %d "$TEMP_WORK")" = "$(stat -c %d "$SAVE/Samples")" ] || fail TEMP_WORKSPACE_WRONG_DEVICE
stat -c '%d:%i' "$TEMP_WORK" > "$R/storage-meta/temp-workspace-identity"
log "TEMP_WORKSPACE_SD_READY path=$TEMP_WORK native_reserve_unchanged=1"
''')
    replace('BindPaths=$R/app:$APP $R/data-image:/data\n',
        'BindPaths=$R/app:$APP $R/data-image:/data $TEMP_WORK:/data/TempProjects\n')
    replace('    [ "$(stat -c \'%d:%i\' /data)" = "$(cat "$APP/storage-meta/data-image-identity")" ] || return 1\n', '''    [ "$(stat -c '%d:%i' /data/TempProjects)" = "$(cat "$APP/storage-meta/temp-workspace-identity")" ] || return 1
    [ "$(stat -f -c %t /data/TempProjects)" = "$save_magic" ] || return 1
    echo "TEMP_WORKSPACE_BOUND filesystem=$save_fs path=/data/TempProjects"
    [ "$(stat -c '%d:%i' /data)" = "$(cat "$APP/storage-meta/data-image-identity")" ] || return 1
''')
    replace('          else { print "UNEXPECTED_WRITABLE_DISK:',
        '          else if (target=="/data/TempProjects" && fs==save_fs && opts ~ /(^|,)rw(,|$)/) allowed_workspace++;\n          else { print "UNEXPECTED_WRITABLE_DISK:')
    replace('exit bad || allowed_projects!=1 || allowed_samples!=1 || visible_data!=1',
        'exit bad || allowed_projects!=1 || allowed_samples!=1 || allowed_workspace!=1 || visible_data!=1')
    replace('    # Exactly two accessible disk-backed mounts may be writable: native SD folders.',
        '    # Native SD folders and their pinned TempProjects cache are writable.')
    replace('    awk -v p="$save_sd" -v fs="$save_fs"', '''    if [ -f "$R/storage-meta/temp-workspace-path" ]; then
        work=$(cat "$R/storage-meta/temp-workspace-path") || return 1
        [ "$work" = "$save_path/Samples/.am-mod-workspace/temp-projects-v1" ] || return 1
        [ "$(readlink -f "$work")" = "$work" ] || return 1
        [ "$(stat -c '%d:%i' "$work")" = "$(cat "$R/storage-meta/temp-workspace-identity")" ] || return 1
    fi
    awk -v p="$save_sd" -v fs="$save_fs"''')
    replace('            df -k "$R/data-changes" "$R/data-image" || true', '''            df -k "$R/data-changes" "$R/data-image" || true
            if [ -f "$R/storage-meta/temp-workspace-path" ]; then
                df -k "$(cat "$R/storage-meta/temp-workspace-path")" || true
            fi''')
    return text
