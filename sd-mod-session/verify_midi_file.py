"""SMF codec and mapped import plans, native pointers and physical media absent."""
from pathlib import Path
import hashlib,json,subprocess
R=Path(__file__).resolve().parent;O=R/'build/midi-file';O.mkdir(parents=True,exist_ok=True)
sources=['src/midi_file.c','src/midi_file.h','src/midi_tools.c','src/midi_tools.h','check_midi_file.c',Path(__file__).name]
sources+=['src/midi_file_native.c','src/midi_file_native.h','src/midi_route.c','src/midi_route.h','src/midi_event.c','src/midi_event.h','check_midi_file_native.c','check_steps.c','src/step_native.c','src/piano_roll.c','src/piano_roll.h','src/piano_roll_native.inc','src/music_settings.c','src/music_settings.h','check_steps_support.cc','check_steps_support.S','src/step_abi.S','src/step_exceptions.cc','src/step_mute.cc']
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
hashes={s:sha(R/s) for s in sources};checks=[]
for arch,cc,runner in [('host','gcc',[]),('arm','aarch64-linux-gnu-gcc',['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808'])]:
 for mode,extra in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
  target=O/(arch+'-'+mode)
  subprocess.run([cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*extra,'check_midi_file.c','src/midi_file.c','src/midi_tools.c','-pthread','-o',str(target)],cwd=R,check=True)
  p=subprocess.run([*runner,str(target)],cwd=R,text=True,capture_output=True,timeout=60)
  (O/(target.name+'.log')).write_text(p.stdout+p.stderr);print(p.stdout+p.stderr,flush=True);p.check_returncode();checks.append(target.name)
for mode,extra in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
 flags=['-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard',*extra];objects=[]
 for source in ['check_steps_support.cc','src/step_exceptions.cc','src/step_mute.cc']:
  obj=O/(mode+'-'+Path(source).stem+'.o');objects.append(str(obj))
  subprocess.run(['aarch64-linux-gnu-g++','-std=c++11',*flags,'-c',source,'-o',str(obj)],cwd=R,check=True)
 target=O/('native-'+mode)
 subprocess.run(['aarch64-linux-gnu-gcc',*flags,'-Ibuild','-ffunction-sections','-fdata-sections','check_midi_file_native.c','src/piano_roll.c','src/midi_file.c','src/midi_file_native.c','src/midi_tools.c','src/midi_route.c','src/midi_event.c','check_steps_support.S','src/step_abi.S',*objects,'-Wl,--gc-sections','-lstdc++','-ldl','-lpthread','-lm','-o',str(target)],cwd=R,check=True)
 p=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(target),str(R.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample')],cwd=R,text=True,capture_output=True,timeout=60)
 (O/(target.name+'.log')).write_text(p.stdout+p.stderr);print(p.stdout+p.stderr,flush=True);p.check_returncode();checks.append(target.name)
assert hashes=={s:sha(R/s) for s in sources}
(O/'verification.json').write_text(json.dumps(dict(passed=True,checks=checks,sources_sha256=hashes,native_import_connected=False),indent=2)+'\n')
