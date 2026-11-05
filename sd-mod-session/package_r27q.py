"""Package the r27q test candidate over the verified r27p ZIP.

Run in WSL after:
  python3 build_chords_candidate.py   # build/chords/phone-input.so + compile-verification.json
  python3 verify_early_drm.py         # build/early-drm/boot-splash + verification.json
Only phone-input.so, boot-splash and the files that pin their hashes change."""
from pathlib import Path
import hashlib,json,re,subprocess,sys,zipfile
R=Path(__file__).resolve().parent
sha=lambda b:hashlib.sha256(b).hexdigest()
base=R/'build/r27p-simple-routing/am-mod-r27p.zip'
assert sha(base.read_bytes())=='7effb3e544892bfbf703a98fae7b5fa33ae8b912318af6e455fd82fb89c667cb','unexpected r27p base'
assert re.search(r'#define AM_MOD_VERSION "r27q"',(R/'src/mod_build.h').read_text())
app=json.loads((R/'build/chords/compile-verification.json').read_text());drm=json.loads((R/'build/early-drm/verification.json').read_text())
assert app['passed'] and drm['passed']
for proof in (app,drm):
 for n,h in proof['sources_sha256'].items():assert sha((R/n).read_bytes())==h,('stale build, rebuild:',n)
library=(R/'build/chords/phone-input.so').read_bytes();splash=(R/'build/early-drm/boot-splash').read_bytes()
assert sha(library)==app['library_sha256'] and sha(splash)==drm['splash_sha256']
with zipfile.ZipFile(base) as z:assert z.testzip() is None;old={n:z.read(n) for n in z.namelist()}
files=dict(old);launcher=old['autorun/mod-launcher.sh'].decode()
for name,data,count in [('phone-input.so',library,1),('boot-splash',splash,2)]:
 previous=sha(old['autorun/'+name]);assert launcher.count(previous)==count,(name,launcher.count(previous))
 launcher=launcher.replace(previous,sha(data));files['autorun/'+name]=data
files['autorun/mod-launcher.sh']=launcher.encode()
entry=old['autorun/autorun.sh'].decode();assert entry.count(sha(old['autorun/mod-launcher.sh']))==1 and entry.count('revision=r27p')==1
files['autorun/autorun.sh']=entry.replace(sha(old['autorun/mod-launcher.sh']),sha(launcher.encode())).replace('revision=r27p','revision=r27q').encode()
meta=json.loads(old['autorun/autorun.json']);meta['value0']['scripts'][0]['message']='Am MOD r27q - Degrees lights, search and window polish; test candidate'
files['autorun/autorun.json']=(json.dumps(meta,indent=2)+'\n').encode()
files['autorun/autorun.check']=sha(files['autorun/autorun.json']+files['autorun/autorun.sh']+b'f7bed1c9-7e90-4731-9194-c996666b4331').encode()
changed={n for n,b in files.items() if b!=old.get(n)}
assert changed=={'autorun/'+n for n in ['phone-input.so','boot-splash','mod-launcher.sh','autorun.sh','autorun.json','autorun.check']},changed
for name in ['autorun.sh','mod-launcher.sh']:subprocess.run(['sh','-n','/dev/stdin'],input=files['autorun/'+name],check=True)
O=R/'build/r27q';O.mkdir(exist_ok=True);out=O/'am-mod-r27q.zip'
if out.exists():out.unlink()
with zipfile.ZipFile(out,'x',zipfile.ZIP_DEFLATED) as z:
 for n,b in sorted(files.items()):z.writestr(n,b)
sys.path.insert(0,str(R.parent/'installer'))
from mpc_mod_installer import Package
package=Package.from_file(out);assert package.revision=='r27q'
(O/'SHA256SUMS').write_text(sha(out.read_bytes())+'  am-mod-r27q.zip\n')
copy=R.parent.parent.parent/'am-mod-r27q.zip'
copy.write_bytes(out.read_bytes())
print('r27q package:',out,sha(out.read_bytes()))
print('copy for the installer:',copy)
