"""Package r22 over the immutable r21d payload; no physical card writes."""
from pathlib import Path
import hashlib, json, subprocess, zipfile

R = Path(__file__).resolve().parent
O = R / 'build/r22'
digest = lambda data: hashlib.sha256(data).hexdigest()
base_zip = R.parent / 'mpc-sample-r21d-mono-dialog.zip'
assert digest(base_zip.read_bytes()) == 'f40ac39a21d35321bdc3b396ff10c12322666dd2c9933e29ec61a45cd63334f2'
base = json.loads((R / 'build/r21d/build-info.json').read_text())
report = json.loads((O / 'verification.json').read_text())
assert report['passed'] and report['revision'] == 'r22-exit-project-copy'
assert json.loads((R/'build/mod-splash/verification.json').read_text())['passed']
native = json.loads((O/'lab-large-copy.json').read_text())
assert native['passed'] and native['library_sha256'] == report['library_sha256']
for name, expected in report['sources_sha256'].items():
    assert digest((R / name).read_bytes()) == expected, name
with zipfile.ZipFile(base_zip) as archive:
    files = {name: archive.read(name) for name in archive.namelist()}
for name, expected in base['payload_sha256'].items():
    assert digest(files['autorun/' + name]) == expected, name
old_library = digest(files['autorun/phone-input.so'])
old_launcher = digest(files['autorun/mod-launcher.sh'])
files['autorun/phone-input.so'] = (O / 'phone-input.so').read_bytes()
assert digest(files['autorun/phone-input.so']) == report['library_sha256']
launcher = files['autorun/mod-launcher.sh'].decode()
def guard_block(text):
    return text.split('  guard)\n', 1)[1].split('    ;;', 1)[0]
old_guard = guard_block(launcher)
new_guard = guard_block((R/'src/autorun.template.sh').read_text())
assert launcher.count(old_guard) == 1
assert json.loads((O/'guard-verification.json').read_text())['passed']
launcher = launcher.replace(old_guard, new_guard)
template = (R/'src/autorun.template.sh').read_text()
functions = template.split('boot_splash_start() {',1)[1].split('flush_report_sd() {',1)[0]
assert 'boot_splash_start() {' not in launcher
launcher = launcher.replace('flush_report_sd() {','boot_splash_start() {'+functions+'flush_report_sd() {',1)
assert launcher.count('    checkpoint STOCK_STOPPED || fail SD_JOURNAL_CHECKPOINT_FAILED\n') == 1
launcher = launcher.replace('    checkpoint STOCK_STOPPED || fail SD_JOURNAL_CHECKPOINT_FAILED\n',
    '    checkpoint STOCK_STOPPED || fail SD_JOURNAL_CHECKPOINT_FAILED\n    boot_splash_start\n')
launcher = launcher.replace('    data_layer_start || fail DATA_LAYER_START_FAILED\n',
    '    data_layer_start || fail DATA_LAYER_START_FAILED\n    boot_splash_stop\n',1)
launcher = launcher.replace('    log RECOVERY_BEGIN\n','    log RECOVERY_BEGIN\n    boot_splash_stop\n',1)
files['autorun/boot-splash'] = (O/'boot-splash').read_bytes()
splash_sha = digest(files['autorun/boot-splash'])
anchor='cp "$DIR/stage-data" "$R/stage-data"\n'
assert launcher.count(anchor)==1
launcher=launcher.replace(anchor,anchor+'''[ "$(hash "$DIR/boot-splash")" = "SPLASH_SHA" ] || fail WRONG_SPLASH_HELPER
cp "$DIR/boot-splash" "$R/boot-splash"
chmod 500 "$R/boot-splash"
[ "$(hash "$R/boot-splash")" = "SPLASH_SHA" ] || fail SPLASH_HELPER_COPY_MISMATCH
'''.replace('SPLASH_SHA',splash_sha))
assert launcher.count('SAVE_IO_SHA=' + old_library) == 1
files['autorun/mod-launcher.sh'] = launcher.replace(
    'SAVE_IO_SHA=' + old_library, 'SAVE_IO_SHA=' + report['library_sha256']).encode()
entry = files['autorun/autorun.sh'].decode()
assert entry.count(old_launcher) == 1 and entry.count('revision=r21d-mono-dialog') == 1
files['autorun/autorun.sh'] = entry.replace(old_launcher, digest(files['autorun/mod-launcher.sh'])).replace(
    'revision=r21d-mono-dialog', 'revision=r22-exit-project-copy').encode()
manifest = json.loads(files['autorun/autorun.json'])
script = manifest['value0']['scripts'][0]
script['message'] = 'Start r22? Automatic SD project copy before Mod Off; Mono dialog and sequence colour routing fixed.'
script['askQuestion'] = False
script['yesButtonText'] = 'Start r22'
files['autorun/autorun.json'] = (json.dumps(manifest, indent=2) + '\n').encode()
files['autorun/autorun.check'] = digest(files['autorun/autorun.json'] + files['autorun/autorun.sh'] +
    b'f7bed1c9-7e90-4731-9194-c996666b4331').encode()
files['README_RU.md'] = files['R22_RU.md'] = (R / 'R22_RU.md').read_bytes()
payload = O / 'autorun'
payload.mkdir(exist_ok=True)
for name, data in files.items():
    if name.startswith('autorun/'):
        target = O / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
for name in ['autorun.sh', 'mod-launcher.sh']:
    subprocess.run(['sh', '-n', str(payload / name)], check=True)
import sys
candidate = '--candidate' in sys.argv
output = O/'candidate.zip' if candidate else R.parent/'mpc-sample-r22-exit-project-copy.zip'
if candidate and output.exists(): output.unlink()
if not output.exists():
    with zipfile.ZipFile(output, 'x', zipfile.ZIP_DEFLATED) as archive:
        for name, data in sorted(files.items()):
            archive.writestr(name, data)
with zipfile.ZipFile(output) as archive:
    assert archive.testzip() is None and set(archive.namelist()) == set(files)
    assert all(archive.read(name) == data for name, data in files.items()), 'Frozen archive differs'
info = dict(base, revision=report['revision'], zip=output.name, zip_sha256=digest(output.read_bytes()),
    library_sha256=report['library_sha256'], hardware_tested=False,
    boot_splash_sha256=splash_sha,
    known_unfixed=['live simultaneous Tune/Poly recording','Levels/Step Edit lighting pending device review','Standard/Low Battery charging control unavailable'],
    mod_off='unique native SD project copy including samples; retained FileIO futures checked after SaveProject returns; flush then return stock',
    source_commit=subprocess.check_output(['git', '-c', 'safe.directory=' + str(R.parent), 'rev-parse', 'HEAD'],
        cwd=R, text=True).strip(),
    payload_sha256={name[8:]: digest(data) for name, data in files.items() if name.startswith('autorun/')})
(O / 'build-info.json').write_text(json.dumps(info, indent=2) + '\n')
print(json.dumps({'zip': str(output), 'sha256': info['zip_sha256']}, indent=2))
