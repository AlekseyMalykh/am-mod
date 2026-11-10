"""Package the experimental r28 build over the pinned public r27r release."""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parent
BASE_SHA = "2b2012edb340e99c5610216becddaa0663b038885426692495469c620e9c25eb"
SALT = b"f7bed1c9-7e90-4731-9194-c996666b4331"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def verified_asset(folder, proof_name, asset_name, hash_key):
    proof = json.loads((folder / proof_name).read_text())
    assert proof["passed"]
    for source, expected in proof["sources_sha256"].items():
        assert sha((ROOT / source).read_bytes()) == expected, f"Stale build input: {source}"
    asset = (folder / asset_name).read_bytes()
    assert sha(asset) == proof[hash_key], asset_name
    return asset


def protect_internal_exports(launcher):
    """Pin the native Song export path to the SD and protect internal user files."""
    def replace(old, new):
        nonlocal launcher
        assert launcher.count(old) == 1, old
        launcher = launcher.replace(old, new)

    replace('storage_host_check || fail SD_SAVE_DIRECTORY_NOT_PINNED\n',
            '''storage_host_check || fail SD_SAVE_DIRECTORY_NOT_PINNED
EXPORT_PATH="$SAVE/Samples/Export"
[ ! -L "$EXPORT_PATH" ] || fail SONG_EXPORT_SYMLINK
if [ ! -d "$EXPORT_PATH" ]; then mkdir "$EXPORT_PATH" || fail SONG_EXPORT_CREATE; fi
[ "$(readlink -f "$EXPORT_PATH")" = "$EXPORT_PATH" ] || fail SONG_EXPORT_PATH
[ "$(stat -c %d "$EXPORT_PATH")" = "$(stat -c %d "$SAVE/Samples")" ] || fail SONG_EXPORT_DEVICE
stat -c '%d:%i' "$EXPORT_PATH" > "$R/storage-meta/song-export-identity"
mkdir -p "$R/data-image/UserContent/Samples/User/Export" || fail SONG_EXPORT_MOUNTPOINT
''')
    replace('    touch "$R/data-staged"\n',
            '''    mkdir -p "$R/data-image/UserContent/Samples/User/Export" || return 1
    [ "$(readlink -f "$R/data-image/UserContent/Samples/User/Export")" = "$R/data-image/UserContent/Samples/User/Export" ] || return 1
    touch "$R/data-staged"
''')
    replace('    echo "TEMP_WORKSPACE_BOUND filesystem=$save_fs path=/data/TempProjects"\n',
            '''    echo "TEMP_WORKSPACE_BOUND filesystem=$save_fs path=/data/TempProjects"
    song_export=/data/UserContent/Samples/User/Export
    [ "$(stat -c '%d:%i' "$song_export")" = "$(cat "$APP/storage-meta/song-export-identity")" ] || return 1
    [ "$(stat -c '%d:%i' "$save_path/Samples/Export")" = "$(cat "$APP/storage-meta/song-export-identity")" ] || return 1
    [ "$(stat -f -c %t "$song_export")" = "$save_magic" ] || return 1
''')
    replace('        if (fs ~ /^(ext[234]|exfat|vfat|f2fs|btrfs|xfs|ntfs|ntfs3|fuseblk|overlay)$/',
            '''        if (target=="/data/UserContent" && opts ~ /(^|,)ro(,|$)/) protected_internal++;
        if (fs ~ /^(ext[234]|exfat|vfat|f2fs|btrfs|xfs|ntfs|ntfs3|fuseblk|overlay)$/''')
    replace('          else if (target=="/data/TempProjects"',
            '''          else if (target=="/data/UserContent/Samples/User/Export" && fs==save_fs && opts ~ /(^|,)rw(,|$)/) allowed_song_export++;
          else if (target=="/data/TempProjects"''')
    replace(' || allowed_workspace!=1 || visible_data!=1',
            ' || allowed_workspace!=1 || allowed_song_export!=1 || protected_internal!=1 || visible_data!=1')
    replace('BindPaths=$R/app:$APP $R/data-image:/data $TEMP_WORK:/data/TempProjects\n',
            'BindPaths=$R/app:$APP $R/data-image:/data $TEMP_WORK:/data/TempProjects $EXPORT_PATH:/data/UserContent/Samples/User/Export\n')
    replace('ReadWritePaths=$SAVE/Projects $SAVE/Samples\n',
            '''ReadWritePaths=$SAVE/Projects $SAVE/Samples /data/UserContent/Samples/User/Export
ReadOnlyPaths=/data/UserContent
''')
    return launcher


def add_screen_mirror(launcher, digest):
    stage = 'storage_host_check || fail SD_SAVE_DIRECTORY_NOT_PINNED\n'
    start = '    systemctl start "$U-clone.service"\n'
    assert launcher.count(stage) == launcher.count(start) == 1
    launcher = launcher.replace(stage, stage + f'''if [ "$(hash "$DIR/screen-mirror")" = "{digest}" ] &&
   cp "$DIR/screen-mirror" "$R/screen-mirror" &&
   chmod 500 "$R/screen-mirror" &&
   [ "$(hash "$R/screen-mirror")" = "{digest}" ]; then
    touch "$R/screen-mirror-ready"
else
    log SCREEN_MIRROR_STAGE_FAILED
fi
''')
    return launcher.replace(start, start + '''    if [ -f "$R/screen-mirror-ready" ]; then
        systemd-run --quiet --unit="$U-screen-mirror" \\
            -p "BindsTo=$U-clone.service" -p "After=$U-clone.service" \\
            -p NoNewPrivileges=yes -p ProtectSystem=strict -p ProtectHome=yes \\
            -p DevicePolicy=closed \\
            -p Nice=15 -p MemoryMax=16M -p TasksMax=2 \\
            "$R/screen-mirror" "$R/app/screen-mirror.on" "$R/app/screen-mirror.bmp" || log SCREEN_MIRROR_START_FAILED
    fi
''')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("base", type=Path, help="Published am-mod-r27r.zip")
    parser.add_argument("revision", choices=("r28a", "r28b", "r28c"), help="Release revision")
    args = parser.parse_args()
    revision = args.revision
    assert sha(args.base.read_bytes()) == BASE_SHA, "Unexpected r27r base"
    assert f'#define AM_MOD_VERSION "{revision}"' in (ROOT / "src/mod_build.h").read_text()
    library = verified_asset(ROOT / "build/mixer-r27r", "compile-verification.json",
                             "phone-input.so", "library_sha256")
    splash = verified_asset(ROOT / "build/early-drm", "verification.json", "boot-splash", "splash_sha256")

    with zipfile.ZipFile(args.base) as package:
        assert package.testzip() is None
        files = {name: package.read(name) for name in package.namelist()}

    launcher_path = "autorun/mod-launcher.sh"
    old_launcher = files[launcher_path]
    launcher = old_launcher.decode()
    for name, asset, count in [("phone-input.so", library, 1), ("boot-splash", splash, 2)]:
        path = f"autorun/{name}"
        old_hash = sha(files[path])
        assert launcher.count(old_hash) == count, name
        launcher = launcher.replace(old_hash, sha(asset))
        files[path] = asset
    if revision == "r28c":
        launcher = protect_internal_exports(launcher)
        helper = ROOT / "build" / "screen-mirror"
        helper.parent.mkdir(exist_ok=True)
        subprocess.run(["aarch64-linux-gnu-gcc", "-O2", "-Wall", "-Wextra", "-Werror", "-static",
                        str(ROOT / "src/screen_mirror.c"), "-o", str(helper)], check=True)
        files["autorun/screen-mirror"] = helper.read_bytes()
        launcher = add_screen_mirror(launcher, sha(files["autorun/screen-mirror"]))
    files[launcher_path] = launcher.encode()

    entry = files["autorun/autorun.sh"].decode()
    assert entry.count(sha(old_launcher)) == 1 and entry.count("revision=r27r") == 1
    files["autorun/autorun.sh"] = entry.replace(sha(old_launcher), sha(files[launcher_path])).replace(
        "revision=r27r", f"revision={revision}"
    ).encode()
    manifest = json.loads(files["autorun/autorun.json"])
    manifest["value0"]["scripts"][0]["message"] = f"Am MOD {revision} - Experimental Mixer and Stems"
    files["autorun/autorun.json"] = (json.dumps(manifest, indent=2) + "\n").encode()
    files["autorun/autorun.check"] = sha(
        files["autorun/autorun.json"] + files["autorun/autorun.sh"] + SALT
    ).encode()
    for name in ("autorun.sh", "mod-launcher.sh"):
        subprocess.run(["sh", "-n", "/dev/stdin"], input=files[f"autorun/{name}"], check=True)
    for suffix in ("", ".ru"):
        files[f"README{suffix}.md"] = (REPO / "docs" / f"RELEASE_NOTES-{revision}{suffix}.md").read_bytes()

    out = ROOT / "build" / revision / f"am-mod-{revision}.zip"
    out.parent.mkdir(parents=True, exist_ok=True)
    sys.path.insert(0, str(REPO / "installer"))
    from mpc_mod_installer import Package
    with zipfile.ZipFile(out, "x", zipfile.ZIP_DEFLATED) as package:
        for name, data in sorted(files.items()):
            package.writestr(name, data)
    assert Package.from_file(out).revision == revision
    digest = sha(out.read_bytes())
    (out.parent / "SHA256SUMS.txt").write_text(f"{digest}  {out.name}\n")
    print(out, digest)


if __name__ == "__main__":
    main()
