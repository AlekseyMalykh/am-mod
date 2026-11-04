"""First-load ABI, selection, splash packing, loader ordering and early rollback."""
from pathlib import Path
import subprocess,hashlib,json,zipfile,tempfile,os
from early_startup import transform_launcher
R=Path(__file__).resolve().parent;O=R/'build/early-startup';O.mkdir(exist_ok=True)
sources=['src/startup_project.cc','src/startup_project.h','src/project_recovery.h','src/early_splash.c','src/early_startup.sh','src/am_mod_splash.inc','src/phone_native.c','src/workflow_ui.c','build_chords_candidate.py','early_startup.py','check_startup_project.cc','check_early_splash.c','verify_early_startup.py']
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();before={s:sha(R/s) for s in sources}
stock=R.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample';assert sha(stock)=='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
for mode,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
 for case,cc in [('project','check_startup_project.cc'),('splash','check_early_splash.c')]:
  exe=O/(mode+'-'+case);cpp=cc.endswith('.cc')
  subprocess.run(['aarch64-linux-gnu-g++' if cpp else 'aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror',*flags,cc,'-o',str(exe),*(['-ldl'] if cpp else [])],cwd=R,check=True)
  p=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(exe),*([str(stock)] if cpp else [])],cwd=R,text=True,capture_output=True,timeout=45);(O/(mode+'-'+case+'.log')).write_text(p.stdout+p.stderr);print(p.stdout+p.stderr,end='',flush=True);p.check_returncode()
helper=O/'boot-splash';subprocess.run(['aarch64-linux-gnu-gcc','-O2','-static','-Wall','-Wextra','-Werror','src/early_splash.c','-o',str(helper)],cwd=R,check=True)
with zipfile.ZipFile(R.parent/'am-mod-r26x.zip') as z:launcher=transform_launcher(z.read('autorun/mod-launcher.sh').decode(),sha(helper))
(O/'mod-launcher.sh').write_text(launcher);subprocess.run(['sh','-n',str(O/'mod-launcher.sh')],check=True)
assert launcher.index('\nearly_begin\n')<launcher.index('|| fail WRONG_STOCK_HASH')
assert launcher.index('touch "$R/early-handoff"')>launcher.index('|| fail GUARD_NOT_ACTIVE')
fragment=(R/'src/early_startup.sh').read_text()
assert fragment.index('|| fail EARLY_WATCH_NOT_ACTIVE')<fragment.index('    systemctl stop "$STOCK_UNIT"')<fragment.index('    "$R/boot-splash" --show')
with tempfile.TemporaryDirectory(prefix='am-early-watch-') as directory:
 folder=Path(directory);(folder/'events.txt').touch();(folder/'early-main-id').write_text('2147483647 invalid\n');(folder/'early-deadline').write_text('0\n')
 mock=folder/'systemctl';mock.write_text('#!/bin/sh\nprintf "%s\\n" "$*" >> "$R/calls"\n');mock.chmod(0o700)
 script=folder/'watch.sh';script.write_text(fragment+'\nearly_watch\n')
 env=dict(os.environ,R=str(folder),STOCK_UNIT='am-fixture-stock.service',PATH=str(folder)+':'+os.environ['PATH'])
 subprocess.run(['setsid','sh',str(script)],env=env,check=True,timeout=5);assert (folder/'calls').read_text()=='--no-block start am-fixture-stock.service\n'
 assert (folder/'early-watch-ready').read_text().strip().split()[0].isdigit()
 (folder/'calls').unlink();(folder/'early-handoff').touch();subprocess.run(['sh',str(script)],env=env,check=True,timeout=5);assert not (folder/'calls').exists()
print('PASS early stop precedes large hash/probe; independent early rollback restores stock and retires only after permanent recovery is armed',flush=True)
assert before=={s:sha(R/s) for s in sources}
(O/'verification.json').write_text(json.dumps(dict(passed=True,ubsan=True,first_load_abi_verified=True,one_load_only_verified=True,early_rollback_verified=True,splash_bounds_verified=True,sources_sha256=before,splash_sha256=sha(helper),stock_sha256=sha(stock),hardware_tested=False),indent=2)+'\n')
