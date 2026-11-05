"""Render both metadata bands with padded rows and guard bytes on ARM."""
from pathlib import Path
import hashlib,json,subprocess
R=Path(__file__).resolve().parent;O=R/'build/early-drm'
sources=['check_branding.c','src/mod_build.h','src/branding_font_aa.h','src/splash_branding.h','src/am_mod_splash.inc']
sha=lambda b:hashlib.sha256(b).hexdigest();before={n:sha((R/n).read_bytes()) for n in sources}
for name,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
 exe=O/('branding-'+name)
 subprocess.run(['aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror',*flags,'check_branding.c','-o',str(exe)],cwd=R,check=True)
 subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(exe),str(O/'branding.ppm')],cwd=R,check=True)
assert before=={n:sha((R/n).read_bytes()) for n in sources}
(O/'branding-verification.json').write_text(json.dumps(dict(passed=True,ubsan=True,sources_sha256=before,image_sha256=sha((O/'branding.ppm').read_bytes())),indent=2)+'\n')
