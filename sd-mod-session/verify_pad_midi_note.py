"""Isolated AArch64/QEMU prototype; no application launch, SD or device writes."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--stock', default=os.environ.get('MPC_STOCK_APP'))
parser.add_argument('--sysroot', default=os.environ.get('MPC_QEMU_SYSROOT', '/home/user/mpc-rootfs-user-20260808'))
args = parser.parse_args()
if not args.stock:
    parser.error('pass --stock or set MPC_STOCK_APP to the unmodified 1.3.0.12 ELF')
root = Path(__file__).resolve().parent
stock = Path(args.stock).resolve()
expected = 'b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
if hashlib.sha256(stock.read_bytes()).hexdigest() != expected:
    raise SystemExit('Refusing a different native firmware image')
out = root / 'build/pad-midi-note'
out.mkdir(parents=True, exist_ok=True)
proof = out / 'verification.json'
proof.unlink(missing_ok=True)
sources = ['check_pad_midi_note_native.c', 'check_poly_record.c', 'check_fx_native.c',
           'src/pad_midi_note.c', 'src/pad_midi_note.h', 'src/midi_event.c',
           'src/midi_event.h', 'src/step_native.h', 'src/poly_record.c',
           'src/poly_record.h', 'src/synth.c', 'src/synth.h', 'verify_pad_midi_note.py']
digest = lambda: {p: hashlib.sha256((root/p).read_bytes()).hexdigest() for p in sources}
before = digest()
checks = []
for mode, extra in [('normal', []), ('ubsan', ['-fsanitize=undefined', '-static-libubsan', '-fno-sanitize-recover=all'])]:
    binary = out / ('native-' + mode)
    subprocess.run(['aarch64-linux-gnu-gcc', '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                    '-mbranch-protection=standard', *extra, 'check_pad_midi_note_native.c',
                    'src/pad_midi_note.c', 'src/midi_event.c', 'src/synth.c',
                    '-ldl', '-lm', '-pthread', '-o', str(binary)], cwd=root, check=True)
    result = subprocess.run(['qemu-aarch64', '-L', args.sysroot, str(binary), str(stock)],
                            cwd=root, capture_output=True, text=True, timeout=60)
    (out / (mode + '.log')).write_text(result.stdout + result.stderr)
    print(result.stdout + result.stderr, flush=True)
    result.check_returncode()
    checks.append(mode)
assert before == digest()
proof.write_text(json.dumps(dict(passed=True, checks=checks, sources_sha256=before,
    stock_sha256=expected, hardware_tested=False, live_engine_input_tested=False,
    project_save_tested=False, native_factory_tested=True,
    native_recorder_finalization_tested=True, synth_pcm_tested=True,
    runtime_hooks_added=False), indent=2) + '\n')
