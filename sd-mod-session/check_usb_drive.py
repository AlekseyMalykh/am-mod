"""Run the production ownership script with simulated kernel/mount/systemd.

No real block device, mount or gadget is touched. Each test has private files.
"""
from pathlib import Path
import os,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parent
FAKE=r'''#!/usr/bin/python3
from pathlib import Path
import os,sys,shutil
r=Path(os.environ['USB_DRIVE_TEST_ROOT']);name=Path(sys.argv[0]).name;args=sys.argv[1:]
g=r/'gadget';d=r/'runtime/usb-drive';sd=r/'media/MPC'
def log(s):
 with (r/'calls').open('a') as f:f.write(s+'\n')
def table(mounted):
 row=f'1 0 179:97 / {sd} rw - exfat /dev/mmcblk1p1 rw\n' if mounted else ''
 (r/'proc/self/mountinfo').write_text(row);(r/'proc/123/mountinfo').write_text(row)
 (r/'proc/mounts').write_text(f'/dev/mmcblk1p1 {sd} exfat rw,nosuid,nodev,noexec 0 0\n' if mounted else '')
if name=='systemctl':
 op=args[0]
 if op=='is-active':sys.exit(0 if (r/'daemon-active').exists() else 3)
 if op=='is-enabled':print('masked' if (r/'masked').exists() else 'static');sys.exit(0)
 log('systemctl '+' '.join(args))
 if op=='mask':(r/'masked').touch()
 elif op=='unmask':(r/'masked').unlink(missing_ok=True)
 elif op=='stop':(r/'daemon-active').unlink(missing_ok=True)
 elif op=='start':
  if args[-1]=='edisksd.service':(r/'daemon-active').touch()
elif name=='sync':log('sync')
elif name=='umount':
 log('umount')
 if (r/'busy').exists():sys.exit(1)
 table(False)
elif name=='mount':
 assert not (g/'functions/mass_storage.block').exists(),'mount while USB owns the SD'
 log('mount');table(True)
elif name=='gadget':
 if args==['switch','sd-access']:
  assert not (r/'proc/self/mountinfo').read_text(),'export while mounted'
  if (r/'async-setup').exists():
   log('export-trigger');(r/'pending-gadget').write_text('3');sys.exit(0)
  log('export');lun=g/'functions/mass_storage.block/lun.0';lun.mkdir(parents=True,exist_ok=True)
  for f in ['file','ro','removable']:(lun/f).write_text('')
  (g/'UDC').write_text('dummy-udc\n')
  if (r/'switch-fails').exists():sys.exit(1)
 else:
  assert args==['switch'];log('unexport')
  (r/'pending-gadget').unlink(missing_ok=True)
  p=g/'functions/mass_storage.block';assert p.resolve().is_relative_to(r.resolve())
  if p.exists():shutil.rmtree(p)
  (g/'UDC').write_text('dummy-udc\n')
elif name=='sleep':
 if (r/'pending-gadget').exists():
  pending=r/'pending-gadget';n=int(pending.read_text())-1;pending.write_text(str(n));log('wait-setup')
  if n<=0 and not (r/'setup-timeout').exists():
   lun=g/'functions/mass_storage.block/lun.0';lun.mkdir(parents=True,exist_ok=True)
   for f in ['file','ro','removable']:(lun/f).write_text('')
   (g/'UDC').write_text('dummy-udc\n');pending.unlink()
  sys.exit(0)
 mode=(r/'mode').read_text()
 if mode=='eject':(g/'functions/mass_storage.block/lun.0/file').write_text('')
 elif mode=='disconnect':(r/'udc/dummy-udc/state').write_text('not attached\n')
 elif mode=='local-exit':(d/'exit-request').write_text('AM_USB_EXIT 1\n')
 elif mode=='swap':
  (r/'sys/mmcblk1/device/cid').write_text('OTHER_CARD\n');(g/'functions/mass_storage.block/lun.0/file').write_text('')
else:raise RuntimeError(name)
'''
class Drive(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name);r=self.root
  for p in ['runtime','media/MPC/MPC-Sample','proc/self','proc/123','sys/mmcblk1/device','sys/mmcblk1p1','gadget','udc/dummy-udc','bin']:(r/p).mkdir(parents=True,exist_ok=True)
  (r/'sys/mmcblk1/removable').write_text('1\n');(r/'sys/mmcblk1p1/dev').write_text('179:97\n');(r/'sys/mmcblk1/device/cid').write_text('SAME_CARD\n')
  (r/'sys/mmcblk1/device/type').write_text('SD\n')
  (r/'gadget/UDC').write_text('dummy-udc\n');(r/'udc/dummy-udc/state').write_text('configured\n')
  (r/'daemon-active').touch();(r/'mode').write_text('eject');(r/'calls').write_text('')
  for cmd in ['systemctl','mount','umount','sync','gadget','sleep']:
   p=r/'bin'/cmd;p.write_text(FAKE);p.chmod(0o755)
  sd=r/'media/MPC';row=f'1 0 179:97 / {sd} rw - exfat /dev/mmcblk1p1 rw\n'
  (r/'proc/self/mountinfo').write_text(row);(r/'proc/123/mountinfo').write_text(row);(r/'proc/mounts').write_text(f'/dev/mmcblk1p1 {sd} exfat rw,nosuid,nodev,noexec 0 0\n')
  s=(ROOT/'src/usb_drive.sh').read_text().replace('R=/run/mpc-clone-trial-v1',f'R={r}/runtime').replace('G=/sys/kernel/config/usb_gadget/ac50',f'G={r}/gadget').replace('SYS=/sys/class/block',f'SYS={r}/sys').replace('PROC=/proc',f'PROC={r}/proc').replace('GADGET=/usr/bin/az0x-usb-gadget',f'GADGET={r}/bin/gadget').replace('/sys/class/udc/',f'{r}/udc/').replace('/media/',f'{r}/media/')
  self.script=r/'script';self.script.write_text(s)
  (r/'runtime/usb-drive-boot.sh').write_text((ROOT/'src/usb_drive_boot.sh').read_text())
 def tearDown(self):self.temp.cleanup()
 def run_script(self,*args):return subprocess.run(['sh',str(self.script),*map(str,args)],env={**os.environ,'PATH':str(self.root/'bin')+':'+os.environ['PATH'],'USB_DRIVE_TEST_ROOT':str(self.root)},capture_output=True,text=True,timeout=15)
 def prepare(self):return self.run_script('prepare',self.root/'media/MPC/MPC-Sample')
 def test_eject_and_repeated_entry(self):
  for _ in range(2):
   p=self.prepare();self.assertEqual(p.returncode,0,p.stderr)
   self.assertFalse((self.root/'proc/self/mountinfo').read_text())
   p=self.run_script('share');self.assertEqual(p.returncode,0,p.stderr)
   self.assertTrue((self.root/'proc/self/mountinfo').read_text());self.assertFalse((self.root/'masked').exists());self.assertTrue((self.root/'daemon-active').exists())
  calls=(self.root/'calls').read_text();self.assertLess(calls.index('umount'),calls.index('export'));self.assertLess(calls.index('unexport'),calls.index('mount\n',calls.index('unexport')))
 def test_disconnect(self):
  (self.root/'mode').write_text('disconnect');self.assertEqual(self.prepare().returncode,0)
  p=self.run_script('share');self.assertEqual(p.returncode,0,p.stderr);self.assertTrue((self.root/'proc/self/mountinfo').read_text())
 def test_local_exit_with_cable_configured(self):
  (self.root/'mode').write_text('local-exit');self.assertEqual(self.prepare().returncode,0)
  p=self.run_script('share');self.assertEqual(p.returncode,0,p.stderr)
  self.assertIn('USB_DRIVE_LOCAL_EXIT',p.stderr)
  self.assertEqual((self.root/'udc/dummy-udc/state').read_text(),'configured\n')
  self.assertFalse((self.root/'gadget/functions/mass_storage.block').exists())
  self.assertTrue((self.root/'proc/self/mountinfo').read_text())
  (self.root/'mode').write_text('eject');self.assertEqual(self.prepare().returncode,0)
  self.assertFalse((self.root/'runtime/usb-drive/exit-request').exists())
 def test_async_gadget_setup_waits(self):
  (self.root/'async-setup').touch();self.assertEqual(self.prepare().returncode,0)
  p=self.run_script('share');self.assertEqual(p.returncode,0,p.stderr)
  self.assertEqual((self.root/'calls').read_text().count('wait-setup'),3)
  self.assertIn('start az0x-usb-gadget@ac50.service',(self.root/'calls').read_text())
 def test_async_setup_timeout_restores(self):
  (self.root/'async-setup').touch();(self.root/'setup-timeout').touch();self.assertEqual(self.prepare().returncode,0)
  p=self.run_script('share');self.assertNotEqual(p.returncode,0);self.assertIn('MASS_STORAGE_SETUP_TIMEOUT',p.stderr)
  p=self.run_script('restore');self.assertEqual(p.returncode,0,p.stderr);self.assertTrue((self.root/'proc/self/mountinfo').read_text())
 def test_no_computer_timeout(self):
  (self.root/'mode').write_text('none');(self.root/'udc/dummy-udc/state').write_text('not attached\n');self.assertEqual(self.prepare().returncode,0)
  p=self.run_script('share');self.assertEqual(p.returncode,0,p.stderr);self.assertTrue((self.root/'proc/self/mountinfo').read_text())
 def test_busy_never_exports(self):
  (self.root/'busy').touch();self.assertNotEqual(self.prepare().returncode,0)
  p=self.run_script('restore');self.assertEqual(p.returncode,0,p.stderr);self.assertNotIn('export\n',(self.root/'calls').read_text());self.assertFalse((self.root/'masked').exists())
 def test_private_namespace_never_exports(self):
  (self.root/'proc/456').mkdir();(self.root/'proc/456/mountinfo').write_text((self.root/'proc/self/mountinfo').read_text())
  self.assertNotEqual(self.prepare().returncode,0);self.assertNotIn('export\n',(self.root/'calls').read_text())
  (self.root/'proc/456/mountinfo').unlink();p=self.run_script('restore');self.assertEqual(p.returncode,0,p.stderr)
 def test_internal_storage_rejected(self):
  # Reject eMMC even with a misleading removable hint.
  (self.root/'sys/mmcblk1/device/type').write_text('MMC\n');self.assertNotEqual(self.prepare().returncode,0);self.assertNotIn('umount',(self.root/'calls').read_text());self.assertNotIn('export',(self.root/'calls').read_text())
 def test_real_mpc_sd_with_fixed_hint(self):
  (self.root/'sys/mmcblk1/removable').write_text('0\n')
  p=self.prepare();self.assertEqual(p.returncode,0,p.stderr)
  p=self.run_script('share');self.assertEqual(p.returncode,0,p.stderr)
 def test_missing_card_type_rejected(self):
  (self.root/'sys/mmcblk1/device/type').unlink();self.assertNotEqual(self.prepare().returncode,0)
  self.assertNotIn('umount',(self.root/'calls').read_text());self.assertNotIn('export',(self.root/'calls').read_text())
  p=self.run_script('restore');self.assertEqual(p.returncode,0,p.stderr);self.assertNotIn('No such file',p.stderr)
 def test_switch_failure_recovers(self):
  self.assertEqual(self.prepare().returncode,0);(self.root/'switch-fails').touch();self.assertNotEqual(self.run_script('share').returncode,0)
  p=self.run_script('restore');self.assertEqual(p.returncode,0,p.stderr);self.assertFalse((self.root/'gadget/functions/mass_storage.block').exists());self.assertTrue((self.root/'proc/self/mountinfo').read_text())
 def test_card_swap_detaches_before_refusal(self):
  self.assertEqual(self.prepare().returncode,0);(self.root/'mode').write_text('swap');self.assertNotEqual(self.run_script('share').returncode,0)
  self.assertFalse((self.root/'gadget/functions/mass_storage.block').exists());self.assertFalse((self.root/'proc/self/mountinfo').read_text())
  (self.root/'sys/mmcblk1/device/cid').write_text('SAME_CARD\n');self.assertEqual(self.run_script('restore').returncode,0)
 def test_interrupted_export_recovers(self):
  self.assertEqual(self.prepare().returncode,0);(self.root/'runtime/usb-drive/switch-intent').touch()
  env={**os.environ,'USB_DRIVE_TEST_ROOT':str(self.root)};subprocess.run([str(self.root/'bin/gadget'),'switch','sd-access'],env=env,check=True)
  (self.root/'gadget/functions/mass_storage.block/lun.0/file').write_text('/dev/mmcblk1p1\n')
  p=self.run_script('restore');self.assertEqual(p.returncode,0,p.stderr);self.assertTrue((self.root/'proc/self/mountinfo').read_text());self.assertFalse((self.root/'masked').exists())
class HostCycle(unittest.TestCase):
 def test_restart_repin_proxy_and_services(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);r=root/'runtime';save=root/'card/MPC-Sample';sd=save.parent
   for p in [r/'app',r/'proxy',r/'storage-meta',save/'Projects',save/'Samples/Export',save/'Samples/.am-mod-workspace/temp-projects-v1',sd/'autorun',root/'bin',root/'systemd',root/'snd']:
    p.mkdir(parents=True,exist_ok=True)
   (save/'Projects/Mod-Exit-Last.xpj').write_text('saved snapshot')
   (r/'app/usb-resume-project').write_text(str(save/'Projects/Mod-Exit-Last.xpj'))
   (r/'app/usb-files.on').touch();(r/'app/screen-mirror.on').touch();(r/'app/usb-drive.request').write_text('1')
   (r/'storage-meta/save-path').write_text(str(save));(r/'storage-meta/temp-workspace-path').write_text(str(save/'Samples/.am-mod-workspace/temp-projects-v1'))
   (r/'sd-device').write_text(str(sd.stat().st_dev));(r/'events.txt').touch()
   for name in ['stage-data','usb-mode.sh']:
    p=r/name;p.write_text(f'#!/bin/sh\nprintf "{name} %s\\n" "$*" >> "{r}/events.txt"\n');p.chmod(0o755)
   p=r/'usb-drive.sh';p.write_text(f'''#!/bin/sh
printf 'drive %s\\n' "$1" >> "{r}/events.txt"
mkdir -p "{r}/usb-drive"
case "$1" in prepare) echo ready ;; *) echo done ;; esac > "{r}/usb-drive/phase"
''');p.chmod(0o755)
   p=r/'usb-drive-screen';p.write_text('''#!/bin/sh
echo 1 > "$2"
while [ "$(cat "$1")" != done ]; do sleep 0.02; done
''');p.chmod(0o755)
   p=root/'bin/systemctl';p.write_text(r'''#!/usr/bin/python3
from pathlib import Path
import os,sys,socket
r=Path(os.environ['R']);a=sys.argv[1:]
with (r/'events.txt').open('a') as f:f.write('systemctl '+' '.join(a)+'\n')
if a[0]=='start' and a[1].endswith('bus-proxy.service'):
 s=socket.socket(socket.AF_UNIX);s.bind(str(r/'proxy/system_bus_socket'));s.close()
''');p.chmod(0o755)
   p=root/'bin/udevadm';p.write_text('#!/bin/sh\nexit 0\n');p.chmod(0o755)
   s=(ROOT/'src/usb_drive_launcher.inc.sh').read_text().replace('/run/systemd/system/',str(root/'systemd')+'/').replace('/dev/snd/',str(root/'snd')+'/')
   script=root/'cycle.sh';script.write_text('''set -eu
prop() { echo 0; }
storage_host_check() {
 [ "$(stat -c '%d:%i' "$(cat "$R/storage-meta/save-path")")" = "$(cat "$R/storage-meta/save-identity")" ]
}
usb_start_aux() { echo auxiliary-restarted >> "$R/events.txt"; }
log() { echo "$*" >> "$R/events.txt"; }
'''+s+'\nusb_drive_cycle\n')
   p=subprocess.run(['sh',str(script)],env={**os.environ,'R':str(r),'U':'drive-test','PATH':str(root/'bin')+':'+os.environ['PATH']},capture_output=True,text=True,timeout=10)
   self.assertEqual(p.returncode,0,p.stderr)
   log=(r/'events.txt').read_text()
   self.assertLess(log.index('stop drive-test-clone'),log.index('drive prepare'))
   self.assertLess(log.index('drive share'),log.index('start drive-test-bus-proxy'))
   self.assertLess(log.index('start drive-test-bus-proxy'),log.index('start drive-test-clone'))
   self.assertIn('auxiliary-restarted',log);self.assertFalse((r/'usb-drive-active').exists())
   self.assertTrue((r/'storage-meta/save-identity').exists());self.assertTrue((r/'app/proxy-socket-id').exists())
   self.assertFalse((r/'app/usb-files.on').exists());self.assertFalse((r/'app/screen-mirror.on').exists())
if __name__=='__main__':unittest.main(verbosity=2)
