"""Isolated control-thread model checks. No application/preload/package build."""
from pathlib import Path
import hashlib,json,os,subprocess
R=Path(__file__).resolve().parent;O=R/'build/pattern-model';O.mkdir(exist_ok=True)
sources=['src/pattern_model.cc','src/midi_event.c','check_pattern_model.cc']
tracked=sources+['src/pattern_model.h','src/midi_event.h','src/step_native.h','verify_pattern_model.py']
digest=lambda:{f:hashlib.sha256((R/f).read_bytes()).hexdigest() for f in tracked}
before=digest();proof=O/'verification.json';proof.unlink(missing_ok=True);checks=[]
for name,prefix,flags,runner in [('host','',[],[]),('host-asan','',['-fsanitize=address,undefined','-fno-sanitize-recover=all'],[]),('arm','aarch64-linux-gnu-',[],['qemu-aarch64','-L',os.environ.get('MPC_QEMU_SYSROOT','/home/user/mpc-rootfs-user-20260808')]),('arm-ubsan','aarch64-linux-gnu-',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'],['qemu-aarch64','-L',os.environ.get('MPC_QEMU_SYSROOT','/home/user/mpc-rootfs-user-20260808')])]:
    objects=[]
    for source in sources:
        cpp=source.endswith('.cc');obj=O/(name+'-'+Path(source).name+'.o');objects.append(str(obj))
        subprocess.run([prefix+('g++' if cpp else 'gcc'),'-std=c++11' if cpp else '-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-c',source,'-o',str(obj)],cwd=R,check=True)
    target=O/name;subprocess.run([prefix+'g++',*flags,*objects,'-o',str(target)],cwd=R,check=True)
    result=subprocess.run([*runner,str(target)],cwd=R,text=True,capture_output=True,timeout=45)
    (O/(name+'.log')).write_text(result.stdout+result.stderr);print(name,result.stdout+result.stderr,flush=True);result.check_returncode()
    assert 'PASS pattern model:' in result.stdout;checks.append(name)
assert before==digest()
proof.write_text(json.dumps(dict(passed=True,checks=checks,sources_sha256=before,native_input='value fixtures',native_choice_getter='fixture',native_snapshot_adapter=False,application_built=False,ui_connected=False,persistence_connected=False,playback_connected=False,hardware_tested=False),indent=2)+'\n')
