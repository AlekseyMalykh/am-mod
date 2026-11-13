"""Native RAM Sample proof; no production build, SD or file export."""
from pathlib import Path
from hashlib import sha256
import subprocess,json,sys
sanitizer='--ubsan' in sys.argv
flags=['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'] if sanitizer else []
R=Path(__file__).resolve().parent;OUT=R/('build/resample-memory-ubsan' if sanitizer else 'build/resample-memory');OUT.mkdir(exist_ok=True)
stock=R.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample';digest=lambda p:sha256(p.read_bytes()).hexdigest()
assert digest(stock)=='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
info=json.loads((R/'build-info.json').read_text());assert info['revision']=='r16d-sample-input-preview'
protected=[R/'build-info.json',R.parent/info['zip']]+sorted(p for p in (R/'autorun').rglob('*') if p.is_file());before={str(p):digest(p) for p in protected}
objects=[]
for source in ['check_steps_support.cc','src/step_exceptions.cc','src/step_mute.cc','src/pad_output_stream.cc','src/pad_sample_source.cc','src/pad_memory_sample.cc','src/pad_sample_pool.cc','src/pad_assignment.cc']:
 obj=OUT/(Path(source).stem+'.o');objects.append(str(obj));subprocess.run(['aarch64-linux-gnu-g++','-c','-O1','-g','-std=c++11','-Wall','-Wextra','-Werror',*flags,'-mbranch-protection=standard',source,'-o',str(obj)],cwd=R,check=True)
target=OUT/'native-memory'
subprocess.run(['aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-Ibuild','-mbranch-protection=standard','check_pad_memory_lifecycle.c','check_chop_pcm_support.S','check_steps_support.S','src/step_abi.S','src/pad_output_abi.S','src/pad_source_abi.S','src/pad_memory_abi.S','src/pad_assignment_abi.S','src/pad_render.c','src/fx_chain.c', 'src/fx_own.c', 'src/compressor_dsp.c','src/fx_eq.c',*objects,'-lstdc++','-ldl','-pthread','-lm','-o',str(target)],cwd=R,check=True)
result=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(target),str(stock)],cwd=R,capture_output=True,text=True,timeout=30)
(OUT/'native-memory.log').write_text(result.stdout+result.stderr);result.check_returncode();assert before=={str(p):digest(p) for p in protected}
sources=['src/pad_memory_sample.h','src/pad_memory_sample.cc','src/pad_memory_abi.S','src/pad_native_block.h','src/pad_sample_pool.h','src/pad_sample_pool.cc','src/pad_sample_source.cc','src/pad_source_abi.S','src/pad_output_stream.cc','src/pad_output_abi.S','check_pad_memory_sample.c','check_pad_memory_lifecycle.c','verify_pad_memory_sample.py']
report={'prototype':'native-ram-sample','passed':True,'ubsan':sanitizer,'production_unchanged':True,'hardware_tested':False,'device_trial_ready':False,'native_read_abi':'explicit requested frame count,64-byte owned block result and completion/release','not_connected':['device model/worker/UI entry','actual device audio sampler','whole Apply registration+assignment+parameter-reset Undo command','live compare','Save/reopen of generated Sample'],'results':[x for x in result.stdout.splitlines() if x.startswith('PASS')],'sources_sha256':{s:digest(R/s) for s in sources}}
(OUT/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
