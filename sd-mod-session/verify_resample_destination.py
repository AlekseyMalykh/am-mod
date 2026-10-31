"""Destination-only resample prototype; no package or SD writes."""
from pathlib import Path
from hashlib import sha256
import json
import subprocess

root = Path(__file__).resolve().parent
stock = root.parent / 'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
assert sha256(stock.read_bytes()).hexdigest() == 'b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
out = root / 'build/resample-destination'
out.mkdir(exist_ok=True)
sources = ['src/resample_destination.c', 'src/resample_destination.h', 'src/resample_view.h', 'check_resample_destination.c', 'check_fx_native.c', 'verify_resample_destination.py']
before = {s: sha256((root / s).read_bytes()).hexdigest() for s in sources}
for name, flags in [('normal', []), ('ubsan', ['-fsanitize=undefined', '-static-libubsan', '-fno-sanitize-recover=all'])]:
    target = out / name
    subprocess.run(['aarch64-linux-gnu-gcc', '-O1', '-g', '-Wall', '-Wextra', '-Werror', '-mbranch-protection=standard', *flags,
                    'check_resample_destination.c', 'src/resample_destination.c', '-ldl', '-lm', '-o', str(target)], cwd=root, check=True)
    result = subprocess.run(['qemu-aarch64', '-L', '/home/user/mpc-rootfs-user-20260808', str(target), str(stock), str(out)], cwd=root, capture_output=True, text=True, timeout=20)
    (out / (name + '.log')).write_text(result.stdout + result.stderr)
    result.check_returncode()
assert before == {s: sha256((root / s).read_bytes()).hexdigest() for s in sources}
(out / 'verification.json').write_text(json.dumps({'passed': True, 'ubsan': True, 'card_written': False, 'hardware_tested': False,
    'host_connected': False, 'audio_connected': False, 'scope': 'destination chooser and actual native queue target ABI only',
    'sources_sha256': before}, indent=2) + '\n')
print(result.stdout)
