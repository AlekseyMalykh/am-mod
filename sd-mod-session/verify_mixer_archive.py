"""Archive owner/callback tests; host and ARM, no project files or SD written."""
from pathlib import Path
import hashlib
import json
import os
import subprocess

R = Path(__file__).resolve().parent
out = R / "build/mixer-archive"
out.mkdir(parents=True, exist_ok=True)
sources = ["src/mixer_model.c", "src/fx_eq.c", "src/fx_chain.c",
           "src/mixer_archive.cc", "check_mixer_archive.cc"]
tracked = sources + ["src/mixer_archive.h", "src/mixer_model.h", "verify_mixer_archive.py",
    "check_mixer_archive_native.c", "check_mixer_archive_native.cc", "check_fx_native.c",
    "src/music_archive.c", "src/music_settings.c", "src/music_settings.h"]
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
before = {p: sha(R / p) for p in tracked}
sysroot = os.environ.get("MPC_QEMU_SYSROOT", "/home/user/mpc-rootfs-user-20260808")
checks = []
for name, prefix, flags, runner in [
    ("host", "", [], []),
    ("host-asan", "", ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    ("arm", "aarch64-linux-gnu-", ["-mbranch-protection=standard"], ["qemu-aarch64", "-L", sysroot]),
    ("arm-ubsan", "aarch64-linux-gnu-", ["-mbranch-protection=standard", "-fsanitize=undefined", "-static-libubsan", "-fno-sanitize-recover=all"], ["qemu-aarch64", "-L", sysroot]),
]:
    objects = []
    for source in sources:
        cpp = source.endswith(".cc")
        obj = out / (name + "-" + Path(source).stem + ".o")
        objects.append(str(obj))
        subprocess.run([prefix + ("g++" if cpp else "gcc"), "-std=c++11" if cpp else "-std=c11",
                        "-O1", "-g", "-Wall", "-Wextra", "-Werror", *flags,
                        "-c", source, "-o", str(obj)], cwd=R, check=True)
    target = out / name
    subprocess.run([prefix + "g++", *flags, *objects, "-pthread", "-lm", "-o", str(target)], cwd=R, check=True)
    result = subprocess.run([*runner, str(target)], cwd=R, capture_output=True, text=True, timeout=45)
    (out / (name + ".log")).write_text(result.stdout + result.stderr)
    print(result.stdout + result.stderr, end="", flush=True)
    result.check_returncode()
    assert "PASS Mixer archive owner" in result.stdout
    checks.append(dict(name=name, passed=True))
runtime = Path(os.environ.get("MPC_RUNTIME_ROOT", str(R.parent))).resolve()
stock = runtime / "extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample"
expected = "b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508"
assert sha(stock) == expected
native_sources = [p for p in sources if p != "check_mixer_archive.cc"] + [
    "src/music_archive.c", "src/music_settings.c", "check_mixer_archive_native.c", "check_mixer_archive_native.cc"]
for name, extra in [("native", []), ("native-ubsan", ["-fsanitize=undefined", "-static-libubsan", "-fno-sanitize-recover=all"])]:
    objects = []
    for source in native_sources:
        cpp = source.endswith(".cc");obj = out / (name + "-" + Path(source).name + ".o");objects.append(str(obj))
        subprocess.run(["aarch64-linux-gnu-g++" if cpp else "aarch64-linux-gnu-gcc", "-std=c++11" if cpp else "-std=c11",
            "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-mbranch-protection=standard", *extra,
            "-c", source, "-o", str(obj)], cwd=R, check=True)
    target = out / name
    subprocess.run(["aarch64-linux-gnu-g++", *extra, *objects, "-pthread", "-lm", "-ldl", "-o", str(target)], cwd=R, check=True)
    result = subprocess.run(["qemu-aarch64", "-L", sysroot, str(target), str(stock)], cwd=R,
        capture_output=True, text=True, timeout=45)
    (out / (name + ".log")).write_text(result.stdout + result.stderr)
    print(result.stdout + result.stderr, end="", flush=True);result.check_returncode()
    assert "PASS native JSON named-string writer/escaping" in result.stdout
    checks.append(dict(name=name, passed=True))
assert before == {p: sha(R / p) for p in tracked}
(out / "verification.json").write_text(json.dumps(dict(passed=True, checks=checks,
    sources_sha256=before, hardware_tested=False, card_written=False,
    native_string_writer_reader_tested=True, firmware_sha256=expected,
    scope="Real model/archive owner/callback transactions, native string writer/escaping/reader and existing music metadata. Object framing/services fixtures; no full XPJ disk roundtrip or physical Save/Load."
), indent=2) + "\n")
