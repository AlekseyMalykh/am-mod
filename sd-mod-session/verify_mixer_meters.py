"""PCM metering and audio/UI ownership tests; never opens an audio device."""
from pathlib import Path
import hashlib
import json
import os
import subprocess

R = Path(__file__).resolve().parent
out = R / "build/mixer-meters"
out.mkdir(parents=True, exist_ok=True)
sources = ["check_mixer_meters.c", "src/mixer_meters.c"]
tracked = sources + ["src/mixer_meters.h", "verify_mixer_meters.py"]
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
before = {p: sha(R / p) for p in tracked}
sysroot = os.environ.get("MPC_QEMU_SYSROOT", "/home/user/mpc-rootfs-user-20260808")
checks = []
for name, compiler, flags, runner in [
    ("host", "gcc", [], []),
    ("host-asan", "gcc", ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    ("arm", "aarch64-linux-gnu-gcc", ["-mbranch-protection=standard"], ["qemu-aarch64", "-L", sysroot]),
    ("arm-ubsan", "aarch64-linux-gnu-gcc", ["-mbranch-protection=standard", "-fsanitize=undefined", "-static-libubsan", "-fno-sanitize-recover=all"], ["qemu-aarch64", "-L", sysroot]),
]:
    target = out / name
    subprocess.run([compiler, "-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
                    *flags, *sources, "-lm", "-pthread", "-o", str(target)], cwd=R, check=True)
    result = subprocess.run([*runner, str(target)], cwd=R, capture_output=True, text=True, timeout=45)
    (out / (name + ".log")).write_text(result.stdout + result.stderr)
    print(result.stdout + result.stderr, end="", flush=True)
    result.check_returncode()
    assert "PASS concurrent audio/UI telemetry" in result.stdout
    checks.append(dict(name=name, passed=True))
assert before == {p: sha(R / p) for p in tracked}
(out / "verification.json").write_text(json.dumps(dict(
    passed=True, checks=checks, sources_sha256=before, native_taps_connected=False,
    hardware_tested=False, card_written=False,
    scope="Actual supplied PCM, numerical ballistics and concurrent SPSC handoff; native bus selection and UI rendering not connected."
), indent=2) + "\n")
