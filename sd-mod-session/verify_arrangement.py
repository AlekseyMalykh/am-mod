"""Isolated Stage 4 value model/compiler; never builds app/preload/package."""
from pathlib import Path
import hashlib,json,os,subprocess
R=Path(__file__).resolve().parent;O=R/'build/arrangement';O.mkdir(exist_ok=True)
sources=['src/arrangement.cc','src/arrangement_codec.cc','src/pattern_model.cc','src/pattern_library.cc','src/pattern_codec.cc','src/midi_event.c','check_arrangement.cc']
tracked=sources+['src/arrangement.h','src/pattern_model.h','src/pattern_library.h','src/midi_event.h','src/step_native.h','verify_arrangement.py']
digest=lambda:{s:hashlib.sha256((R/s).read_bytes()).hexdigest() for s in tracked}
before=digest();proof=O/'verification.json';proof.unlink(missing_ok=True);checks=[]
for name,prefix,flags,runner in [('host','',[],[]),('host-asan','',['-fsanitize=address,undefined','-fno-sanitize-recover=all'],[]),('arm','aarch64-linux-gnu-',[],['qemu-aarch64','-L',os.environ.get('MPC_QEMU_SYSROOT','/home/user/mpc-rootfs-user-20260808')]),('arm-ubsan','aarch64-linux-gnu-',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'],['qemu-aarch64','-L',os.environ.get('MPC_QEMU_SYSROOT','/home/user/mpc-rootfs-user-20260808')])]:
    objects=[]
    for source in sources:
        cpp=source.endswith('.cc');obj=O/(name+'-'+Path(source).name+'.o');objects.append(str(obj))
        subprocess.run([prefix+('g++' if cpp else 'gcc'),'-std=c++11' if cpp else '-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-c',source,'-o',str(obj)],cwd=R,check=True)
    target=O/name;subprocess.run([prefix+'g++',*flags,*objects,'-o',str(target)],cwd=R,check=True)
    result=subprocess.run([*runner,str(target)],cwd=R,text=True,capture_output=True,timeout=60)
    (O/(name+'.log')).write_text(result.stdout+result.stderr);print(name,result.stdout+result.stderr,flush=True);result.check_returncode();assert 'PASS arrangement:' in result.stdout;checks.append(name)
assert before==digest()
proof.write_text(json.dumps(dict(passed=True,checks=checks,sources_sha256=before,application_built=False,ui_connected=False,native_playback_connected=False,persistence_connected=False,hardware_tested=False),indent=2)+'\n')
