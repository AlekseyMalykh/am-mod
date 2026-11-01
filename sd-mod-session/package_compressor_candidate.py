"""Local trial package only; never accesses a physical card."""
from pathlib import Path
import hashlib,json,zipfile
R=Path(__file__).resolve().parent;O=R/'build/compressor'
sha=lambda b:hashlib.sha256(b).hexdigest()
compiled=json.loads((O/'compile-verification.json').read_text())
tests=json.loads((O/'verification.json').read_text())
for proof in (compiled,tests):
 assert proof['passed']
 for n,h in proof['sources_sha256'].items():assert sha((R/n).read_bytes())==h,n
assert compiled['full_preload_constructor'] and tests['ubsan']
assert sha((O/'phone-input.so').read_bytes())==compiled['library_sha256']
assert compiled['base_revision']=='r26d'
base=O/'base/am-mod-r26d.zip'
assert sha(base.read_bytes())=='63c7b248f6f0acbecc170cc91f16af1df6fd771e74b593c5a57c4c98a44d87f8'
with zipfile.ZipFile(base) as z:
 assert z.testzip() is None
 old={n:z.read(n) for n in z.namelist()}
files=dict(old);files['autorun/phone-input.so']=(O/'phone-input.so').read_bytes()
launcher=old['autorun/mod-launcher.sh'].decode();needle='SAVE_IO_SHA='+sha(old['autorun/phone-input.so']);assert launcher.count(needle)==1
files['autorun/mod-launcher.sh']=launcher.replace(needle,'SAVE_IO_SHA='+sha(files['autorun/phone-input.so'])).encode()
entry=old['autorun/autorun.sh'].decode();old_sha=sha(old['autorun/mod-launcher.sh']);assert entry.count(old_sha)==1
revision='r26d-compressor-type-trial';assert entry.count('revision=r26d')==1
files['autorun/autorun.sh']=entry.replace(old_sha,sha(files['autorun/mod-launcher.sh'])).replace('revision=r26d','revision='+revision).encode()
manifest=json.loads(old['autorun/autorun.json']);script=manifest['value0']['scripts'][0]
script['message']='Start r26d Type trial? Shift+Pad 5 > B2 Type: Duck / OptoCat. OptoCat K1 Threshold, K2 Ratio, K3 Gain; B3 Bypass.'
script['yesButtonText']='Start trial'
files['autorun/autorun.json']=(json.dumps(manifest,indent=2)+'\n').encode()
files['autorun/autorun.check']=sha(files['autorun/autorun.json']+files['autorun/autorun.sh']+b'f7bed1c9-7e90-4731-9194-c996666b4331').encode()
files['README_RU.md']=files['COMPRESSOR_TYPE_RU.md']=(R/'COMPRESSOR_TYPE_RU.md').read_bytes()
changed={n for n in old if old[n]!=files[n]}
assert changed=={'autorun/phone-input.so','autorun/mod-launcher.sh','autorun/autorun.sh',
                 'autorun/autorun.json','autorun/autorun.check','README_RU.md'},changed
archive=R.parent/'am-mod-r26d-compressor-type-trial.zip'
if archive.exists():
 with zipfile.ZipFile(archive) as z:
  assert z.testzip() is None and set(z.namelist())==set(files) and all(z.read(n)==b for n,b in files.items()),'Trial exists with other contents; keep it immutable'
else:
 with zipfile.ZipFile(archive,'x',zipfile.ZIP_DEFLATED) as z:
  for n,b in sorted(files.items()):z.writestr(n,b)
live_path=O/'live/verification.json'
live=json.loads(live_path.read_text()) if live_path.exists() else None
if live:assert live['passed'] and live['library_sha256']==compiled['library_sha256']
proof=dict(revision=revision,base_revision='r26d',base_zip_sha256=sha(base.read_bytes()),zip=archive.name,zip_sha256=sha(archive.read_bytes()),
 library_sha256=sha(files['autorun/phone-input.so']),hardware_tested=False,card_written=False,
 unchanged_base_files_sha256={n:sha(b) for n,b in old.items() if n not in changed},
 full_vm_verified=bool(live),live_verification=live,compile_verification=compiled,verification=tests)
(O/'build-info.json').write_text(json.dumps(proof,indent=2)+'\n')
print(json.dumps({k:proof[k] for k in ('revision','zip','zip_sha256','hardware_tested','card_written')},indent=2))
