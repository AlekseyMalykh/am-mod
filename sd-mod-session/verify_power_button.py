"""ARM checks of the native shutdown hook and the host handoff.

This does not press a hardware button, write a physical SD or power off the OS.
The native entry instructions execute under QEMU user mode; save completion
and the final OS command have explicit receivers. Full-VM proof is separate.
"""
from pathlib import Path
import hashlib
import json
import subprocess

R = Path(__file__).resolve().parent
O = R / 'build/power-button'
O.mkdir(exist_ok=True)
sources = ['src/power_control.cc', 'src/power_control.h', 'check_power_button.cc',
           'check_power_button.S', 'verify_power_button.py', 'verify_power_host.py',
           'src/autorun.template.sh']
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
before = {n: digest(R / n) for n in sources}
qemu = ['qemu-aarch64', '-L', '/home/user/mpc-rootfs-user-20260808']
for name, extra in [('normal', []), ('ubsan', ['-fsanitize=undefined', '-static-libubsan', '-fno-sanitize-recover=all'])]:
    exe = O / name
    subprocess.run(['aarch64-linux-gnu-g++', '-std=c++11', '-O1', '-g',
                    '-Wall', '-Wextra', '-Werror', '-mbranch-protection=standard',
                    *extra, 'check_power_button.cc', 'check_power_button.S',
                    '-o', str(exe)], cwd=R, check=True)
    p = subprocess.run([*qemu, str(exe)], cwd=R, capture_output=True, text=True, timeout=30)
    (O / (name + '.log')).write_text(p.stdout + p.stderr)
    p.check_returncode()
    assert 'PASS relocated native shutdown-question' in p.stdout
    assert 'QUESTION_WITHOUT_UI_CONTEXT shutdown_cancelled' in p.stderr
    print(p.stdout, end='')
p = subprocess.run(['python3', 'verify_power_host.py'], cwd=R, capture_output=True, text=True, timeout=30)
(O / 'host.log').write_text(p.stdout + p.stderr)
p.check_returncode()
print(p.stdout, end='')
assert before == {n: digest(R / n) for n in sources}
proof = dict(passed=True, scope='Native entry/PAC and state machine; save and OS receivers mocked',
             no_without_ui_context=True, failed_save_keeps_device_on=True,
             unfinished_copy_or_resample_blocks_shutdown=True,
             sd_flush_before_host_poweroff=True, full_vm_verified=False,
             physical_device_tested=False, physical_card_written=False,
             sources_sha256=before)
(O / 'verification.json').write_text(json.dumps(proof, indent=2) + '\n')
print(json.dumps({k: v for k, v in proof.items() if k != 'sources_sha256'}, indent=2))
