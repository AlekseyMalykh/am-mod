"""Source ownership domain, with real concurrent frees under ASan; no app build."""
from pathlib import Path
import hashlib,json,os,subprocess
R=Path(__file__).resolve().parent;O=R/'build/pattern-source';O.mkdir(exist_ok=True)
sources=['src/pattern_source.cc','check_pattern_source.cc']
tracked=sources+['src/pattern_source.h','src/pattern_request.h','src/pattern_native.h','src/pattern_model.h','src/midi_event.h','src/step_native.h','verify_pattern_source.py']
digest=lambda:{f:hashlib.sha256((R/f).read_bytes()).hexdigest() for f in tracked}
before=digest();proof=O/'verification.json';proof.unlink(missing_ok=True);checks=[]
for name,prefix,flags,runner in [('host','',[],[]),('host-asan','',['-fsanitize=address,undefined','-fno-sanitize-recover=all'],[]),('arm','aarch64-linux-gnu-',[],['qemu-aarch64','-L',os.environ.get('MPC_QEMU_SYSROOT','/home/user/mpc-rootfs-user-20260808')]),('arm-ubsan','aarch64-linux-gnu-',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'],['qemu-aarch64','-L',os.environ.get('MPC_QEMU_SYSROOT','/home/user/mpc-rootfs-user-20260808')])]:
    target=O/name
    subprocess.run([prefix+'g++','-std=c++11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*sources,'-pthread','-o',str(target)],cwd=R,check=True)
    result=subprocess.run([*runner,str(target)],cwd=R,text=True,capture_output=True,timeout=45)
    (O/(name+'.log')).write_text(result.stdout+result.stderr);print(name,result.stdout+result.stderr,flush=True);result.check_returncode()
    assert 'PASS source lifetime domain:' in result.stdout;checks.append(name)
assert before==digest()
proof.write_text(json.dumps(dict(passed=True,checks=checks,sources_sha256=before,concurrent_free_tested=True,production_writer_coverage=False,lifecycle_callbacks='fixtures',application_built=False,hardware_tested=False),indent=2)+'\n')
