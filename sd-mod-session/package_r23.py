"""Package an explicitly requested r23 hardware trial; no physical SD I/O.

Do not promote mocked power receivers or constructor checks to full-VM proof.
"""
from pathlib import Path
import hashlib,json,subprocess,zipfile

R=Path(__file__).resolve().parent
O=R/'build/r23';O.mkdir(exist_ok=True)
sha=lambda b:hashlib.sha256(b).hexdigest()
subprocess.run(['python3','prepare_power_candidate.py'],cwd=R,check=True)
candidate=R/'build/power-refresh'
compile_proof=json.loads((candidate/'compile-verification.json').read_text())
power=json.loads((R/'build/power-button/verification.json').read_text())
payload=json.loads((candidate/'candidate-payload.json').read_text())
assert compile_proof['passed'] and power['passed']
assert payload['library_sha256']==compile_proof['library_sha256']
base=R.parent/'mpc-sample-r22c-seq-lights.zip'
assert sha(base.read_bytes())=='9b3168a70cc826b40372eb1aa8f88fb0d319586dc379465329fbf67d81f7999c'
with zipfile.ZipFile(base) as z:old={n:z.read(n) for n in z.namelist()}
files=dict(old)
for n,h in payload['payload_sha256'].items():
    b=(candidate/'autorun'/n).read_bytes();assert sha(b)==h,n
    files['autorun/'+n]=b
guide=(R/'R23_RU.md').read_bytes()
files['README_RU.md']=files['R23_RU.md']=guide
changes=sorted(n for n,b in files.items() if b!=old.get(n))
assert set(changes)=={'autorun/phone-input.so','autorun/mod-launcher.sh',
    'autorun/autorun.sh','autorun/autorun.json','autorun/autorun.check',
    'README_RU.md','R23_RU.md'}
for n,b in files.items():
    if n.startswith('autorun/'):
        p=O/n;p.parent.mkdir(exist_ok=True);p.write_bytes(b)
for n in ['autorun.sh','mod-launcher.sh']:
    subprocess.run(['sh','-n',str(O/'autorun'/n)],check=True)
# The virtual lab's shutdown receiver must never enter the device package.
launcher=files['autorun/mod-launcher.sh'].decode()
assert '/usr/bin/systemctl --no-block poweroff' in launcher
assert 'lab-poweroff-receiver' not in launcher
assert 'BindReadOnlyPaths=/run/udev/data:/run/udev/data' in launcher
archive=R.parent/'mpc-sample-r23-power-off-trial.zip'
if not archive.exists():
    with zipfile.ZipFile(archive,'x',zipfile.ZIP_DEFLATED) as z:
        for n,b in sorted(files.items()):z.writestr(n,b)
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None and set(z.namelist())==set(files)
    assert all(z.read(n)==b for n,b in files.items())
sources=set(compile_proof['sources_sha256'])|set(power['sources_sha256'])
sources.update(['prepare_power_candidate.py','package_r23.py',
                'POWER_BUTTON_RU.md','R23_RU.md','check_sequence_rgb.c',
                'check_steps.c','check_workflow.c','check_resample_simple_ui.c'])
hashes={n:sha((R/n).read_bytes()) for n in sorted(sources)}
info=json.loads((R/'build/r22c/build-info.json').read_text())
info.update(revision='r23-power-off',zip=archive.name,zip_sha256=sha(archive.read_bytes()),
    source_commit=subprocess.check_output(['git','-c','safe.directory='+str(R.parent),'rev-parse','HEAD'],cwd=R,text=True).strip(),
    library_sha256=payload['library_sha256'],sources_sha256=hashes,
    payload_sha256=payload['payload_sha256'],limited_device_trial_ready=True,
    full_vm_verified=False,hardware_tested=False,card_written=False,
    native_vm_verification=dict(passed=False,native_clone_main_reached=False,
        result='Proxy startup and socket identity prevented full lab test'),
    power_unit_verification=power,compile_verification=compile_proof)
(O/'build-info.json').write_text(json.dumps(info,indent=2)+'\n')
(O/'verification.json').write_text(json.dumps(dict(passed=True,
    scope='Trial package integrity and ARM checks; full VM not verified',
    library_sha256=payload['library_sha256'],sources_sha256=hashes,
    full_vm_verified=False,hardware_tested=False,card_written=False,
    changed_files=changes),indent=2)+'\n')
print(json.dumps(dict(revision=info['revision'],library_sha256=info['library_sha256'],
    zip_sha256=info['zip_sha256'],full_vm_verified=False,hardware_tested=False),indent=2))
