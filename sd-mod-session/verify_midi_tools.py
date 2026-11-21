"""Host + target-ABI checks; no card/device writes. Real ALSA replaced only in IO fixture."""
from pathlib import Path
import hashlib,json,subprocess
R=Path(__file__).resolve().parent;O=R/'build/midi-tools';O.mkdir(parents=True,exist_ok=True)
sources=['src/midi_wire.c','src/midi_wire.h','src/midi_voice.c','src/midi_voice.h','src/midi_route.c','src/midi_route.h','src/midi_event.c','src/midi_event.h','src/midi_tools.c','src/midi_tools.h','src/midi_alsa.c','src/midi_ui.c','src/midi_ui.h','check_midi_tools.c','check_midi_ui.c','check_midi_alsa.c','check_midi_alsa_fake.c','verify_midi_tools.py']
sha=lambda p:hashlib.sha256((R/p).read_bytes()).hexdigest();before={p:sha(p) for p in sources}
def run(args):
 p=subprocess.run(args,cwd=R,capture_output=True,text=True,timeout=60)
 if p.returncode:print(p.stdout+p.stderr,flush=True);p.check_returncode()
 return p
flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-pthread']
proof=[]
for architecture,cc,prefix in [('host','gcc',[]),('arm64','aarch64-linux-gnu-gcc',['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808'])]:
 for mode,extra in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
  target=O/(architecture+'-'+mode)
  run([cc,*flags,*extra,'check_midi_tools.c','src/midi_tools.c','-o',str(target)])
  p=run([*prefix,str(target)]);print(architecture,mode,p.stdout.strip(),flush=True);proof.append(p.stdout.strip())
  ui_target=O/('ui-'+architecture+'-'+mode)
  run([cc,*flags,*extra,'check_midi_ui.c','src/midi_tools.c','-o',str(ui_target)])
  p=run([*prefix,str(ui_target)]);print(architecture,mode,p.stdout.strip(),flush=True);proof.append(p.stdout.strip())
 # Exercise RTLD_NEXT through a separate shared library, not direct fake callbacks.
 fake=O/('libmidi-fake-'+architecture+'.so');bridge=O/('libmidi-bridge-'+architecture+'.so');target=O/('io-'+architecture)
 run([cc,*flags,'-fPIC','-shared','check_midi_alsa_fake.c','-o',str(fake)])
 run([cc,*flags,'-fPIC','-shared','src/midi_alsa.c','src/midi_tools.c','src/midi_wire.c','src/midi_voice.c','src/midi_route.c','src/midi_event.c','-lm','-ldl','-o',str(bridge)])
 run([cc,*flags,'check_midi_alsa.c',str(bridge),str(fake),'-Wl,-rpath,'+str(O),'-o',str(target)])
 p=run([*prefix,str(target)]);(O/('io-'+architecture+'.log')).write_text(p.stdout+p.stderr);print(p.stdout.strip(),flush=True);proof.append(p.stdout.strip())
 # Compile the real UI with the same target compiler and strict ABI flags.
 run([cc,*flags,'-fPIC','-c','src/midi_ui.c','-o',str(O/('ui-'+architecture+'.o'))])
assert before=={p:sha(p) for p in sources}
(O/'verification.json').write_text(json.dumps(dict(passed=True,sources_sha256=before,checks=proof,hardware_tested=False,card_written=False),indent=2)+'\n')
