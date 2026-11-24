"""Exact phone lifecycle include, baseline/trial; no application build."""
from pathlib import Path
import ast,hashlib,json,os,subprocess
R=Path(__file__).resolve().parent;O=R/'build/pattern-lifecycle';O.mkdir(exist_ok=True)
sources=['src/pattern_source.cc','src/pattern_lifecycle.cc','check_pattern_lifecycle_phone.c','check_pattern_lifecycle.cc']
tracked=sources+['src/pattern_lifecycle.h','src/pattern_phone_lifecycle.inc','src/pattern_source.h','src/pattern_request.h','src/pattern_native.h','src/pattern_model.h','src/midi_event.h','src/step_native.h','src/phone_native.c','build_mixer_r27r.py','verify_pattern_lifecycle.py']
digest=lambda:{f:hashlib.sha256((R/f).read_bytes()).hexdigest() for f in tracked}
before=digest();proof=O/'verification.json';proof.unlink(missing_ok=True);checks=[]
for mode,prefix,flags,runner in [('host','',[],[]),('host-asan','',['-fsanitize=address,undefined','-fno-sanitize-recover=all'],[]),('arm','aarch64-linux-gnu-',[],['qemu-aarch64','-L',os.environ.get('MPC_QEMU_SYSROOT','/home/user/mpc-rootfs-user-20260808')]),('arm-ubsan','aarch64-linux-gnu-',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'],['qemu-aarch64','-L',os.environ.get('MPC_QEMU_SYSROOT','/home/user/mpc-rootfs-user-20260808')])]:
    for trial in (False,True):
        name=mode+('-trial' if trial else '-baseline');extra=['-DAM_PATTERN_LIFECYCLE_TEST_ACCESS=1']+(['-DMPC_PATTERN_SOURCE_TRIAL=1'] if trial else []);objects=[]
        for source in sources:
            cpp=source.endswith('.cc');obj=O/(name+'-'+Path(source).name+'.o');objects.append(str(obj))
            subprocess.run([prefix+('g++' if cpp else 'gcc'),'-std=c++11' if cpp else '-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*extra,'-c',source,'-o',str(obj)],cwd=R,check=True)
        target=O/name;subprocess.run([prefix+'g++',*flags,*objects,'-pthread','-o',str(target)],cwd=R,check=True)
        result=subprocess.run([*runner,str(target)],cwd=R,text=True,capture_output=True,timeout=45)
        (O/(name+'.log')).write_text(result.stdout+result.stderr);print(name,result.stdout+result.stderr,flush=True);result.check_returncode();assert 'PASS phone lifecycle' in result.stdout;checks.append(name)
# Compile only, no library link. Production object must not expose test publish access.
prod=O/'production-lifecycle.o'
subprocess.run(['aarch64-linux-gnu-g++','-std=c++11','-Wall','-Wextra','-Werror','-c','src/pattern_lifecycle.cc','-o',str(prod)],cwd=R,check=True)
assert 'lifecycle_test_domain' not in subprocess.check_output(['aarch64-linux-gnu-nm','-C',str(prod)],text=True)
# Read supported common flags as data. Never import/execute the build script.
tree=ast.parse((R/'build_mixer_r27r.py').read_text())
common=next(ast.literal_eval(node.value) for node in tree.body if isinstance(node,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='common' for t in node.targets))
for flags in [[],['-DMPC_PATTERN_SOURCE_TRIAL=1'],['-DMPC_PATTERN_SOURCE_TRIAL=1','-DMPC_PAD_MIDI_RECORDING_TRIAL=1','-DMPC_SYNTH_NATIVE_PLAYBACK_TRIAL=1']]:
    subprocess.run(['aarch64-linux-gnu-gcc','-std=c11',*common,'-DMPC_MIDI_EVENTS=1',*flags,'-fsyntax-only','src/phone_native.c'],cwd=R,check=True)
assert before==digest()
proof.write_text(json.dumps(dict(passed=True,checks=checks,sources_sha256=before,phone_syntax_only_variants=3,production_test_publish_symbol=False,application_built=False,production_writer_coverage=False,runtime_admission='closed; no production publisher',hardware_tested=False),indent=2)+'\n')
