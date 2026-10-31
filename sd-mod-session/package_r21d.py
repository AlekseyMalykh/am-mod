"""Freeze the verified Mono dialog fix over the immutable r21c payload."""
from pathlib import Path
import hashlib, json, subprocess, zipfile

R = Path(__file__).resolve().parent
O = R / 'build/r21d'
digest = lambda data: hashlib.sha256(data).hexdigest()
base_zip = R.parent / 'mpc-sample-r21c-lighting-power-audit.zip'
assert digest(base_zip.read_bytes()) == '24a09c69ae7832d3a55a07b3ffb5f85c4d5b54aaf283f8e8cac7ada8a1d4c27a'
base = json.loads((R / 'build/r21c/build-info.json').read_text())
report = json.loads((O / 'verification.json').read_text())
assert report['passed'] and report['revision'] == 'r21d-mono-dialog'
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
assert launcher.count('SAVE_IO_SHA=' + old_library) == 1
files['autorun/mod-launcher.sh'] = launcher.replace(
    'SAVE_IO_SHA=' + old_library, 'SAVE_IO_SHA=' + report['library_sha256']).encode()
entry = files['autorun/autorun.sh'].decode()
assert entry.count(old_launcher) == 1 and entry.count('revision=r21c-lighting-power-audit') == 1
files['autorun/autorun.sh'] = entry.replace(old_launcher, digest(files['autorun/mod-launcher.sh'])).replace(
    'revision=r21c-lighting-power-audit', 'revision=r21d-mono-dialog').encode()
manifest = json.loads(files['autorun/autorun.json'])
script = manifest['value0']['scripts'][0]
script['message'] = 'Start r21d? Stereo > Mono destination/channel dialog corrected. Insert SD after the normal interface has loaded.'
script['yesButtonText'] = 'Start r21d'
files['autorun/autorun.json'] = (json.dumps(manifest, indent=2) + '\n').encode()
files['autorun/autorun.check'] = digest(files['autorun/autorun.json'] + files['autorun/autorun.sh'] +
    b'f7bed1c9-7e90-4731-9194-c996666b4331').encode()
files['README_RU.md'] = files['R21D_RU.md'] = (R / 'R21D_RU.md').read_bytes()
payload = O / 'autorun'
payload.mkdir(exist_ok=True)
for name, data in files.items():
    if name.startswith('autorun/'):
        target = O / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
for name in ['autorun.sh', 'mod-launcher.sh']:
    subprocess.run(['sh', '-n', str(payload / name)], check=True)
output = R.parent / 'mpc-sample-r21d-mono-dialog.zip'
if not output.exists():
    with zipfile.ZipFile(output, 'x', zipfile.ZIP_DEFLATED) as archive:
        for name, data in sorted(files.items()):
            archive.writestr(name, data)
with zipfile.ZipFile(output) as archive:
    assert archive.testzip() is None and set(archive.namelist()) == set(files)
    assert all(archive.read(name) == data for name, data in files.items()), 'Frozen archive differs'
info = dict(base, revision=report['revision'], zip=output.name, zip_sha256=digest(output.read_bytes()),
    library_sha256=report['library_sha256'], hardware_tested=False,
    source_commit=subprocess.check_output(['git', '-c', 'safe.directory=' + str(R.parent), 'rev-parse', 'HEAD'],
        cwd=R, text=True).strip(),
    payload_sha256={name[8:]: digest(data) for name, data in files.items() if name.startswith('autorun/')})
(O / 'build-info.json').write_text(json.dumps(info, indent=2) + '\n')
print(json.dumps({'zip': str(output), 'sha256': info['zip_sha256']}, indent=2))
