"""Package verified r30b-user-fixes-dev7 changes over the pinned r30b-program-import-dev6 card, without SD writes."""
from pathlib import Path
import hashlib,json,subprocess,sys,zipfile
from package_bootloader_compat import add_bootloader_entry
R=Path(__file__).resolve().parent;O=R/'build/r30b-user-fixes-dev7';O.mkdir(exist_ok=True)
sha=lambda data:hashlib.sha256(data).hexdigest()
sys.path.insert(0,str(R.parent/'installer'));from mpc_mod_installer import Package,AUTORUN_CHECK_SALT
base=R/'build/r30b-program-import-dev6/am-mod-r30b-program-import-dev6.zip';assert sha(base.read_bytes())=='7612c265d7ad0170e1b21844833c4ce78ef9eba2f991da47ef4a1535a7a43758'
proofs={}
for name,path in [('build','build/mixer-r27r/compile-verification.json'),('browser','build/browser-audition/verification.json'),('fx','build/fx-layout/verification.json'),('boot','build/bootloader-compat/entry-verification.json'),('piano','build/piano-roll/verification.json'),('palette','build/global-palette/verification.json'),('lighting','build/pad-lighting/verification.json'),('overlay','build/sample-overlay/verification.json')]:
 p=json.loads((R/path).read_text());assert p.get('passed') or p.get('compiled')
 for n,h in p['sources_sha256'].items():assert sha((R/n).read_bytes())==h,n
 proofs[name]=p
with zipfile.ZipFile(base) as z:files={n:z.read(n) for n in z.namelist() if not n.endswith('/')}
old_launcher=files['autorun/mod-launcher.sh'];launcher=old_launcher
library=(R/'build/mixer-r27r/phone-input.so').read_bytes();assert sha(library)==proofs['build']['library_sha256']
for name,data in [('phone-input.so',library)]:
 old=sha(files['autorun/'+name]).encode();assert old in launcher;files['autorun/'+name]=data;launcher=launcher.replace(old,sha(data).encode())
for name,source in [('boot-splash','src/early_drm_splash.c'),('usb-drive-screen','src/usb_drive_screen.c')]:
 target=O/name;subprocess.run(['aarch64-linux-gnu-gcc','-std=c11','-O2','-static','-Wall','-Wextra','-Werror',source,'-o',str(target)],cwd=R,check=True)
 data=target.read_bytes();old=sha(files['autorun/'+name]).encode();assert old in launcher;files['autorun/'+name]=data;launcher=launcher.replace(old,sha(data).encode())
files['autorun/mod-launcher.sh']=launcher
entry=files['autorun/autorun.sh'];assert sha(old_launcher).encode() in entry
files['autorun/autorun.sh']=entry.replace(sha(old_launcher).encode(),sha(launcher).encode()).replace(b'revision=r30b-program-import-dev6',b'revision=r30b-user-fixes-dev7')
manifest=json.loads(files['autorun/autorun.json']);manifest['value0']['scripts'][0]['message']='Am MOD r30b-user-fixes-dev7'
files['autorun/autorun.json']=(json.dumps(manifest,indent=2)+'\n').encode();files=add_bootloader_entry(files)
files['README.ru.md']=(R.parent/'docs/RELEASE_NOTES-r30b-user-fixes-dev7.ru.md').read_bytes()
files['BOOTLOADER_README.txt']=b'D.S.B Boot: copy autorun contents to autorun/os/AM-Mod/. Keep sampleboot.inplace. AM installer 1.4+ detects D.S.B automatically. Physical testing is pending.\n'
source_names=set(proofs['build']['sources_sha256'])
for proof in proofs.values():source_names.update(proof['sources_sha256'])
source_names.update(['src/bootloader_entry.inc.sh','package_user_fixes.py','package_bootloader_compat.py','audit_release_lineage.py','run_branch_audit.py','check_fx_layout.c','src/piano_roll_native.inc','check_piano_roll_native.c'])
for n in sorted(source_names):
 files['SOURCE/r30b-user-fixes-dev7/'+n]=(R/n).read_bytes()
for name,proof in proofs.items():files['SOURCE/r30b-user-fixes-dev7/'+name+'-verification.json']=(json.dumps(proof,indent=2)+'\n').encode()
for n in ['audio.json','features.json','device.json','lineage.json']:
 data=(R/'build/branch-audit'/n).read_bytes();assert json.loads(data)['passed']
 files['SOURCE/r30b-user-fixes-dev7/branch-audit/'+n]=data
files['BRANCH_AUDIT.ru.md']=(R.parent/'docs/BRANCH_AUDIT-dev7.ru.md').read_bytes()
for n in ['autorun.sh','mod-launcher.sh']:subprocess.run(['sh','-n','/dev/stdin'],input=files['autorun/'+n],check=True)
target=O/'am-mod-r30b-user-fixes-dev7.zip'
with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED) as z:
 for n,data in sorted(files.items()):z.writestr(n,data)
pkg=Package.from_file(target);assert pkg.revision=='r30b-user-fixes-dev7'
report=dict(passed=True,revision='r30b-user-fixes-dev7',base_sha256=sha(base.read_bytes()),package_sha256=pkg.sha256,library_sha256=sha(library),payload_sha256={n:sha(data) for n,data in files.items() if n.startswith('autorun/')},hardware_tested=False,card_written=False)
(O/'package-verification.json').write_text(json.dumps(report,indent=2)+'\n');(O/'SHA256SUMS.txt').write_text(pkg.sha256+'  '+target.name+'\n');print(target,pkg.sha256)
