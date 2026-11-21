"""Narrow persistent Internal storage for a verified current AM controller."""
from pathlib import Path
R=Path(__file__).resolve().parent
def persistent_exports(launcher):
 if 'INTERNAL_EXPORT_PERSISTENT' in launcher:return launcher
 old=' $EXPORT_PATH:/data/UserContent/Samples/User/Export'
 assert launcher.count(old)==1;launcher=launcher.replace(old,'')
 launcher=launcher.replace('ReadWritePaths=$SAVE/Projects $SAVE/Samples /data/UserContent/Projects /data/UserContent/Samples/User /data/UserContent/Samples/User/Export','ReadWritePaths=$SAVE/Projects $SAVE/Samples /data/UserContent/Projects /data/UserContent/Samples/User')
 old='    [ "$(stat -c \'%d:%i\' "$song_export")" = "$(cat "$APP/storage-meta/song-export-identity")" ] || return 1'
 assert launcher.count(old)==1
 launcher=launcher.replace(old,'    # INTERNAL_EXPORT_PERSISTENT: inherits the pinned Internal Samples/User bind.\n    [ "$(readlink -f "$song_export")" = "$song_export" ] || return 1\n    [ "$(stat -c %d "$song_export")" = "$(stat -c %d /data/UserContent/Samples/User)" ] || return 1')
 launcher=launcher.replace('[ "$(stat -f -c %t "$song_export")" = "$save_magic" ]','[ "$(stat -f -c %t "$song_export")" = ef53 ]')
 line='          else if (target=="/data/UserContent/Samples/User/Export" && fs==save_fs && opts ~ /(^|,)rw(,|$)/) allowed_song_export++;\n'
 assert launcher.count(line)==1;launcher=launcher.replace(line,'').replace(' || allowed_song_export!=1','')
 return launcher
def transform(launcher):
 if 'INTERNAL_USER_STORAGE_PINNED' in launcher:return persistent_exports(launcher)
 def replace(old,new):
  nonlocal launcher
  assert launcher.count(old)==1,old
  launcher=launcher.replace(old,new)
 definitions=(R/'src/internal_storage_controller.inc.sh').read_text()
 replace('storage_host_check() {',definitions+'\nstorage_host_check() {')
 replace('# Common sandbox. Only the two pinned native SD save folders are writable.',
         'internal_storage_prepare || fail INTERNAL_USER_STORAGE_NOT_PINNED\ntouch "$R/data-staged" || fail INTERNAL_FLUSH_STATE\n# Common sandbox: pinned SD and Internal user folders only.')
 replace('BindPaths=$R/app:$APP $R/data-image:/data $TEMP_WORK:/data/TempProjects $EXPORT_PATH:/data/UserContent/Samples/User/Export',
         'BindPaths=$R/app:$APP $R/data-image:/data /data/UserContent/Projects:/data/UserContent/Projects /data/UserContent/Samples/User:/data/UserContent/Samples/User $TEMP_WORK:/data/TempProjects $EXPORT_PATH:/data/UserContent/Samples/User/Export')
 replace('ReadWritePaths=$SAVE/Projects $SAVE/Samples /data/UserContent/Samples/User/Export',
         'ReadWritePaths=$SAVE/Projects $SAVE/Samples /data/UserContent/Projects /data/UserContent/Samples/User /data/UserContent/Samples/User/Export')
 replace('ReadOnlyPaths=/data/UserContent\n','BindReadOnlyPaths=/data/UserContent:/data/UserContent\n')
 anchor='    song_export=/data/UserContent/Samples/User/Export\n'
 replacement='''    internal_identity=$(printf 'AM_INTERNAL1\\n%s\\n%s\\n' "$(stat -c '%d:%i' /data/UserContent/Projects)" "$(stat -c '%d:%i' /data/UserContent/Samples/User)") || return 1
    [ "$internal_identity" = "$(cat "$APP/storage-meta/internal-storage")" ] || return 1
    for internal_dir in /data/UserContent/Projects /data/UserContent/Samples/User; do
        [ "$(readlink -f "$internal_dir")" = "$internal_dir" ] || return 1
        [ "$(stat -f -c %t "$internal_dir")" = ef53 ] || return 1
    done
    echo INTERNAL_USER_STORAGE_BOUND
'''+anchor
 replace(anchor,replacement)
 anchor='          else if (target=="/data/TempProjects"'
 replace(anchor,'''          else if (target=="/data/UserContent/Projects" && fs=="ext4") allowed_internal_projects++;
          else if (target=="/data/UserContent/Samples/User" && fs=="ext4") allowed_internal_samples++;
'''+anchor)
 replace(' || protected_internal!=1 || visible_data!=1',' || allowed_internal_projects!=1 || allowed_internal_samples!=1 || protected_internal!=1 || visible_data!=1')
 replace('    "$R/stage-data" --flush-sd >> "$R/events.txt" 2>&1 || return 1',
         '    "$R/stage-data" --flush-sd >> "$R/events.txt" 2>&1 || return 1\n    "$R/stage-data" --flush-internal >> "$R/events.txt" 2>&1 || return 1')
 replace('        if ! "$R/stage-data" --flush-sd >> "$R/events.txt" 2>&1; then',
         '        if ! "$R/stage-data" --flush-sd >> "$R/events.txt" 2>&1 || ! "$R/stage-data" --flush-internal >> "$R/events.txt" 2>&1; then')
 return persistent_exports(launcher)
