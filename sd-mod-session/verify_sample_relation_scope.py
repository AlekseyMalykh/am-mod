"""Exercise the production UI scope predicate with every overlay built in."""
from pathlib import Path
import hashlib,json,subprocess
R=Path(__file__).resolve().parent;O=R/'build/sample-relation-scope';O.mkdir(exist_ok=True)
tracked=['src/phone_native.c','src/step_native.c','src/step_native.h','src/music_settings.c','src/music_settings.h','check_sample_relation_scope.c','verify_sample_relation_scope.py']
sha=lambda n:hashlib.sha256((R/n).read_bytes()).hexdigest();before={n:sha(n) for n in tracked}
for name,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
 target=O/name
 subprocess.run(['aarch64-linux-gnu-gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Ibuild','-mbranch-protection=standard','-ffunction-sections','-fdata-sections','-Wl,--gc-sections',*flags,'check_sample_relation_scope.c','-pthread','-ldl','-lm','-o',str(target)],cwd=R,check=True)
 p=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(target)],cwd=R,capture_output=True,text=True,timeout=30)
 (O/(name+'.log')).write_text(p.stdout+p.stderr);print(p.stdout+p.stderr,end='',flush=True);p.check_returncode()
assert before=={n:sha(n) for n in tracked}
(O/'verification.json').write_text(json.dumps(dict(passed=True,ubsan=True,sources_sha256=before,hardware_tested=False),indent=2)+'\n')
