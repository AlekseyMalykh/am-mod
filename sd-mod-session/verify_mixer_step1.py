"""Mixer block updates and real C painter on host/sanitizers/AArch64.
Native installed-hook checks: verify_mixer_route_native.py; full preload:
build_mixer_core_candidate.py. No device or card access.
"""
from pathlib import Path
import hashlib
import json
import os
import subprocess
R = Path(__file__).resolve().parent
out = R / "build/mixer-step1"
out.mkdir(parents=True, exist_ok=True)
core = ["src/mixer_buses.c", "src/mixer_model.c", "src/mixer_meters.c", "src/fx_chain.c", "src/fx_eq.c"]
tracked = core + ["check_mixer_updates.c", "check_mixer_view.c", "src/mixer_view.c", "src/mixer_view.h", "src/mixer_buses.h", "src/fx_chain.h", "verify_mixer_step1.py"]
sha = lambda p: hashlib.sha256((R / p).read_bytes()).hexdigest()
before = {p: sha(p) for p in tracked}
sysroot = os.environ.get("MPC_QEMU_SYSROOT", "/home/user/mpc-rootfs-user-20260808")
checks = []
for name, compiler, flags, runner in [
    ("host", "gcc", [], []),
    ("host-asan", "gcc", ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    ("arm", "aarch64-linux-gnu-gcc", ["-mbranch-protection=standard"], ["qemu-aarch64", "-L", sysroot]),
    ("arm-ubsan", "aarch64-linux-gnu-gcc", ["-mbranch-protection=standard", "-fsanitize=undefined", "-static-libubsan", "-fno-sanitize-recover=all"], ["qemu-aarch64", "-L", sysroot]),
]:
    for suite in ("updates", "view"):
        target = out / (suite + "-" + name)
        sources = ["check_mixer_" + suite + ".c", *core]
        if suite == "view":
            sources += ["src/mixer_view.c"]
        command = [compiler, "-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror", *flags, *sources]
        if suite == "updates":
            command += ["-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free"]
        subprocess.run([*command, "-lm", "-pthread", "-o", str(target)], cwd=R, check=True)
        args = [str(out)] if suite == "view" else []
        result = subprocess.run([*runner, str(target), *args], cwd=R, text=True, capture_output=True, timeout=45)
        (out / (target.name + ".log")).write_text(result.stdout + result.stderr)
        print(target.name + ": " + result.stdout + result.stderr, end="", flush=True)
        result.check_returncode()
        checks.append(dict(name=target.name, passed=True))
assert before == {p: sha(p) for p in tracked}
(out / "verification.json").write_text(json.dumps(dict(passed=True, checks=checks, sources_sha256=before,
    hardware_tested=False, native_controls_connected=False), indent=2) + "\n")
