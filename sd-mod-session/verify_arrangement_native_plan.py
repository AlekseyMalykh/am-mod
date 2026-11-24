"""Native destination preflight only; no app build, native writes or playback."""
from pathlib import Path
import hashlib,json,os,subprocess
R=Path(__file__).resolve().parent;O=R/'build/arrangement-native-plan';O.mkdir(parents=True,exist_ok=True)
sources=['src/arrangement_native_plan.cc','src/arrangement.cc','src/pattern_model.cc','src/pattern_library.cc','src/pattern_codec.cc','src/midi_event.c','check_arrangement_native_plan.cc']
tracked=sources+['src/arrangement_native_plan.h','src/arrangement.h','src/pattern_model.h','src/pattern_library.h','src/midi_event.h','src/step_native.h','verify_arrangement_native_plan.py']
digest=lambda:{s:hashlib.sha256((R/s).read_bytes()).hexdigest() for s in tracked};before=digest();proof=O/'verification.json';proof.unlink(missing_ok=True);checks=[]
sysroot=os.environ.get('MPC_QEMU_SYSROOT','/home/user/mpc-rootfs-user-20260808')
for name,prefix,flags,runner in [('host','',[],[]),('host-asan','',['-fsanitize=address,undefined','-fno-sanitize-recover=all'],[]),('arm','aarch64-linux-gnu-',[],['qemu-aarch64','-L',sysroot]),('arm-ubsan','aarch64-linux-gnu-',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'],['qemu-aarch64','-L',sysroot])]:
    objects=[]
    for source in sources:
        cpp=source.endswith('.cc');obj=O/(name+'-'+Path(source).name+'.o');objects.append(str(obj))
        subprocess.run([prefix+('g++' if cpp else 'gcc'),'-std=c++11' if cpp else '-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-c',source,'-o',str(obj)],cwd=R,check=True)
    target=O/name;subprocess.run([prefix+'g++',*flags,*objects,'-o',str(target)],cwd=R,check=True)
    result=subprocess.run([*runner,str(target)],cwd=R,capture_output=True,text=True,timeout=60)
    (O/(name+'.log')).write_text(result.stdout+result.stderr);print(name,result.stdout+result.stderr,flush=True);result.check_returncode();assert 'PASS arrangement native plan:' in result.stdout;checks.append(name)
assert before==digest()
proof.write_text(json.dumps(dict(passed=True,checks=checks,sources_sha256=before,application_built=False,native_publication_connected=False,native_playback_connected=False,hardware_tested=False,choice_decoder='fixture seam only; native packed codec tested separately by verify_pattern_tools.py'),indent=2)+'\n')
