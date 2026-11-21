"""Replay the shipping scope audit, including a real isolated D.S.B bind tree."""
from pathlib import Path
import hashlib,json,re,subprocess,sys,tempfile,zipfile
from bootloader_mount_scope import transform,PREAMBLE
R=Path(__file__).resolve().parent;O=R/'build/bootloader-mount-scope';O.mkdir(exist_ok=True)
base=R/'build/r30b-user-fixes-dev7/am-mod-r30b-user-fixes-dev7.zip'
with zipfile.ZipFile(base) as z:original=z.read('autorun/mod-launcher.sh').decode()
script=transform(original);assert transform(script)==script
subprocess.run(['sh','-n'],input=script,text=True,check=True)
program=re.search(r"awk -v boot_config=.*?'\n(.*?)\n    ' /proc/self/mountinfo",script,re.S)[1]
def rows(save,fs):return [
 '10 1 179:12 / /data rw - ext4 /dev/internal rw',
 '11 10 0:26 /data-image /data rw - tmpfs tmpfs rw',
 f'12 1 8:1 /Projects {save}/Projects rw - {fs} /dev/card rw',
 f'13 1 8:1 /Samples {save}/Samples rw - {fs} /dev/card rw',
 f'14 11 8:1 /TempProjects /data/TempProjects rw - {fs} /dev/card rw',
 f'15 11 8:1 /Export /data/UserContent/Samples/User/Export rw - {fs} /dev/card rw',
 '16 11 179:12 /UserContent /data/UserContent ro - ext4 /dev/internal ro']
def scope(lines,save='/media/MPC/MPC-Sample',fs='exfat',hidden=0):
 config=str(Path(save).parent/'autorun/os/mpcqol.conf')
 return subprocess.run(['awk','-v','save='+save,'-v','save_fs='+fs,'-v','data_type=tmpfs','-v','data_id=11','-v','boot_config='+config,'-v','boot_config_hidden='+str(hidden),program],input='\n'.join(lines)+'\n',text=True,capture_output=True)
checks=[]
for fs in ('exfat','vfat'):
 normal=rows('/media/MPC/MPC-Sample',fs)
 alias=f'551 1 8:1 /mpcqol.conf /media/MPC/autorun/os/mpcqol.conf rw,nosuid,nodev,noexec - {fs} /dev/card rw'
 for label,lines,hidden,expected in [
  ('plain',normal,0,True),('reported DSB hidden config',normal+[alias],1,True),
  ('visible writable config rejected',normal+[alias],0,False),
  ('duplicate alias rejected',normal+[alias,alias],1,False),
  ('other config rejected',normal+[alias.replace('mpcqol.conf','foreign.conf')],1,False),
  ('writable internal rejected',normal+['17 11 179:12 /UserContent/Projects /data/UserContent/Projects rw - ext4 /dev/internal rw'],1,False),
  ('wrong config filesystem rejected',normal+[alias.replace(' - '+fs+' ',' - xfs ')],1,False),
  ('missing workspace rejected',normal[:4]+normal[5:]+[alias],1,False)]:
  p=scope(lines,fs=fs,hidden=hidden);assert (p.returncode==0)==expected,(label,p.stdout)
  checks.append(fs+': '+label)
with tempfile.TemporaryDirectory(prefix='am-dsb-scope-') as temporary:
 root=Path(temporary);save=root/'card/MPC-Sample';save.mkdir(parents=True)
 config=save.parent/'autorun/os/mpcqol.conf'
 def hidden():
  source='save_path='+str(save)+'\n'+PREAMBLE+'printf "%s\\n" "$boot_config_hidden"\n'
  return int(subprocess.check_output(['sh'],input=source,text=True).strip())
 assert hidden()==1
 config.parent.mkdir(parents=True);assert hidden()==0
 config.parent.rmdir();config.parent.symlink_to(root/'missing');assert hidden()==0
 checks.append('real shell predicate rejects existing and dangling-symlink parents')
if '--namespace' in sys.argv:
 with tempfile.TemporaryDirectory(prefix='am-dsb-mount-') as temporary:
  root=Path(temporary);card=root/'card';save=card/'MPC-Sample';save.mkdir(parents=True)
  autorun=card/'autorun';app=autorun/'os/AM-Mod';app.mkdir(parents=True)
  config=autorun/'os/mpcqol.conf';config.write_text('original copied configuration')
  original_config=card/'mpcqol.conf';original_config.write_text('user configuration')
  subprocess.run(['mount','--bind',str(original_config),str(config)],check=True)
  fs=next(line.split(' - ')[1].split()[0] for line in Path('/proc/self/mountinfo').read_text().splitlines() if line.split()[4]==str(config))
  actual=next(line for line in Path('/proc/self/mountinfo').read_text().splitlines() if line.split()[4]==str(config))
  assert scope(rows(str(save),fs)+[actual],str(save),fs,0).returncode!=0
  subprocess.run(['mount','--bind',str(app),str(autorun)],check=True)
  assert not config.parent.exists()
  assert scope(rows(str(save),fs)+[actual],str(save),fs,1).returncode==0
  subprocess.run(['umount',str(autorun)],check=True)
  assert config.read_text()=='user configuration'
  assert scope(rows(str(save),fs)+[actual],str(save),fs,0).returncode!=0
  subprocess.run(['umount',str(config)],check=True)
  assert original_config.read_text()=='user configuration' and config.read_text()=='original copied configuration'
  checks.append('real private mount namespace: DSB config bind is rejected visible, accepted hidden, rejected when revealed; both config files unchanged')
sources={n:hashlib.sha256((R/n).read_bytes()).hexdigest() for n in ['bootloader_mount_scope.py',Path(__file__).name]}
report=dict(passed=True,checks=checks,real_namespace_tested='--namespace' in sys.argv,sources_sha256=sources,hardware_tested=False)
(O/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
(O/'launcher.sh').write_text(script)
print(json.dumps(report,indent=2))
