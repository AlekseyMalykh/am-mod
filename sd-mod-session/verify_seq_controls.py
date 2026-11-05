"""Targeted ARM/QEMU regressions for Seq Colour and Copy bank navigation.
Usage: python3 verify_seq_controls.py "/path/to/stock/MPC Sample" [--rootfs PATH]
Fixtures do not establish hardware gesture/audio coverage.
"""
from pathlib import Path
import argparse
import hashlib
import json
import struct
import subprocess
import phone_build

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('stock', type=Path)
parser.add_argument('--rootfs', type=Path, default=Path('/home/user/mpc-rootfs-user-20260808'))
args = parser.parse_args()
r = Path(__file__).resolve().parent
out = r / 'build/seq-controls'
out.mkdir(parents=True, exist_ok=True)
stock = args.stock.resolve()
assert hashlib.sha256(stock.read_bytes()).hexdigest() == 'b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'

def run(command):
    subprocess.run(command, cwd=r, check=True)

class HeadersReady(Exception):
    pass

def bridges(*command):
    if command[0] == 'aarch64-linux-gnu-g++':
        raise HeadersReady()
    run(list(command))

def branch(a, b, link=False):
    return struct.pack('<I', (0x94000000 if link else 0x14000000) | (((b-a)//4) & 0x3ffffff))

# Generate matching bridge/tab headers without building the whole library.
try:
    phone_build.build(stock.read_bytes(), bridges, branch)
except HeadersReady:
    pass
run(['python3', 'build_pad_lighting_menu.py'])
run(['aarch64-linux-gnu-gcc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-Ibuild', '-fsyntax-only', 'src/phone_native.c'])
results = []
for variant, flags in [('normal', []), ('ubsan', ['-fsanitize=undefined', '-static-libubsan', '-fno-sanitize-recover=all'])]:
    common = ['aarch64-linux-gnu-gcc', '-O1', '-g', '-std=c11', '-Wall', '-Wextra', '-Werror', '-I.', '-Ibuild', '-Isrc', '-mbranch-protection=standard', *flags]
    cases = [
        ('colour', ['-DMPC_PAD_EQ=1', '-DSEQ_COLOUR_ONLY=1', 'check_pad_eq_ui.c', 'check_header_toggle.S', 'src/step_abi.S', 'src/music_ui.c', 'src/pad_eq.c', 'src/pad_eq_native.c', 'src/music_settings.c', 'src/fx_eq.c']),
        ('startup-repaint', ['-DMPC_EXPERIMENTAL_CHOP_EXPORT=1', 'check_startup_repaint.c', 'src/workflow_abi.S', 'src/phone_input.c', 'src/browser_tree.c']),
        ('copy-bank', ['-DMPC_EXPERIMENTAL_CHOP_EXPORT=1', 'check_copy_sequence_bank.c', 'src/workflow_abi.S', 'src/phone_input.c', 'src/browser_tree.c']),
        ('takeover-handoff', ['check_takeover_handoff.c', 'src/takeover_native.c', 'src/phone_events.c']),
        ('lighting-transition', ['check_lighting_transition.c']),
        ('recall-route', ['-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections', 'check_recall_route.c']),
        ('knob-fx-routing', ['-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections', 'check_knob_fx_visibility.c', 'src/phone_events.c', 'src/phone_input.c']),
        ('queue', ['check_knob_queue.c', 'src/phone_events.c']),
    ]
    for case, sources in cases:
        target = out / (variant + '-' + case)
        run([*common, *sources, '-lstdc++', '-pthread', '-ldl', '-lm', '-o', str(target)])
        cmd = ['qemu-aarch64', '-L', str(args.rootfs), str(target)]
        if case not in ['queue','lighting-transition','recall-route','knob-fx-routing']:
            cmd.append(str(stock))
        if case == 'colour':
            cmd.append(str(out / (variant + '.draw')))
        result = subprocess.run(cmd, cwd=r, capture_output=True, text=True, timeout=60)
        (out / (variant + '-' + case + '.log')).write_text(result.stdout + result.stderr)
        result.check_returncode()
        assert 'PASS ' in result.stdout
        print(variant, case, 'PASS', flush=True)
        results.append({'variant': variant, 'case': case, 'passed': True})
(out / 'verification.json').write_text(json.dumps({'results': results, 'hardware_tested': False, 'card_written': False}, indent=2) + '\n')
