"""Combined native commit core proof, no device or native history wiring."""
from pathlib import Path
from hashlib import sha256
import subprocess,json,sys
sanitizer='--ubsan' in sys.argv
flags=['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'] if sanitizer else []
R=Path(__file__).resolve().parent;OUT=R/('build/resample-commit-ubsan' if sanitizer else 'build/resample-commit');OUT.mkdir(exist_ok=True)
stock=R.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample';digest=lambda p:sha256(p.read_bytes()).hexdigest()
assert digest(stock)=='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
info=json.loads((R/'build-info.json').read_text());assert info['revision']=='r16d-sample-input-preview'
protected=[R/'build-info.json',R.parent/info['zip']]+sorted(p for p in (R/'autorun').rglob('*') if p.is_file());before={str(p):digest(p) for p in protected}
sources=['check_steps_support.cc','src/step_exceptions.cc','src/step_mute.cc','src/pad_output_stream.cc','src/pad_sample_source.cc','src/pad_memory_sample.cc','src/pad_sample_pool.cc','src/pad_assignment.cc','src/pad_resample_commit.cc','src/pad_resample_history.cc']
objects=[]
for source in sources:
 obj=OUT/(Path(source).stem+'.o');objects.append(str(obj));subprocess.run(['aarch64-linux-gnu-g++','-c','-O1','-g','-std=c++11','-Wall','-Wextra','-Werror',*flags,'-mbranch-protection=standard',source,'-o',str(obj)],cwd=R,check=True)
target=OUT/'native-commit'
subprocess.run(['aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-Ibuild','-mbranch-protection=standard','check_pad_resample_history.c','check_chop_pcm_support.S','check_steps_support.S','src/step_abi.S','src/pad_output_abi.S','src/pad_source_abi.S','src/pad_memory_abi.S','src/pad_assignment_abi.S',*objects,'-lstdc++','-ldl','-pthread','-lm','-o',str(target)],cwd=R,check=True)
result=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(target),str(stock)],cwd=R,capture_output=True,text=True,timeout=30)
(OUT/'native-commit.log').write_text(result.stdout+result.stderr);result.check_returncode();assert before=={str(p):digest(p) for p in protected}
sources+=['src/pad_resample_commit.h','src/pad_resample_history.h','check_pad_resample_commit.c','check_pad_resample_history.c','verify_pad_resample_commit.py']
report={'prototype':'combined-native-commit-history','passed':True,'ubsan':sanitizer,'production_unchanged':True,'hardware_tested':False,'device_trial_ready':False,'not_connected':['device owner queue/UI and durable native history storage','actual device parameter receiver/sampler','device source identity/recovery guard','live compare','Save/reopen'],'results':[x for x in result.stdout.splitlines() if x.startswith('PASS')],'sources_sha256':{s:digest(R/s) for s in sources}}
(OUT/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
