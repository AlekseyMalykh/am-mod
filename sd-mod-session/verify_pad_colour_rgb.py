"""ARM/QEMU regression for Pad Colour leaking into FX Select. No SD writes."""
from pathlib import Path
import hashlib,json,subprocess
R=Path(__file__).resolve().parent
O=R/'build/pad-colour-rgb';O.mkdir(parents=True,exist_ok=True)
sources=['check_pad_colour_rgb.c','check_pad_colour_steps.c','src/phone_native.c','src/phone_input.c','src/music_settings.c','src/step_native.c','src/step_native.h','src/step_abi.S','src/pad_palette.h','verify_pad_colour_rgb.py']
hashes={n:hashlib.sha256((R/n).read_bytes()).hexdigest() for n in sources}
for name,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
 for case,inputs in [('rgb',['check_pad_colour_rgb.c','src/phone_input.c','src/music_settings.c']),('steps',['check_pad_colour_steps.c','src/music_settings.c','src/step_abi.S'])]:
  target=O/(name+'-'+case)
  subprocess.run(['aarch64-linux-gnu-gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Ibuild',
  '-mbranch-protection=standard','-ffunction-sections','-fdata-sections','-Wl,--gc-sections',*flags,
  *inputs,'-pthread','-ldl','-lm','-o',str(target)],cwd=R,check=True)
  p=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(target)],cwd=R,text=True,capture_output=True,timeout=30)
  (O/(name+'-'+case+'.log')).write_text(p.stdout+p.stderr)
  print(p.stdout+p.stderr,end='',flush=True);p.check_returncode();assert 'PASS ' in p.stdout
assert hashes=={n:hashlib.sha256((R/n).read_bytes()).hexdigest() for n in sources}
(O/'verification.json').write_text(json.dumps(dict(passed=True,ubsan=True,sources_sha256=hashes,hardware_tested=False),indent=2)+'\n')
