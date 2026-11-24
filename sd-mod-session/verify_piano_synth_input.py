"""Existing native Piano input vs opt-in Synth trial. No device/project writes."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--stock', default=os.environ.get('MPC_STOCK_APP'))
p.add_argument('--sysroot', default=os.environ.get('MPC_QEMU_SYSROOT', '/home/user/mpc-rootfs-user-20260808'))
a = p.parse_args()
if not a.stock:
    p.error('pass --stock or set MPC_STOCK_APP')
root = Path(__file__).resolve().parent
stock = Path(a.stock).resolve()
expected = 'b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
assert hashlib.sha256(stock.read_bytes()).hexdigest() == expected, 'Wrong stock ELF'
out = root / 'build/piano-synth-input'
out.mkdir(parents=True, exist_ok=True)
proof = out / 'verification.json'
proof.unlink(missing_ok=True)
files = ['src/piano_input.inc', 'src/batch_copy.h', 'src/batch_copy.cc',
         'src/pad_midi_note.c', 'src/pad_midi_note.h', 'src/midi_event.c',
         'src/synth.c', 'src/synth.h', 'check_piano_input.c',
         'check_piano_synth_input.c', 'check_piano_synth_coexist.inc', 'check_batch_dispatch.c', 'check_batch_native.c',
         'check_steps.c', 'src/batch_events.cc', 'src/piano_roll.c',
         'phone_build.py', 'verify_piano_synth_input.py']
digest = lambda: {f: hashlib.sha256((root/f).read_bytes()).hexdigest() for f in files}
before = digest()

# Prepare real pinned bridge headers used by check_steps.c. Stop the existing
# builder after its bridge/header stage, BEFORE building a preload library or
# patching the clone. All generated data remains in ignored build/.
spec = importlib.util.spec_from_file_location('trial_phone_build', root/'phone_build.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
class BridgesReady(Exception):
    pass
def bridge_command(*cmd):
    if 'src/step_exceptions.cc' in cmd:
        raise BridgesReady()
    subprocess.run(cmd, cwd=root, check=True)
try:
    module.build(stock.read_bytes(), bridge_command, None)
except BridgesReady:
    pass
assert (root/'build/phone_hooks.h').is_file()

checks = []
for mode, sanitizer in [('normal', []), ('ubsan', ['-fsanitize=undefined', '-static-libubsan', '-fno-sanitize-recover=all'])]:
    for variant in ['baseline', 'trial']:
        flags = ['-O1', '-g', '-Wall', '-Wextra', '-Werror', '-ffunction-sections',
                 '-fdata-sections', '-mbranch-protection=standard', *sanitizer]
        if variant == 'trial':
            flags.append('-DMPC_PAD_MIDI_RECORDING_TRIAL')
        objects = []
        for source in ['src/batch_copy.cc', 'src/batch_events.cc', 'check_steps_support.cc', 'src/step_exceptions.cc', 'src/step_mute.cc']:
            obj = out / (mode+'-'+variant+'-'+Path(source).stem+'.o')
            subprocess.run(['aarch64-linux-gnu-g++', '-std=c++11', *flags, '-c', source, '-o', str(obj)], cwd=root, check=True)
            objects.append(str(obj))
        suites = ['check_piano_input.c']
        if variant == 'trial':
            suites.append('check_piano_synth_input.c')
        for suite in suites:
            name = mode+'-'+variant+'-'+Path(suite).stem
            binary = out / name
            subprocess.run(['aarch64-linux-gnu-gcc', '-std=c11', *flags, '-Ibuild', suite,
                            'src/piano_roll.c', 'src/pad_midi_note.c', 'src/midi_event.c',
                            'src/synth.c', 'check_steps_support.S', 'src/step_abi.S', *objects,
                            '-Wl,--gc-sections', '-lstdc++', '-ldl', '-lpthread', '-lm', '-o', str(binary)], cwd=root, check=True)
            result = subprocess.run(['qemu-aarch64', '-L', a.sysroot, str(binary), str(stock)],
                                    cwd=root, text=True, capture_output=True, timeout=60)
            (out/(name+'.log')).write_text(result.stdout+result.stderr)
            if result.returncode:
                print(result.stdout+result.stderr, flush=True)
                result.check_returncode()
            if suite == 'check_piano_synth_input.c':
                assert 'PASS live/playback coexistence:' in result.stdout
            print('PASS', name, flush=True)
            checks.append(name)
assert before == digest()
proof.write_text(json.dumps(dict(passed=True, checks=checks, sources_sha256=before,
    stock_sha256=expected, native_worker_tested=True, real_synth_dsp_tested=True,
    native_engine_input_tested=False, recorder_receivers_are_fixtures=True,
    live_playback_dsp_coexistence=True, live_chord_voices=8,
    hardware_tested=False, production_trial_enabled=False), indent=2)+'\n')
