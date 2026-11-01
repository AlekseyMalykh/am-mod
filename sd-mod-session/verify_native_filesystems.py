"""Linux image integration test; root + a private mount namespace required.
Never formats a physical device. Uses only newly created regular image files.
Requires mkfs.exfat, mkfs.fat, qemu-aarch64 and the MPC ARM sysroot.
Usage: sudo unshare --mount --propagation private python3 verify_native_filesystems.py SYSROOT PACKAGE.zip
"""
from pathlib import Path
import hashlib,json,os,shutil,subprocess,sys,tempfile

R=Path(__file__).resolve().parent
def run(args,**kw):
    return subprocess.run(args,check=True,capture_output=True,text=True,**kw).stdout.strip()
assert os.geteuid()==0
assert Path('/proc/self/ns/mnt').readlink()!=Path('/proc/1/ns/mnt').readlink(), 'use unshare --mount'
sys.path.insert(0,str(R.parent/'installer'));import mpc_mod_installer as installer
helper=R/'build/filesystems/stage-data'
assert helper.is_file()
sysroot,package=map(Path,sys.argv[1:])
pkg=installer.Package(package.read_bytes(),package.name)
ram=Path('/run/mpc-clone-trial-v1');ram_existed=ram.exists();ram.mkdir(exist_ok=True)
run(['mount','-t','tmpfs','-o','size=8M,mode=0700','tmpfs',str(ram)])
meta=ram/'storage-meta';meta.mkdir()
script=(R/'src/autorun.template.sh').read_text()
start=script.index('storage_host_check() {');function=script[start:script.index('\n}\n',start)+3]
results=[]
try:
 with tempfile.TemporaryDirectory(prefix='mpc-filesystems-',dir='/var/tmp') as folder:
  root=Path(folder);os.environ['MPC_MOD_BACKUP_DIR']=str(root/'backups')
  for fs in ['exfat','vfat']:
   image=root/(fs+'.img')
   with image.open('xb') as f:f.truncate(512<<20)
   assert image.is_file() and not image.is_symlink() and image.resolve().parent==root.resolve()
   loop=run(['losetup','--find','--show',str(image)])
   mount=Path('/media/MPCFS'+fs.upper());mount_existed=mount.exists();mount.mkdir(exist_ok=True)
   mounted=False
   try:
    fmt=['mkfs.exfat','-L','MPCFS',loop] if fs=='exfat' else [shutil.which('mkfs.fat') or '/opt/mpc-fat32-check/tools/usr/sbin/mkfs.fat','-F','32','-n','MPCFS',loop]
    run(fmt);run(['mount','-o','iocharset=utf8',loop,str(mount)]);mounted=True
    assert run(['findmnt','-n','-o','FSTYPE','--target',str(mount)])==fs
    project=mount/'MPC-Sample/Projects/test проект.xpj';project.parent.mkdir(parents=True)
    project.write_bytes(b'unchanged user project\0\xff');(mount/'MPC-Sample/Samples').mkdir()
    card=installer.Card(mount,'MPCFS',fs)
    installer.install(pkg,card,lambda _:None);installer.install(pkg,card,lambda _:None)
    assert all((mount/'autorun'/name).read_bytes()==data for name,data in pkg.files.items())
    installer.remove(card,lambda _:None)
    assert project.read_bytes()==b'unchanged user project\0\xff' and not (mount/'autorun').exists()
    (mount/'autorun').mkdir()
    def metadata(name,value):(meta/name).write_text(str(value)+'\n')
    for mode,suffix,prefix in [('--flush-sd','MPC-Sample','save'),('--flush-journal','autorun','journal')]:
     path=mount/suffix;identity=run(['stat','-c','%d:%i',str(path)])
     metadata('save-path' if prefix=='save' else 'journal-dir',path)
     metadata(prefix+'-identity',identity);metadata('sd-filesystem',fs)
     args=['qemu-aarch64','-L',str(sysroot),str(helper),mode]
     assert 'FLUSHED' in run(args)
     for bad in ['ext4','vfat' if fs=='exfat' else 'exfat']:
      metadata('sd-filesystem',bad);assert subprocess.run(args,capture_output=True).returncode!=0
     metadata('sd-filesystem',fs);metadata(prefix+'-identity','0:0')
     assert subprocess.run(args,capture_output=True).returncode!=0
     metadata(prefix+'-identity',identity)
     (meta/'sd-filesystem').unlink();(meta/'sd-filesystem').symlink_to(meta/(prefix+'-identity'))
     assert subprocess.run(args,capture_output=True).returncode!=0
     (meta/'sd-filesystem').unlink();metadata('sd-filesystem',fs)
    metadata('sd-identity',run(['stat','-c','%d:%i',str(mount)]))
    metadata('save-path',mount/'MPC-Sample');metadata('save-identity',run(['stat','-c','%d:%i',str(mount/'MPC-Sample')]))
    for child in ['Projects','Samples']:
     metadata('save-'+child+'-identity',run(['stat','-c','%d:%i',str(mount/'MPC-Sample'/child)]))
    def host_check():
     return subprocess.run(['sh','-c','R='+str(ram)+'\n'+function+'\nstorage_host_check'],capture_output=True,text=True)
    result=host_check();assert result.returncode==0,result.stdout+result.stderr
    metadata('sd-filesystem','vfat' if fs=='exfat' else 'exfat');assert host_check().returncode!=0
    metadata('sd-filesystem',fs);run(['mount','-o','remount,ro',str(mount)])
    assert host_check().returncode!=0
    run(['mount','-o','remount,rw',str(mount)])
    results.append(dict(filesystem=fs,installer_install_reinstall_remove=True,user_project_preserved=True,arm_save_and_journal_syncfs=True,mismatched_type_and_identity_and_symlink_refused=True,host_mount_guard=True,read_only_refused=True))
    print('PASS real image:',fs,flush=True)
   finally:
    if mounted:run(['umount',str(mount)])
    run(['losetup','-d',loop])
    if not mount_existed:mount.rmdir()
finally:
 run(['umount',str(ram)])
 if not ram_existed:ram.rmdir()
report=dict(passed=True,package_sha256=hashlib.sha256(package.read_bytes()).hexdigest(),helper_sha256=hashlib.sha256(helper.read_bytes()).hexdigest(),tests=results,physical_card_tested=False)
(R/'build/filesystems/native-verification.json').write_text(json.dumps(report,indent=2)+'\n')
