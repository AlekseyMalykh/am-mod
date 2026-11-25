"""Real stock Events ownership/cleanup, isolated ARM/QEMU; no Project or app build."""
from pathlib import Path
import argparse,ast,hashlib,json,os,subprocess
p=argparse.ArgumentParser();p.add_argument('--stock',required=True);a=p.parse_args()
R=Path(__file__).resolve().parent;stock=Path(a.stock).resolve();O=R/'build/arrangement-native-material';O.mkdir(parents=True,exist_ok=True)
expected='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
assert hashlib.sha256(stock.read_bytes()).hexdigest()==expected
sources=['src/arrangement_native_material.cc','src/midi_event.c','check_arrangement_material_fixture.c','check_arrangement_native_material.cc']
sources+=['src/pattern_tools.cc','src/pattern_model.cc','src/pattern_library.cc','src/pattern_codec.cc','src/arrangement.cc','src/arrangement_codec.cc','src/arrangement_native_plan.cc','src/piano_roll.c']
tracked=sources+['src/arrangement_native_material.h','src/arrangement_native_plan.h','src/arrangement.h','src/pattern_library.h','src/pattern_model.h','src/midi_event.h','src/step_native.h','check_fx_native.c','verify_arrangement_native_material.py']
tracked+=['src/pattern_tools.h','src/piano_roll.h']
tracked+=['build_mixer_r27r.py']
digest=lambda:{s:hashlib.sha256((R/s).read_bytes()).hexdigest() for s in tracked}
before=digest();proof=O/'verification.json';proof.unlink(missing_ok=True);checks=[]
for name,extra in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
    objects=[]
    for source in sources:
        cpp=source.endswith('.cc');obj=O/(name+'-'+Path(source).name+'.o');objects.append(str(obj))
        subprocess.run(['aarch64-linux-gnu-g++' if cpp else 'aarch64-linux-gnu-gcc','-std=c++11' if cpp else '-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard',*extra,'-c',source,'-o',str(obj)],cwd=R,check=True)
    target=O/name;subprocess.run(['aarch64-linux-gnu-g++',*extra,'-Wl,--export-dynamic',*objects,'-ldl','-pthread','-lm','-o',str(target)],cwd=R,check=True)
    result=subprocess.run(['qemu-aarch64','-L',os.environ.get('MPC_QEMU_SYSROOT','/home/user/mpc-rootfs-user-20260808'),str(target),str(stock)],cwd=R,text=True,capture_output=True,timeout=90)
    (O/(name+'.log')).write_text(result.stdout+result.stderr);print(name,result.stdout+result.stderr,flush=True);result.check_returncode();assert all(label in result.stdout for label in ['PASS native Arrange material:','PASS material Store bridge:','PASS native Holder lifetime:']);checks.append(name)
tree=ast.parse((R/'build_mixer_r27r.py').read_text())
common=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='common' for t in n.targets))
for source in ['src/arrangement_native_material.cc','src/pattern_tools.cc']:
    subprocess.run(['aarch64-linux-gnu-g++','-std=c++11',*common,'-DMPC_PATTERN_TOOLS_TRIAL=1','-c',source,'-o',str(O/('production-'+Path(source).stem+'.o'))],cwd=R,check=True)
assert before==digest()
proof.write_text(json.dumps(dict(passed=True,checks=checks,sources_sha256=before,stock_sha256=expected,native_type='SafeLifetime::Holder<UnsafeSequenceEventList>',native_events_object_size=0x160,native_unsafe_base_size=0x148,native_ctor='0x170a2f0',native_insert='0x27ee5d0',native_dtor='0x14c4a20',native_source_retirement_wait_tested=True,native_audio_cache_publication_tested=False,native_allocations_tracked=True,native_exception_unwind='real ELF .eh_frame registered only in fixture',events_limit=32768,origin_index_tested=True,store_edit_reset_during_native_allocation_tested=True,native_retirement_outside_store_mutex_tested=True,production_objects_compiled=True,native_destination_objects_used=False,transport_connected=False,application_built=False,hardware_tested=False),indent=2)+'\n')
