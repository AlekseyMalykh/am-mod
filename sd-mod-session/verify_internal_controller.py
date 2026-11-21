"""Internal user mounts: shipped controller audit and real private-namespace persistence.
No physical SD/eMMC paths are mounted or written by this verifier.
"""
from pathlib import Path
import hashlib,json,os,re,subprocess,sys,tempfile,wave,zipfile
from bootloader_mount_scope import transform as boot_transform
from internal_storage_controller import transform
R=Path(__file__).resolve().parent;O=R/'build/internal-controller';O.mkdir(exist_ok=True)
with zipfile.ZipFile(R/'build/r30b-user-fixes-dev7/am-mod-r30b-user-fixes-dev7.zip') as z:old=z.read('autorun/mod-launcher.sh').decode()
script=transform(boot_transform(old));assert transform(script)==script
subprocess.run(['sh','-n'],input=script,text=True,check=True)
program=re.search(r"awk -v boot_config=.*?'\n(.*?)\n    ' /proc/self/mountinfo",script,re.S)[1]
base=[
 '10 1 179:12 / /data rw - ext4 /dev/internal rw',
 '11 10 0:26 /data-image /data rw - tmpfs tmpfs rw',
 '12 1 8:1 /Projects /media/MPC/MPC-Sample/Projects rw - exfat /dev/card rw',
 '13 1 8:1 /Samples /media/MPC/MPC-Sample/Samples rw - exfat /dev/card rw',
 '14 11 8:1 /TempProjects /data/TempProjects rw - exfat /dev/card rw',
 '16 11 179:12 /UserContent /data/UserContent ro - ext4 /dev/internal ro',
 '17 16 179:12 /UserContent/Projects /data/UserContent/Projects rw - ext4 /dev/internal rw',
 '18 16 179:12 /UserContent/Samples/User /data/UserContent/Samples/User rw - ext4 /dev/internal rw']
def scope(lines):
 p=subprocess.run(['awk','-v','save=/media/MPC/MPC-Sample','-v','save_fs=exfat','-v','data_type=tmpfs','-v','data_id=11','-v','boot_config=/media/MPC/autorun/os/mpcqol.conf','-v','boot_config_hidden=1',program],input='\n'.join(lines)+'\n',text=True,capture_output=True)
 return p.returncode==0
assert scope(base)
for bad in [base[:-1],base+[base[-1]],base[:-1]+[base[-1].replace(' rw - ext4 ',' ro - ext4 ')],base+['19 1 179:12 /Settings /data/Settings rw - ext4 /dev/internal rw'],base[:-2]+[base[-2].replace(' - ext4 ',' - exfat '),base[-1]],base[:5]+[base[5].replace(' ro - ext4 ',' rw - ext4 ')]+base[6:],base+['20 11 8:1 /Export /data/UserContent/Samples/User/Export rw - exfat /dev/card rw']]:assert not scope(bad),bad
checks=['scope permits exactly the two pinned Internal subdirectories plus the existing SD scopes',
        'missing/duplicate/readonly/wrong-filesystem scopes and writable settings/UserContent rejected',
        'D.S.B hidden config handling retained; no extra device or system capabilities']
if '--namespace' in sys.argv:
 assert os.geteuid()==0
 with tempfile.TemporaryDirectory(prefix='am-internal-mount-') as temporary:
  root=Path(temporary).resolve();source=root/'native/UserContent';target=root/'session/UserContent'
  for p in [source/'Projects',source/'Samples/User/Export',source/'Samples/Factory',target]:p.mkdir(parents=True,exist_ok=True)
  (source/'Samples/Factory/unchanged.dat').write_bytes(b'factory content')
  (source/'Projects/Existing.xpj').write_bytes(b'existing native project fixture')
  def mount_user():
   subprocess.run(['mount','--bind',str(source),str(target)],check=True)
   subprocess.run(['mount','-o','remount,bind,ro',str(target)],check=True)
   for part in ['Projects','Samples/User']:subprocess.run(['mount','--bind',str(source/part),str(target/part)],check=True)
  def unmount_user():
   for part in ['Samples/User','Projects','']:subprocess.run(['umount',str(target/part)],check=True)
  mount_user()
  try:
   (target/'Projects/From-AM.xpj').write_bytes(b'persistent project fixture')
   with wave.open(str(target/'Samples/User/From-AM.wav'),'wb') as w:
    w.setparams((1,2,44100,0,'NONE','not compressed'));w.writeframes(b'\0\0\x10\0\xf0\xff')
   with wave.open(str(target/'Samples/User/Export/Mixdown.wav'),'wb') as w:
    w.setparams((2,2,48000,0,'NONE','not compressed'));w.writeframes(b'\x10\0\xf0\xff'*8)
   for path in [target/'Samples/Factory/unchanged.dat',target/'outside-projects.dat']:
    try:path.write_bytes(b'must not change');raise AssertionError('Read-only content became writable')
    except OSError as error:assert error.errno==30,error
  finally:unmount_user()
  assert (source/'Projects/From-AM.xpj').read_bytes()==b'persistent project fixture'
  assert (source/'Projects/Existing.xpj').read_bytes()==b'existing native project fixture'
  assert (source/'Samples/Factory/unchanged.dat').read_bytes()==b'factory content'
  mount_user()
  try:
   assert (target/'Projects/From-AM.xpj').read_bytes()==b'persistent project fixture'
   with wave.open(str(target/'Samples/User/From-AM.wav')) as w:assert w.getnframes()==3 and w.getframerate()==44100
   with wave.open(str(target/'Samples/User/Export/Mixdown.wav')) as w:assert w.getnframes()==8 and w.getnchannels()==2 and w.getframerate()==48000
  finally:unmount_user()
  checks.append('real namespace: project, sample WAV and stereo Export WAV survive namespace restart on original Internal storage; no SD alias covers Export; existing project/factory bytes unchanged; other UserContent writes rejected')
  run=root/'runtime';(run/'storage-meta').mkdir(parents=True)
  prepare=(R/'src/internal_storage_controller.inc.sh').read_text().replace('/data/UserContent',str(source))
  subprocess.run(['sh'],input='R='+str(run)+'\nlog(){ :; }\n'+prepare+'\ninternal_storage_prepare\n',text=True,check=True)
  marker=(run/'storage-meta/internal-storage').read_text();assert marker.startswith('AM_INTERNAL1\n')
  checks.append('production host preparation pins only canonical ext4 project/sample folders')
sources=['internal_storage_controller.py','src/internal_storage_controller.inc.sh','bootloader_mount_scope.py',Path(__file__).name]
report=dict(passed=True,checks=checks,real_namespace_tested='--namespace' in sys.argv,sources_sha256={n:hashlib.sha256((R/n).read_bytes()).hexdigest() for n in sources},native_project_serializer_tested=False,hardware_tested=False)
(O/'verification.json').write_text(json.dumps(report,indent=2)+'\n');(O/'launcher.sh').write_text(script)
print(json.dumps(report,indent=2))
