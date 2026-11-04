"""Package a verified palette/loop candidate over its immutable predecessor."""
from pathlib import Path
import argparse,hashlib,json,re,subprocess,sys,zipfile
from verify_callbacks import verify
R=Path(__file__).resolve().parent
sha=lambda b:hashlib.sha256(b).hexdigest()
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--revision',choices=['r27h','r27i','r27j','r27k','r27l','r27m','r27n'],default='r27n')
parser.add_argument('--base',type=Path)
args=parser.parse_args()
revision=args.revision
previous,expected={
 'r27h':('r27g','2c7d8b74c3a7f7a64fd8970f4ae93affb698827446871487a13c84abbd2d4176'),
 'r27i':('r27h','a70dd42f7c91c665f48c221595f23ca1e0109b8024077e5514c561ecf6b6868f'),
 'r27j':('r27i','108826b3aa80e95e851763d106b9d370eb309cec6c86abcf3fcb14fde8b6b89f'),
 'r27k':('r27j','c5af0b62c6703b7f1e6796c2be459c014ec5c99cc42c17084d2c916903291c5e'),
 'r27l':('r27k','15f4e35d26d3440ebc1880ceff78419517dc8a9a8906230a6284445737e3a33e'),
 'r27m':('r27l','e7b3c079d20639b94827ddf3b8a4d6e8e16016f6b577fcddc0716b8bef57f55c'),
 'r27n':('r27m','fa5876d043dc3c22e17a5f6c2e066d9996b040cd9792447228be8e4aecb81735'),
}[revision]
args.base=args.base or R/('build/'+previous+'/am-mod-'+previous+'.zip')
O=R/('build/'+revision);O.mkdir(exist_ok=True)
assert sha(args.base.read_bytes())==expected
assert re.search(r'#define AM_MOD_VERSION "'+revision+'"',(R/'src/mod_build.h').read_text())
proofs={}
proof_paths={'app':'build/chords/compile-verification.json','settings':'build/pad-lighting/verification.json',
 'recovery':'build/project-recovery/verification.json','startup':'build/early-startup/verification.json',
 'colours':'build/pad-colour-rgb/verification.json','controls':'build/pad-colour-controls/verification.json',
 'voices':'build/sample-playing/verification.json','ui':'build/pad-eq/verification.json',
 'drm':'build/early-drm/verification.json','branding':'build/early-drm/branding-verification.json'}
if revision in ('r27k','r27l','r27m','r27n'):proof_paths['steps']='build/step-toggle/verification.json'
if revision=='r27n':proof_paths['preview']='build/project-restore/verification.json'
for name,path in proof_paths.items():
 p=json.loads((R/path).read_text());assert p['passed'],name
 for n,h in p['sources_sha256'].items():assert sha((R/n).read_bytes())==h,(name,n)
 proofs[name]=p
assert proofs['app']['compressor_included'] and proofs['app']['native_takeover_preserved']
callbacks=verify(R/'build/chords/phone-input.so');assert callbacks['callbacks_verified']==51
assert 'SAMPLE_PLAYING NATIVE_LIGHTING_OWNER_SNAPSHOT_INSTALLED' in (R/'build/chords/bootstrap.log').read_text()
with zipfile.ZipFile(args.base) as z:assert z.testzip() is None;old={n:z.read(n) for n in z.namelist()}
files=dict(old);launcher=old['autorun/mod-launcher.sh'].decode()
for name,path,key,field,count in [('phone-input.so','build/chords/phone-input.so','app','library_sha256',1),
 ('boot-splash','build/early-drm/boot-splash','drm','splash_sha256',2)]:
 data=(R/path).read_bytes();assert sha(data)==proofs[key][field]
 previous_hash=sha(old['autorun/'+name]);assert launcher.count(previous_hash)==count
 launcher=launcher.replace(previous_hash,sha(data));files['autorun/'+name]=data
files['autorun/mod-launcher.sh']=launcher.encode()
entry=old['autorun/autorun.sh'].decode();assert entry.count(sha(old['autorun/mod-launcher.sh']))==1 and entry.count('revision='+previous)==1
files['autorun/autorun.sh']=entry.replace(sha(old['autorun/mod-launcher.sh']),sha(launcher.encode())).replace('revision='+previous,'revision='+revision).encode()
message={'r27k':'loop and Shift lighting, Steps length and pad range',
         'r27l':'distinct muted and unmuted Steps pads',
         'r27m':'consistent Sample mute colours, waveform and spectrum',
         'r27n':'Stop + pad end preview for Sample and Chop'}
meta=json.loads(old['autorun/autorun.json']);meta['value0']['scripts'][0]['message']='AM Mod '+revision+' - '+message.get(revision,'eight pad colours, active loop feedback and saved project names')+'; test candidate'
files['autorun/autorun.json']=(json.dumps(meta,indent=2)+'\n').encode()
files['autorun/autorun.check']=sha(files['autorun/autorun.json']+files['autorun/autorun.sh']+b'f7bed1c9-7e90-4731-9194-c996666b4331').encode()
changed={n for n,b in files.items() if n.startswith('autorun/') and b!=old.get(n)}
assert changed=={'autorun/'+n for n in ['phone-input.so','boot-splash','mod-launcher.sh','autorun.sh','autorun.json','autorun.check']}
assert files['autorun/temp-workspace']==old['autorun/temp-workspace']
for name in ['autorun.sh','mod-launcher.sh']:subprocess.run(['sh','-n','/dev/stdin'],input=files['autorun/'+name],check=True)
for name in ['docs/RELEASE_NOTES-'+revision+'.ru.md','docs/KNOWN_ISSUES.ru.md','docs/MOD_SETTINGS.ru.md']:
 files[name]=(R.parent/name).read_bytes()
out=O/('am-mod-'+revision+'.zip')
with zipfile.ZipFile(out,'x',zipfile.ZIP_DEFLATED) as z:
 for n,b in sorted(files.items()):z.writestr(n,b)
sys.path.insert(0,str(R.parent/'installer'))
from mpc_mod_installer import Package
package=Package.from_file(out);assert package.revision==revision and package.filesystems=={'exfat','vfat'}
report=dict(passed=True,revision=revision,hardware_tested=False,published=False,package=out.name,
 package_sha256=sha(out.read_bytes()),payload_sha256={n:sha(b) for n,b in package.files.items()},proofs=proofs,callbacks=callbacks,
 user_colours=8,palette_size=14,previous_palette_migration=True,project_settings_version=2,
 source_sha256={n:sha((R/n).read_bytes()) for n in ['package_palette_recovery.py','src/project_recovery.h','src/sample_playing.h']})
(O/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(dict(revision=revision,sha256=report['package_sha256'],payload_files=len(package.files),hardware_tested=False),indent=2))
