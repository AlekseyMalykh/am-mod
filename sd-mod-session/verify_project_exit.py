"""Exit-copy filesystem and failure boundaries; native saver remains a receiver."""
from pathlib import Path
import hashlib, json, subprocess
R=Path(__file__).resolve().parent
O=R/'build/project-exit';O.mkdir(exist_ok=True)
sources=['src/project_exit.cc','src/project_exit.h','src/music_archive.c','src/music_archive.h','check_project_exit.cc','verify_project_exit.py']
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
sources.append('src/project_exit_abi.S')
before={s:sha(R/s) for s in sources}
for name,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
 exe=O/name
 subprocess.run(['aarch64-linux-gnu-g++','-O1','-g','-Wall','-Wextra','-Werror','-std=c++11','-mbranch-protection=standard',*flags,'check_project_exit.cc','src/project_exit_abi.S','-pthread','-o',str(exe)],cwd=R,check=True)
 p=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(exe)],cwd=R,capture_output=True,text=True,timeout=30)
 (O/(name+'.log')).write_text(p.stdout+p.stderr);print(p.stdout+p.stderr,end='');p.check_returncode()
assert before=={s:sha(R/s) for s in sources}
(O/'verification.json').write_text(json.dumps({'passed':True,'sources_sha256':before,'native_saver':'receiver','hardware_tested':False},indent=2)+'\n')
