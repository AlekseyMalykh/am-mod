"""Exercise the recovery snapshot in temporary RAM-like fixtures, never an SD."""
from pathlib import Path
import json
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parent
source = (root / 'src/autorun.template.sh').read_text()
function = source.split('native_crash_snapshot() {', 1)[1].split('\nexport_report()', 1)[0]
function = 'native_crash_snapshot() {' + function
recover = source.split('  recover)', 1)[1].split('  supervise)', 1)[0]
assert recover.index('fail CLONE_STILL_RUNNING') < recover.index('    native_crash_snapshot\n') < recover.index('    data_layer_stop ')
assert '"$R/app/native-crash.txt"' in source.split('export_report() {', 1)[1].split('\nmasked_nodes_check()', 1)[0]
subprocess.run(['sh', '-n', str(root / 'src/autorun.template.sh')], check=True)

results = []
with tempfile.TemporaryDirectory(prefix='mpc-crash-test-') as temporary:
    base = Path(temporary)
    for case in ('missing', 'valid', 'oversize', 'file-link', 'parent-link', 'root-link', 'fifo'):
        runtime = base / case
        settings = runtime / 'data-changes/upper/Settings'
        folder = settings / 'Akai/MPC Sample'
        folder.mkdir(parents=True)
        (runtime / 'app').mkdir()
        path = folder / 'MPC Sample.crashinfo'
        if case == 'valid':
            path.write_text('native backtrace fixture\n')
            other = settings / 'MPC Sample'
            other.mkdir()
            (other / path.name).write_text('alternate settings fixture\n')
        elif case == 'oversize':
            path.write_text('x' * 70000)
        elif case == 'file-link':
            secret = base / 'outside-file'
            secret.write_text('DO_NOT_READ')
            path.symlink_to(secret)
        elif case in ('parent-link', 'root-link'):
            target = base / (case + '-outside')
            target.mkdir()
            (target / path.name).write_text('DO_NOT_READ')
            if case == 'parent-link':
                folder.rmdir()
                folder.symlink_to(target, target_is_directory=True)
            else:
                # No alias through the root of the upper layer either.
                folder.rmdir()
                folder.parent.rmdir()
                settings.rmdir()
                settings.symlink_to(target, target_is_directory=True)
        elif case == 'fifo':
            os.mkfifo(path)
        subprocess.run(['sh', '-s', '--', str(runtime)], input='set -eu\nR=$1\n' + function + '\nnative_crash_snapshot\n', text=True, check=True, timeout=3)
        output = (runtime / 'app/native-crash.txt').read_text()
        assert 'DO_NOT_READ' not in output
        if case == 'valid':
            assert 'native backtrace fixture' in output and 'alternate settings fixture' in output
        elif case == 'oversize':
            assert output.count('x') == 65536 and 'NATIVE_CRASH_TRUNCATED' in output
        elif case.endswith('link'):
            assert 'REJECTED' in output or 'ROOT_UNAVAILABLE' in output
        else:
            assert 'NOT_REGULAR_OR_MISSING' in output
        results.append(case)
out = root / 'build/native-crash-snapshot'
out.mkdir(exist_ok=True)
(out / 'verification.json').write_text(json.dumps({'passed': True, 'cases': results, 'after_application_stop': True, 'before_ram_unmount': True, 'max_bytes_per_file': 65536, 'sd_written': False}, indent=2) + '\n')
print('PASS recovery crashinfo: ' + ', '.join(results) + '; bounded capture after stop, before unmount, included in exported report')
