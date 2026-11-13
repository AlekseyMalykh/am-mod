"""Local-only resampling prototype: no build.py, autorun, archives or SD writes."""
from hashlib import sha256
from pathlib import Path
import json
import subprocess

R=Path(__file__).resolve().parent
STOCK=R.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
ROOT=Path('/home/user/mpc-rootfs-user-20260808')
OUT=R/'build'/'resample'
OUT.mkdir(exist_ok=True)
digest=lambda p:sha256(p.read_bytes()).hexdigest()
assert digest(STOCK)=='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
info=json.loads((R/'build-info.json').read_text())
assert info['revision']=='r16d-sample-input-preview'
assert digest(R.parent/info['zip'])==info['zip_sha256']
for name,value in info['payload_sha256'].items():assert digest(R/'autorun'/name)==value
protected=[R/'build-info.json',R.parent/info['zip']]+sorted(p for p in (R/'autorun').rglob('*') if p.is_file())
before={str(p):digest(p) for p in protected}
checks=[]
for name,source,flags in [('core','check_pad_render.c',[]),
    ('core-ubsan','check_pad_render.c',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all']),
    ('stream','check_pad_render_stream.c',[]),
    ('stream-ubsan','check_pad_render_stream.c',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all']),
    ('native','check_pad_render_native.c',[])]:
    executable=OUT/('check-pad-render-'+name)
    subprocess.run(['aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror',
        '-mbranch-protection=standard',*flags,source,'src/pad_render.c','src/fx_chain.c', 'src/fx_own.c', 'src/compressor_dsp.c','src/fx_eq.c',
        '-lstdc++','-lm','-ldl','-o',str(executable)],cwd=R,check=True)
    command=['qemu-aarch64','-L',str(ROOT),str(executable)]
    if name=='native':command.append(str(STOCK))
    result=subprocess.run(command,cwd=R,capture_output=True,text=True)
    (OUT/(name+'.log')).write_text(result.stdout+result.stderr)
    result.check_returncode()
    checks.append({'name':name,'passed':True,'results':[s for s in result.stdout.splitlines() if s.startswith('PASS')]})
support=[]
for source in ['check_steps_support.cc','src/step_exceptions.cc','src/step_mute.cc']:
    obj=OUT/(Path(source).stem+'.o');support.append(str(obj))
    subprocess.run(['aarch64-linux-gnu-g++','-c','-O1','-g','-Wall','-Wextra','-Werror','-std=c++11',
        '-mbranch-protection=standard',source,'-o',str(obj)],cwd=R,check=True)
source_object=OUT/'pad_sample_source.o'
subprocess.run(['aarch64-linux-gnu-g++','-c','-O1','-g','-Wall','-Wextra','-Werror','-std=c++11',
    '-mbranch-protection=standard','src/pad_sample_source.cc','-o',str(source_object)],cwd=R,check=True)
executable=OUT/'check-pad-sample-source'
subprocess.run(['aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror','-Ibuild','-mbranch-protection=standard',
    'check_pad_sample_source.c','check_pad_sample_source_support.S','check_chop_pcm_support.S','check_steps_support.S','src/step_abi.S',
    *support,str(source_object),'src/pad_source_abi.S',
    'src/pad_render.c','src/fx_chain.c', 'src/fx_own.c', 'src/compressor_dsp.c','src/fx_eq.c','-lstdc++','-ldl','-lpthread','-lm','-o',str(executable)],cwd=R,check=True)
result=subprocess.run(['qemu-aarch64','-L',str(ROOT),str(executable),str(STOCK)],cwd=R,capture_output=True,text=True,timeout=60)
(OUT/'sample-source.log').write_text(result.stdout+result.stderr);result.check_returncode()
checks.append({'name':'native-sample-source','passed':True,'results':[s for s in result.stdout.splitlines() if s.startswith('PASS')]})
assert before=={str(p):digest(p) for p in protected},'Production package changed'
report={'prototype':'pad-resample-offline-2-native-source','checks':checks,'card_written':False,
    'production_revision':info['revision'],'production_files_unchanged':len(protected),
    'production_package_unchanged':True,'hardware_tested':False,'device_trial_ready':False,
    'native_audio':'legacy EQ81 actual factory/prepare/parameters/stereo DSP/destroy under QEMU',
    'source_input':'native Program286bbe0/layer Sample2803b90, Trim snapshot, Sample2943250 reader getter, metadata, frame span/EOF/seek/deleter and bounded capture; deterministic provider/PCM decoder in test',
    'not_connected':['device UI/worker hookup','pad pitch/envelope/filter','live A/B in mix',
        'modern AC50 FX factory','Program/SamplePool assignment','Undo/Redo','Save/reopen','MPC UI'],
    'scope':'trim and finite repeats of immutable PCM -> max5FX -> explicit tail, stereo at original rate',
    'pcm_budget_bytes':64*1024*1024,'step_max_frames':4096,'processor_block_max_frames':256,
    'native_parameter_capacity':16,
    'production_sha256':before}
(OUT/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
