"""WSL-only exFAT image builder. Uses one checked loop device for a regular lab file.
Never takes a physical disk path. Requires exfatprogs, exfat-fuse and fdisk.
"""
from pathlib import Path
import os, re, shutil, subprocess, sys
sys.path.insert(0, '/opt/mpc-sample-lab')
import lab

root=Path('/opt/mpc-sample-lab/out/run').resolve()
image=root/'lab-large-card.img'
mount=root/'large-card-mount'
if os.getuid()!=0:raise SystemExit('Run as root inside the lab WSL distro')
if image.exists() or image.is_symlink():raise SystemExit('Lab image already exists; refusing to replace it')
card=Path(lab.unpack_autorun(sys.argv[1],str(root/'large-card-package')))
with image.open('xb') as f:f.truncate(2<<30)
subprocess.run(['sfdisk',str(image)],input='label: dos\nstart=2048, type=7\n',text=True,check=True,capture_output=True)
loop=subprocess.check_output(['losetup','--find','--show','--offset','1048576',str(image)],text=True).strip()
if not re.fullmatch(r'/dev/loop[0-9]+',loop):raise RuntimeError('Unexpected loop device')
backing=subprocess.check_output(['losetup','-n','-O','BACK-FILE',loop],text=True).strip()
if Path(backing).resolve()!=image:raise RuntimeError('Loop device does not refer to the lab image')
mounted=False
try:
 subprocess.run(['mkfs.exfat','-L','MPCSAMPLE',loop],check=True)
 mount.mkdir(exist_ok=True)
 if list(mount.iterdir()) or os.path.ismount(mount):raise RuntimeError('Lab mount directory is not empty')
 subprocess.run(['mount.exfat-fuse',loop,str(mount)],check=True);mounted=True
 shutil.copytree(card,mount,dirs_exist_ok=True)
 os.sync()
 subprocess.run(['umount',str(mount)],check=True);mounted=False
finally:
 if mounted:subprocess.run(['umount',str(mount)],check=False)
 if not os.path.ismount(mount):subprocess.run(['losetup','--detach',loop],check=True)
print(f'Virtual SD ready: {image} ({image.stat().st_size} bytes)')
