"""Real owner queue/worker/RAM/history bridge. No device package or SD IO."""
from pathlib import Path
from hashlib import sha256
import subprocess,json,sys
sanitizer='--ubsan' in sys.argv
flags=['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'] if sanitizer else []
R=Path(__file__).resolve().parent;OUT=R/('build/resample-native-ubsan' if sanitizer else 'build/resample-native');OUT.mkdir(exist_ok=True)
stock=R.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample';digest=lambda p:sha256(p.read_bytes()).hexdigest()
assert digest(stock)=='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
info=json.loads((R/'build-info.json').read_text());assert info['revision']=='r16d-sample-input-preview'
protected=[R/'build-info.json',R.parent/info['zip']]+sorted(p for p in (R/'autorun').rglob('*') if p.is_file());before={str(p):digest(p) for p in protected}
sources=['check_steps_support.cc','src/step_exceptions.cc','src/step_mute.cc','src/pad_output_stream.cc','src/pad_sample_source.cc','src/pad_memory_sample.cc','src/pad_sample_pool.cc','src/pad_assignment.cc','src/pad_resample_commit.cc','src/pad_resample_history.cc','src/resample_queue.cc','src/resample_native.cc','src/resample_context.cc','src/fx_native.cc','src/resample_runtime.cc']
objects=[]
for source in sources:
 obj=OUT/(Path(source).stem+'.o');objects.append(str(obj));subprocess.run(['aarch64-linux-gnu-g++','-c','-O1','-g','-std=c++11','-Wall','-Wextra','-Werror',*flags,'-mbranch-protection=standard',source,'-o',str(obj)],cwd=R,check=True)
target=OUT/'native-bridge'
c_sources=['src/resample_worker.c','src/resample_flow.c','src/resample_view.c','src/fx_catalog.c','src/resample_panel.c','src/pad_render.c','src/fx_chain.c', 'src/fx_own.c', 'src/compressor_dsp.c','src/fx_eq.c']
subprocess.run(['aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-Ibuild','-mbranch-protection=standard','check_resample_runtime.c','check_chop_pcm_support.S','check_steps_support.S','src/step_abi.S','src/pad_output_abi.S','src/pad_source_abi.S','src/pad_memory_abi.S','src/pad_assignment_abi.S',*c_sources,*objects,'-lstdc++','-ldl','-pthread','-lm','-o',str(target)],cwd=R,check=True)
result=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(target),str(stock)],cwd=R,capture_output=True,text=True,timeout=30)
(OUT/'native-bridge.log').write_text(result.stdout+result.stderr);result.check_returncode();assert before=={str(p):digest(p) for p in protected}
sources+=c_sources+['src/resample_native.h','src/resample_context.h','src/resample_runtime.h','src/resample_queue.h','src/resample_panel.h','src/resample_worker.h','src/resample_flow.h','src/resample_view.h','src/pad_memory_sample.h','src/pad_resample_commit.h','src/pad_resample_history.h','check_pad_memory_sample.c','check_pad_resample_commit.c','check_pad_resample_history.c','check_resample_native.c','check_resample_context.c','check_resample_runtime.c','verify_resample_native.py']
report={'prototype':'native-context-runtime-queue-history','passed':True,'ubsan':sanitizer,'production_unchanged':True,'hardware_tested':False,'device_trial_ready':False,'not_connected':['physical device UI/lifecycle hook installation and durable native history storage','actual device parameter receiver/sampler','complete source/target unbaked-settings guard and full shutdown ordering','automatic model restoration after partial Undo failure','live compare and modern native FX','Save/reopen'],'results':[x for x in result.stdout.splitlines() if x.startswith('PASS')],'sources_sha256':{s:digest(R/s) for s in sources}}
(OUT/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
