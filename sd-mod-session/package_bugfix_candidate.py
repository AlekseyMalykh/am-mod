"""Package the r27q-fixN candidate named in src/mod_build.h over the published r27q payload; preserve splash artwork."""
from pathlib import Path
import zipfile,json,hashlib,subprocess,sys,re
r=Path(__file__).resolve().parent
rev=re.search(r'#define AM_MOD_VERSION "(r27q-fix[0-9]+)"',(r/'src/mod_build.h').read_text()).group(1)
base=r.parent.parent.parent/'Desktop/akai mpc sample/am-mod-r27q.zip'
# Explicit reproducible base supplied by the caller.
base=Path(sys.argv[1]);out=Path(sys.argv[2]);sha=lambda b:hashlib.sha256(b).hexdigest()
assert sha(base.read_bytes())=='1a37b93701a664acb5185def4f44c85e92cece8b14f421e8391ad049da7bde57'
proof=json.loads((r/'build/chords/compile-verification.json').read_text());assert proof['passed']
for n,h in proof['sources_sha256'].items():assert sha((r/n).read_bytes())==h,('stale build',n)
lib=(r/'build/chords/phone-input.so').read_bytes();assert sha(lib)==proof['library_sha256']
with zipfile.ZipFile(base) as z:assert z.testzip() is None;old={n:z.read(n) for n in z.namelist()}
files=dict(old);launch=old['autorun/mod-launcher.sh'].decode();previous=sha(old['autorun/phone-input.so']);assert launch.count(previous)==1
files['autorun/phone-input.so']=lib;files['autorun/mod-launcher.sh']=launch.replace(previous,sha(lib)).encode()
entry=old['autorun/autorun.sh'].decode();assert entry.count(sha(old['autorun/mod-launcher.sh']))==1
files['autorun/autorun.sh']=entry.replace(sha(old['autorun/mod-launcher.sh']),sha(files['autorun/mod-launcher.sh'])).replace('revision=r27q','revision='+rev).encode()
meta=json.loads(old['autorun/autorun.json']);meta['value0']['scripts'][0]['message']='Am MOD '+rev+' - feedback fixes; device test candidate'
files['autorun/autorun.json']=(json.dumps(meta,indent=2)+'\n').encode()
files['autorun/autorun.check']=sha(files['autorun/autorun.json']+files['autorun/autorun.sh']+b'f7bed1c9-7e90-4731-9194-c996666b4331').encode()
changed=sorted(n for n in files if files[n]!=old[n]);assert changed==sorted('autorun/'+n for n in ['phone-input.so','mod-launcher.sh','autorun.sh','autorun.json','autorun.check'])
assert files['autorun/boot-splash']==old['autorun/boot-splash']
for n in ['autorun.sh','mod-launcher.sh']:subprocess.run(['sh','-n','/dev/stdin'],input=files['autorun/'+n],check=True)
out.parent.mkdir(parents=True,exist_ok=True)
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED) as z:
 for n,b in sorted(files.items()):z.writestr(n,b)
with zipfile.ZipFile(out) as z:assert z.testzip() is None
sys.path.insert(0,str(r.parent/'installer'));from mpc_mod_installer import Package
assert Package.from_file(out).revision==rev
(out.parent/'SHA256SUMS.txt').write_text(sha(out.read_bytes())+'  '+out.name+'\n')
(out.parent/'build-verification.json').write_text(json.dumps({'base':'r27q','candidate':rev,'zip_sha256':sha(out.read_bytes()),'library_sha256':sha(lib),'changed_payload':changed,'splash_binary_unchanged':True,'compile':proof},indent=2)+'\n')
print(out,sha(out.read_bytes()))
