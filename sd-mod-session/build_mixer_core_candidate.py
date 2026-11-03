"""Link dormant Mixer cores into an isolated full-preload candidate.

No Mixer mode, native routing/archive hooks, package, release or physical card
is enabled by this builder. Baseline build/runtime inputs use the existing
compressor candidate workflow; Mixer outputs go only to build/mixer-candidate.
"""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import sys

R = Path(__file__).resolve().parent
git = ["git"]
git_file = R.parent / ".git"
if os.name != "nt" and git_file.is_file():
    # A worktree created by Windows Git records a Windows admin-dir path.
    # Resolve it for this WSL process without rewriting shared Git metadata.
    git_dir = git_file.read_text().strip().removeprefix("gitdir: ")
    if re.match(r"^[A-Za-z]:[\\/]", git_dir):
        git_dir = subprocess.check_output(["wslpath", "-u", git_dir], text=True).strip()
        git += ["--git-dir=" + git_dir, "--work-tree=" + str(R.parent)]
branch = subprocess.check_output([*git, "branch", "--show-current"], cwd=R, text=True).strip()
if branch != "feature/mixer-song-stems":
    raise RuntimeError("Build Mixer work only in feature/mixer-song-stems")
runtime = Path(os.environ.get("MPC_RUNTIME_ROOT", str(R.parent))).resolve()
out = R / "build/mixer-candidate"
out.mkdir(parents=True, exist_ok=True)
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
sources = ["src/mixer_model.c", "src/mixer_meters.c", "src/fx_chain.c"]
tracked = sources + ["src/mixer_model.h", "src/mixer_meters.h", "build_mixer_core_candidate.py"]
before = {p: sha(R / p) for p in tracked}
print("Building the existing full-preload baseline (log in build/mixer-candidate/baseline.log)", flush=True)
with (out / "baseline.log").open("w") as log:
    subprocess.run([sys.executable, str(R / "build_compressor_candidate.py")], cwd=R,
                   stdout=log, stderr=subprocess.STDOUT, check=True)
baseline = json.loads((R / "compressor_base.json").read_text())
base_out = R / "build/compressor" / baseline["revision"]
proof = json.loads((base_out / "compile-verification.json").read_text())
objects = [base_out / (Path(p).stem + ".o") for p in proof["linked_sources"]]
for source in sources:
    if source in proof["linked_sources"]:
        continue
    obj = out / (Path(source).stem + ".o")
    subprocess.run(["aarch64-linux-gnu-gcc", "-std=c11", "-O2", "-g", "-fPIC",
                    "-Wall", "-Wextra", "-Werror", "-mbranch-protection=standard",
                    "-c", source, "-o", str(obj)], cwd=R, check=True)
    objects.append(obj)
library = out / "phone-input.so"
subprocess.run(["aarch64-linux-gnu-g++", "-shared", "-Wl,-z,noexecstack,-z,relro,-z,now",
                "-Wl,--no-undefined", *map(str, objects), "-ldl", "-pthread", "-lm",
                "-o", str(library)], cwd=R, check=True)
sysroot = os.environ.get("MPC_QEMU_SYSROOT", "/home/user/mpc-rootfs-user-20260808")
bootstrap = runtime / "sd-mod-session/build/r22/bootstrap"
stock = runtime / "extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample"
patches = runtime / "sd-mod-session/build/r22/bootstrap-patches.bin"
result = subprocess.run(["qemu-aarch64", "-L", sysroot, str(bootstrap), str(stock),
                         str(library), str(patches)], cwd=R, capture_output=True,
                        text=True, timeout=45)
(out / "bootstrap.log").write_text(result.stdout + result.stderr)
result.check_returncode()
assert "PASS real full preload constructor" in result.stdout
assert before == {p: sha(R / p) for p in tracked}
for source, digest in proof["sources_sha256"].items():
    assert sha(R / source) == digest, source
report = dict(passed=True, branch=branch, full_preload_constructor=True,
              library_sha256=sha(library), sources_sha256={**proof["sources_sha256"], **before},
              linked_sources=list(dict.fromkeys(proof["linked_sources"] + sources)),
              mixer_mode_enabled=False, native_taps_connected=False,
              native_archive_integrated=False, hardware_tested=False,
              card_written=False, release_packaged=False)
(out / "compile-verification.json").write_text(json.dumps(report, indent=2) + "\n")
print("PASS full library link and real pinned preload constructor with dormant Mixer model/meter cores", flush=True)
