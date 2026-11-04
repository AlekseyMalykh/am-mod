"""ARM workspace helper, FAT32 binds, native space/copy regression, mount scope.

Run as WSL root. No physical card, firmware service or native app main is used.
"""
from pathlib import Path
import hashlib,json,os,re,shutil,subprocess,sys,tempfile
R=Path(__file__).resolve().parent
O=R/'build/sd-workspace';O.mkdir(exist_ok=True)
STOCK=R.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
# Avoid the firmware sysroot's own /data overriding the namespace fixture via
# QEMU's -L path prefix. Cross libc has no /data tree and retains ARM ABI.
SYSROOT='/usr/aarch64-linux-gnu'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def run(*args,**kw):
    result=subprocess.run([str(a) for a in args],cwd=R,**kw)
    if result.returncode and kw.get('capture_output'):
        print(result.stdout,result.stderr,flush=True)
    result.check_returncode();return result
def arm(exe,*args,**kw):return run('qemu-aarch64','-L',SYSROOT,exe,*args,**kw)
def namespace(folder):
    folder=Path(folder);media=Path('/media/AM-SD-TEST');fixed=Path('/run/mpc-clone-trial-v1');data=Path('/data')
    assert not fixed.exists() and not media.exists()
    media.mkdir();created_data=not data.exists()
    if created_data:data.mkdir()
    mounted=[]
    try:
        image=folder/'card.img'
        with image.open('wb') as f:f.truncate(2<<30)
        run('mkfs.vfat','-F','32',image,stdout=subprocess.DEVNULL)
        run('mount','-t','vfat','-o','loop',image,media);mounted.append(media)
        samples=media/'MPC-Sample/Samples';samples.mkdir(parents=True)
        original=folder/'original';seed=original/'TempProjects';seed.mkdir(parents=True)
        (seed/'Temp-Project.xpj').write_text('original stock temp project')
        original_audio=seed/'Temp-Project_[ProjectData]';original_audio.mkdir()
        (original_audio/'original.wav').write_bytes(b'original seed audio')
        run('mount','--bind',original,data);mounted.append(data)
        fixed.mkdir();meta=fixed/'storage-meta';meta.mkdir()
        identity=lambda p:str(p.stat().st_dev)+':'+str(p.stat().st_ino)+'\n'
        for name,value in {'save-path':str(media/'MPC-Sample')+'\n','sd-filesystem':'vfat\n',
          'save-identity':identity(media/'MPC-Sample'),'save-Samples-identity':identity(samples),'original-data-identity':identity(data)}.items():
            (meta/name).write_text(value)
        old=folder/'old-ram';old.mkdir();run('mount','-t','tmpfs','-o','size=512M','tmpfs',old);mounted.append(old)
        with (old/'fill').open('wb') as f:
            for _ in range(93):f.write(bytes(1<<20))
        audio=folder/'Sample 001.wav'
        with audio.open('wb') as f:f.truncate(47680506)
        before=arm(O/'check-native-space',STOCK,audio,old/'copied.wav',0,capture_output=True,text=True)
        prepared=arm(O/'temp-workspace',capture_output=True,text=True)
        work=Path(prepared.stdout.strip());assert work==samples/'.am-mod-workspace/temp-projects-v1'
        assert (work/'Temp-Project.xpj').read_text()=='original stock temp project'
        assert (work/'Temp-Project_[ProjectData]/original.wav').read_bytes()==b'original seed audio'
        run('mount','--bind',work,data/'TempProjects');mounted.append(data/'TempProjects')
        assert identity(work)==identity(data/'TempProjects')
        after=arm(O/'check-native-space',STOCK,audio,data/'TempProjects/Temp-Project_[ProjectData]/copied.wav',1,capture_output=True,text=True)
        assert sha(audio)==sha(work/'Temp-Project_[ProjectData]/copied.wav')
        (work/'Temp-Project.xpj').write_text('new mod project')
        arm(O/'temp-workspace',capture_output=True,text=True)
        assert (work/'Temp-Project.xpj').read_text()=='new mod project'
        assert (seed/'Temp-Project.xpj').read_text()=='original stock temp project'
        (folder/'native-space.log').write_text(before.stdout+after.stdout)
        (folder/'namespace-result.json').write_text(json.dumps(dict(passed=True,fat32=True,
          bind_identity_verified=True,seed_preserved=True,existing_cache_preserved=True,native_reserve_unchanged=True,
          native_copy_sha256=sha(audio),native_log=before.stdout+after.stdout),indent=2))
    finally:
        for mount in reversed(mounted):subprocess.run(['umount',str(mount)],check=True)
        if fixed.exists():shutil.rmtree(fixed)
        media.rmdir()
        if created_data:data.rmdir()
if '--namespace' in sys.argv:
    namespace(sys.argv[-1]);sys.exit(0)
assert os.geteuid()==0
sources=['src/temp_workspace_store.c','src/temp_workspace_store.h','src/temp_workspace_sd.c',
    'check_temp_workspace_store.c','check_temp_space_native.c','sd_workspace.py','verify_sd_workspace.py']
hashes={n:sha(R/n) for n in sources}
common=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard']
run('aarch64-linux-gnu-gcc',*common,'src/temp_workspace_sd.c','src/temp_workspace_store.c','-o',O/'temp-workspace')
run('aarch64-linux-gnu-gcc',*common,'check_temp_space_native.c','-ldl','-lm','-o',O/'check-native-space')
for mode,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
    exe=O/('store-'+mode)
    run('aarch64-linux-gnu-gcc',*common,*flags,'check_temp_workspace_store.c','src/temp_workspace_store.c','-o',exe)
    with tempfile.TemporaryDirectory(prefix='am-store-test-') as folder:
        result=arm(exe,folder,capture_output=True,text=True)
        assert result.stdout.startswith('PASS ');(O/(mode+'.log')).write_text(result.stdout+result.stderr)
with tempfile.TemporaryDirectory(prefix='am-sd-bind-test-') as folder:
    run('unshare','--mount','--propagation','private','python3',__file__,'--namespace',folder)
    proof=json.loads((Path(folder)/'namespace-result.json').read_text())
    (O/'native-space.log').write_text((Path(folder)/'native-space.log').read_text())
from sd_workspace import transform
import zipfile
with zipfile.ZipFile(R.parent/'am-mod-r27f.zip') as z:launcher=transform(z.read('autorun/mod-launcher.sh').decode(),sha(O/'temp-workspace'))
(O/'launcher-transformed.sh').write_text(launcher);run('sh','-n',O/'launcher-transformed.sh')
match=re.search(r"awk -v save=.*?'\n(.*?)\n    ' /proc/self/mountinfo",launcher,re.S);assert match
rows=['10 1 0:50 / /data rw - overlay overlay rw',
 '11 1 8:1 /MPC-Sample/Projects /media/AM-SD-TEST/MPC-Sample/Projects rw - vfat /dev/fixture rw',
 '12 1 8:1 /MPC-Sample/Samples /media/AM-SD-TEST/MPC-Sample/Samples rw - vfat /dev/fixture rw',
 '13 10 8:1 /MPC-Sample/Samples/.am-mod-workspace/temp-projects-v1 /data/TempProjects rw - vfat /dev/fixture rw']
def scope(lines):return subprocess.run(['awk','-v','save=/media/AM-SD-TEST/MPC-Sample','-v','save_fs=vfat','-v','data_type=overlayfs','-v','data_id=10',match.group(1)],input='\n'.join(lines)+'\n',text=True,capture_output=True).returncode
assert scope(rows)==0
assert scope(rows[:-1])!=0 and scope(rows+[rows[-1]])!=0
assert scope(rows+['14 1 8:2 / /other rw - ext4 /dev/other rw'])!=0
assert scope(rows[:-1]+[rows[-1].replace(' - vfat ',' - ext4 ')])!=0
assert hashes=={n:sha(R/n) for n in sources}
proof.update(passed=True,ubsan=True,mount_scope_verified=True,hardware_tested=False,exfat_hardware_tested=False,
    helper_sha256=sha(O/'temp-workspace'),sources_sha256=hashes)
(O/'verification.json').write_text(json.dumps(proof,indent=2)+'\n')
print(json.dumps(proof,indent=2))
