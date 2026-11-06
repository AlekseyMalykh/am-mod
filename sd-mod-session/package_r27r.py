"""Package Am MOD r27r over the published r27q ZIP (run in WSL from sd-mod-session).

Run after build_chords_candidate.py (phone-input.so) and verify_early_drm.py
(boot-splash with the r27r version text). Only phone-input.so, boot-splash and
the files that pin their hashes change."""
from pathlib import Path
import hashlib,json,re,subprocess,sys,zipfile
R=Path(__file__).resolve().parent
sha=lambda b:hashlib.sha256(b).hexdigest()
base=Path(sys.argv[1]) if len(sys.argv)>1 else Path('/mnt/c/Users/Алексей/Desktop/akai mpc sample/am-mod-r27q.zip')
assert sha(base.read_bytes())=='1a37b93701a664acb5185def4f44c85e92cece8b14f421e8391ad049da7bde57','unexpected r27q base'
assert re.search(r'#define AM_MOD_VERSION "r27r"',(R/'src/mod_build.h').read_text())
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
entry=old['autorun/autorun.sh'].decode();assert entry.count(sha(old['autorun/mod-launcher.sh']))==1 and entry.count('revision=r27q')==1
files['autorun/autorun.sh']=entry.replace(sha(old['autorun/mod-launcher.sh']),sha(launcher.encode())).replace('revision=r27q','revision=r27r').encode()
meta=json.loads(old['autorun/autorun.json']);meta['value0']['scripts'][0]['message']='Am MOD r27r - lighting, Recall and search fixes'
files['autorun/autorun.json']=(json.dumps(meta,indent=2)+'\n').encode()
files['autorun/autorun.check']=sha(files['autorun/autorun.json']+files['autorun/autorun.sh']+b'f7bed1c9-7e90-4731-9194-c996666b4331').encode()
changed={n for n,b in files.items() if b!=old.get(n)}
assert changed=={'autorun/'+n for n in ['phone-input.so','boot-splash','mod-launcher.sh','autorun.sh','autorun.json','autorun.check']},changed
for name in ['autorun.sh','mod-launcher.sh']:subprocess.run(['sh','-n','/dev/stdin'],input=files['autorun/'+name],check=True)
O=R/'build/r27r';O.mkdir(parents=True,exist_ok=True);out=O/'am-mod-r27r.zip'
if out.exists():out.unlink()
with zipfile.ZipFile(out,'x',zipfile.ZIP_DEFLATED) as z:
 for n,b in sorted(files.items()):z.writestr(n,b)
sys.path.insert(0,str(R.parent/'installer'))
from mpc_mod_installer import Package
assert Package.from_file(out).revision=='r27r'
(O/'SHA256SUMS').write_text(sha(out.read_bytes())+'  am-mod-r27r.zip\n')
copy=Path('/mnt/c/Users/Алексей/Desktop/akai mpc sample/am-mod-r27r.zip')
if copy.parent.is_dir():copy.write_bytes(out.read_bytes());print('copy for the installer:',copy)
print('r27r package:',out,sha(out.read_bytes()))
