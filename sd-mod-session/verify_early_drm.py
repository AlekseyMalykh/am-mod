"""DRM scanout bounds, ownership/errors and immutable-loader integration."""
from pathlib import Path
import hashlib,json,subprocess,zipfile
from early_drm_startup import transform
R=Path(__file__).resolve().parent;O=R/'build/early-drm';O.mkdir(exist_ok=True)
sources=['src/early_drm_splash.c','src/early_drm_startup.sh','src/am_mod_splash.inc','check_early_drm.c','early_drm_startup.py','verify_early_drm.py']
sha=lambda b:hashlib.sha256(b).hexdigest();before={n:sha((R/n).read_bytes()) for n in sources}
sources+=['src/mod_build.h','src/splash_branding.h','src/branding_font.h']
before={n:sha((R/n).read_bytes()) for n in sources}
for name,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
 exe=O/name;subprocess.run(['aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror',*flags,'check_early_drm.c','-o',str(exe)],cwd=R,check=True)
 p=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(exe)],capture_output=True,text=True,timeout=30,cwd=R)
 (O/(name+'.log')).write_text(p.stdout+p.stderr);print(p.stdout+p.stderr,end='',flush=True);p.check_returncode()
helper=O/'boot-splash';subprocess.run(['aarch64-linux-gnu-gcc','-O2','-static','-Wall','-Wextra','-Werror','src/early_drm_splash.c','-o',str(helper)],cwd=R,check=True)
with zipfile.ZipFile(R.parent/'am-mod-r26z.zip') as z:
 text=transform(z.read('autorun/mod-launcher.sh').decode(),sha(z.read('autorun/boot-splash')),sha(helper.read_bytes()))
(O/'mod-launcher.sh').write_text(text);subprocess.run(['sh','-n',str(O/'mod-launcher.sh')],check=True)
assert text.count('boot_display_start ||')==2 and 'boot_display_stop\n            systemctl --no-block start' in text
assert text.index('DRM child')<text.index('early_watch() {')
assert before=={n:sha((R/n).read_bytes()) for n in sources}
(O/'verification.json').write_text(json.dumps(dict(passed=True,ubsan=True,active_scanout_verified=True,master_release_verified=True,cleanup_boundaries_verified=True,unchanged_application=True,sources_sha256=before,splash_sha256=sha(helper.read_bytes()),physical_visibility_tested=False),indent=2)+'\n')
print('PASS DRM display ownership, failure unwind and two-call idempotent launcher integration',flush=True)
