"""Resample chooser/controller/worker prototype. No device package or SD writes."""
from pathlib import Path
from hashlib import sha256
import json, subprocess
R=Path.cwd(); out=R/'build/resample-flow';out.mkdir(exist_ok=True)
info=json.loads((R/'build-info.json').read_text());assert info['revision']=='r16d-sample-input-preview'
protected=[R/'build-info.json',R.parent/info['zip']]+sorted(p for p in (R/'autorun').rglob('*') if p.is_file())
digest=lambda p:sha256(p.read_bytes()).hexdigest();before={str(p):digest(p) for p in protected}
checks=[]
for name,source,extra in [('flow','check_resample_flow.c',[]),('flow-ubsan','check_resample_flow.c',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all']),('worker','check_resample_worker.c',[]),('worker-ubsan','check_resample_worker.c',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
 target=out/name
 sources=[source,'src/resample_flow.c','src/resample_view.c','src/fx_catalog.c','src/pad_render.c','src/fx_chain.c','src/fx_eq.c']
 if name.startswith('worker'):sources+=['src/resample_worker.c']
 subprocess.run(['aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror',*extra,*sources,'-lm','-pthread','-o',str(target)],check=True,cwd=R)
 args=['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(target)]
 if name.startswith('flow'):args.append(str(out/'choice-draw.txt'))
 result=subprocess.run(args,cwd=R,capture_output=True,text=True,timeout=60)
 (out/(name+'.log')).write_text(result.stdout+result.stderr);result.check_returncode()
 checks.append({'name':name,'passed':True,'results':[x for x in result.stdout.splitlines() if x.startswith('PASS')]})
assert before=={str(p):digest(p) for p in protected}
stock=R.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
assert digest(stock)=='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
objects=[]
for source in ['check_steps_support.cc','src/step_exceptions.cc','src/step_mute.cc','src/pad_output_stream.cc']:
 obj=out/(Path(source).stem+'.o');objects.append(str(obj))
 subprocess.run(['aarch64-linux-gnu-g++','-c','-O1','-g','-std=c++11','-Wall','-Wextra','-Werror','-mbranch-protection=standard',source,'-o',str(obj)],cwd=R,check=True)
target=out/'native-output'
subprocess.run(['aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror','-Ibuild','-mbranch-protection=standard',
 'check_pad_output_stream.c','check_chop_pcm_support.S','check_steps_support.S','src/step_abi.S','src/pad_output_abi.S',
 'src/pad_render.c','src/fx_chain.c','src/fx_eq.c',*objects,'-lstdc++','-ldl','-pthread','-lm','-o',str(target)],cwd=R,check=True)
result=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(target),str(stock)],cwd=R,capture_output=True,text=True,timeout=60)
(out/'native-output.log').write_text(result.stdout+result.stderr);result.check_returncode()
checks.append({'name':'native-output','passed':True,'results':[x for x in result.stdout.splitlines() if x.startswith('PASS')]})
assert before=={str(p):digest(p) for p in protected}
sources=['src/resample_flow.c','src/resample_flow.h','src/resample_view.c','src/fx_catalog.c','src/resample_view.h','src/resample_worker.c','src/resample_worker.h','src/pad_native_block.h','src/pad_output_stream.cc','src/pad_output_stream.h','src/pad_output_abi.S','src/fx_chain.c','src/fx_chain.h','src/fx_eq.c','src/fx_eq.h','src/pad_render.c','src/pad_render.h','check_resample_flow.c','check_resample_worker.c','check_pad_output_stream.c']
report={'prototype':'resample-chooser-controller-worker','fx_limit':5,'fx_move':'Shift+B2 up, Shift+B3 down; settings and selection follow stage','checks':checks,'production_unchanged':True,'production_revision':info['revision'],'hardware_tested':False,'device_trial_ready':False,'not_connected':['native Shift-pad entry/paint/lifecycle hook','actual device worker scheduler','modern FX and pad Tune/envelope/filter/velocity stage','native mix A/B','native Apply/Undo/Save'],'sources_sha256':{s:digest(R/s) for s in sources}}
(out/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
