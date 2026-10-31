"""Package the verified native Poly/Seq candidate over frozen r22a. No SD I/O."""
from pathlib import Path
import hashlib,json,subprocess,zipfile

R=Path(__file__).resolve().parent
O=R/'build/r22b';O.mkdir(exist_ok=True)
sha=lambda body:hashlib.sha256(body).hexdigest()
base=R.parent/'mpc-sample-r22a-no-splash.zip'
assert sha(base.read_bytes())=='ca59d7f4ededea2ec6e5d850d01e46dde1c17a0db76afea956dc494316d65cbb'
with zipfile.ZipFile(base) as z:original={n:z.read(n) for n in z.namelist()}
files=dict(original)
lib=(R/'build/poly-record/phone-input.so').read_bytes()
proof=json.loads((R/'build/poly-record/vm-verification.json').read_text())
assert proof['passed'] and proof['library_sha256']==sha(lib)
for key in ['two_notes_reverse_release','three_notes_reverse_release','undo_redo','playback_two_notes','stock_reopen','sequence_rgb']:
    assert proof[key],key
old=sha(original['autorun/phone-input.so'])
files['autorun/phone-input.so']=lib
launcher=original['autorun/mod-launcher.sh'].decode()
assert launcher.count('SAVE_IO_SHA='+old)==1
launcher=launcher.replace('SAVE_IO_SHA='+old,'SAVE_IO_SHA='+sha(lib))
assert 'boot_splash' not in launcher and 'boot-splash' not in launcher
files['autorun/mod-launcher.sh']=launcher.encode()
entry=original['autorun/autorun.sh'].decode()
assert entry.count(sha(original['autorun/mod-launcher.sh']))==1
entry=entry.replace(sha(original['autorun/mod-launcher.sh']),sha(files['autorun/mod-launcher.sh']))
assert entry.count('revision=r22a-no-splash')==1
files['autorun/autorun.sh']=entry.replace('revision=r22a-no-splash','revision=r22b-poly-seq').encode()
manifest=json.loads(files['autorun/autorun.json']);script=manifest['value0']['scripts'][0]
script['message']='Start r22b: Poly chord recording and Seq pad colours. Mod Off saves a separate SD project copy.'
script['yesButtonText']='Start r22b'
files['autorun/autorun.json']=(json.dumps(manifest,indent=2)+'\n').encode()
files['autorun/autorun.check']=sha(files['autorun/autorun.json']+files['autorun/autorun.sh']+b'f7bed1c9-7e90-4731-9194-c996666b4331').encode()
files['README_RU.md']=files['R22B_RU.md']=(R/'R22B_RU.md').read_bytes()
changed={n for n in files if files[n]!=original.get(n)}
assert changed=={'autorun/phone-input.so','autorun/mod-launcher.sh','autorun/autorun.sh','autorun/autorun.json','autorun/autorun.check','README_RU.md','R22B_RU.md'}
for n,b in files.items():
    if n.startswith('autorun/'):
        p=O/n;p.parent.mkdir(exist_ok=True);p.write_bytes(b)
for n in ['autorun.sh','mod-launcher.sh']:subprocess.run(['sh','-n',str(O/'autorun'/n)],check=True)
archive=R.parent/'mpc-sample-r22b-poly-seq.zip'
if not archive.exists():
    with zipfile.ZipFile(archive,'x',zipfile.ZIP_DEFLATED) as z:
        for n,b in sorted(files.items()):z.writestr(n,b)
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None and set(z.namelist())==set(files)
    assert all(z.read(n)==b for n,b in files.items())
sources=json.loads((R/'build/r22/verification.json').read_text())['linked_sources']+['src/poly_record.c','src/poly_record.h','check_poly_record.c','lab/check_live_chord.py','package_r22b.py','R22B_RU.md']
info=json.loads((R/'build/r22a/build-info.json').read_text())
info.update(revision='r22b-poly-seq',zip=archive.name,zip_sha256=sha(archive.read_bytes()),
    source_commit=subprocess.check_output(['git','-c','safe.directory='+str(R.parent),'rev-parse','HEAD'],text=True).strip(),
    library_sha256=sha(lib),sources_sha256={n:sha((R/n).read_bytes()) for n in sources},
    payload_sha256={n[8:]:sha(b) for n,b in files.items() if n.startswith('autorun/')},
    native_vm_verification=proof,card_written=False,hardware_tested=False,
    known_unfixed=['Standard/Low Battery charging control unavailable','Short power-button dialog in mod','Live Gate chord has shared final release'],
    custom_splash_enabled=False,limited_device_trial_ready=True)
(O/'build-info.json').write_text(json.dumps(info,indent=2)+'\n')
verification=dict(passed=True,library_sha256=sha(lib),sources_sha256=info['sources_sha256'],
    vm=proof,changed_files=sorted(changed),hardware_tested=False,card_written=False)
(O/'verification.json').write_text(json.dumps(verification,indent=2)+'\n')
print(json.dumps(dict(revision=info['revision'],zip_sha256=info['zip_sha256'],library_sha256=sha(lib),changed=sorted(changed)),indent=2))
