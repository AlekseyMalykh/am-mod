"""Execute the launcher guard with controlled service state and marker timing."""
from pathlib import Path
import json
import os
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parent.parent

def guard(text):
    return text.split('  guard)\n', 1)[1].split('    ;;', 1)[0]

def probe(body, verified=False, healthy=True, pending=True, race=False):
    with tempfile.TemporaryDirectory(prefix='mpc-guard-') as tmp:
        root = Path(tmp)
        (root/'bin').mkdir()
        (root/'startup-deadline').write_text('200\n' if pending else '50\n')
        if verified:
            (root/'clone-verified').touch()
        for name, script in {
            'cut': '#!/bin/sh\n[ "$RACE" != 1 ] || touch "$R/clone-verified"\necho 100\n',
            'systemctl': '#!/bin/sh\nif [ "$1" = is-active ]; then [ "$HEALTHY" = 1 ]; else echo "$*" >> "$R/calls"; fi\n'
        }.items():
            file = root/'bin'/name
            file.write_text(script)
            file.chmod(0o700)
        env = dict(os.environ, R=tmp, U='test', RACE=str(int(race)),
            HEALTHY=str(int(healthy)), PATH=str(root/'bin')+':'+os.environ['PATH'])
        subprocess.run(['sh', '-c', 'set -eu\nlog(){ :; }\n'+body], env=env, check=True)
        return (root/'calls').exists()

if __name__ == '__main__':
    body = guard((ROOT/'src/autorun.template.sh').read_text())
    assert not probe(body, verified=True)
    assert probe(body, verified=True, healthy=False)
    assert not probe(body)
    assert probe(body, pending=False)
    assert not probe(body, race=True), 'A new verification marker must not cause recovery'
    with zipfile.ZipFile(ROOT.parent/'mpc-sample-r21d-mono-dialog.zip') as frozen:
        old = guard(frozen.read('autorun/mod-launcher.sh').decode())
    assert probe(old, race=True), 'Negative control must reproduce the old race'
    report = {'passed': True, 'old_race_reproduced': True,
        'new_guard_passed': True, 'hardware_tested': False}
    (ROOT/'build/r22/guard-verification.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report))
