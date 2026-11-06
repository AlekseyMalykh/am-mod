"""Build the independent Mixer model on host and ARM; no card/firmware needed."""
from pathlib import Path
import hashlib
import json
import os
import struct
import subprocess
import zlib

R = Path(__file__).resolve().parent
out = R / "build/mixer-model"
out.mkdir(parents=True, exist_ok=True)
tracked = ["src/mixer_model.c", "src/mixer_model.h", "check_mixer_model.c",
           "src/fx_chain.c", "src/fx_chain.h", "src/fx_eq.c", "src/fx_eq.h", "verify_mixer_model.py"]
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
before = {p: digest(R / p) for p in tracked}
sources = ["check_mixer_model.c", "src/mixer_model.c", "src/fx_chain.c", "src/fx_eq.c"]
sysroot = os.environ.get("MPC_QEMU_SYSROOT", "/home/user/mpc-rootfs-user-20260808")
checks = []
fixtures = []
for name, compiler, flags, runner in [
    ("host", "gcc", [], []),
    ("host-asan", "gcc", ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
    ("arm", "aarch64-linux-gnu-gcc", ["-mbranch-protection=standard"], ["qemu-aarch64", "-L", sysroot]),
    ("arm-ubsan", "aarch64-linux-gnu-gcc", ["-mbranch-protection=standard", "-fsanitize=undefined", "-static-libubsan", "-fno-sanitize-recover=all"], ["qemu-aarch64", "-L", sysroot]),
]:
    target = out / name
    subprocess.run([compiler, "-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
                    *flags, *sources, "-lm", "-o", str(target)], cwd=R, check=True)
    result = subprocess.run([*runner, str(target)], cwd=R, capture_output=True, text=True, timeout=45)
    (out / (name + ".log")).write_text(result.stdout + result.stderr)
    print(result.stdout + result.stderr, end="", flush=True)
    result.check_returncode()
    assert "PASS Mixer data model" in result.stdout
    payload = subprocess.check_output([*runner, str(target), "--fixture"], cwd=R, text=True, timeout=15).strip()
    fixtures.append(payload)
    checks.append(dict(name=name, passed=True))
assert len(set(fixtures)) == 1, "Host and ARM serialized different project data"
text = fixtures[0]
assert text.startswith("AMMX:")
raw = bytes.fromhex(text[5:])
version, flags, length = struct.unpack_from("<HHI", raw)
assert version == 1 and flags == 0 and length == len(raw) - 12
assert zlib.crc32(raw[:-4]) == struct.unpack_from("<I", raw, len(raw) - 4)[0]
# Independent decoding of the first fixture channel and last native parameter.
name_bytes = raw[8]
assert raw[9:9+name_bytes].decode("utf-8") == "Бас"
offset = 9 + name_bytes
colour, level, pan, send, mute, solo, bypass, fx_count = struct.unpack_from("<IfffBBBB", raw, offset)
assert colour == 0x00c700 and level == 0 and pan == -1 and send == 0
assert mute == solo == bypass == 0 and fx_count == 2
offset += struct.calcsize("<IfffBBBB")
kind, fx_bypass, native_id = struct.unpack_from("<BBI", raw, offset)
assert kind == 2 and fx_bypass == 1 and native_id == 0x12345678
parameters = struct.unpack_from("<32f", raw, offset + 6)
assert parameters[0] == 0 and parameters[-1] == 1
assert before == {p: digest(R / p) for p in tracked}
report = dict(passed=True, checks=checks, host_arm_payload_identical=True,
              independent_python_header_crc_utf8_and_parameter_check=True,
              payload_sha256=hashlib.sha256(text.encode("ascii")).hexdigest(),
              sources_sha256=before, hardware_tested=False, native_archive_integrated=False,
              live_routing_installed=False, card_written=False)
(out / "verification.json").write_text(json.dumps(report, indent=2) + "\n")
print("PASS independent Python CRC/schema check and identical host/ARM payload")
