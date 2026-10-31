"""Verify the device-mask guard on a disposable dummy node, never real storage."""
from pathlib import Path
import json
import os
import shlex
import stat
import subprocess
import tempfile

here = Path(__file__).parent
rootfs = '/home/user/mpc-rootfs-user-20260808'
qemu = ['/usr/bin/qemu-aarch64', '-L', rootfs, rootfs + '/usr/bin/busybox.nosuid']
source = (here / 'src/autorun.sh').read_text()
function = source.split('masked_nodes_check() {', 1)[1].split('\nsandbox_check()', 1)[0]
results = []
with tempfile.TemporaryDirectory(prefix='mpc-mask-identity-', dir='/run') as temp:
    folder = Path(temp)
    node = folder / 'dummy-block'
    os.mknod(node, stat.S_IFBLK, os.makedev(0, 231))
    node.chmod(0)
    (folder / 'blocked-nodes').write_text(str(node) + '\n')
    identities = ''.join(subprocess.check_output(
        qemu + ['stat', '-Lc', '%d:%i:%f:%t:%T:%u:%g', '/run/systemd/inaccessible/' + k],
        text=True) for k in ('blk', 'chr', 'sock'))
    identity_file = folder / 'mask-identities'
    identity_file.write_text(identities)
    # Use the actual firmware's stat, grep and awk too, not their host substitutes.
    wrappers = ''.join(f'{a}() {{ {shlex.join(qemu)} {a} "$@"; }}\n' for a in ('stat', 'grep', 'awk'))
    script = folder / 'check.sh'
    script.write_text('set -eu\nAPP=' + shlex.quote(temp) + '\n' + wrappers
                      + 'masked_nodes_check() {' + function + '\nmasked_nodes_check\n')
    for label, masked, expected in [('masked', True, 0), ('unmasked', False, 1), ('wrong_identity', True, 1)]:
        if label == 'wrong_identity':
            identity_file.write_text('0:0:0:0:0:0:0\n')
        args = ['systemd-run', '--quiet', '--wait', '--pipe', '--collect',
                '--unit=mpc-mask-check-' + str(os.getpid()) + '-' + label,
                '-p', 'Type=exec', '-p', 'NoNewPrivileges=yes',
                '-p', 'CapabilityBoundingSet=CAP_IPC_LOCK CAP_SYS_NICE',
                '-p', 'SupplementaryGroups=' + str(os.stat('/home/user').st_gid)]
        if masked:
            args += ['-p', 'InaccessiblePaths=' + str(node)]
        run = subprocess.run(args + qemu + ['sh', str(script)], text=True, capture_output=True)
        item = dict(test=label, code=run.returncode, output=run.stdout + run.stderr)
        results.append(item)
        print(json.dumps(item), flush=True)
        assert run.returncode == expected, item
        if expected == 0:
            assert 'RAW_DEVICE_MASK_IDENTITIES_VERIFIED' in run.stdout
        else:
            assert 'CHECK_FAILED: not_systemd_inaccessible_node' in run.stdout
(here / 'mask-identity-results.json').write_text(json.dumps(results, indent=2))
