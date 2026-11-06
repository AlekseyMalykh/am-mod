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
core = ["src/mixer_route_native.c", "src/mixer_buses.c", "src/mixer_model.c",
        "src/mixer_meters.c", "src/fx_chain.c", "src/fx_eq.c"]
suites = [
    ("routing", ["check_mixer_route_native.c", *core], "PASS scoped Mixer native adapter"),
    ("native-dsp", ["check_mixer_route_dsp.c", *core, "src/pad_eq.c", "src/pad_eq_native.c", "src/music_settings.c"],
     "PASS Mixer native allocated-buffer and Pad EQ integration"),
    ("installed-live", ["check_mixer_live_native.c", *core, "src/pad_eq.c", "src/pad_eq_native.c", "src/music_settings.c", "src/mixer_live.c", "src/mixer_live_native.c"],
     "PASS installed pinned live hooks"),
]
tracked = list(dict.fromkeys(p for _, sources, _ in suites for p in sources)) + [
    "check_fx_native.c", "src/mixer_route_native.h", "src/mixer_buses.h", "src/mixer_model.h",
    "src/mixer_meters.h", "src/pad_eq.h", "src/pad_eq_native.h", "src/mixer_live.h", "src/fx_chain.h", "src/fx_eq.h", Path(__file__).name]
before = {p: digest(R / p) for p in tracked}
checks = []
for suite, sources, marker in suites:
    for mode, flags in [("normal", []), ("ubsan", ["-fsanitize=undefined", "-static-libubsan", "-fno-sanitize-recover=all"])]:
        name = suite + "-" + mode
        target = out / name
        subprocess.run(["aarch64-linux-gnu-gcc", "-std=c11", "-O1", "-g", "-Wall", "-Wextra",
                        "-Werror", "-mbranch-protection=standard", *flags, *sources,
                        "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free",
                        "-ldl", "-lm", "-o", str(target)], cwd=R, check=True)
        result = subprocess.run(["qemu-aarch64", "-L", sysroot, str(target), str(stock)],
                                cwd=R, capture_output=True, text=True, timeout=30,
                                preexec_fn=lambda: resource.setrlimit(resource.RLIMIT_CORE, (0, 0)))
        (out / (name + ".log")).write_text(result.stdout + result.stderr)
        print(name + ":\n" + result.stdout + result.stderr, end="", flush=True)
        result.check_returncode()
        assert marker in result.stdout
        checks.append(dict(name=name, passed=True))
assert digest(stock) == expected
assert before == {p: digest(R / p) for p in tracked}
(out / "verification.json").write_text(json.dumps(dict(
    passed=True, checks=checks, firmware_sha256=expected, sources_sha256=before,
    hardware_tested=False, live_routing_installed=False, card_written=False,
    native_track_taps_tested=True, final_master_physical_pair_tested=True, voice_oscillators_fixture=True,
    scope="Real installed entry hooks, engine2226fb0/downstream18c79f0, Program output sum, primary physical Master; DrumProgram entry/chunking/voice walk, native buffer allocation/reset, Instrument/Program processing, AM Pad EQ, optimized merge. Native object/service/voice oscillator/downstream insert fixtures; all code patches private to QEMU ELF mapping."
), indent=2) + "\n")
