"""Exit-copy filesystem and failure boundaries; native saver remains a receiver."""
from pathlib import Path
import hashlib, json, subprocess
R=Path(__file__).resolve().parent
O=R/'build/project-exit';O.mkdir(exist_ok=True)
sources=['src/project_exit.cc','src/project_exit.h','src/music_archive.c','src/music_archive.h','check_project_exit.cc','verify_project_exit.py']
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
sources.append('src/project_exit_abi.S')
sources+=['src/project_recovery.c','src/project_recovery.h']
before={s:sha(R/s) for s in sources}
for name,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
 exe=O/name
 subprocess.run(['aarch64-linux-gnu-g++','-O1','-g','-Wall','-Wextra','-Werror','-std=c++11','-mbranch-protection=standard',*flags,'check_project_exit.cc','src/project_exit_abi.S','-pthread','-o',str(exe)],cwd=R,check=True)
 p=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(exe)],cwd=R,capture_output=True,text=True,timeout=30)
 (O/(name+'.log')).write_text(p.stdout+p.stderr);print(p.stdout+p.stderr,end='');p.check_returncode()
 # Execute the production worker with recovery origin capture enabled too.
 obj=O/(name+'-recovery.o');target=O/(name+'-with-recovery')
 declaration=O/'recovery-test-metadata.h';declaration.write_text('extern const char*project_recovery_test_metadata(void);\n')
 subprocess.run(['aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror','-std=c11',*flags,'-include',str(declaration),'-DPROJECT_RECOVERY_STORAGE=project_recovery_test_metadata()','-c','src/project_recovery.c','-o',str(obj)],cwd=R,check=True)
 # The fixture creates a unique metadata path. Its compiled storage override
 # is supplied by a small exported getter rather than a shared filesystem name.
 subprocess.run(['aarch64-linux-gnu-g++','-O1','-g','-Wall','-Wextra','-Werror','-std=c++11','-DMPC_PROJECT_RECOVERY=1',*flags,'check_project_exit.cc','src/project_exit_abi.S',str(obj),'-pthread','-o',str(target)],cwd=R,check=True)
 p=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(target)],cwd=R,capture_output=True,text=True,timeout=30)
 (O/(name+'-with-recovery.log')).write_text(p.stdout+p.stderr);print(p.stdout+p.stderr,end='');p.check_returncode()
assert before=={s:sha(R/s) for s in sources}
(O/'verification.json').write_text(json.dumps({'passed':True,'sources_sha256':before,'native_saver':'receiver','hardware_tested':False},indent=2)+'\n')
