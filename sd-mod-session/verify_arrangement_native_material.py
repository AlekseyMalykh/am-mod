"""Real stock Events ownership/cleanup, isolated ARM/QEMU; no Project or app build."""
from pathlib import Path
import argparse,ast,hashlib,json,os,subprocess
p=argparse.ArgumentParser();p.add_argument('--stock',required=True);variant=p.add_mutually_exclusive_group();variant.add_argument('--cache',action='store_true');variant.add_argument('--queue',action='store_true');a=p.parse_args()
R=Path(__file__).resolve().parent;stock=Path(a.stock).resolve();O=R/'build'/('arrangement-native-queue' if a.queue else 'arrangement-native-cache' if a.cache else 'arrangement-native-material');O.mkdir(parents=True,exist_ok=True)
expected='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
assert hashlib.sha256(stock.read_bytes()).hexdigest()==expected
sources=['src/arrangement_native_material.cc','src/midi_event.c','check_arrangement_material_fixture.c','check_arrangement_native_material.cc']
sources+=['src/pattern_tools.cc','src/pattern_model.cc','src/pattern_library.cc','src/pattern_codec.cc','src/arrangement.cc','src/arrangement_codec.cc','src/arrangement_native_plan.cc','src/piano_roll.c']
tracked=sources+['src/arrangement_native_material.h','src/arrangement_native_plan.h','src/arrangement.h','src/pattern_library.h','src/pattern_model.h','src/midi_event.h','src/step_native.h','check_fx_native.c','verify_arrangement_native_material.py']
tracked+=['src/pattern_tools.h','src/piano_roll.h']
tracked+=['build_mixer_r27r.py']
if a.cache:
    sources[sources.index('check_arrangement_native_material.cc')]='check_arrangement_native_cache.cc'
    tracked+=['check_arrangement_native_cache.cc']
if a.queue:
    sources[sources.index('check_arrangement_native_material.cc')]='check_arrangement_native_queue.cc'
    sources+=['src/arrangement_native_queue.cc']
    tracked+=['check_arrangement_native_queue.cc','src/arrangement_native_queue.cc','src/arrangement_native_queue.h']
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
    if a.cache:assert 'PASS native cache handoff:' in result.stdout
    if a.queue:assert 'PASS native Arrange queue:' in result.stdout
tree=ast.parse((R/'build_mixer_r27r.py').read_text())
common=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='common' for t in n.targets))
for source in ['src/arrangement_native_material.cc','src/pattern_tools.cc']+(['src/arrangement_native_queue.cc'] if a.queue else []):
    subprocess.run(['aarch64-linux-gnu-g++','-std=c++11',*common,'-DMPC_PATTERN_TOOLS_TRIAL=1','-c',source,'-o',str(O/('production-'+Path(source).stem+'.o'))],cwd=R,check=True)
assert before==digest()
proof.write_text(json.dumps(dict(passed=True,checks=checks,sources_sha256=before,stock_sha256=expected,native_type='SafeLifetime::Holder<UnsafeSequenceEventList>',native_events_object_size=0x160,native_unsafe_base_size=0x148,native_ctor='0x170a2f0',native_insert='0x27ee5d0',native_dtor='0x14c4a20',native_source_retirement_wait_tested=True,native_audio_cache_publication_tested=False,native_allocations_tracked=True,native_exception_unwind='real ELF .eh_frame registered only in fixture',events_limit=32768,origin_index_tested=True,store_edit_reset_during_native_allocation_tested=True,native_retirement_outside_store_mutex_tested=True,production_objects_compiled=True,native_destination_objects_used=False,transport_connected=False,application_built=False,hardware_tested=False),indent=2)+'\n')

if a.cache:
    data=json.loads(proof.read_text())
    data.update(native_cache_copy_and_audio_swap_tested=True,
        native_cache_copy='0x170b060',native_cache_audio_swap='0x17088c0',
        native_cache_wrapper_fixture=True,native_queue_fixture=True,native_cache_guard_binding_fixture=True,
        native_cache_full_constructor_tested=False,copied_origin_index_tested=True,
        native_cache_copy_allocation_failures_tested=True,audio_allocation_forbidden=True,
        native_audio_cache_publication_tested=False)
    proof.write_text(json.dumps(data,indent=2)+'\n')
if a.queue:
    data=json.loads(proof.read_text())
    data.update(native_cache_copy_and_swap_tested=True,native_queue_fixture=False,
        native_queue_storage_fixture=True,native_cache_wrapper_fixture=True,
        native_cache_owner_and_guard_binding_fixture=True,native_cache_full_constructor_tested=False,
        native_queue='0x2b59300',native_runner='0x2b59200/0x2b59160',native_reclaim='0x2b58c70',
        separate_consumer_thread=True,full_queue_no_inline_tested=True,ring_reuse_tested=True,
        native_cache_copy_allocation_failures_tested=True,audio_allocation_forbidden=True,
        production_owner_provider_installed=False)
    proof.write_text(json.dumps(data,indent=2)+'\n')
