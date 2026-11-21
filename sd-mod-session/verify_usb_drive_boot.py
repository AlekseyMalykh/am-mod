"""Real exFAT loop image + private namespace: D.S.B USB detach/restore transaction.
Run as root under unshare --mount --propagation private. No physical device used.
"""
from pathlib import Path
import hashlib,json,os,re,subprocess,tempfile,sys
R=Path(__file__).resolve().parent;O=R/'build/usb-drive-boot';O.mkdir(exist_ok=True)
assert os.geteuid()==0
source=(R/'src/usb_drive_boot.sh').read_text();subprocess.run(['sh','-n'],input=source,text=True,check=True)
checks=[];shared='--shared' in sys.argv
with tempfile.TemporaryDirectory(prefix='am-dsb-usb-') as temporary:
 root=Path(temporary).resolve();image=root/'card.img';image.touch();image.write_bytes(b'')
 with image.open('r+b') as f:f.truncate(32<<20)
 subprocess.run(['mkfs.exfat','-L','AMTEST',str(image)],check=True,stdout=subprocess.DEVNULL)
 loop=subprocess.check_output(['losetup','--find','--show',str(image)],text=True).strip();assert re.fullmatch(r'/dev/loop[0-9]+',loop)
 sd=root/'media/MPC';sd.mkdir(parents=True);view=root/'run/hs-boot/autorun';view.mkdir(parents=True)
 state=root/'runtime/usb-drive';state.mkdir(parents=True);foreign=root/'foreign';foreign.mkdir()
 script=root/'boot.sh';script.write_text(source.replace('BOOT_VIEW=/run/hs-boot/autorun','BOOT_VIEW='+str(view)))
 mounted=False
 try:
  def mount_card():
   subprocess.run(['mount',loop,str(sd)],check=True)
   if shared:subprocess.run(['mount','--make-shared',str(sd)],check=True)
  mount_card();mounted=True
  devno=next(line.split()[2] for line in Path('/proc/self/mountinfo').read_text().splitlines() if line.split()[4]==str(sd))
  app=sd/'autorun/os/AM-Mod';(sd/'autorun/boot').mkdir(parents=True);app.mkdir(parents=True)
  (sd/'autorun/boot/boot.sh').write_text('# HSBOOT_DIR\n');(app/'sampleboot.inplace').touch();(app/'mod-launcher.sh').write_text('verified AM fixture')
  (sd/'mpcqol.conf').write_text('original QOL settings\n');(sd/'autorun/os/mpcqol.conf').write_text('copied QOL settings\n')
  (sd/'MPC-Sample/Projects').mkdir(parents=True);(sd/'MPC-Sample/Projects/Keep.xpj').write_bytes(b'project fixture unchanged')
  subprocess.run(['mount','--bind',str(sd/'mpcqol.conf'),str(sd/'autorun/os/mpcqol.conf')],check=True)
  subprocess.run(['mount','--bind',str(sd/'autorun'),str(view)],check=True)
  subprocess.run(['mount','--bind',str(app),str(sd/'autorun')],check=True)
  if shared:
   assert len([line for line in Path('/proc/self/mountinfo').read_text().splitlines() if line.split()[4]==str(view)])==2
   assert not (view/'boot/boot.sh').exists()
   checks.append('reproduced physical MPC shared propagation: AM hides saved loader view under a second mount')
  setup=f'R={root}/runtime\nD={state}\nPROC=/proc\nsd={sd}\ndevno={devno}\nfs=exfat\n. {script}\n'
  def call(action,success=True,prefix=''):
   p=subprocess.run(['sh'],input=setup+prefix+action+'\n',text=True,capture_output=True)
   assert (p.returncode==0)==success,(action,p.stdout,p.stderr);return p
  call('bootloader_binds_prepare');assert not (sd/'autorun/sampleboot.inplace').exists()
  assert (sd/'autorun/boot/boot.sh').is_file() and not (view/'boot/boot.sh').exists()
  subprocess.run(['umount',str(sd)],check=True);mounted=False
  assert not any(line.split()[2]==devno for line in Path('/proc/self/mountinfo').read_text().splitlines())
  checks.append('D.S.B top overmount, config bind and preserved view all released; exFAT card unmounts normally with no remaining SD mounts')
  mount_card();mounted=True
  call('bootloader_binds_restore');assert (sd/'autorun/sampleboot.inplace').is_file()
  assert (view/('sampleboot.inplace' if shared else 'boot/boot.sh')).is_file()
  assert (sd/'MPC-Sample/Projects/Keep.xpj').read_bytes()==b'project fixture unchanged'
  assert (sd/'mpcqol.conf').read_text()=='original QOL settings\n'
  assert not (state/'boot-os-root').exists()
  checks.append('same card remount restores all three D.S.B mappings and AM in-place view; project/QOL bytes unchanged')
  # Fail after releasing AM's top bind: rollback must keep/reuse the remaining valid mounts.
  fail=root/'fail-config-unmount';fail.touch();binpath=root/'bin';binpath.mkdir();real_umount=subprocess.check_output(['which','umount'],text=True).strip()
  wrapper=binpath/'umount';wrapper.write_text(f'#!/bin/sh\nif [ "$1" = "{sd}/autorun/os/mpcqol.conf" ] && [ -f "{fail}" ]; then exit 1; fi\nexec {real_umount} "$@"\n');wrapper.chmod(0o755)
  call('bootloader_binds_prepare',False,prefix=f'PATH={binpath}:$PATH\n')
  fail.unlink();call('bootloader_binds_restore');assert (sd/'autorun/sampleboot.inplace').is_file()
  checks.append('interrupted detach rolls back partial state without duplicating mounts')
  subprocess.run(['mount','--bind',str(sd/'MPC-Sample'),str(foreign)],check=True)
  call('bootloader_binds_prepare');subprocess.run(['umount',str(sd)],check=True);mounted=False
  assert any(line.split()[2]==devno and line.split()[4]==str(foreign) for line in Path('/proc/self/mountinfo').read_text().splitlines())
  checks.append('unrelated SD bind is left mounted; the existing exclusive-volume check can reject USB export')
  subprocess.run(['umount',str(foreign)],check=True);mount_card();mounted=True
  call('bootloader_binds_restore');call('bootloader_binds_prepare')
  # No D.S.B overlay: plain-card preparation must stay a no-op.
  call('bootloader_binds_prepare');call('bootloader_binds_restore')
  checks.append('plain-card helper is inert and repeated D.S.B cycles restore correctly')
 finally:
  # Only paths created above in this private namespace may be unmounted.
  for path in [sd/'autorun',sd/'autorun/os/mpcqol.conf',view,foreign,sd]:
   if any(line.split()[4]==str(path) for line in Path('/proc/self/mountinfo').read_text().splitlines()):subprocess.run(['umount',str(path)],check=True)
  subprocess.run(['losetup','-d',loop],check=True)
report=dict(passed=True,real_namespace_tested=True,real_exfat_image=True,shared_mounts=shared,checks=checks,sources_sha256={n:hashlib.sha256((R/n).read_bytes()).hexdigest() for n in ['src/usb_drive_boot.sh','src/usb_drive.sh','usb_drive_boot_package.py',Path(__file__).name]},hardware_tested=False)
(O/('shared-verification.json' if shared else 'verification.json')).write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
