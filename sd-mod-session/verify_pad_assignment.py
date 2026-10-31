"""Native command proof only; no device build, SD writes or firmware updates."""
from pathlib import Path
from hashlib import sha256
import subprocess,json
R=Path(__file__).resolve().parent
OUT=R/'build/resample-assignment';OUT.mkdir(exist_ok=True)
stock=R.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
digest=lambda p:sha256(p.read_bytes()).hexdigest()
assert digest(stock)=='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
info=json.loads((R/'build-info.json').read_text());assert info['revision']=='r16d-sample-input-preview'
protected=[R/'build-info.json',R.parent/info['zip']]+sorted(p for p in (R/'autorun').rglob('*') if p.is_file())
before={str(p):digest(p) for p in protected}
objects=[]
for source in ['check_steps_support.cc','src/step_exceptions.cc','src/step_mute.cc','src/pad_assignment.cc']:
 obj=OUT/(Path(source).stem+'.o');objects.append(str(obj))
 subprocess.run(['aarch64-linux-gnu-g++','-c','-O1','-g','-std=c++11','-Wall','-Wextra','-Werror','-mbranch-protection=standard',source,'-o',str(obj)],cwd=R,check=True)
target=OUT/'native-assignment'
subprocess.run(['aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror','-Ibuild','-mbranch-protection=standard','check_pad_assignment.c','check_pad_assignment_support.S','check_chop_pcm_support.S','check_steps_support.S','src/step_abi.S','src/pad_assignment_abi.S',*objects,'-lstdc++','-ldl','-pthread','-lm','-o',str(target)],cwd=R,check=True)
result=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(target),str(stock)],cwd=R,capture_output=True,text=True,timeout=30)
(OUT/'native-assignment.log').write_text(result.stdout+result.stderr);result.check_returncode()
assert before=={str(p):digest(p) for p in protected}
sources=['src/pad_assignment.h','src/pad_assignment.cc','src/pad_assignment_abi.S','check_pad_assignment.c','check_pad_assignment_support.S','verify_pad_assignment.py']
report={'prototype':'native-single-layer-assignment','passed':True,'production_unchanged':True,'hardware_tested':False,'device_trial_ready':False,'results':[x for x in result.stdout.splitlines() if x.startswith('PASS')],'not_connected':['creation and SamplePool registration of rendered output','native parameter receivers and actual audio','project/source/target change guards under device model owner','live compare and complete Apply/reset/Undo/Save adapter'],'sources_sha256':{s:digest(R/s) for s in sources}}
(OUT/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
