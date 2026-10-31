"""Build only; never writes to an SD or connects to the MPC."""
from pathlib import Path
from hashlib import sha256
import json
import re
import zipfile
import io
import wave
import shutil

root = Path(__file__).parent
workspace = root.parent
source = (root / 'src/autorun.sh').read_text(encoding='utf-8').replace('\r\n', '\n')
stock = workspace / 'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
unit = workspace / 'extracted_v1.3.0/partitions/part3/usr/lib/systemd/system/ac50.service'
for variable, file in [('STOCK_SHA', stock), ('UNIT_SHA', unit)]:
    expected = re.search(r'^' + variable + r'=([a-f0-9]{64})$', source, re.M).group(1)
    assert sha256(file.read_bytes()).hexdigest() == expected, variable
assert 'systemd-analyze' not in source.split('for cmd in ', 1)[1].split(';', 1)[0]
assert 'exec setarch -R -- "$STOCK"' in source
assert not re.search(r'^\s*(?:/usr/bin/)?az0x-firmware-tool\b', source, re.M)
timing = {name: int(re.search(r'^' + name + r'=(\d+)$', source, re.M).group(1))
          for name in ('CLONE_SECONDS', 'SUPERVISOR_SECONDS', 'GUARD_SECONDS', 'PROXY_SECONDS')}
assert timing['CLONE_SECONDS'] == 180
assert timing['SUPERVISOR_SECONDS'] >= timing['CLONE_SECONDS'] + 30
assert timing['GUARD_SECONDS'] >= timing['SUPERVISOR_SECONDS'] + 30
assert timing['PROXY_SECONDS'] >= timing['GUARD_SECONDS'] + 90
out = root / 'autorun'
out.mkdir(exist_ok=True)
vendor = root / 'vendor/xdg-dbus-proxy'
proxy_file = vendor / 'arm64/xdg-dbus-proxy'
policy_file = root / 'proxy-policy.args'
for variable, file in [('PROXY_SHA', proxy_file), ('POLICY_SHA', policy_file)]:
    expected = re.search(r'^' + variable + r'=([a-f0-9]{64})$', source, re.M).group(1)
    assert sha256(file.read_bytes()).hexdigest() == expected, variable
shutil.copyfile(proxy_file, out / 'xdg-dbus-proxy')
shutil.copyfile(policy_file, out / 'proxy-policy.args')
(out / 'autorun.sh').write_bytes(source.encode('utf-8'))
manifest = {'value0': {'scripts': [{
    'path': 'autorun.sh', 'askQuestion': True,
    'message': 'Run an unchanged MPC copy in RAM for up to 3 minutes? Save first, then test sample loading, pads and sound. Wait for automatic stock return before ejecting the card.',
    'yesButtonText': 'Run trial', 'noButtonText': 'Cancel',
}], 'files': []}}
(out / 'autorun.json').write_bytes((json.dumps(manifest, indent=2) + '\n').encode('ascii'))
salt = b'f7bed1c9-7e90-4731-9194-c996666b4331'
digest = sha256((out / 'autorun.json').read_bytes() + (out / 'autorun.sh').read_bytes() + salt).hexdigest()
(out / 'autorun.check').write_bytes(digest.encode('ascii'))
archive = workspace / 'mpc-sample-unmodified-clone-trial.zip'
status_dir = root / 'MPC_TRIAL_STATUS'
status_dir.mkdir(exist_ok=True)
octal = re.search(r"^STATUS_WAV_OCTAL='(.*)'$", source, re.M).group(1)
wav_bytes = bytes(int(part, 8) for part in octal.split("\\0")[1:])
with wave.open(io.BytesIO(wav_bytes)) as w:
    assert w.getnchannels() == 1 and w.getnframes() == 80
(status_dir / 'READY.wav').write_bytes(wav_bytes)
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as z:
    for filename in ('autorun.json', 'autorun.sh', 'autorun.check', 'xdg-dbus-proxy', 'proxy-policy.args'):
        z.write(out / filename, 'autorun/' + filename)
    z.write(root / 'README_RU.md', 'README_RU.md')
    z.write(status_dir / 'READY.wav', 'MPC_TRIAL_STATUS/READY.wav')
    z.write(vendor / 'COPYRIGHT', 'licenses/xdg-dbus-proxy-COPYRIGHT')
    z.write(vendor / 'provenance.json', 'licenses/xdg-dbus-proxy-provenance.json')
    for source_file in sorted((vendor / 'source').iterdir()):
        z.write(source_file, 'licenses/source/' + source_file.name)
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
    expected_files = {'autorun/autorun.json', 'autorun/autorun.sh', 'autorun/autorun.check', 'autorun/xdg-dbus-proxy', 'autorun/proxy-policy.args', 'README_RU.md', 'MPC_TRIAL_STATUS/READY.wav', 'licenses/xdg-dbus-proxy-COPYRIGHT', 'licenses/xdg-dbus-proxy-provenance.json'}
    expected_files.update('licenses/source/' + p.name for p in (vendor / 'source').iterdir())
    assert set(z.namelist()) == expected_files
    assert b'\r' not in z.read('autorun/autorun.sh')
    assert sha256(z.read('autorun/autorun.json') + z.read('autorun/autorun.sh') + salt).hexdigest().encode() == z.read('autorun/autorun.check')
validation_path = root / 'hardware-validation.json'
validation = json.loads(validation_path.read_text(encoding='utf-8')) if validation_path.exists() else {}
payload_files = ('autorun.sh', 'autorun.json', 'autorun.check', 'xdg-dbus-proxy', 'proxy-policy.args')
hardware_tested = bool(validation) and all(
    validation.get('payload_sha256', {}).get(name) == sha256((out / name).read_bytes()).hexdigest()
    for name in payload_files)
summary = {'zip': archive.name, 'zip_sha256': sha256(archive.read_bytes()).hexdigest(),
           'autorun_check': digest, 'script_sha256': sha256(source.encode()).hexdigest(),
           'hardware_tested': hardware_tested,
           'hardware_validation_scope': validation.get('scope') if hardware_tested else None,
           'timing_seconds': timing}
(root / 'build-info.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
print(json.dumps(summary, indent=2))
