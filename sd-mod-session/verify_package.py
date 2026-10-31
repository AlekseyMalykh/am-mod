from pathlib import Path
from hashlib import sha256
import json,subprocess,struct,zipfile
R=Path(__file__).resolve().parent;W=R.parent
info=json.loads((R/'build-info.json').read_text())
assert info['revision']=='r4-project-mod-off', 'This harness covers r4 only; r5 tab-preview runtime coverage is not implemented'
q=['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808']
stock=W/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample';original=stock.read_bytes()
assert sha256(original).hexdigest()==info['stock_sha256']
checks=[]
with zipfile.ZipFile(W/info['zip']) as z:
 assert z.testzip() is None
 for n,h in info['payload_sha256'].items():
  assert sha256((R/'autorun'/n).read_bytes()).hexdigest()==h
  assert sha256(z.read('autorun/'+n)).hexdigest()==h
 assert sha256(z.read('autorun/autorun.json')+z.read('autorun/autorun.sh')+b'f7bed1c9-7e90-4731-9194-c996666b4331').hexdigest().encode()==z.read('autorun/autorun.check')
checks.append('Archive, payload hashes and native autorun checksum verified')
d=Path('/run/mpc-clone-trial-v1');d.mkdir(mode=0o700)
t=d/'MPC-Sample';helper=R/'autorun/apply-session';helper.chmod(0o500)
assert helper.read_bytes()==(R/'build/apply-session').read_bytes()
try:
 t.write_bytes(original)
 assert subprocess.check_output(q+[str(helper)],text=True).strip()=='SESSION_RAM_PATCH_OK'
 modified=t.read_bytes();assert len(modified)==len(original)
 assert sha256(modified).hexdigest()==info['patched_copy_sha256']
 undo=bytearray(modified)
 for p in info['patches']:
  o=int(p['offset'],16);n=p['size'];undo[o:o+n]=original[o:o+n]
 assert undo==original
 assert modified[0x6d43bb0:]==original[0x6d43bb0:]
 assert subprocess.run(q+[str(helper)],capture_output=True).returncode!=0
 assert t.read_bytes()==modified
 checks.append('Actual ARM RAM patcher: exact full hash, only nine declared ranges, RW segment unchanged, repeat refused')
 t.write_bytes(b'SENTINEL')
 assert subprocess.run(q+[str(helper)],capture_output=True).returncode!=0
 assert subprocess.run(q+[str(helper),'/usr/bin/MPC Sample'],capture_output=True).returncode!=0
 assert t.read_bytes()==b'SENTINEL'
 t.unlink();sentinel=d/'sentinel';sentinel.write_bytes(b'UNCHANGED');t.symlink_to(sentinel)
 assert subprocess.run(q+[str(helper)],capture_output=True).returncode!=0
 assert sentinel.read_bytes()==b'UNCHANGED'
 t.unlink();sentinel.unlink()
 checks.append('Wrong size, path argument and symlink refused without mutation')
finally:
 if t.exists() or t.is_symlink():t.unlink()
 d.rmdir()
subprocess.run(q+['/home/user/mpc-rootfs-user-20260808/usr/bin/busybox.nosuid','sh','-n',str(R/'autorun/autorun.sh')],check=True)
checks.append('Firmware ARM BusyBox parses generated loader')
# Exercise the exact firmware BusyBox facilities used for handoff/relaunch.
f=Path('/run/mpc-session-busybox-check');f.mkdir();(f/'s').symlink_to('missing');(f/'d').mkdir();(f/'d/x').write_text('x')
subprocess.run(q+['/home/user/mpc-rootfs-user-20260808/usr/bin/busybox.nosuid','rm','-rf','--',str(f)],check=True)
assert not f.exists()
subprocess.run(q+['/home/user/mpc-rootfs-user-20260808/usr/bin/busybox.nosuid','sleep','0.1'],check=True)
checks.append('Firmware BusyBox supports fractional sleep and constrained runtime-directory cleanup')
subprocess.run(['aarch64-linux-gnu-gcc','-O2','-Wall','-Wextra','-Werror','-mbranch-protection=standard','-Wno-misleading-indentation',str(R/'check_project.c'),'-o',str(R/'build/check-project')],check=True)
output=subprocess.check_output(q+[str(R/'build/check-project'),str(stock),str(R/'build/project.bin')],text=True)
assert output.count('PASS')==2
checks.append('Actual native ARM strings, vector growth, map deep copy and row destruction: 256 menu cycles across two PIE bases')
checks.append('Original four dispatch paths preserved; actual Mod Off RAM syscall and AArch64 O_NOFOLLOW verified')
# r3 is unchanged byte-for-byte; rerun its callback tests with the shipped code.
bad=W/'sd-k-value-peek/releases/20261023-r1-encoder-crash/autorun/peek-code.bin'
peek=subprocess.check_output(q+[str(W/'sd-k-value-peek/build/check-arm'),str(stock),str(R/'autorun/peek-code.bin'),str(bad)],text=True)
assert peek.count('PASS')==4
checks.append('Included yellow-preview/white-edit hooks passed r3 behavioral checks')
result={'result':'PASS','hardware_tested':False,'payload_sha256':info['payload_sha256'],'patched_copy_sha256':info['patched_copy_sha256'],'checks':checks,'project_output':output.splitlines(),'limitations':['UI and audio require device trial','Display repaint is substituted in ARM menu harness','Filesystem write permission remains restricted; saving is not enabled']}
(R/'verification-results.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
