"""Reproduce BusyBox root permission tests using a disposable regular file only."""
from pathlib import Path
import json
import os
import subprocess
import tempfile

rootfs = '/home/user/mpc-rootfs-user-20260808'
with tempfile.TemporaryDirectory(prefix='mpc-busybox-access-', dir='/run') as temp:
    fixture = Path(temp) / 'denied-file'
    fixture.write_bytes(b'fixture only\n')
    fixture.chmod(0)
    script = '''
if [ -r "$1" ]; then echo TEST_R=yes; else echo TEST_R=no; fi
if [ -w "$1" ]; then echo TEST_W=yes; else echo TEST_W=no; fi
if ( : < "$1" ) 2>/dev/null; then echo OPEN_READ=allowed; else echo OPEN_READ=denied; fi
'''
    run = subprocess.run([
        'setpriv', '--groups=' + str(os.stat('/home/user').st_gid),
        '--bounding-set=-all', '--inh-caps=-all',
        '--ambient-caps=-all', '--no-new-privs',
        '/usr/bin/qemu-aarch64', '-L', rootfs,
        rootfs + '/usr/bin/busybox.nosuid', 'sh', '-c', script, 'test', str(fixture)
    ], text=True, capture_output=True)
    if run.returncode:
        raise RuntimeError(run.stdout + run.stderr)
    result = dict(output=run.stdout, stderr=run.stderr,
                  fixture='disposable regular file, mode 000, no capabilities',
                  physical_devices_opened=False)
    assert 'TEST_R=yes' in run.stdout and 'TEST_W=yes' in run.stdout, run.stdout
    assert 'OPEN_READ=denied' in run.stdout, run.stdout
    (Path(__file__).parent / 'busybox-access-result.json').write_text(json.dumps(result, indent=2))
    print(json.dumps(result, indent=2))
