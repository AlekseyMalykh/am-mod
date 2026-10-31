"""Package the Seq lighting refresh over immutable r22b; no physical SD I/O."""
from pathlib import Path
import hashlib,json,subprocess,zipfile

R=Path(__file__).resolve().parent
O=R/'build/r22c';O.mkdir(exist_ok=True)
sha=lambda b:hashlib.sha256(b).hexdigest()
base=R.parent/'mpc-sample-r22b-poly-seq.zip'
assert sha(base.read_bytes())=='a404237ce3ae565006bfcefcdd14f92569d05f15ea4e7489c9482c1ddda85b36'
with zipfile.ZipFile(base) as z:old={n:z.read(n) for n in z.namelist()}
files=dict(old);lib=(R/'build/sequence-refresh/phone-input.so').read_bytes()
proof=json.loads((R/'build/sequence-refresh/vm-verification.json').read_text())
assert proof['passed'] and proof['library_sha256']==sha(lib)
files['autorun/phone-input.so']=lib
launcher=old['autorun/mod-launcher.sh'].decode()
assert launcher.count('SAVE_IO_SHA='+sha(old['autorun/phone-input.so']))==1
files['autorun/mod-launcher.sh']=launcher.replace('SAVE_IO_SHA='+sha(old['autorun/phone-input.so']),'SAVE_IO_SHA='+sha(lib)).encode()
entry=old['autorun/autorun.sh'].decode()
assert entry.count(sha(old['autorun/mod-launcher.sh']))==1
assert entry.count('revision=r22b-poly-seq')==1
files['autorun/autorun.sh']=entry.replace(sha(old['autorun/mod-launcher.sh']),sha(files['autorun/mod-launcher.sh'])).replace('revision=r22b-poly-seq','revision=r22c-seq-lights').encode()
manifest=json.loads(old['autorun/autorun.json']);script=manifest['value0']['scripts'][0]
script['message']='Start r22c: brighter Seq colours, empty slots and playback pulse. Mod Off saves a separate SD project copy.'
script['yesButtonText']='Start r22c'
files['autorun/autorun.json']=(json.dumps(manifest,indent=2)+'\n').encode()
files['autorun/autorun.check']=sha(files['autorun/autorun.json']+files['autorun/autorun.sh']+b'f7bed1c9-7e90-4731-9194-c996666b4331').encode()
files['README_RU.md']=files['R22C_RU.md']=(R/'R22C_RU.md').read_bytes()
changed={n for n in files if files[n]!=old.get(n)}
assert changed=={'autorun/phone-input.so','autorun/mod-launcher.sh','autorun/autorun.sh','autorun/autorun.json','autorun/autorun.check','README_RU.md','R22C_RU.md'}
for n,b in files.items():
    if n.startswith('autorun/'):
        p=O/n;p.parent.mkdir(exist_ok=True);p.write_bytes(b)
for n in ['autorun.sh','mod-launcher.sh']:subprocess.run(['sh','-n',str(O/'autorun'/n)],check=True)
archive=R.parent/'mpc-sample-r22c-seq-lights.zip'
if not archive.exists():
    with zipfile.ZipFile(archive,'x',zipfile.ZIP_DEFLATED) as z:
        for n,b in sorted(files.items()):z.writestr(n,b)
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None and set(z.namelist())==set(files)
    assert all(z.read(n)==b for n,b in files.items())
sources=list(json.loads((R/'build/r22b/build-info.json').read_text())['sources_sha256'])
sources+=['package_r22c.py','R22C_RU.md','check_sequence_rgb.c','check_music_lights.c','check_pad_eq_ui.c','build_poly_candidate.py','lab/check_sequence_lights.py']
info=json.loads((R/'build/r22b/build-info.json').read_text())
info.update(revision='r22c-seq-lights',zip=archive.name,zip_sha256=sha(archive.read_bytes()),
    source_commit=subprocess.check_output(['git','-c','safe.directory='+str(R.parent),'rev-parse','HEAD'],text=True).strip(),
    library_sha256=sha(lib),sources_sha256={n:sha((R/n).read_bytes()) for n in sources},
    payload_sha256={n[8:]:sha(b) for n,b in files.items() if n.startswith('autorun/')},
    native_vm_verification=proof,card_written=False,hardware_tested=False)
(O/'build-info.json').write_text(json.dumps(info,indent=2)+'\n')
(O/'verification.json').write_text(json.dumps(dict(passed=True,library_sha256=sha(lib),sources_sha256=info['sources_sha256'],vm=proof,changed_files=sorted(changed),hardware_tested=False,card_written=False),indent=2)+'\n')
print(json.dumps(dict(revision=info['revision'],zip_sha256=info['zip_sha256'],library_sha256=sha(lib),changed=sorted(changed)),indent=2))
