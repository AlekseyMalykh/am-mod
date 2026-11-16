"""Run the real USB boot transaction with simulated mount/systemd operations."""
from pathlib import Path
import os, subprocess, tempfile, unittest
ROOT=Path(__file__).resolve().parent
class Mode(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name)
  self.runtime=self.root/'runtime';(self.runtime/'app').mkdir(parents=True)
  (self.runtime/'am_usb_names.ko').write_bytes(b'module fixture')
  self.modules=self.root/'modules';self.modules.mkdir();self.calls=self.root/'calls'
  self.save=self.root/'MPC-Sample';(self.save/'Projects').mkdir(parents=True)
  self.scheme=self.root/'ac50.scheme';self.stock='p_chmask = 3;\np_srate = 44100;\np_ssize = 4;\nc_chmask = 3;\n'
  self.scheme.write_text(self.stock);(self.root/'stock').write_text(self.stock)
  self.gadget=self.root/'gadget';self.gadget.mkdir();(self.gadget/'p_chmask').write_text('3\n')
  self.mountinfo=self.root/'mountinfo';self.mountinfo.write_text('')
  self.bin=self.root/'bin';self.bin.mkdir()
  commands={
   'mount':f'cp "$2" "$3"\nprintf "1 0 0:1 / %s rw - tmpfs tmpfs rw\\n" "$3" > "{self.mountinfo}"',
   'umount':f'cp "{self.root}/stock" "$1"\n: > "{self.mountinfo}"',
   'uname':f'cat "{self.root}/kernel"',
   'insmod':f'''printf 'insmod\\n' >> "{self.calls}"
[ ! -f "{self.root}/insmod-fails" ] || exit 1
mkdir "{self.modules}/am_usb_names"''',
   'rmmod':f'''printf 'rmmod\\n' >> "{self.calls}"
[ ! -f "{self.root}/rmmod-fails" ] || exit 1
rmdir "{self.modules}/am_usb_names"''',
   'systemctl':f'''[ "$2" = az0x-usb-gadget@ac50.service ] || exit 50
printf 'systemctl %s\\n' "$1" >> "{self.calls}"
case "$1" in
stop) exit 0 ;;
start)
 if [ -f "{self.root}/fail-once" ]; then rm "{self.root}/fail-once"; exit 1; fi
 sed -n 's/^p_chmask = \\([0-9]*\\);/\\1/p' "{self.scheme}" > "{self.gadget}/p_chmask" ;;
*) exit 50 ;;
esac'''}
  (self.root/'kernel').write_text('6.12.60-imb-2025-12-03-rt13\n')
  for name,body in commands.items():
   p=self.bin/name;p.write_text('#!/bin/sh\nset -eu\n'+body+'\n');p.chmod(0o755)
  source=(ROOT/'src/usb_mode.sh').read_text().replace('R=/run/mpc-clone-trial-v1',f'R={self.runtime}').replace('SCHEME=/usr/lib/az0x-usb-gadget/ac50.scheme',f'SCHEME={self.scheme}').replace('GADGET=/sys/kernel/config/usb_gadget/ac50/functions/uac2.audio',f'GADGET={self.gadget}').replace('/proc/self/mountinfo',str(self.mountinfo))
  source=source.replace('MODULES=/sys/module',f'MODULES={self.modules}')
  self.script=self.root/'mode.sh';self.script.write_text(source)
 def tearDown(self):self.temp.cleanup()
 def run_mode(self,*args):return subprocess.run(['sh',str(self.script),*map(str,args)],env={**os.environ,'PATH':str(self.bin)+':'+os.environ['PATH']},capture_output=True,text=True)
 def enabled(self):return self.runtime/'app/usb-audio-18.active'
 def test_stereo_default(self):
  self.assertEqual(self.run_mode('prepare',self.save).returncode,0);self.assertFalse(self.enabled().exists());self.assertEqual(self.scheme.read_text(),self.stock)
 def test_prepare_and_restore(self):
  (self.save/'Projects/.am-mod-usb-audio').write_text('AM_USBAUDIO 1\n')
  r=self.run_mode('prepare',self.save);self.assertEqual(r.returncode,0,r.stderr);self.assertTrue(self.enabled().exists());self.assertIn('p_chmask = 262143;',self.scheme.read_text());self.assertIn('c_chmask = 3;',self.scheme.read_text())
  self.assertEqual(self.run_mode('restore').returncode,0);self.assertEqual(self.scheme.read_text(),self.stock);self.assertFalse(self.enabled().exists())
  self.assertEqual(self.run_mode('restore').returncode,0)
  calls=self.calls.read_text();self.assertLess(calls.index('systemctl stop'),calls.index('insmod'))
  self.assertLess(calls.index('insmod'),calls.index('systemctl start'))
  self.assertLess(calls.index('systemctl stop',calls.index('insmod')),calls.index('rmmod'))
  self.assertFalse((self.modules/'am_usb_names').exists())
 def test_stock_stays_stereo(self):
  (self.save/'Projects/.am-mod-usb-audio').write_text('AM_USBAUDIO 1\n');(self.save/'Projects/.am-mod-fx-mode').write_text('AM_FXMODE 0\n')
  self.assertEqual(self.run_mode('prepare',self.save).returncode,0);self.assertFalse(self.enabled().exists());self.assertEqual(self.scheme.read_text(),self.stock)
 def test_failed_restart_rolls_back(self):
  (self.save/'Projects/.am-mod-usb-audio').write_text('AM_USBAUDIO 1\n');(self.root/'fail-once').touch()
  self.assertNotEqual(self.run_mode('prepare',self.save).returncode,0);self.assertEqual(self.scheme.read_text(),self.stock);self.assertFalse(self.enabled().exists());self.assertFalse((self.runtime/'usb-scheme-bound').exists())
 def test_interrupted_prepare_restores(self):
  (self.runtime/'usb-scheme-bound').touch();self.scheme.write_text(self.stock.replace('p_chmask = 3;','p_chmask = 262143;'));self.mountinfo.write_text(f'1 0 0:1 / {self.scheme} rw - tmpfs tmpfs rw\n')
  self.assertEqual(self.run_mode('restore').returncode,0);self.assertEqual(self.scheme.read_text(),self.stock)
 def test_wrong_kernel_leaves_stock_untouched(self):
  (self.save/'Projects/.am-mod-usb-audio').write_text('AM_USBAUDIO 1\n');(self.root/'kernel').write_text('other-kernel\n')
  self.assertNotEqual(self.run_mode('prepare',self.save).returncode,0)
  self.assertFalse(self.calls.exists());self.assertEqual(self.scheme.read_text(),self.stock)
 def test_failed_module_load_restores_stock(self):
  (self.save/'Projects/.am-mod-usb-audio').write_text('AM_USBAUDIO 1\n');(self.root/'insmod-fails').touch()
  p=self.run_mode('prepare',self.save);self.assertNotEqual(p.returncode,0)
  self.assertEqual(self.scheme.read_text(),self.stock);self.assertFalse((self.runtime/'usb-names-owned').exists())
 def test_unload_failure_does_not_start_stock_with_named_driver(self):
  (self.save/'Projects/.am-mod-usb-audio').write_text('AM_USBAUDIO 1\n')
  self.assertEqual(self.run_mode('prepare',self.save).returncode,0)
  (self.root/'rmmod-fails').touch();before=self.calls.read_text().count('systemctl start')
  self.assertNotEqual(self.run_mode('restore').returncode,0)
  self.assertEqual(self.calls.read_text().count('systemctl start'),before)
  self.assertTrue((self.runtime/'usb-names-owned').exists())
if __name__=='__main__':unittest.main(verbosity=2)
