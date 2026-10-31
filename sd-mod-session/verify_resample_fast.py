"""Pinned private one-event export proof. No packaging or SD installation."""
from pathlib import Path
import hashlib,json,subprocess,zipfile
R=Path(__file__).resolve().parent;O=R/'build/resample-fast';O.mkdir(exist_ok=True)
stock=R.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(stock)=='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
sources=['src/resample_fast_native.cc','src/resample_fast_native.h','src/resample_fast_plan.c','src/resample_fast_plan.h','src/resample_fault.c','src/resample_fault.h','check_resample_fault.c','check_resample_writer.c','src/resample_output_file.h','check_resample_output_file.cc','check_resample_output_callback.cc','src/pad_source_abi.S','src/resample_stock.cc','src/resample_stock.h','src/resample_file_probe.h','check_resample_fast_plan.c','check_resample_fast_native.c','check_resample_pad_link.c','check_resample_fast_import.c','check_resample_service_context.c','check_fx_native.c','check_resample_stock.c','check_resample_stock_callback.cc','build/phone_hooks.h','verify_resample_fast.py']
before={s:sha(R/s) for s in sources}
archive=R.parent/'mpc-sample-r20-private-sample-seq.zip';assert sha(archive)=='b7e1431082718c3327b1b0f4000a0a6b73cb2fb2901450bf59d5753a16af5ee4'
with zipfile.ZipFile(archive) as z:
 for name in ['phone-code.bin','phone-table.bin','phone-muted-sequence-gate.bin','phone-muted-sequence_input-gate.bin']:
  assert (R/'build/r20/autorun'/name).read_bytes()==z.read('autorun/'+name)
for variant,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
 common=['-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard',*flags]
 def run(args):
  subprocess.run(args,cwd=R,check=True)
 def check(binary,*args):
  p=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(binary),*map(str,args)],cwd=R,capture_output=True,text=True,timeout=60)
  (O/(binary.name+'.log')).write_text(p.stdout+p.stderr);print(p.stdout+p.stderr,end='');p.check_returncode()
 output=O/(variant+'-output-file');run(['aarch64-linux-gnu-g++','-std=c++11',*common,'check_resample_output_file.cc','-o',str(output)]);check(output)
 writer=O/(variant+'-writer');run(['aarch64-linux-gnu-gcc',*common,'check_resample_writer.c','-ldl','-lm','-o',str(writer)]);check(writer,stock)
 obj=O/(variant+'-native.o');run(['aarch64-linux-gnu-g++','-std=c++11',*common,'-c','src/resample_fast_native.cc','-o',str(obj)])
 fault=O/(variant+'-fault');run(['aarch64-linux-gnu-gcc',*common,'check_resample_fault.c','src/resample_fault.c','-ldl','-pthread','-o',str(fault)]);check(fault)
 plan=O/(variant+'-plan');run(['aarch64-linux-gnu-gcc',*common,'check_resample_fast_plan.c','src/resample_fast_plan.c','-lm','-o',str(plan)]);check(plan)
 native=O/(variant+'-native');run(['aarch64-linux-gnu-gcc',*common,'check_resample_fast_native.c','src/pad_source_abi.S','src/resample_fast_plan.c','src/resample_fault.c',str(obj),'-lstdc++','-pthread','-ldl','-lm','-o',str(native)]);check(native,stock);check(native,stock,'existing-bridge');check(native,stock,'frozen-bridge');check(native,stock,'primary-writer');check(native,stock,'seq-trace')
 check(native,stock,'pad-link');check(native,stock,'physical-output')
 links=O/(variant+'-pad-link');run(['aarch64-linux-gnu-gcc',*common,'check_resample_pad_link.c','-ldl','-lm','-o',str(links)]);check(links,stock)
 adapter=O/(variant+'-stock.o');callback=O/(variant+'-callback.o')
 for source,dest in [('src/resample_stock.cc',adapter),('check_resample_stock_callback.cc',callback)]:
  run(['aarch64-linux-gnu-g++','-std=c++11',*common,'-DMPC_FAST_RESAMPLE=1','-c',source,'-o',str(dest)])
 output_callback=O/(variant+'-output-callback.o');run(['aarch64-linux-gnu-g++','-std=c++11',*common,'-c','check_resample_output_callback.cc','-o',str(output_callback)])
 imported=O/(variant+'-import');run(['aarch64-linux-gnu-gcc',*common,'check_resample_fast_import.c','src/resample_fault.c',str(adapter),str(callback),str(output_callback),'-lstdc++','-pthread','-ldl','-lm','-o',str(imported)]);check(imported,stock)
 service_obj=O/(variant+'-service-stock.o');run(['aarch64-linux-gnu-g++','-std=c++11',*common,'-c','src/resample_stock.cc','-o',str(service_obj)])
 service=O/(variant+'-service');run(['aarch64-linux-gnu-gcc',*common,'check_resample_service_context.c',str(service_obj),str(callback),'-lstdc++','-pthread','-ldl','-lm','-o',str(service)]);check(service,stock)
# Reproduce the exact device failure with the frozen r20a adapter and its
# original test, changing only the false filename field to the observed0x70.
repro=O/'r20a-repro';repro.mkdir(exist_ok=True)
def old(name):return subprocess.check_output(['git','-c','safe.directory='+str(R.parent),'show','39802e1:sd-mod-session/'+name],cwd=R,text=True)
(repro/'resample_fast_native.h').write_text(old('src/resample_fast_native.h'))
(repro/'native.cc').write_text(old('src/resample_fast_native.cc'))
fixture=old('check_resample_fast_native.c').replace('"src/resample_fast_native.h"','"resample_fast_native.h"')
needle='F(settings,0x6e8,const char*)="/tmp/mpc-private-export-not-written.wav";assert(resample_fast_native_arm(F(settings,0x6e8,const char*)));'
assert needle in fixture;fixture=fixture.replace(needle,needle+'F(settings,0x6e8,uintptr_t)=0x70;')
(repro/'check.c').write_text(fixture)
run(['aarch64-linux-gnu-g++','-std=c++11','-O1','-g','-Wall','-Wextra','-Werror','-Isrc','-c',str(repro/'native.cc'),'-o',str(repro/'native.o')])
run(['aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror','-I.',str(repro/'check.c'),'src/pad_source_abi.S','src/resample_fast_plan.c','src/resample_fault.c',str(repro/'native.o'),'-lstdc++','-pthread','-ldl','-lm','-o',str(repro/'check')])
expected=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(repro/'check'),str(stock)],cwd=R,capture_output=True,text=True,timeout=30)
(repro/'failure.log').write_text(expected.stdout+expected.stderr)
assert expected.returncode!=0 and 'signal=11' in expected.stderr and 'address=0x70' in expected.stderr
print('PASS frozen r20a reproduces device SIGSEGV at address0x70; fixed adapter passes the same poisoned service fields, including invalid old rate offset, in normal/UBSan native render-loop checks')
# The old fixture always populated output+8. Reproduce r20b's missed tap
# using the current fixture with that optional direct output absent.
tap_old=O/'r20b-tap-repro';tap_old.mkdir(exist_ok=True)
tap_source=subprocess.check_output(['git','-c','safe.directory='+str(R.parent),'show','35368fa:sd-mod-session/src/resample_fast_native.cc'],cwd=R,text=True)
(tap_old/'native.cc').write_text(tap_source)
run(['aarch64-linux-gnu-g++','-std=c++11','-O1','-g','-Wall','-Wextra','-Werror','-Isrc','-c',str(tap_old/'native.cc'),'-o',str(tap_old/'native.o')])
run(['aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror','check_resample_fast_native.c','src/pad_source_abi.S','src/resample_fast_plan.c','src/resample_fault.c',str(tap_old/'native.o'),'-lstdc++','-pthread','-ldl','-lm','-o',str(tap_old/'check')])
missed=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(tap_old/'check'),str(stock),'tap-only'],cwd=R,capture_output=True,text=True,timeout=30)
(tap_old/'failure.log').write_text(missed.stdout+missed.stderr)
assert missed.returncode!=0 and 'state=0 failure=4' in missed.stderr and 'writes=0 written=376000' in missed.stderr
print('PASS frozen r20b misses the native master tap and exhausts the full budget; fixed normal/UBSan fixtures cover tap-only and combined outputs, with unchanged native loop branches. DSP remains simulated.')
# Exact r20c device condition: both native output branches disabled.
route_old=O/'r20c-output-repro';route_old.mkdir(exist_ok=True)
route_source=subprocess.check_output(['git','-c','safe.directory='+str(R.parent),'show','13740cd:sd-mod-session/src/resample_fast_native.cc'],cwd=R,text=True)
(route_old/'native.cc').write_text(route_source)
run(['aarch64-linux-gnu-g++','-std=c++11','-O1','-g','-Wall','-Wextra','-Werror','-Isrc','-c',str(route_old/'native.cc'),'-o',str(route_old/'native.o')])
run(['aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror','check_resample_fast_native.c','src/pad_source_abi.S','src/resample_fast_plan.c','src/resample_fault.c',str(route_old/'native.o'),'-lstdc++','-pthread','-ldl','-lm','-o',str(route_old/'check')])
missing=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(route_old/'check'),str(stock),'missing-output'],cwd=R,capture_output=True,text=True,timeout=30)
(route_old/'failure.log').write_text(missing.stdout+missing.stderr)
assert missing.returncode!=0 and 'direct=0 tap=0' in missing.stderr and 'state=0 failure=4' in missing.stderr
print('PASS frozen r20c reproduces absent native output registration; scoped master-route enabling/restoration passes normal/UBSan, including cancellation and errors. Device DSP output remains unproven.')
# Same production adapter/render loop and real native primary accumulator/flush.
# Frozen r20d sends its nonzero bus only to the side registration and leaves primary zero.
primary_old=O/'r20d-primary-repro';primary_old.mkdir(exist_ok=True)
primary_source=subprocess.check_output(['git','-c','safe.directory='+str(R.parent),'show','751e516:sd-mod-session/src/resample_fast_native.cc'],cwd=R,text=True)
(primary_old/'native.cc').write_text(primary_source)
run(['aarch64-linux-gnu-g++','-std=c++11','-O1','-g','-Wall','-Wextra','-Werror','-Isrc','-c',str(primary_old/'native.cc'),'-o',str(primary_old/'native.o')])
run(['aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror','check_resample_fast_native.c','src/pad_source_abi.S','src/resample_fast_plan.c','src/resample_fault.c',str(primary_old/'native.o'),'-lstdc++','-pthread','-ldl','-lm','-o',str(primary_old/'check')])
missed_primary=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(primary_old/'check'),str(stock),'primary-writer'],cwd=R,capture_output=True,text=True,timeout=30)
(primary_old/'failure.log').write_text(missed_primary.stdout+missed_primary.stderr)
assert missed_primary.returncode!=0 and 'NATIVE_PRIMARY peak=0 frames=12032' in missed_primary.stderr
print('PASS frozen r20d produces nonzero monitored bus but zero destination PCM through the real native primary writer; fixed production adapter produces PCM peak0.25 with the same native loop/accumulator/flush. DSP remains simulated; separate native WAV-encoder roundtrip also passes.')
# Frozen r23 reads only the intermediate master bus. A directly routed pad
# has nonzero physical PCM and zero intermediate PCM, reproducing the card.
previous=O/'r23-output-repro';previous.mkdir(exist_ok=True)
source=subprocess.check_output(['git','-c','safe.directory='+str(R.parent),'show','3e41128:sd-mod-session/src/resample_fast_native.cc'],cwd=R,text=True)
(previous/'native.cc').write_text(source)
run(['aarch64-linux-gnu-g++','-std=c++11','-O1','-g','-Wall','-Wextra','-Werror','-Isrc','-c',str(previous/'native.cc'),'-o',str(previous/'native.o')])
run(['aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror','check_resample_fast_native.c','src/pad_source_abi.S','src/resample_fast_plan.c','src/resample_fault.c',str(previous/'native.o'),'-lstdc++','-pthread','-ldl','-lm','-o',str(previous/'check')])
for scenario in ['physical-output','pad-link']:
 bad=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(previous/'check'),str(stock),scenario],cwd=R,capture_output=True,text=True,timeout=30)
 (previous/(scenario+'.log')).write_text(bad.stdout+bad.stderr)
 assert bad.returncode!=0
 if scenario=='physical-output':assert 'NATIVE_PRIMARY peak=0 frames=12032' in bad.stderr
 else:assert 'linked_layers' in bad.stderr
print('PASS frozen r23 reproduces silence with direct physical routing and misses linked Loop guards; updated production adapter passes both normal/UBSan. Device DSP remains untested for this revision.')
assert before=={s:sha(R/s) for s in sources}
report={'passed':True,'ubsan':True,'hardware_tested':False,'audio_dsp_fixture':True,'card_written':False,'r20a_device_crash_reproduced_locally':True,'scope':'native submission context identity and Seq loop/input/filter/dispatch, Sample metadata/refcounts/Loop restore and callback import gate; DSP, mixer, file writer and task services are fixtures','sources_sha256':before}
report['r20b_master_tap_regression_reproduced']=True
report['r20c_missing_output_reproduced']=True
report['r20d_native_writer_primary_silent_stem_nonzero_reproduced']=True
report['native_wav_encoder_roundtrip']=True
report['native_seq_diagnostics_preserve_transport_duration']=True
report['native_pad_link_getters_and_direct_launch_expansion']=True
report['linked_duration_loop_restore_success_failure_cancel']=True
report['r23_physical_output_silence_regression_reproduced']=True
report['r20f_mismatched_native_stem_import_regression']=True
(O/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
