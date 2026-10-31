"""Linux/WSL checks of the experimental package; never connects to the MPC."""
from hashlib import sha256
import json
from pathlib import Path
import struct
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parent
WORKSPACE = ROOT.parent
BUSYBOX = Path('/home/user/mpc-rootfs-user-20260808/usr/bin/busybox.nosuid')
SYSROOT = BUSYBOX.parents[2]
QEMU = ['/usr/bin/qemu-aarch64', '-L', str(SYSROOT)]
info = json.loads((ROOT / 'build-info.json').read_text())
stock = WORKSPACE / 'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
original = stock.read_bytes()
assert sha256(original).hexdigest() == info['stock_sha256']
assert (ROOT / 'build/peek.bin').read_bytes() == (ROOT / 'autorun/peek-code.bin').read_bytes(), 'Stale package code'
assert (ROOT / 'build/apply-peek').read_bytes() == (ROOT / 'autorun/apply-peek').read_bytes(), 'Stale package helper'

with zipfile.ZipFile(WORKSPACE / info['zip']) as archive:
    assert archive.testzip() is None
    for name, expected in info['payload_sha256'].items():
        assert sha256(archive.read('autorun/' + name)).hexdigest() == expected
    salt = b'f7bed1c9-7e90-4731-9194-c996666b4331'
    assert sha256(archive.read('autorun/autorun.json') + archive.read('autorun/autorun.sh') + salt).hexdigest().encode() == archive.read('autorun/autorun.check')

symbols = subprocess.check_output(['aarch64-linux-gnu-nm', str(ROOT / 'build/peek.elf')], text=True)
for address, name in [('0000000006d3b000', 'peek_encoder'),
                      ('0000000006d3b124', 'peek_rotation_timer'),
                      ('0000000006d3b200', 'peek_value_colour')]:
    assert address + ' T ' + name in symbols
checks = []
directory = Path('/run/mpc-clone-trial-v1')
directory.mkdir(mode=0o700)  # Refuse to touch an existing runtime directory.
target = directory / 'MPC-Sample'
try:
    target.write_bytes(original)
    for patch in info['patches']:
        source = ROOT / 'autorun' / patch['file']
        assert sha256(source.read_bytes()).hexdigest() == patch['sha256']
    helper = ROOT / 'autorun/apply-peek'
    helper.chmod(0o500)
    completed = subprocess.run(QEMU + [str(helper)], check=True, text=True,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    assert completed.stdout.strip() == 'K_PEEK_RAM_PATCH_OK'
    modified = target.read_bytes()
    assert len(modified) == len(original)
    assert sha256(modified).hexdigest() == info['patched_copy_sha256']
    checks.append('Actual ARM patch helper produced the expected full patched SHA-256 on tmpfs')
    assert subprocess.run(QEMU + [str(helper)], stdout=subprocess.PIPE, stderr=subprocess.PIPE).returncode != 0
    assert target.read_bytes() == modified
    checks.append('Repeat patch refused without changes')
    # Undo exactly the declared ranges, then require complete byte equality.
    undone = bytearray(modified)
    for patch in info['patches']:
        offset, size = int(patch['offset'], 16), patch['size']
        undone[offset:offset + size] = original[offset:offset + size]
    assert undone == original
    checks.append('Only the five declared ranges differ; original size is unchanged')
    for site, destination, is_call in [(0x16541a4, 0x6d3b000, False),
                                       (0x3b7d360, 0x6d3b124, True),
                                       (0x15cdeb4, 0x6d3b200, True)]:
        word = struct.unpack_from('<I', modified, site)[0]
        assert word >> 26 == (0x25 if is_call else 0x5)
        immediate = word & 0x3ffffff
        if immediate & 0x2000000:
            immediate -= 0x4000000
        assert site + 4 * immediate == destination
    assert struct.unpack_from('<QQ', modified, 0xd0) == (0x6d3b280, 0x6d3b280)
    assert modified[0x6d43bb0:] == original[0x6d43bb0:]
    checks.append('Branches, RX segment expansion and untouched writable segment verified')
    subprocess.run(['readelf', '-l', str(target)], check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    target.write_bytes(b'wrong size fixture')
    assert subprocess.run(QEMU + [str(helper)], stdout=subprocess.PIPE, stderr=subprocess.PIPE).returncode != 0
    assert target.read_bytes() == b'wrong size fixture'
    assert subprocess.run(QEMU + [str(helper), '/usr/bin/MPC Sample'], stdout=subprocess.PIPE, stderr=subprocess.PIPE).returncode != 0
    assert target.read_bytes() == b'wrong size fixture'
    checks.append('Wrong size and path argument refused without changes')
    target.unlink()
    sentinel = directory / 'sentinel'
    sentinel.write_bytes(b'untouched')
    target.symlink_to(sentinel)
    assert subprocess.run(QEMU + [str(helper)], stdout=subprocess.PIPE, stderr=subprocess.PIPE).returncode != 0
    assert sentinel.read_bytes() == b'untouched'
    target.unlink()
    sentinel.unlink()
    checks.append('Symlink refused; sentinel remained untouched')
finally:
    if target.exists() or target.is_symlink():
        target.unlink()
    directory.rmdir()

subprocess.run(QEMU + [str(BUSYBOX), 'sh', '-n', str(ROOT / 'autorun/autorun.sh')], check=True)
checks.append('Actual firmware ARM BusyBox accepts the generated shell syntax')
bad_payload = ROOT / 'releases/20261023-r1-encoder-crash/autorun/peek-code.bin'
assert sha256(bad_payload.read_bytes()).hexdigest() == 'a280e13d1456fadefeff7157f071eb9841060d5dbdf1a425d21fceff8ae3b319'
arm_output = subprocess.check_output(QEMU + [str(ROOT / 'build/check-arm'), str(stock), str(ROOT / 'build/peek.bin'), str(bad_payload)], text=True)
assert arm_output.count('PASS') == 4
checks.append('Untouched stock callback established the ABI; saved r1 crash reproduced with that ABI in isolated children')
checks.append('Actual ARM patch passed behavioral checks at two PIE bases; stock traversal/expiry code used')
checks.append('Preview is yellow only for current focus; edits/background changes are native white; multiple yellow values rejected')
result = {'result': 'PASS', 'hardware_tested': False, 'payload_sha256': info['payload_sha256'],
          'patched_copy_sha256': info['patched_copy_sha256'], 'checks': checks,
          'arm_output': arm_output.splitlines(),
          'limitations': 'Timer registration, repaint and hardware are substituted; real UI/audio await device trial'}
(ROOT / 'verification-results.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
