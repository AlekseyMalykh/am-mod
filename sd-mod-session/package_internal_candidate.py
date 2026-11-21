"""Private test candidate for persistent Internal storage, not the public r30c release."""
from pathlib import Path
import hashlib,json,subprocess,sys,zipfile
from bootloader_mount_scope import transform as boot_transform
from internal_storage_controller import transform as internal_transform
from usb_drive_boot_package import transform as usb_boot_transform
R=Path(__file__).resolve().parent;O=R/'build/r30c-internal-dev1';O.mkdir(exist_ok=True)
sys.path.insert(0,str(R.parent/'installer'));from mpc_mod_installer import Package,AUTORUN_CHECK_SALT
sha=lambda b:hashlib.sha256(b).hexdigest();version='r30c-internal-dev1'
base=R/'build/r30b-user-fixes-dev7/am-mod-r30b-user-fixes-dev7.zip'
assert sha(base.read_bytes())=='0882a3b10af9538d430b8daf6019302be2a4097a17a4308aef0bc23e00b3a0e3'
with zipfile.ZipFile(base) as z:files={n:z.read(n) for n in z.namelist() if not n.endswith('/')}
proofs={}
for name,path in [('build','build/mixer-r27r/compile-verification.json'),('internal','build/internal-storage/verification.json'),('controller','build/internal-controller/verification.json'),('flush','build/internal-flush/verification.json'),('save','build/early-startup/internal-save-verification.json'),('recovery','build/project-recovery/verification.json'),('usb-boot','build/usb-drive-boot/verification.json'),('usb-boot-shared','build/usb-drive-boot/shared-verification.json')]:
 p=json.loads((R/path).read_text());assert p['passed'],name
 for n,h in p['sources_sha256'].items():assert sha((R/n).read_bytes())==h,n
 proofs[name]=p
assert proofs['controller']['real_namespace_tested']
assert proofs['usb-boot-shared']['shared_mounts']
old_launcher=files['autorun/mod-launcher.sh'];launcher=old_launcher
library=(R/'build/mixer-r27r/phone-input.so').read_bytes();assert sha(library)==proofs['build']['library_sha256']
new_assets={'phone-input.so':library}
for target,source in [('stage-data','src/stage_data.c'),('boot-splash','src/early_drm_splash.c'),('usb-drive-screen','src/usb_drive_screen.c')]:
 binary=O/target;subprocess.run(['aarch64-linux-gnu-gcc','-std=c11','-O2','-static','-Wall','-Wextra','-Werror',source,'-o',str(binary)],cwd=R,check=True)
 new_assets[target]=binary.read_bytes()
for n,b in new_assets.items():
 old_hash=sha(files['autorun/'+n]).encode();assert old_hash in launcher,n
 launcher=launcher.replace(old_hash,sha(b).encode());files['autorun/'+n]=b
drive=(R/'src/usb_drive.sh').read_bytes();boot=(R/'src/usb_drive_boot.sh').read_bytes()
files['autorun/usb-drive.sh']=drive;files['autorun/usb-drive-boot.sh']=boot
launcher=usb_boot_transform(internal_transform(boot_transform(launcher.decode())),sha(drive),sha(boot)).encode();files['autorun/mod-launcher.sh']=launcher
entry=files['autorun/autorun.sh'];assert sha(old_launcher).encode() in entry
entry=entry.replace(sha(old_launcher).encode(),sha(launcher).encode()).replace(b'revision=r30b-user-fixes-dev7',('revision='+version).encode())
files['autorun/autorun.sh']=entry
manifest=json.loads(files['autorun/autorun.json']);manifest['value0']['scripts'][0]['message']='AM Mod '+version
files['autorun/autorun.json']=(json.dumps(manifest,indent=2)+'\n').encode()
files['autorun/autorun.check']=sha(files['autorun/autorun.json']+entry+AUTORUN_CHECK_SALT).encode()
for n in ['autorun.sh','mod-launcher.sh']:subprocess.run(['sh','-n'],input=files['autorun/'+n],check=True)
files['CANDIDATE.txt']=b'Internal storage development candidate. Internal user Projects and Samples/User persist; system data remains private/RAM and factory user content read-only. Public r30c docs and remaining features are not finalized. Physical testing pending.\n'
source_names={'src/internal_storage_controller.inc.sh','internal_storage_controller.py','bootloader_mount_scope.py','package_internal_candidate.py'}
for name,p in proofs.items():
 source_names.update(p['sources_sha256']);files['SOURCE/'+version+'/'+name+'-verification.json']=(json.dumps(p,indent=2)+'\n').encode()
for n in sorted(source_names):files['SOURCE/'+version+'/'+n]=(R/n).read_bytes()
target=O/('am-mod-'+version+'.zip')
with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED) as z:
 for n,b in sorted(files.items()):z.writestr(n,b)
pkg=Package.from_file(target);assert pkg.revision==version
report=dict(passed=True,version=version,package_sha256=pkg.sha256,library_sha256=sha(library),stage_data_sha256=sha(new_assets['stage-data']),native_vm_tested=False,hardware_tested=False,card_written=False)
(O/'package-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(target,pkg.sha256)
