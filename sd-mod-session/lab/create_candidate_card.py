"""WSL-only: build a fresh 2 GB exFAT lab image with a candidate package.

  sudo python3 lab/create_candidate_card.py build/r27q-fix2/am-mod-r27q-fix2.zip fix2-card

Regular lab file under /opt/mpc-sample-lab/out/run only; never a physical disk.
Requires exfatprogs, exfat-fuse and fdisk (same as build/recall-audit/create_final_card.py)."""
from pathlib import Path
import os,re,shutil,subprocess,sys
sys.path.insert(0,'/opt/mpc-sample-lab')
import lab
if os.getuid()!=0:raise SystemExit('Run with sudo inside the lab WSL distro')
package,name=sys.argv[1],sys.argv[2]
assert re.fullmatch(r'[a-z0-9-]+',name),'image name: lowercase letters, digits, dashes'
root=Path('/opt/mpc-sample-lab/out/run').resolve()
image=root/(name+'.img');mount=root/(name+'-mount')
if image.exists() or image.is_symlink():raise SystemExit(f'{image} already exists; choose another name')
card=Path(lab.unpack_autorun(package,str(root/(name+'-package'))))
with image.open('xb') as f:f.truncate(2<<30)
subprocess.run(['sfdisk',str(image)],input='label: dos\nstart=2048, type=7\n',text=True,check=True,capture_output=True)
loop=subprocess.check_output(['losetup','--find','--show','--offset','1048576',str(image)],text=True).strip()
if not re.fullmatch(r'/dev/loop[0-9]+',loop):raise RuntimeError('Unexpected loop device')
if Path(subprocess.check_output(['losetup','-n','-O','BACK-FILE',loop],text=True).strip()).resolve()!=image:raise RuntimeError('Loop device is not the lab image')
mounted=False
try:
 subprocess.run(['mkfs.exfat','-L','MPCSAMPLE',loop],check=True)
 mount.mkdir(exist_ok=True)
 if list(mount.iterdir()) or os.path.ismount(mount):raise RuntimeError('Mount directory is not empty')
 subprocess.run(['mount.exfat-fuse',loop,str(mount)],check=True);mounted=True
 shutil.copytree(card,mount,dirs_exist_ok=True);os.sync()
 subprocess.run(['umount',str(mount)],check=True);mounted=False
finally:
 if mounted:subprocess.run(['umount',str(mount)],check=False)
 if not os.path.ismount(mount):subprocess.run(['losetup','--detach',loop],check=True)
print(f'Virtual SD ready: {image}')
