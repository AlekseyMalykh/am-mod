"""Piano Roll model and native adapter, no physical device writes."""
from pathlib import Path
import hashlib,json,subprocess
R=Path(__file__).resolve().parent;O=R/'build/piano-roll';O.mkdir(parents=True,exist_ok=True)
sources=['src/piano_roll.c','src/piano_roll.h','src/piano_roll_native.inc','src/step_native.c','src/music_settings.c','src/music_settings.h','src/music_ui.c','check_piano_roll.c','check_piano_roll_native.c','src/midi_route.c','src/midi_route.h','src/midi_event.c','src/midi_tools.c','src/midi_tools.h','check_midi_layers.c','check_steps.c','check_steps_support.cc','check_steps_support.S','src/step_abi.S','src/step_exceptions.cc','src/step_mute.cc',Path(__file__).name]
sources+=['src/midi_voice.c','src/midi_voice.h']
sources+=['src/usb_drive_lights.h']
sources+=['src/seq_mutes.c','src/seq_mutes.h']
hashes={s:hashlib.sha256((R/s).read_bytes()).hexdigest() for s in sources}
checks=[]
for arch,cc,runner in [('host','gcc',[]),('arm','aarch64-linux-gnu-gcc',['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808'])]:
 for mode,extra in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
  target=O/(arch+'-'+mode)
  subprocess.run([cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*extra,'check_piano_roll.c','src/piano_roll.c','-lm','-o',str(target)],cwd=R,check=True)
  p=subprocess.run([*runner,str(target)],cwd=R,text=True,capture_output=True,timeout=45)
  (O/(target.name+'.log')).write_text(p.stdout+p.stderr);print(p.stdout+p.stderr,flush=True);p.check_returncode();checks.append(target.name)
  midi=O/('midi-layers-'+arch+'-'+mode)
  subprocess.run([cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*extra,'check_midi_layers.c','src/midi_route.c','src/midi_voice.c','src/midi_event.c','src/midi_tools.c','-pthread','-o',str(midi)],cwd=R,check=True)
  p=subprocess.run([*runner,str(midi)],cwd=R,text=True,capture_output=True,timeout=45)
  (O/(midi.name+'.log')).write_text(p.stdout+p.stderr);print(p.stdout+p.stderr,flush=True);p.check_returncode();checks.append(midi.name)
for mode,extra in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
 objects=[];flags=['-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard',*extra]
 for source in ['check_steps_support.cc','src/step_exceptions.cc','src/step_mute.cc']:
  obj=O/(mode+'-'+Path(source).stem+'.o');objects.append(str(obj))
  subprocess.run(['aarch64-linux-gnu-g++','-std=c++11',*flags,'-c',source,'-o',str(obj)],cwd=R,check=True)
 native=O/('native-'+mode)
 subprocess.run(['aarch64-linux-gnu-gcc',*flags,'-Ibuild','-ffunction-sections','-fdata-sections','check_piano_roll_native.c','src/piano_roll.c','src/seq_mutes.c','check_steps_support.S','src/step_abi.S',*objects,'-Wl,--gc-sections','-lstdc++','-ldl','-lpthread','-lm','-o',str(native)],cwd=R,check=True)
 p=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(native),str(R.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'),str(O/('draw-'+mode+'.txt'))],cwd=R,text=True,capture_output=True,timeout=45)
 (O/(native.name+'.log')).write_text(p.stdout+p.stderr);print(p.stdout+p.stderr,flush=True);p.check_returncode();checks.append(native.name)
assert hashes=={s:hashlib.sha256((R/s).read_bytes()).hexdigest() for s in sources}
(O/'verification.json').write_text(json.dumps(dict(passed=True,checks=checks,sources_sha256=hashes,hardware_tested=False,native_history_executor_tested=False),indent=2)+'\n')
