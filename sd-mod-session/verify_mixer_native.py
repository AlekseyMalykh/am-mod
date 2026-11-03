"""Read-only pinned native audio audit; isolated ARM/QEMU, no packaging or SD.

Set MPC_RUNTIME_ROOT to the local runtime/firmware workspace when testing from
an isolated worktree. Requires aarch64-linux-gnu-gcc and qemu-aarch64 in Linux.
"""
from pathlib import Path
import hashlib
import json
import os
import resource
import subprocess

R = Path(__file__).resolve().parent
runtime = Path(os.environ.get("MPC_RUNTIME_ROOT", str(R.parent))).resolve()
stock = runtime / "extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample"
sysroot = os.environ.get("MPC_QEMU_SYSROOT", "/home/user/mpc-rootfs-user-20260808")
out = R / "build/mixer-native"
out.mkdir(parents=True, exist_ok=True)
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
expected = "b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508"
if digest(stock) != expected:
    raise RuntimeError("Native audit requires the exact pinned 1.3.0.12 ELF")
tracked = [R / "check_mixer_native.c", R / "check_fx_native.c", Path(__file__).resolve()]
before = {str(p.relative_to(R)): digest(p) for p in tracked}
checks = []
for name, flags in [("normal", []), ("ubsan", ["-fsanitize=undefined", "-static-libubsan", "-fno-sanitize-recover=all"])]:
    target = out / name
    subprocess.run(["aarch64-linux-gnu-gcc", "-std=c11", "-O1", "-g", "-Wall",
                    "-Wextra", "-Werror", "-mbranch-protection=standard", *flags,
                    "check_mixer_native.c", "-ldl", "-lm", "-o", str(target)], cwd=R, check=True)
    result = subprocess.run(["qemu-aarch64", "-L", sysroot, str(target), str(stock)],
                            cwd=R, capture_output=True, text=True, timeout=30,
                            preexec_fn=lambda: resource.setrlimit(resource.RLIMIT_CORE, (0, 0)))
    (out / (name + ".log")).write_text(result.stdout + result.stderr)
    print(result.stdout + result.stderr, end="", flush=True)
    result.check_returncode()
    assert "PASS Mixer Phase0 native audit" in result.stdout
    checks.append(dict(name=name, passed=True))
assert digest(stock) == expected
assert before == {str(p.relative_to(R)): digest(p) for p in tracked}
(out / "verification.json").write_text(json.dumps(dict(
    passed=True, checks=checks, firmware_sha256=expected, sources_sha256=before,
    hardware_tested=False, live_routing_installed=False, card_written=False,
    scope="Real native resolver/summing/export wrappers. Synthetic PCM/router/service fixtures; no Song traversal or voices executed."
), indent=2) + "\n")
