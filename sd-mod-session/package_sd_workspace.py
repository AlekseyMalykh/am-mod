"""Package the r27g SD-workspace test candidate from the public r27f ZIP."""
from pathlib import Path
import hashlib,json,re,subprocess,sys,zipfile
from sd_workspace import transform
from verify_callbacks import verify
R=Path(__file__).resolve().parent;O=R/'build/r27g';O.mkdir(exist_ok=True)
sha=lambda b:hashlib.sha256(b).hexdigest()
assert re.search(r'#define AM_MOD_VERSION "r27g"',(R/'src/mod_build.h').read_text())
proofs={}
for name,path in {'sd':'build/sd-workspace/verification.json','app':'build/chords/compile-verification.json',
 'drm':'build/early-drm/verification.json','branding':'build/early-drm/branding-verification.json'}.items():
 p=json.loads((R/path).read_text());assert p['passed']
 for n,h in p['sources_sha256'].items():assert sha((R/n).read_bytes())==h,(name,n)
 proofs[name]=p
assert proofs['sd']['native_reserve_unchanged'] and proofs['sd']['mount_scope_verified']
assert proofs['app']['compressor_included'] and proofs['app']['native_takeover_preserved']
callbacks=verify(R/'build/chords/phone-input.so');assert callbacks['callbacks_verified']==51
base=R.parent/'am-mod-r27f.zip';assert sha(base.read_bytes())=='dd047009d4d7f2a13d0ace6c1f57787b7d94ae686a7251303f4d4fdb0c6922e1'
with zipfile.ZipFile(base) as z:old={n:z.read(n) for n in z.namelist()};assert z.testzip() is None
files=dict(old);helper=(R/'build/sd-workspace/temp-workspace').read_bytes();assert sha(helper)==proofs['sd']['helper_sha256']
files['autorun/temp-workspace']=helper
launcher=transform(old['autorun/mod-launcher.sh'].decode(),sha(helper))
for name,path,key,field,count in [('phone-input.so','build/chords/phone-input.so','app','library_sha256',1),
 ('boot-splash','build/early-drm/boot-splash','drm','splash_sha256',2)]:
 data=(R/path).read_bytes();assert sha(data)==proofs[key][field]
 previous=sha(old['autorun/'+name]);assert launcher.count(previous)==count
 launcher=launcher.replace(previous,sha(data));files['autorun/'+name]=data
assert 'MemoryMax=800M' in launcher and 'native reserve400MiB' in launcher
files['autorun/mod-launcher.sh']=launcher.encode()
entry=old['autorun/autorun.sh'].decode();assert entry.count(sha(old['autorun/mod-launcher.sh']))==1 and entry.count('revision=r27f')==1
files['autorun/autorun.sh']=entry.replace(sha(old['autorun/mod-launcher.sh']),sha(launcher.encode())).replace('revision=r27f','revision=r27g').encode()
manifest=json.loads(old['autorun/autorun.json']);manifest['value0']['scripts'][0]['message']='AM Mod r27g - SD temporary audio workspace; test candidate'
files['autorun/autorun.json']=(json.dumps(manifest,indent=2)+'\n').encode()
files['autorun/autorun.check']=sha(files['autorun/autorun.json']+files['autorun/autorun.sh']+b'f7bed1c9-7e90-4731-9194-c996666b4331').encode()
allowed={'autorun/'+n for n in ['temp-workspace','phone-input.so','boot-splash','mod-launcher.sh','autorun.sh','autorun.json','autorun.check']}
assert {n for n,b in files.items() if n.startswith('autorun/') and b!=old.get(n)}==allowed
for name in ['autorun.sh','mod-launcher.sh']:subprocess.run(['sh','-n','/dev/stdin'],input=files['autorun/'+name],check=True)
files['docs/RELEASE_NOTES-r27g.ru.md']=(R.parent/'docs/RELEASE_NOTES-r27g.ru.md').read_bytes()
files['docs/KNOWN_ISSUES.ru.md']=(R.parent/'docs/KNOWN_ISSUES.ru.md').read_bytes()
out=O/'am-mod-r27g.zip'
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED) as z:
 for n,b in sorted(files.items()):z.writestr(n,b)
sys.path.insert(0,str(R.parent/'installer'))
from mpc_mod_installer import Package
package=Package.from_file(out);assert package.revision=='r27g'
report=dict(passed=True,revision='r27g',hardware_tested=False,published=False,package=out.name,package_sha256=sha(out.read_bytes()),
 payload_sha256={n:sha(b) for n,b in package.files.items()},proofs=proofs,callbacks=callbacks,
 native_reserve_unchanged=True,memory_limit_unchanged=True)
(O/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(dict(revision='r27g',package_sha256=report['package_sha256'],payload_files=len(package.files),published=False),indent=2))
