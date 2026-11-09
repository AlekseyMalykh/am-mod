"""Package the experimental r28a build over the pinned public r27r release."""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parent
REVISION = "r28a"
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("base", type=Path, help="Published am-mod-r27r.zip")
    args = parser.parse_args()
    assert sha(args.base.read_bytes()) == BASE_SHA, "Unexpected r27r base"
    assert f'#define AM_MOD_VERSION "{REVISION}"' in (ROOT / "src/mod_build.h").read_text()
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
    files[launcher_path] = launcher.encode()

    entry = files["autorun/autorun.sh"].decode()
    assert entry.count(sha(old_launcher)) == 1 and entry.count("revision=r27r") == 1
    files["autorun/autorun.sh"] = entry.replace(sha(old_launcher), sha(files[launcher_path])).replace(
        "revision=r27r", f"revision={REVISION}"
    ).encode()
    manifest = json.loads(files["autorun/autorun.json"])
    manifest["value0"]["scripts"][0]["message"] = f"Am MOD {REVISION} - Experimental Mixer and Stems"
    files["autorun/autorun.json"] = (json.dumps(manifest, indent=2) + "\n").encode()
    files["autorun/autorun.check"] = sha(
        files["autorun/autorun.json"] + files["autorun/autorun.sh"] + SALT
    ).encode()
    for name in ("autorun.sh", "mod-launcher.sh"):
        subprocess.run(["sh", "-n", "/dev/stdin"], input=files[f"autorun/{name}"], check=True)
    for suffix in ("", ".ru"):
        files[f"README{suffix}.md"] = (REPO / "docs" / f"RELEASE_NOTES-{REVISION}{suffix}.md").read_bytes()

    out = ROOT / "build" / REVISION / f"am-mod-{REVISION}.zip"
    out.parent.mkdir(parents=True, exist_ok=True)
    sys.path.insert(0, str(REPO / "installer"))
    from mpc_mod_installer import Package
    with zipfile.ZipFile(out, "x", zipfile.ZIP_DEFLATED) as package:
        for name, data in sorted(files.items()):
            package.writestr(name, data)
    assert Package.from_file(out).revision == REVISION
    digest = sha(out.read_bytes())
    (out.parent / "SHA256SUMS.txt").write_text(f"{digest}  {out.name}\n")
    print(out, digest)


if __name__ == "__main__":
    main()
