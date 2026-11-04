"""Execute scoped separate-buffer adapter against pinned ELF in isolated QEMU.

No device, persistent executable patch, SD package or live installer is used.
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
out = R / "build/mixer-route-native"
out.mkdir(parents=True, exist_ok=True)
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
expected = "b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508"
if digest(stock) != expected:
    raise RuntimeError("Scoped adapter test requires the exact pinned 1.3.0.12 ELF")
sources = ["check_mixer_route_native.c", "src/mixer_route_native.c", "src/mixer_buses.c",
           "src/mixer_model.c", "src/mixer_meters.c", "src/fx_chain.c", "src/fx_eq.c"]
tracked = sources + ["check_fx_native.c", "src/mixer_route_native.h", "src/mixer_buses.h",
                     "src/mixer_model.h", "src/mixer_meters.h", Path(__file__).name]
before = {p: digest(R / p) for p in tracked}
checks = []
for name, flags in [("normal", []), ("ubsan", ["-fsanitize=undefined", "-static-libubsan", "-fno-sanitize-recover=all"])]:
    target = out / name
    subprocess.run(["aarch64-linux-gnu-gcc", "-std=c11", "-O1", "-g", "-Wall", "-Wextra",
                    "-Werror", "-mbranch-protection=standard", *flags, *sources,
                    "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free",
                    "-ldl", "-lm", "-o", str(target)], cwd=R, check=True)
    result = subprocess.run(["qemu-aarch64", "-L", sysroot, str(target), str(stock)],
                            cwd=R, capture_output=True, text=True, timeout=30,
                            preexec_fn=lambda: resource.setrlimit(resource.RLIMIT_CORE, (0, 0)))
    (out / (name + ".log")).write_text(result.stdout + result.stderr)
    print(result.stdout + result.stderr, end="", flush=True)
    result.check_returncode()
    assert "PASS scoped Mixer native adapter" in result.stdout
    checks.append(dict(name=name, passed=True))
assert digest(stock) == expected
assert before == {p: digest(R / p) for p in tracked}
(out / "verification.json").write_text(json.dumps(dict(
    passed=True, checks=checks, firmware_sha256=expected, sources_sha256=before,
    hardware_tested=False, live_routing_installed=False, card_written=False,
    scope="Real separate-buffer DrumProgram loop, optimized two-buffer merge, resolver and summing. Fixture DSP/services and native objects; two call sites patched only in private ELF mapping."
), indent=2) + "\n")
