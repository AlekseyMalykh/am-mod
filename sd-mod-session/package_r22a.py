"""Remove only the experimental splash from the frozen, tested r22 payload.

No library rebuild, card write, hardware action or display daemon call.
"""
from pathlib import Path
import hashlib
import json
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parent
OUT = ROOT / 'build/r22a'
OUT.mkdir(exist_ok=True)
sha = lambda body: hashlib.sha256(body).hexdigest()
base = ROOT.parent / 'mpc-sample-r22-exit-project-copy.zip'
assert sha(base.read_bytes()) == '91164475c905e302a9805ad12e05c3bb7cf915cac12ef9421cb9440e6dd3d20e'
with zipfile.ZipFile(base) as archive:
    original = {name: archive.read(name) for name in archive.namelist()}
files = dict(original)
launcher = original['autorun/mod-launcher.sh'].decode()
start = launcher.index('boot_splash_start() {\n')
end = launcher.index('flush_report_sd() {\n', start)
removed_functions = launcher[start:end]
launcher = launcher[:start] + launcher[end:]
assert launcher.count('    boot_splash_start\n') == 1
assert launcher.count('    boot_splash_stop\n') == 2
launcher = launcher.replace('    boot_splash_start\n', '').replace('    boot_splash_stop\n', '')
copy_start = launcher.index('[ "$(hash "$DIR/boot-splash")" = ')
copy_end = launcher.index('\n', launcher.index('] || fail SPLASH_HELPER_COPY_MISMATCH', copy_start)) + 1
removed_copy = launcher[copy_start:copy_end]
launcher = launcher[:copy_start] + launcher[copy_end:]
assert 'boot-splash' not in launcher and 'boot_splash' not in launcher
files['autorun/mod-launcher.sh'] = launcher.encode()
del files['autorun/boot-splash']
entry = original['autorun/autorun.sh'].decode()
old_hash = sha(original['autorun/mod-launcher.sh'])
assert entry.count(old_hash) == 1
entry = entry.replace(old_hash, sha(files['autorun/mod-launcher.sh']))
assert entry.count('revision=r22-exit-project-copy') == 1
files['autorun/autorun.sh'] = entry.replace('revision=r22-exit-project-copy', 'revision=r22a-no-splash').encode()
manifest = json.loads(files['autorun/autorun.json'])
script = manifest['value0']['scripts'][0]
script['message'] = 'Start r22a: experimental splash removed. Mod Off saves a separate SD project copy.'
script['yesButtonText'] = 'Start r22a'
files['autorun/autorun.json'] = (json.dumps(manifest, indent=2) + '\n').encode()
files['autorun/autorun.check'] = sha(files['autorun/autorun.json'] + files['autorun/autorun.sh'] + b'f7bed1c9-7e90-4731-9194-c996666b4331').encode()
guide = (ROOT / 'R22A_RU.md').read_bytes()
files['README_RU.md'] = files['R22A_RU.md'] = guide

# Reverse the exact removals to prove all unrelated launcher code is unchanged.
restored = launcher.replace('flush_report_sd() {\n', removed_functions + 'flush_report_sd() {\n', 1)
restored = restored.replace('    log RECOVERY_BEGIN\n', '    log RECOVERY_BEGIN\n    boot_splash_stop\n', 1)
restored = restored.replace('    checkpoint STOCK_STOPPED || fail SD_JOURNAL_CHECKPOINT_FAILED\n',
    '    checkpoint STOCK_STOPPED || fail SD_JOURNAL_CHECKPOINT_FAILED\n    boot_splash_start\n', 1)
restored = restored.replace('    data_layer_start || fail DATA_LAYER_START_FAILED\n',
    '    data_layer_start || fail DATA_LAYER_START_FAILED\n    boot_splash_stop\n', 1)
restored = restored.replace('cp "$DIR/stage-data" "$R/stage-data"\n',
    'cp "$DIR/stage-data" "$R/stage-data"\n' + removed_copy, 1)
assert restored.encode() == original['autorun/mod-launcher.sh']
changed = {name for name in files if files[name] != original.get(name)}
assert changed == {'autorun/mod-launcher.sh', 'autorun/autorun.sh', 'autorun/autorun.json',
    'autorun/autorun.check', 'README_RU.md', 'R22A_RU.md'}
assert set(original) - set(files) == {'autorun/boot-splash'}
assert files['autorun/phone-input.so'] == original['autorun/phone-input.so']
for name, body in files.items():
    if name.startswith('autorun/'):
        target = OUT / name
        target.parent.mkdir(exist_ok=True)
        target.write_bytes(body)
for name in ['autorun.sh', 'mod-launcher.sh']:
    subprocess.run(['sh', '-n', str(OUT / 'autorun' / name)], check=True)
template = (ROOT / 'src/autorun.template.sh').read_text()
assert 'boot_splash' not in template and 'boot-splash' not in template
archive_path = ROOT.parent / 'mpc-sample-r22a-no-splash.zip'
if not archive_path.exists():
    with zipfile.ZipFile(archive_path, 'x', zipfile.ZIP_DEFLATED) as archive:
        for name, body in sorted(files.items()): archive.writestr(name, body)
with zipfile.ZipFile(archive_path) as archive:
    assert archive.testzip() is None and set(archive.namelist()) == set(files)
    assert all(archive.read(name) == body for name, body in files.items())
info = json.loads((ROOT / 'build/r22/build-info.json').read_text())
info.update(revision='r22a-no-splash', zip=archive_path.name, zip_sha256=sha(archive_path.read_bytes()),
    source_commit=subprocess.check_output(['git', '-c', 'safe.directory=' + str(ROOT.parent),
        'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
    boot_splash_sha256=None, custom_splash_enabled=False, hardware_tested=False, card_written=False,
    payload_sha256={name[8:]: sha(body) for name, body in files.items() if name.startswith('autorun/')})
(OUT / 'build-info.json').write_text(json.dumps(info, indent=2) + '\n')
report = dict(passed=True, revision=info['revision'], library_unchanged=True,
    launcher_other_code_unchanged=True, shell_syntax_checked=True,
    splash_calls_removed=True, helper_removed=True, card_written=False,
    hardware_tested=False, changed_files=sorted(changed), zip_sha256=info['zip_sha256'])
(OUT / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
