"""Package r27f over the device-confirmed workspace fix (no card/remote writes)."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import subprocess
import sys
import zipfile

R = Path(__file__).resolve().parent
O = R/'build/r27f'
O.mkdir(exist_ok=True)
sha = lambda data: hashlib.sha256(data).hexdigest()
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--base', type=Path, required=True)
parser.add_argument('--output', type=Path, default=R.parent/'am-mod-r27f.zip')
args = parser.parse_args()
assert sha(args.base.read_bytes()) == 'cd44591e24355446ad1f2e972b08c14e094f0a77a892af0668fdac44d5ca2a06'
assert re.search(r'#define AM_MOD_VERSION "r27f"', (R/'src/mod_build.h').read_text())
assert 'AM_MOD_SPLASH_VERSION' in (R/'src/splash_branding.h').read_text()
proofs = {}
for name, path in {
    'app':'build/chords/compile-verification.json',
    'startup':'build/early-startup/verification.json',
    'recovery':'build/project-recovery/verification.json',
    'drm':'build/early-drm/verification.json',
    'branding':'build/early-drm/branding-verification.json',
    'ui':'build/pad-eq/verification.json',
    'render_adapters':'build/resample-stock/verification.json',
}.items():
    proof = json.loads((R/path).read_text())
    assert proof['passed'], name
    for source, expected in proof['sources_sha256'].items():
        assert sha((R/source).read_bytes()) == expected, (name,source)
    proofs[name] = proof
assert proofs['app']['full_preload_constructor'] and proofs['app']['compressor_included']
assert proofs['app']['native_takeover_preserved'] and proofs['drm']['master_release_verified']
assert proofs['startup']['ubsan'] and proofs['ui']['ubsan']
from verify_callbacks import verify
callbacks = verify(R/'build/chords/phone-input.so')
assert callbacks['callbacks_verified'] == 51
with zipfile.ZipFile(args.base) as z:
    assert z.testzip() is None
    old = {name:z.read(name) for name in z.namelist() if not name.endswith('/')}
files = dict(old)
files.pop('SAMPLE_LOAD_DIAGNOSTIC.ru.md', None)
launcher = old['autorun/mod-launcher.sh'].decode()
for name, path, key, field, count in [
    ('phone-input.so','build/chords/phone-input.so','app','library_sha256',1),
    ('boot-splash','build/early-drm/boot-splash','drm','splash_sha256',2),
]:
    data = (R/path).read_bytes()
    assert sha(data) == proofs[key][field]
    previous = sha(old['autorun/'+name])
    assert launcher.count(previous) == count
    launcher = launcher.replace(previous, sha(data))
    files['autorun/'+name] = data
files['autorun/mod-launcher.sh'] = launcher.encode()
entry = old['autorun/autorun.sh'].decode()
assert entry.count(sha(old['autorun/mod-launcher.sh'])) == 1
assert entry.count('revision=r27e-workspace-fix1') == 1
entry = entry.replace(sha(old['autorun/mod-launcher.sh']), sha(files['autorun/mod-launcher.sh']))
files['autorun/autorun.sh'] = entry.replace('revision=r27e-workspace-fix1','revision=r27f').encode()
meta = json.loads(old['autorun/autorun.json'])
meta['value0']['scripts'][0]['message'] = 'AM Mod r27f - fixed sample loading, Copy Pad and render assignment; experimental FAT32'
files['autorun/autorun.json'] = (json.dumps(meta,indent=2)+'\n').encode()
files['autorun/autorun.check'] = sha(files['autorun/autorun.json']+files['autorun/autorun.sh']+b'f7bed1c9-7e90-4731-9194-c996666b4331').encode()
changed = sorted(name for name,data in files.items() if name.startswith('autorun/') and data != old[name])
assert set(changed) == {'autorun/'+name for name in ['phone-input.so','boot-splash','mod-launcher.sh','autorun.sh','autorun.json','autorun.check']}
for name in list(files):
    if not name.startswith('autorun/') and (R.parent/name).is_file():
        files[name] = (R.parent/name).read_bytes()
for path in [R.parent/'README.md',R.parent/'README.ru.md',R.parent/'CHANGELOG.md',*(R.parent/'docs').rglob('*')]:
    if path.is_file() and path.suffix.lower() in ('.md','.png','.gif','.svg'):
        files[path.relative_to(R.parent).as_posix()] = path.read_bytes()
files['README_RU.md'] = files['README.ru.md']
for name in ['autorun.sh','mod-launcher.sh']:
    subprocess.run(['sh','-n','/dev/stdin'],input=files['autorun/'+name],check=True)
args.output.parent.mkdir(parents=True,exist_ok=True)
with zipfile.ZipFile(args.output,'x',zipfile.ZIP_DEFLATED) as z:
    for name,data in sorted(files.items()):
        z.writestr(name,data)
with zipfile.ZipFile(args.output) as z:
    assert z.testzip() is None
    assert all(z.read(name)==data for name,data in files.items())
sys.path.insert(0,str(R.parent/'installer'))
from mpc_mod_installer import Package
package = Package.from_file(args.output)
assert package.revision == 'r27f' and package.filesystems == {'exfat','vfat'}
report = dict(passed=True,revision='r27f',channel='prerelease',
    package_sha256=sha(args.output.read_bytes()),payload_files=len(package.files),
    payload_sha256={name:sha(data) for name,data in sorted(package.files.items())},
    library_sha256=sha(files['autorun/phone-input.so']),splash_sha256=sha(files['autorun/boot-splash']),
    device_confirmed_base_sha256=sha(args.base.read_bytes()),
    workspace_fix_device_confirmed=True,final_package_hardware_tested=False,
    callbacks=callbacks,proofs=proofs,changed_payload=changed,
    splash_label='version: r27f',installer_package_validation=True,
    source_sha256={name:sha((R/name).read_bytes()) for name in ['package_workspace_release.py','src/mod_build.h','src/splash_branding.h']})
(O/'package-verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({key:value for key,value in report.items() if key not in ['proofs','payload_sha256']},indent=2))
