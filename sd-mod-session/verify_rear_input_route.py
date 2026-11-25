"""Rear signal UI/control regression, ARM QEMU; no device."""
from pathlib import Path
import subprocess,json,hashlib
R=Path(__file__).resolve().parent;O=R/'build/rear-input-route';O.mkdir(parents=True,exist_ok=True)
tracked=['src/rear_ui.c','src/mixer_control.c','src/mixer_view.c','src/mixer_view.h','check_rear_ui.c','check_rear_input_route.c',Path(__file__).name]
sha=lambda n:hashlib.sha256((R/n).read_bytes()).hexdigest();before={n:sha(n) for n in tracked}
for variant,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
 for name,sources in [('route',['check_rear_input_route.c','src/mixer_control.c']),('ui',['check_rear_ui.c'])]:
  target=O/(name+'-'+variant)
  subprocess.run(['aarch64-linux-gnu-gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard','-ffunction-sections','-fdata-sections',*flags,*sources,'-Wl,--gc-sections','-lm','-o',str(target)],cwd=R,check=True)
  result=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(target)],cwd=R,capture_output=True,text=True,timeout=30)
  (O/(name+'-'+variant+'.log')).write_text(result.stdout+result.stderr);print(result.stdout+result.stderr,end='',flush=True);result.check_returncode()
assert before=={n:sha(n) for n in tracked}
(O/'verification.json').write_text(json.dumps(dict(passed=True,sources_sha256=before,hardware_tested=False),indent=2)+'\n')
