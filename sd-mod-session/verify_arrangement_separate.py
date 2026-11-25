"""Separate/copy selected Pattern events: immutable Store, no native pointers."""
from pathlib import Path
import hashlib,json,os,subprocess
R=Path(__file__).resolve().parent;O=R/'build/arrangement-separate';O.mkdir(exist_ok=True)
sources=['src/pattern_tools.cc','src/pattern_library.cc','src/pattern_model.cc','src/pattern_codec.cc','src/arrangement.cc','src/arrangement_codec.cc','src/arrangement_native_plan.cc','src/midi_event.c','src/piano_roll.c','check_arrangement_separate.cc']
sources+=['src/arrangement_native_material.cc']
tracked=sources+['src/pattern_tools.h','src/pattern_library.h','src/pattern_model.h','src/arrangement.h','src/arrangement_native_plan.h','src/piano_roll.h','src/step_native.h','src/midi_event.h','verify_arrangement_separate.py']
tracked+=['src/arrangement_native_material.h']
digest=lambda:{n:hashlib.sha256((R/n).read_bytes()).hexdigest() for n in tracked};before=digest();proof=O/'verification.json';proof.unlink(missing_ok=True);checks=[]
for name,prefix,flags,runner in [('host','',[],[]),('host-asan','',['-fsanitize=address,undefined','-fno-sanitize-recover=all'],[]),('arm','aarch64-linux-gnu-',[],['qemu-aarch64','-L',os.environ.get('MPC_QEMU_SYSROOT','/home/user/mpc-rootfs-user-20260808')]),('arm-ubsan','aarch64-linux-gnu-',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'],['qemu-aarch64','-L',os.environ.get('MPC_QEMU_SYSROOT','/home/user/mpc-rootfs-user-20260808')])]:
 objs=[]
 for source in sources:
  cpp=source.endswith('.cc');obj=O/(name+'-'+Path(source).name+'.o');objs.append(str(obj))
  subprocess.run([prefix+('g++' if cpp else 'gcc'),'-std=c++11' if cpp else '-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-c',source,'-o',str(obj)],cwd=R,check=True)
 target=O/name;subprocess.run([prefix+'g++',*flags,*objs,'-pthread','-o',str(target)],cwd=R,check=True)
 p=subprocess.run([*runner,str(target)],cwd=R,capture_output=True,text=True,timeout=90);(O/(name+'.log')).write_text(p.stdout+p.stderr);print(name,p.stdout+p.stderr,flush=True);p.check_returncode();assert 'PASS Separate Store:' in p.stdout;checks.append(name)
assert before==digest()
proof.write_text(json.dumps(dict(passed=True,checks=checks,sources_sha256=before,original_pattern_unchanged=True,other_references_unchanged=True,atomic_undo=True,stale_receipt_checked=True,full_xpj_roundtrip=False,hardware_tested=False),indent=2)+'\n')
