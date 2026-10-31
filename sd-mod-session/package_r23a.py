"""Freeze the Pad Link/output/header trial over immutable r23; no SD access."""
from pathlib import Path
import hashlib,json,subprocess,zipfile
R=Path(__file__).resolve().parent;O=R/'build/r23a';O.mkdir(exist_ok=True)
sha=lambda b:hashlib.sha256(b).hexdigest()
candidate=R/'build/power-refresh'
compile_proof=json.loads((candidate/'compile-verification.json').read_text())
fast=json.loads((R/'build/resample-fast/verification.json').read_text())
eq=json.loads((R/'build/pad-eq/verification.json').read_text())
for proof in (compile_proof,fast,eq):
    assert proof['passed']
    for n,h in proof['sources_sha256'].items():assert sha((R/n).read_bytes())==h,n
assert fast['ubsan'] and eq['ubsan'] and fast['r23_physical_output_silence_regression_reproduced']
assert compile_proof['full_preload_constructor']
assert 'native header Boolean' in (R/'build/pad-eq/r23-old-ui.log').read_text()
assert 'Assertion' in (R/'build/pad-eq/r23-old-ui.log').read_text()
base=R.parent/'mpc-sample-r23-power-off-trial.zip'
assert sha(base.read_bytes())=='10f96dc5e2673deaa3b29163c1aa964c1be0159bec4839b4512fa86480df23d4'
with zipfile.ZipFile(base) as z:old={n:z.read(n) for n in z.namelist()}
files=dict(old);lib=(candidate/'phone-input.so').read_bytes()
assert sha(lib)==compile_proof['library_sha256']
files['autorun/phone-input.so']=lib
launcher=old['autorun/mod-launcher.sh'].decode()
old_lib=sha(old['autorun/phone-input.so']);assert launcher.count('SAVE_IO_SHA='+old_lib)==1
launcher=launcher.replace('SAVE_IO_SHA='+old_lib,'SAVE_IO_SHA='+sha(lib))
files['autorun/mod-launcher.sh']=launcher.encode()
entry=old['autorun/autorun.sh'].decode();old_launcher=sha(old['autorun/mod-launcher.sh'])
assert entry.count(old_launcher)==1 and entry.count('revision=r23-power-off')==1
entry=entry.replace(old_launcher,sha(files['autorun/mod-launcher.sh'])).replace('revision=r23-power-off','revision=r23a-padlink-render')
files['autorun/autorun.sh']=entry.encode()
manifest=json.loads(old['autorun/autorun.json']);script=manifest['value0']['scripts'][0]
script['message']='Start r23a: Sample render captures output routing and Pad Link duration; Bypass indication corrected.'
script['yesButtonText']='Start r23a'
files['autorun/autorun.json']=(json.dumps(manifest,indent=2)+'\n').encode()
files['autorun/autorun.check']=sha(files['autorun/autorun.json']+files['autorun/autorun.sh']+b'f7bed1c9-7e90-4731-9194-c996666b4331').encode()
files['README_RU.md']=files['R23A_RU.md']=(R/'R23A_RU.md').read_bytes()
changed=sorted(n for n,b in files.items() if old.get(n)!=b)
assert set(changed)=={'autorun/phone-input.so','autorun/mod-launcher.sh','autorun/autorun.sh','autorun/autorun.json','autorun/autorun.check','README_RU.md','R23A_RU.md'}
for n,b in files.items():
    if n.startswith('autorun/'):
        p=O/n;p.parent.mkdir(exist_ok=True);p.write_bytes(b)
for n in ('autorun.sh','mod-launcher.sh'):subprocess.run(['sh','-n',str(O/'autorun'/n)],check=True)
archive=R.parent/'mpc-sample-r23a-padlink-render.zip'
if not archive.exists():
    with zipfile.ZipFile(archive,'x',zipfile.ZIP_DEFLATED) as z:
        for n,b in sorted(files.items()):z.writestr(n,b)
with zipfile.ZipFile(archive) as z:assert z.testzip() is None and set(z.namelist())==set(files) and all(z.read(n)==b for n,b in files.items())
sources=set(compile_proof['sources_sha256'])|set(fast['sources_sha256'])|set(eq['sources_sha256'])|{'package_r23a.py','R23A_RU.md'}
hashes={n:sha((R/n).read_bytes()) for n in sorted(sources)}
info=json.loads((R/'build/r23/build-info.json').read_text())
info.update(revision='r23a-padlink-render',zip=archive.name,zip_sha256=sha(archive.read_bytes()),
    source_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
    library_sha256=sha(lib),sources_sha256=hashes,
    payload_sha256={n[8:]:sha(b) for n,b in files.items() if n.startswith('autorun/')},
    full_vm_verified=False,hardware_tested=False,card_written=False,
    predecessor_device_confirmed=['r23_sequence_lighting','r23_power_button_shutdown'],
    compile_verification=compile_proof,render_verification=fast,ui_verification=eq)
proof=dict(passed=True,scope='Pinned native ARM normal/UBSan and constructor; DSP fixture and no device proof for r23a',
    library_sha256=sha(lib),sources_sha256=hashes,full_vm_verified=False,hardware_tested=False,card_written=False,changed_files=changed)
(O/'build-info.json').write_text(json.dumps(info,indent=2)+'\n')
(O/'verification.json').write_text(json.dumps(proof,indent=2)+'\n')
print(json.dumps(dict(revision=info['revision'],library_sha256=sha(lib),zip_sha256=info['zip_sha256'],hardware_tested=False),indent=2))
