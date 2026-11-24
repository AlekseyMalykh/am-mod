"""Pinned native-ELF adapter checks. No application/preload/SD build."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess
p=argparse.ArgumentParser();p.add_argument('--stock',required=True);a=p.parse_args()
R=Path(__file__).resolve().parent;stock=Path(a.stock).resolve()
expected='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
assert hashlib.sha256(stock.read_bytes()).hexdigest()==expected
O=R/'build/pattern-native';O.mkdir(exist_ok=True);proof=O/'verification.json';proof.unlink(missing_ok=True)
sources=['src/pattern_native.cc','src/pattern_model.cc','src/midi_event.c','check_pattern_native_fixture.c','check_pattern_native.cc']
tracked=sources+['src/pattern_native.h','src/pattern_model.h','src/midi_event.h','src/step_native.h','check_fx_native.c','verify_pattern_native.py']
digest=lambda:{f:hashlib.sha256((R/f).read_bytes()).hexdigest() for f in tracked}
before=digest();checks=[]
for name,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
    objects=[]
    for source in sources:
        cpp=source.endswith('.cc');obj=O/(name+'-'+Path(source).name+'.o');objects.append(str(obj))
        subprocess.run(['aarch64-linux-gnu-g++' if cpp else 'aarch64-linux-gnu-gcc','-std=c++11' if cpp else '-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-c',source,'-o',str(obj)],cwd=R,check=True)
    target=O/name;subprocess.run(['aarch64-linux-gnu-g++',*flags,*objects,'-ldl','-pthread','-lm','-o',str(target)],cwd=R,check=True)
    result=subprocess.run(['qemu-aarch64','-L',os.environ.get('MPC_QEMU_SYSROOT','/home/user/mpc-rootfs-user-20260808'),str(target),str(stock)],cwd=R,text=True,capture_output=True,timeout=60)
    (O/(name+'.log')).write_text(result.stdout+result.stderr);print(name,result.stdout+result.stderr,flush=True);result.check_returncode()
    assert 'PASS native pattern adapter:' in result.stdout;checks.append(name)
assert before==digest()
proof.write_text(json.dumps(dict(passed=True,checks=checks,sources_sha256=before,stock_sha256=expected,real_native_sequence_getter='0x2909be0',real_native_clip_list_getter='0x2957ae0(map_selector=0)',real_native_length_getter='0x29571f0',real_native_event_insert='0x27ee5d0',real_native_choice_getter='0x27e4230',event_traversal='bounded pinned libstdc++ tree ABI, native successor used for fixture cross-check',executor_and_lifetime_lease='fixtures, production not wired',routing_values='fixture copy in lease',allocation_failure_injection=True,application_built=False,ui_connected=False,persistence_connected=False,playback_connected=False,hardware_tested=False),indent=2)+'\n')
