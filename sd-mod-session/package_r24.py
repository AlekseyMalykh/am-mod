"""Freeze Chords over the immutable r23a release. Never accesses an SD."""
from pathlib import Path
import hashlib,json,subprocess,zipfile
R=Path(__file__).resolve().parent;O=R/'build/r24';O.mkdir(exist_ok=True)
sha=lambda b:hashlib.sha256(b).hexdigest()
C=R/'build/chords'
compile_proof=json.loads((C/'compile-verification.json').read_text())
tests=json.loads((C/'verification.json').read_text())
for proof in (compile_proof,tests):
 assert proof['passed']
 for n,h in proof['sources_sha256'].items():assert sha((R/n).read_bytes())==h,n
assert tests['ubsan'] and compile_proof['full_preload_constructor']
base=R.parent/'mpc-sample-r23a-padlink-render.zip'
assert sha(base.read_bytes())=='5274f78f2e4e1bc6feae980b90f9cac64821903bc44650e4934a207634c0a4b1'
with zipfile.ZipFile(base) as z:
 assert z.testzip() is None
 old={n:z.read(n) for n in z.namelist()}
files=dict(old);lib=(C/'phone-input.so').read_bytes()
assert sha(lib)==compile_proof['library_sha256']
files['autorun/phone-input.so']=lib
launcher=old['autorun/mod-launcher.sh'].decode()
key='SAVE_IO_SHA='+sha(old['autorun/phone-input.so']);assert launcher.count(key)==1
files['autorun/mod-launcher.sh']=launcher.replace(key,'SAVE_IO_SHA='+sha(lib)).encode()
entry=old['autorun/autorun.sh'].decode();previous=sha(old['autorun/mod-launcher.sh'])
assert entry.count(previous)==1 and entry.count('revision=r23a-padlink-render')==1
entry=entry.replace(previous,sha(files['autorun/mod-launcher.sh'])).replace('revision=r23a-padlink-render','revision=r24-chords-trial')
files['autorun/autorun.sh']=entry.encode()
manifest=json.loads(old['autorun/autorun.json']);script=manifest['value0']['scripts'][0]
script['message']='Start r24 Chords trial: Sample / 16 Levels Tune / Chop. Set Orig Note and Play Poly first.'
script['yesButtonText']='Start r24'
files['autorun/autorun.json']=(json.dumps(manifest,indent=2)+'\n').encode()
files['autorun/autorun.check']=sha(files['autorun/autorun.json']+files['autorun/autorun.sh']+b'f7bed1c9-7e90-4731-9194-c996666b4331').encode()
files['README_RU.md']=files['R24_RU.md']=(R/'R24_RU.md').read_bytes()
changed=sorted(n for n,b in files.items() if old.get(n)!=b)
assert set(changed)=={'autorun/phone-input.so','autorun/mod-launcher.sh','autorun/autorun.sh','autorun/autorun.json','autorun/autorun.check','README_RU.md','R24_RU.md'}
for n,b in files.items():
 if n.startswith('autorun/'):
  p=O/n;p.parent.mkdir(exist_ok=True);p.write_bytes(b)
for n in ('autorun.sh','mod-launcher.sh'):subprocess.run(['sh','-n',str(O/'autorun'/n)],check=True)
archive=R.parent/'mpc-sample-r24-chords-trial.zip'
if not archive.exists():
 with zipfile.ZipFile(archive,'x',zipfile.ZIP_DEFLATED) as z:
  for n,b in sorted(files.items()):z.writestr(n,b)
with zipfile.ZipFile(archive) as z:assert z.testzip() is None and set(z.namelist())==set(files) and all(z.read(n)==b for n,b in files.items())
sources=set(compile_proof['sources_sha256'])|set(tests['sources_sha256'])|{'package_r24.py','R24_RU.md'}
hashes={n:sha((R/n).read_bytes()) for n in sorted(sources)}
info=json.loads((R/'build/r23a/build-info.json').read_text())
info.update(revision='r24-chords-trial',zip=archive.name,zip_sha256=sha(archive.read_bytes()),
 source_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
 library_sha256=sha(lib),sources_sha256=hashes,
 payload_sha256={n[8:]:sha(b) for n,b in files.items() if n.startswith('autorun/')},
 full_vm_verified=False,hardware_tested=False,card_written=False,
 predecessor_device_confirmed=['r23a_render_padlink_bypass','r23a_poly_separate_events','r23a_power_button_shutdown','r23a_card_in_boot','r23a_orig_note_reopen'],
 compile_verification=compile_proof,chords_verification=tests)
proof=dict(passed=True,scope=tests['scope'],library_sha256=sha(lib),sources_sha256=hashes,
 hardware_tested=False,full_vm_verified=False,card_written=False,changed_files=changed)
(O/'build-info.json').write_text(json.dumps(info,indent=2)+'\n')
(O/'verification.json').write_text(json.dumps(proof,indent=2)+'\n')
print(json.dumps(dict(revision=info['revision'],library_sha256=sha(lib),zip_sha256=info['zip_sha256'],hardware_tested=False,card_written=False),indent=2))
