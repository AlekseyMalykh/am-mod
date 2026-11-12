"""Build a local routing/lighting candidate over the pinned r28d ZIP. No SD writes."""
from pathlib import Path
import argparse, hashlib, json, subprocess, sys, zipfile
from package_r28 import verified_asset, SALT
ROOT=Path(__file__).resolve().parent
VERSION="r28d-routing2"
BASE_SHA="7f79b668ddfcb2f79e914e8cde62f6951e059b8e3271de84c48cd8197221f7c4"
def sha(b):return hashlib.sha256(b).hexdigest()
def main():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument("base",type=Path)
 args=parser.parse_args()
 assert sha(args.base.read_bytes())==BASE_SHA,"Unexpected r28d base"
 assert f'#define AM_MOD_VERSION "{VERSION}"' in (ROOT/'src/mod_build.h').read_text()
 library=verified_asset(ROOT/'build/mixer-r27r','compile-verification.json','phone-input.so','library_sha256')
 with zipfile.ZipFile(args.base) as z:
  assert z.testzip() is None
  infos=z.infolist();files={i.filename:z.read(i) for i in infos}
 original=dict(files)
 files['autorun/phone-input.so']=library
 launcher=files['autorun/mod-launcher.sh'].decode()
 assert launcher.count(sha(original['autorun/phone-input.so']))==1
 files['autorun/mod-launcher.sh']=launcher.replace(sha(original['autorun/phone-input.so']),sha(library)).encode()
 assert '#define MIRROR_PORT 8081' in (ROOT/'src/screen_mirror.c').read_text(),'Screen Mirror port is not 8081'
 helper=ROOT/'build'/'screen-mirror';helper.parent.mkdir(exist_ok=True)
 subprocess.run(['aarch64-linux-gnu-gcc','-O2','-Wall','-Wextra','-Werror','-static',str(ROOT/'src/screen_mirror.c'),'-o',str(helper)],check=True)
 mirror=helper.read_bytes();mirror_old=sha(original['autorun/screen-mirror']);assert mirror!=original['autorun/screen-mirror']
 text=files['autorun/mod-launcher.sh'].decode();assert text.count(mirror_old)==2
 files['autorun/mod-launcher.sh']=text.replace(mirror_old,sha(mirror)).encode();files['autorun/screen-mirror']=mirror
 entry=files['autorun/autorun.sh'].decode()
 assert entry.count(sha(original['autorun/mod-launcher.sh']))==1 and entry.count('revision=r28d')==1
 files['autorun/autorun.sh']=entry.replace(sha(original['autorun/mod-launcher.sh']),sha(files['autorun/mod-launcher.sh'])).replace('revision=r28d',f'revision={VERSION}').encode()
 manifest=json.loads(files['autorun/autorun.json']);manifest['value0']['scripts'][0]['message']=f'Am MOD {VERSION} (test)'
 files['autorun/autorun.json']=(json.dumps(manifest,indent=2)+'\n').encode()
 files['autorun/autorun.check']=sha(files['autorun/autorun.json']+files['autorun/autorun.sh']+SALT).encode()
 for suffix in ('','.ru'):
  files[f'README{suffix}.md']=(ROOT.parent/'docs'/f'RELEASE_NOTES-{VERSION}{suffix}.md').read_bytes()
 for name in ('autorun.sh','mod-launcher.sh'):
  subprocess.run(['sh','-n','/dev/stdin'],input=files[f'autorun/{name}'],check=True)
 changed=sorted(n for n in files if n.startswith('autorun/') and files[n]!=original[n])
 assert changed==sorted('autorun/'+n for n in ('phone-input.so','mod-launcher.sh','autorun.sh','autorun.json','autorun.check','screen-mirror'))
 out=ROOT/'build'/VERSION;out.mkdir(exist_ok=True)
 target=out/f'am-mod-{VERSION}.zip'
 with zipfile.ZipFile(target,'x',zipfile.ZIP_DEFLATED) as z:
  for info in infos:z.writestr(info,files[info.filename])
 with zipfile.ZipFile(target) as z:assert z.testzip() is None
 sys.path.insert(0,str(ROOT.parent/'installer'))
 from mpc_mod_installer import Package
 package=Package.from_file(target);assert package.revision==VERSION
 for name in ('boot-splash','screen-mirror'):
  assert sha(files['autorun/'+name]) in files['autorun/mod-launcher.sh'].decode()
 digest=sha(target.read_bytes());(out/'SHA256SUMS.txt').write_text(f'{digest}  {target.name}\n')
 (out/'package-verification.json').write_text(json.dumps(dict(passed=True,revision=VERSION,base_sha256=BASE_SHA,package_sha256=digest,library_sha256=sha(library),changed_payload_files=changed,installer_parser=True,hardware_tested=False,card_written=False),indent=2)+'\n')
 print(target,digest)
if __name__=='__main__':main()
