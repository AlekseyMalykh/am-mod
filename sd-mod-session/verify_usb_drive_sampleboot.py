"""Real private exFAT loop image: SampleBoot detach/restore, no physical SD.
Run as root inside unshare --mount --propagation private.
"""
from pathlib import Path
import hashlib,json,os,re,subprocess,sys,tempfile
R=Path(__file__).resolve().parent;O=R/'build/usb-drive-sampleboot';O.mkdir(exist_ok=True)
assert os.geteuid()==0
source=(R/'src/usb_drive_boot.sh').read_text();subprocess.run(['sh','-n'],input=source,text=True,check=True)
shared='--shared' in sys.argv;checks=[]
with tempfile.TemporaryDirectory(prefix='am-sampleboot-usb-') as temporary:
 root=Path(temporary).resolve();image=root/'card.img'
 with image.open('wb') as f:f.truncate(32<<20)
 subprocess.run(['mkfs.exfat','-L','AMTEST',str(image)],check=True,stdout=subprocess.DEVNULL)
 loop=subprocess.check_output(['losetup','--find','--show',str(image)],text=True).strip();assert re.fullmatch(r'/dev/loop[0-9]+',loop)
 sd=root/'media/MPC';sd.mkdir(parents=True);state=root/'runtime/usb-drive';state.mkdir(parents=True)
 script=root/'boot.sh';script.write_text(source)
 app=sd/'autorun/mods/AM-Mod-Arrange-Test4';foreign=root/'foreign';foreign.mkdir()
 def mounted(path):return any(line.split()[4]==str(path) for line in Path('/proc/self/mountinfo').read_text().splitlines())
 def mount_card():
  subprocess.run(['mount',loop,str(sd)],check=True)
  if shared:subprocess.run(['mount','--make-shared',str(sd)],check=True)
 try:
  mount_card()
  devno=next(line.split()[2] for line in Path('/proc/self/mountinfo').read_text().splitlines() if line.split()[4]==str(sd))
  (sd/'autorun/sampleboot').mkdir(parents=True);app.mkdir(parents=True)
  (sd/'autorun/autorun.sh').write_text('#!/bin/sh\n# SAMPLEBOOT\n')
  (sd/'autorun/sampleboot/sampleboot.sh').write_text('SampleBoot loader fixture\n')
  (app/'sampleboot.inplace').touch();(app/'mod-launcher.sh').write_text('verified module fixture\n')
  (sd/'MPC-Sample/Projects').mkdir(parents=True);project=sd/'MPC-Sample/Projects/Keep.xpj';project.write_bytes(b'unchanged project\n')
  protected=['autorun/autorun.sh','autorun/sampleboot/sampleboot.sh','MPC-Sample/Projects/Keep.xpj']
  expected={n:hashlib.sha256((sd/n).read_bytes()).hexdigest() for n in protected}
  setup=f'R={root}/runtime\nD={state}\nPROC=/proc\nsd={sd}\ndevno={devno}\nfs=exfat\n. {script}\n'
  def call(action,success=True,prefix=''):
   p=subprocess.run(['sh'],input=setup+prefix+action+'\n',text=True,capture_output=True)
   assert (p.returncode==0)==success,(action,p.stdout,p.stderr);return p
  def bind_module():subprocess.run(['mount','--bind',str(app),str(sd/'autorun')],check=True)
  def preserved():assert expected=={n:hashlib.sha256((sd/n).read_bytes()).hexdigest() for n in protected}
  bind_module()
  assert 'USB_DRIVE_SAMPLEBOOT_RELEASED' in call('bootloader_binds_prepare').stdout
  assert not mounted(sd/'autorun') and not mounted(state/'sampleboot-view');preserved()
  subprocess.run(['umount',str(sd)],check=True)
  assert not any(line.split()[2]==devno for line in Path('/proc/self/mountinfo').read_text().splitlines())
  checks.append('observed mods/<name> mapping released; temporary root view closed; real exFAT SD unmount succeeds')
  mount_card();preserved();assert 'USB_DRIVE_SAMPLEBOOT_RESTORED' in call('bootloader_binds_restore').stdout
  assert mounted(sd/'autorun') and (sd/'autorun/sampleboot.inplace').is_file()
  assert not (state/'boot-os-root').exists();checks.append('same card remount restores SampleBoot module bind; loader and project hashes preserved')
  # Simulate failure of the final owned bind unmount. Recovery must reuse it.
  fail=root/'fail';fail.touch();binpath=root/'bin';binpath.mkdir()
  real_umount=subprocess.check_output(['which','umount'],text=True).strip()
  wrapper=binpath/'umount';wrapper.write_text(f'#!/bin/sh\nif [ "$1" = "{sd}/autorun" ] && [ -f "{fail}" ]; then exit 1; fi\nexec {real_umount} "$@"\n');wrapper.chmod(0o755)
  call('bootloader_binds_prepare',False,prefix=f'PATH={binpath}:$PATH\n')
  assert mounted(sd/'autorun') and not mounted(state/'sampleboot-view')
  fail.unlink();call('bootloader_binds_restore');checks.append('failed unmount rolls back without duplicate binds or temporary SD aliases')
  subprocess.run(['mount','--bind',str(sd/'MPC-Sample'),str(foreign)],check=True)
  call('bootloader_binds_prepare');assert mounted(foreign);preserved()
  checks.append('unrelated SD bind left intact for exclusive-volume rejection')
  subprocess.run(['umount',str(foreign)],check=True)
  # Invalid loader cannot authorize removal of even a matching module bind.
  (sd/'autorun/autorun.sh').write_text('# unknown loader\n');bind_module()
  call('bootloader_binds_prepare',False);assert mounted(sd/'autorun') and not mounted(state/'sampleboot-view')
  call('bootloader_binds_restore');subprocess.run(['umount',str(sd/'autorun')],check=True)
  (sd/'autorun/autorun.sh').write_text('#!/bin/sh\n# SAMPLEBOOT\n');preserved()
  checks.append('unknown loader refused without detaching module; inspection bind always removed')
  # Standard launch has no module bind, so both operations must be inert.
  call('bootloader_binds_prepare');call('bootloader_binds_restore');assert not mounted(sd/'autorun');preserved()
  checks.append('ordinary launch without bootloader bind remains a no-op')
  call('boot_os_root_valid',False,prefix='boot_root=/autorun/mods/../../other\n')
  call('boot_os_root_valid',False,prefix='boot_root=/unrelated/AM-Mod\n')
 finally:
  for path in [state/'sampleboot-view',sd/'autorun',foreign,sd]:
   if mounted(path):subprocess.run(['umount',str(path)],check=True)
  subprocess.run(['losetup','-d',loop],check=True)
report=dict(passed=True,shared_mounts=shared,real_exfat_loop=True,real_mount_namespace=True,checks=checks,hardware_tested=False,sources_sha256={'src/usb_drive_boot.sh':hashlib.sha256(source.encode()).hexdigest(),Path(__file__).name:hashlib.sha256(Path(__file__).read_bytes()).hexdigest()})
(O/('shared-verification.json' if shared else 'verification.json')).write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
