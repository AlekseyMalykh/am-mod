"""CPU cache/KMS regression model against production capture code, no device."""
from pathlib import Path
import hashlib,json,subprocess
R=Path(__file__).resolve().parent;O=R/'build/mirror';O.mkdir(parents=True,exist_ok=True)
tracked=['src/screen_mirror_drm.c','src/screen_mirror_frame.h','check_screen_mirror.c','verify_screen_mirror.py']
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();before={s:sha(R/s) for s in tracked}
for mode,sanitize in [('normal',[]),('san',['-fsanitize=address,undefined','-fno-sanitize-recover=all'])]:
 exe=O/mode
 subprocess.run(['gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*sanitize,
  f'-DSCREEN_MIRROR_FLAG="{O}/on"',f'-DSCREEN_MIRROR_FRAME="{O}/frame.bmp"','check_screen_mirror.c',
  '-Wl,--wrap=ioctl,--wrap=mmap,--wrap=munmap','-ldl','-pthread','-o',str(exe)],cwd=R,check=True)
 p=subprocess.run([str(exe)],capture_output=True,text=True,timeout=15);(O/(mode+'.log')).write_text(p.stdout+p.stderr);print(p.stdout+p.stderr,end='');p.check_returncode()
subprocess.run(['aarch64-linux-gnu-gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-fPIC','-c','src/screen_mirror_drm.c','-o',str(O/'screen_mirror_drm.o')],cwd=R,check=True)
assert before=={s:sha(R/s) for s in tracked}
(O/'verification.json').write_text(json.dumps(dict(passed=True,sources_sha256=before,presented_frame=True,dma_sync=True,host_sanitizers=True,hardware_tested=False),indent=2)+'\n')
