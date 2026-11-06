"""Lab-only ZIP: released r27r with the r27r+Mixer+Song UI library (WSL, from sd-mod-session).

Run after build_mixer_r27r.py. Only phone-input.so and the files pinning its
hash change; revision r27r-mix1. Not a release and not for the device yet."""
from pathlib import Path
import hashlib,json,subprocess,sys,zipfile
R=Path(__file__).resolve().parent
sha=lambda b:hashlib.sha256(b).hexdigest()
base=Path(sys.argv[1]) if len(sys.argv)>1 else Path('/mnt/c/Users/Алексей/Desktop/akai mpc sample/am-mod-r27r.zip')
assert sha(base.read_bytes())=='2b2012edb340e99c5610216becddaa0663b038885426692495469c620e9c25eb','unexpected r27r base'
proof=json.loads((R/'build/mixer-r27r/compile-verification.json').read_text());assert proof['passed']
for n,h in proof['sources_sha256'].items():assert sha((R/n).read_bytes())==h,('stale build, rebuild:',n)
library=(R/'build/mixer-r27r/phone-input.so').read_bytes();assert sha(library)==proof['library_sha256']
with zipfile.ZipFile(base) as z:assert z.testzip() is None;old={n:z.read(n) for n in z.namelist()}
files=dict(old);launcher=old['autorun/mod-launcher.sh'].decode()
previous=sha(old['autorun/phone-input.so']);assert launcher.count(previous)>=1
launcher=launcher.replace(previous,sha(library));files['autorun/phone-input.so']=library
files['autorun/mod-launcher.sh']=launcher.encode()
entry=old['autorun/autorun.sh'].decode()
assert entry.count(sha(old['autorun/mod-launcher.sh']))==1 and entry.count('revision=r27r')==1
files['autorun/autorun.sh']=entry.replace(sha(old['autorun/mod-launcher.sh']),sha(launcher.encode())).replace('revision=r27r','revision=r27r-mix1').encode()
meta=json.loads(old['autorun/autorun.json']);meta['value0']['scripts'][0]['message']='Am MOD r27r-mix1 - LAB ONLY: Mixer + Song UI'
files['autorun/autorun.json']=(json.dumps(meta,indent=2)+'\n').encode()
files['autorun/autorun.check']=sha(files['autorun/autorun.json']+files['autorun/autorun.sh']+b'f7bed1c9-7e90-4731-9194-c996666b4331').encode()
changed={n for n,b in files.items() if b!=old.get(n)}
assert changed=={'autorun/'+n for n in ['phone-input.so','mod-launcher.sh','autorun.sh','autorun.json','autorun.check']},changed
for name in ['autorun.sh','mod-launcher.sh']:subprocess.run(['sh','-n','/dev/stdin'],input=files['autorun/'+name],check=True)
O=R/'build/mixer-r27r';out=O/'am-mod-r27r-mix1.zip'
if out.exists():out.unlink()
with zipfile.ZipFile(out,'x',zipfile.ZIP_DEFLATED) as z:
    for n,b in sorted(files.items()):z.writestr(n,b)
(O/'SHA256SUMS').write_text(sha(out.read_bytes())+'  am-mod-r27r-mix1.zip\n'+sha(library)+'  phone-input.so\n')
print('LAB ZIP',out,sha(out.read_bytes()))
