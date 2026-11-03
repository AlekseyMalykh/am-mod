"""Current Steps WRITE/TOGGLE ARM contracts, without repackaging frozen releases."""
from pathlib import Path
import hashlib,json,subprocess
R=Path(__file__).resolve().parent;O=R/'build/step-toggle';O.mkdir(exist_ok=True)
sources=['check_steps.c','check_steps_support.cc','check_steps_support.S','src/step_abi.S',
         'src/step_native.c','src/step_native.h','src/step_exceptions.cc','src/step_mute.cc',
         'src/music_settings.c','src/music_settings.h','src/pad_palette.h',
         'build/pad-colour-workflows/phone-bridges.bin','build/phone_hooks.h','verify_step_toggle.py']
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
before={n:sha(R/n) for n in sources}
stock=R.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
assert sha(stock)=='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
for mode,extra in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
 common=['-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard',*extra]
 objects=[]
 for source in ('check_steps_support.cc','src/step_exceptions.cc','src/step_mute.cc'):
  obj=O/(mode+'-'+Path(source).stem+'.o');objects.append(str(obj))
  subprocess.run(['aarch64-linux-gnu-g++','-std=c++11',*common,'-c',source,'-o',str(obj)],cwd=R,check=True)
 target=O/mode
 subprocess.run(['aarch64-linux-gnu-gcc',*common,'-Ibuild','-DSTEP_BRIDGE_PATH="build/pad-colour-workflows/phone-bridges.bin"','check_steps.c','check_steps_support.S','src/step_abi.S',*objects,'-lstdc++','-ldl','-lpthread','-lm','-o',str(target)],cwd=R,check=True)
 p=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(target),str(stock),str(O/(mode+'-draw.txt'))],cwd=R,capture_output=True,text=True,timeout=45)
 (O/(mode+'.log')).write_text(p.stdout+p.stderr);p.check_returncode()
 assert 'PASS SAMPLE RECORD toggles a step on/off' in p.stdout
 print(p.stdout[-1500:],end='',flush=True)
assert before=={n:sha(R/n) for n in sources}
(O/'verification.json').write_text(json.dumps(dict(passed=True,ubsan=True,write_toggle_exclusive_verified=True,sources_sha256=before,hardware_tested=False,native_gui_and_executor='fixtures'),indent=2)+'\n')
