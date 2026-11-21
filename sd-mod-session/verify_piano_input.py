"""Native musical-input worker and recorder-finalization fixtures; no hardware."""
from pathlib import Path
import subprocess,hashlib,json
R=Path(__file__).resolve().parent;O=R/'build/piano-input';O.mkdir(parents=True,exist_ok=True)
sources=['src/batch_copy.cc','src/batch_copy.h','src/piano_input.inc','src/poly_record.c','src/poly_record.h','src/workflow_ui.c','src/workflow_ui.h','src/piano_roll_native.inc','src/step_native.c','src/step_native.h','src/phone_native.c','check_piano_input.c','check_steps.c','check_batch_native.c','check_batch_dispatch.c','check_poly_record.c','check_fx_native.c','src/batch_events.cc','src/batch_events.h','check_steps_support.cc','src/step_exceptions.cc','src/step_mute.cc','check_steps_support.S','src/step_abi.S','verify_piano_input.py']
sha=lambda n:hashlib.sha256((R/n).read_bytes()).hexdigest();before={n:sha(n) for n in sources}
stock=R.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
for mode,extra in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
 flags=['-O1','-g','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-mbranch-protection=standard',*extra];objects=[]
 for source in ['src/batch_copy.cc','src/batch_events.cc','check_steps_support.cc','src/step_exceptions.cc','src/step_mute.cc']:
  obj=O/(mode+'-'+Path(source).stem+'.o');objects.append(str(obj))
  subprocess.run(['aarch64-linux-gnu-g++','-std=c++11',*flags,'-c',source,'-o',str(obj)],cwd=R,check=True)
 for name,inputs,objs in [('input',['check_piano_input.c','src/piano_roll.c','check_steps_support.S','src/step_abi.S'],objects),('poly',['check_poly_record.c'],[])]:
  binary=O/(mode+'-'+name)
  subprocess.run(['aarch64-linux-gnu-gcc','-std=c11',*flags,'-Ibuild',*inputs,*objs,'-Wl,--gc-sections','-lstdc++','-ldl','-lpthread','-lm','-o',str(binary)],cwd=R,check=True)
  result=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(binary),str(stock)],cwd=R,text=True,capture_output=True,timeout=60)
  (O/(binary.name+'.log')).write_text(result.stdout+result.stderr);print(result.stdout+result.stderr,flush=True);result.check_returncode()
assert before=={n:sha(n) for n in sources}
(O/'verification.json').write_text(json.dumps(dict(passed=True,sources_sha256=before,hardware_tested=False,live_vm_recording_tested=False),indent=2)+'\n')
