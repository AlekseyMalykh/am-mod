"""ARM production-control regression; native widgets/drawing are fixtures."""
from pathlib import Path
import hashlib,json,subprocess
R=Path(__file__).resolve().parent
O=R/'build/pad-colour-controls';O.mkdir(exist_ok=True)
sources=['check_pad_colour_controls.c','src/note_native.c','src/note_native.h','src/music_settings.c','src/music_settings.h','verify_pad_colour_controls.py']
hashes={n:hashlib.sha256((R/n).read_bytes()).hexdigest() for n in sources}
for name,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
 target=O/name
 subprocess.run(['aarch64-linux-gnu-gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard',*flags,'check_pad_colour_controls.c','src/music_settings.c','-lm','-pthread','-o',str(target)],cwd=R,check=True)
 p=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(target)],cwd=R,text=True,capture_output=True,timeout=30)
 (O/(name+'.log')).write_text(p.stdout+p.stderr);print(p.stdout+p.stderr,end='');p.check_returncode()
assert hashes=={n:hashlib.sha256((R/n).read_bytes()).hexdigest() for n in sources}
(O/'verification.json').write_text(json.dumps(dict(passed=True,ubsan=True,sources_sha256=hashes,hardware_tested=False),indent=2)+'\n')
