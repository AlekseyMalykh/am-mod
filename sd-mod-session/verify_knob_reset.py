from pathlib import Path
import hashlib,json,subprocess
R=Path(__file__).resolve().parent;O=R/'build/knob-reset';O.mkdir(exist_ok=True)
sources=['src/knob_reset.c','src/knob_reset.h','src/phone_events.h','src/phone_events.c','src/phone_native.c','src/music_ui.c','src/note_native.c','src/compressor_native.c','src/compressor_ui.c','src/step_native.c','src/chords_ui.c','check_knob_reset.c','verify_knob_reset.py']
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();before={n:sha(R/n) for n in sources}
for name,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
 exe=O/name
 subprocess.run(['aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror',*flags,'check_knob_reset.c','src/phone_events.c','-pthread','-o',str(exe)],cwd=R,check=True)
 p=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(exe)],cwd=R,text=True,capture_output=True,timeout=30)
 (O/(name+'.log')).write_text(p.stdout+p.stderr);print(p.stdout+p.stderr,end='',flush=True);p.check_returncode()
assert before=={n:sha(R/n) for n in sources}
(O/'verification.json').write_text(json.dumps(dict(passed=True,ubsan=True,sources_sha256=before,physical_device_tested=False),indent=2)+'\n')
