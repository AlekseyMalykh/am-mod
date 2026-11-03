"""Package r27c from immutable small-text r27b, with fresh app/helper verification."""
from pathlib import Path
import hashlib,json,re,subprocess,zipfile
R=Path(__file__).resolve().parent;O=R/'build/r27c';O.mkdir(exist_ok=True)
sha=lambda b:hashlib.sha256(b).hexdigest()
version=re.search(r'#define AM_MOD_VERSION "([^"]+)"',(R/'src/mod_build.h').read_text()).group(1)
assert version=='r27c'
def checked(path):
 p=json.loads((R/path).read_text());assert p['passed'],path
 for n,h in p['sources_sha256'].items():assert sha((R/n).read_bytes())==h,(path,n)
 return p
proofs={n:checked(p) for n,p in {
 'app':'build/chords/compile-verification.json',
 'ui':'build/pad-eq/verification.json',
 'drm':'build/early-drm/verification.json',
 'branding':'build/early-drm/branding-verification.json',
 'recovery':'build/project-recovery/verification.json',
 'filters':'build/filter-spectrum/verification.json',
 'startup':'build/early-startup/verification.json',
 'cursor':'build/pad-colour-rgb/verification.json',
 'workflow':'build/project-restore/verification.json',
 'resample':'build/resample-stock/verification.json',
 'reset':'build/knob-reset/verification.json',
 'knobs':'build/knob-events/verification.json',
 'settings':'build/pad-lighting/verification.json',
 'compressor':'build/compressor/r26k/verification.json',
}.items()}
assert proofs['app']['full_preload_constructor'] and proofs['ui']['ubsan'] and proofs['drm']['master_release_verified']
base=R.parent/'am-mod-r27b.zip';assert sha(base.read_bytes())=='202cee85531200bc1f88ab7478aaecf709066edad1565fb23cbfeb55be4552d0'
with zipfile.ZipFile(base) as z:
 assert z.testzip() is None;old={n:z.read(n) for n in z.namelist() if not n.endswith('/')}
files=dict(old);launcher=old['autorun/mod-launcher.sh'].decode()
for name,path,key,field in [('phone-input.so','build/chords/phone-input.so','app','library_sha256'),('boot-splash','build/early-drm/boot-splash','drm','splash_sha256')]:
 data=(R/path).read_bytes();assert sha(data)==proofs[key][field]
 oldhash=sha(old['autorun/'+name]);assert launcher.count(oldhash)==(1 if name=='phone-input.so' else 2)
 launcher=launcher.replace(oldhash,sha(data));files['autorun/'+name]=data
files['autorun/mod-launcher.sh']=launcher.encode()
entry=old['autorun/autorun.sh'].decode();oldhash=sha(old['autorun/mod-launcher.sh'])
assert entry.count(oldhash)==1 and entry.count('revision=r27b')==1
files['autorun/autorun.sh']=entry.replace(oldhash,sha(launcher.encode())).replace('revision=r27b','revision='+version).encode()
manifest=json.loads(old['autorun/autorun.json']);manifest['value0']['scripts'][0]['message']='AM Mod '+version+' - autosave fix, configurable selection, Pad Bank knob reset; experimental FAT32'
files['autorun/autorun.json']=(json.dumps(manifest,indent=2)+'\n').encode()
files['autorun/autorun.check']=sha(files['autorun/autorun.json']+files['autorun/autorun.sh']+b'f7bed1c9-7e90-4731-9194-c996666b4331').encode()
changed=sorted(n for n,b in files.items() if n.startswith('autorun/') and b!=old[n])
assert set(changed)=={'autorun/phone-input.so','autorun/boot-splash','autorun/mod-launcher.sh','autorun/autorun.sh','autorun/autorun.json','autorun/autorun.check'}
# Refresh all shipped documentation, retaining no stale copies from the base ZIP.
for name in list(files):
 if not name.startswith('autorun/') and (R.parent/name).is_file():files[name]=(R.parent/name).read_bytes()
for path in [R.parent/'README.md',R.parent/'README.ru.md',R.parent/'CHANGELOG.md',*(R.parent/'docs').rglob('*')]:
 if path.is_file() and path.suffix.lower() in ('.md','.png','.gif','.svg'):
  files[path.relative_to(R.parent).as_posix()]=path.read_bytes()
files['README_RU.md']=files['README.ru.md']
for name in ('autorun.sh','mod-launcher.sh'):subprocess.run(['sh','-n','/dev/stdin'],input=files['autorun/'+name],check=True)
out=R.parent/('am-mod-'+version+'.zip');assert not out.exists(),out
with zipfile.ZipFile(out,'x',zipfile.ZIP_DEFLATED) as z:
 for n,b in sorted(files.items()):z.writestr(n,b)
with zipfile.ZipFile(out) as z:
 assert z.testzip() is None
 for n,b in files.items():assert z.read(n)==b,n
report=dict(passed=True,revision=version,package_sha256=sha(out.read_bytes()),library_sha256=sha(files['autorun/phone-input.so']),splash_sha256=sha(files['autorun/boot-splash']),changed_payload=changed,payload_sha256={n:sha(b) for n,b in files.items() if n.startswith('autorun/')},proofs=proofs,published=False,release_channel='prerelease',physical_r27a_splash_confirmed=True,physical_r27b_small_metadata_confirmed=True,physical_r27c_tested=False,card_written=False)
(O/'package-verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k not in ('proofs','payload_sha256')},indent=2))
