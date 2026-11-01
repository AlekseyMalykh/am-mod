"""Filesystem admission and write-boundary regressions. No physical card.
Uses production shell guards with fixture mount rows; native flush/saving
on real filesystem images and full native app startup are separate checks.
"""
from pathlib import Path
import hashlib,json,subprocess,tempfile
R=Path(__file__).resolve().parent;O=R/'build/filesystems';O.mkdir(parents=True,exist_ok=True)
names=['src/autorun.template.sh','src/entry.template.sh','src/power_audit.sh','src/collect_startup.sh','src/stage_data.c','verify_filesystems.py']
sha=lambda b:hashlib.sha256(b).hexdigest();hashes={n:sha((R/n).read_bytes()) for n in names}
script=(R/'src/autorun.template.sh').read_text()
for n in names:
 if n.endswith('.sh'):subprocess.run(['sh','-n',str(R/n)],check=True)
awk=script[script.index('    awk -v save='):script.index("    ' /proc/self/mountinfo",script.index('    awk -v save='))+6]
for fs in ['exfat','vfat']:
 def rows(sd_fs=fs,extra=''):
  return '10 1 0:1 / /data rw - tmpfs tmpfs rw\n'+f'11 1 0:1 / /data rw - overlay overlay rw\n12 1 1:1 /Projects /media/MPC/MPC-Sample/Projects rw - {sd_fs} /dev/test rw\n13 1 1:1 /Samples /media/MPC/MPC-Sample/Samples rw - {sd_fs} /dev/test rw\n'+extra
 with tempfile.TemporaryDirectory() as d:
  p=Path(d)/'mountinfo'
  def check(body,want):
   p.write_text(body);s='save_path=/media/MPC/MPC-Sample\nsave_fs='+fs+'\ndata_type=overlayfs\ndata_mount_id=11\n'+awk+' '+str(p)
   r=subprocess.run(['sh','-c',s],capture_output=True,text=True);assert (r.returncode==0)==want,(fs,r.stdout,r.stderr)
  check(rows(),True)
  check(rows('vfat' if fs=='exfat' else 'exfat'),False)
  check(rows(extra='14 1 1:2 / /unexpected rw - vfat /dev/other rw\n'),False)
  check(rows().replace('/Samples rw','/Samples ro'),False)
  check(rows().replace('/Projects','/Unapproved'),False)
  check(rows().replace(' - '+fs+' ', ' - ext4 '),False)
print('PASS exFAT/FAT32 sandbox mount scope: exactly two writable save folders; filesystem changes, read-only/missing folders and extra disks refused')
for name in ['entry.template.sh','power_audit.sh','collect_startup.sh']:
 text=(R/'src'/name).read_text();line=next(s for s in text.splitlines() if s.startswith('awk -v p="$SD"'))
 # Split at the shell fallback, not the OR inside the quoted AWK predicate.
 guard=line[:line.index(' /proc/mounts')+len(' /proc/mounts')]
 for fs,rw,want in [('exfat','rw',True),('vfat','rw',True),('vfat','ro',False),('ext4','rw',False),('ntfs','rw',False)]:
  with tempfile.NamedTemporaryFile(mode='w') as p:
   p.write('/dev/test /media/MPC '+fs+' '+rw+' 0 0\n');p.flush()
   s='SD=/media/MPC\n'+guard.replace('/proc/mounts',p.name)
   assert (subprocess.run(['sh','-c',s]).returncode==0)==want,(name,fs,rw)
print('PASS native entry/power/startup collectors accept exFAT and FAT32 read-write mounts only')
assert script.index('> "$R/storage-meta/sd-filesystem"')<script.index('checkpoint JOURNAL_HELPER_READY')
assert hashes=={n:sha((R/n).read_bytes()) for n in names}
(O/'guard-verification.json').write_text(json.dumps(dict(passed=True,sources_sha256=hashes,hardware_tested=False),indent=2)+'\n')
