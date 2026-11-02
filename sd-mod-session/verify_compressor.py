"""Pinned ARM tests; no physical card or device access."""
from pathlib import Path
import hashlib,json,os,resource,subprocess
R=Path(__file__).resolve().parent
baseline=json.loads((R/'compressor_base.json').read_text())
O=R/'build/compressor'/baseline['revision'];O.mkdir(parents=True,exist_ok=True)
(O.parent/'art').mkdir(exist_ok=True)
runtime=Path(os.environ.get('MPC_RUNTIME_ROOT',str(R.parent))).resolve()
stock=runtime/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(stock)=='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
tracked=['test_baselines/before-bypass-ui.c','test_baselines/before-sleep-background.c','test_baselines/before-master-rebind.c','src/compressor.h','src/compressor_dsp.c','src/compressor_native.c','src/compressor_ui.c',
 'src/phone_native.c','src/music_ui.h','check_compressor_dsp.c','check_compressor_factory.cc','check_modern_fx.cc',
 'check_modern_fx.c','check_fx_native.c','verify_compressor.py','src/compressor_animation.c',
 'src/compressor_image.cc','src/optocat_atlas.inc','check_compressor_animation.c','check_compressor_gain.cc','check_compressor_warm.c','check_compressor_lifecycle.cc','compressor_base.json']
before={s:sha(R/s) for s in tracked}
legacy=subprocess.check_output(['git','-c','safe.directory='+str(R.parent),'show','152e07c:sd-mod-session/src/compressor_dsp.c'],cwd=R)
(O/'r26i-dsp.c').write_bytes(legacy)
for name,extra in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
 common=['-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard','-ffunction-sections','-fdata-sections',*extra]
 legacy_obj=O/(name+'-legacy-dsp.o')
 subprocess.run(['aarch64-linux-gnu-gcc','-std=c11',*common,'-Isrc','-Dcomp_opto_init=comp_opto_init_r26i','-Dcomp_opto_process=comp_opto_process_r26i','-c',str(O/'r26i-dsp.c'),'-o',str(legacy_obj)],cwd=R,check=True)
 subprocess.run(['aarch64-linux-gnu-gcc','-std=c11',*common,'-DCHECK_LEGACY_OPTO=1','check_compressor_dsp.c','src/compressor_dsp.c',str(legacy_obj),'-lm','-o',str(O/(name+'-dsp'))],cwd=R,check=True)
 subprocess.run(['aarch64-linux-gnu-gcc','-std=c11',*common,'check_compressor_animation.c','src/compressor_animation.c','src/compressor_dsp.c','-lm','-o',str(O/(name+'-animation'))],cwd=R,check=True)
 subprocess.run(['aarch64-linux-gnu-gcc','-std=c11',*common,'check_compressor_warm.c','src/compressor_dsp.c','-lm','-o',str(O/(name+'-warm'))],cwd=R,check=True)
 objects=[]
 for source in ['check_modern_fx.c','check_compressor_factory.cc','src/compressor_dsp.c','src/compressor_native.c','src/compressor_ui.c','src/compressor_animation.c','src/compressor_image.cc']:
  cpp=source.endswith('.cc');obj=O/(name+'-'+Path(source).stem+'.o');objects.append(str(obj))
  subprocess.run(['aarch64-linux-gnu-g++' if cpp else 'aarch64-linux-gnu-gcc','-std=c++11' if cpp else '-std=c11',*common,'-c',source,'-o',str(obj)],cwd=R,check=True)
 target=O/(name+'-native')
 subprocess.run(['aarch64-linux-gnu-g++',*extra,'-Wl,--gc-sections',*objects,'-ldl','-lm','-o',str(target)],check=True)
 if name=='normal':
  baseline_ui=(R/'test_baselines/before-bypass-ui.c').read_bytes()
  baseline_source=O/'before-bypass-ui.c';baseline_source.write_bytes(baseline_ui)
  baseline_object=O/'before-bypass-ui.o';baseline_target=O/'before-bypass-native'
  subprocess.run(['aarch64-linux-gnu-gcc','-std=c11',*common,'-Isrc','-c',str(baseline_source),'-o',str(baseline_object)],cwd=R,check=True)
  baseline_objects=[str(baseline_object) if p==str(O/'normal-compressor_ui.o') else p for p in objects]
  subprocess.run(['aarch64-linux-gnu-g++','-Wl,--gc-sections',*baseline_objects,'-ldl','-lm','-o',str(baseline_target)],cwd=R,check=True)
  baseline_run=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(baseline_target),str(stock)],cwd=R,capture_output=True,text=True,timeout=45,preexec_fn=lambda:resource.setrlimit(resource.RLIMIT_CORE,(0,0)))
  (O/'before-bypass-native.log').write_text(baseline_run.stdout+baseline_run.stderr)
  assert baseline_run.returncode!=0 and '!lit_meters(base)&&displayed_gr==0' in baseline_run.stderr,baseline_run.stderr
  print('REPRODUCED pre-fix OptoCat paint retains live meter bars when Bypass puts the cat to sleep',flush=True)
  baseline_animation=(R/'test_baselines/before-sleep-background.c').read_bytes()
  baseline_animation_source=O/'before-sleep-background.c';baseline_animation_source.write_bytes(baseline_animation)
  baseline_animation_target=O/'before-sleep-background'
  subprocess.run(['aarch64-linux-gnu-gcc','-std=c11',*common,'-Isrc','check_compressor_animation.c',str(baseline_animation_source),'src/compressor_dsp.c','-lm','-o',str(baseline_animation_target)],cwd=R,check=True)
  background_run=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(baseline_animation_target)],cwd=R,capture_output=True,text=True,timeout=45,preexec_fn=lambda:resource.setrlimit(resource.RLIMIT_CORE,(0,0)))
  (O/'before-sleep-background.log').write_text(background_run.stdout+background_run.stderr)
  assert background_run.returncode!=0 and 'assert_sleep_background' in background_run.stderr,background_run.stderr
  print('REPRODUCED pre-fix OptoCat compositor leaves awake grain around the sleeping cat',flush=True)
 gain_object=O/(name+'-gain.o')
 subprocess.run(['aarch64-linux-gnu-g++','-std=c++11',*common,'-c','check_compressor_gain.cc','-o',str(gain_object)],cwd=R,check=True)
 gain_objects=[str(O/(name+'-'+Path(source).stem+'.o')) for source in ['check_modern_fx.c','src/compressor_dsp.c','src/compressor_native.c']]
 subprocess.run(['aarch64-linux-gnu-g++',*extra,'-Wl,--gc-sections',*gain_objects,str(gain_object),'-ldl','-lm','-o',str(O/(name+'-gain'))],check=True)
 lifecycle_object=O/(name+'-lifecycle.o')
 subprocess.run(['aarch64-linux-gnu-g++','-std=c++11',*common,'-c','check_compressor_lifecycle.cc','-o',str(lifecycle_object)],cwd=R,check=True)
 lifecycle_objects=[str(O/(name+'-'+Path(source).stem+'.o')) for source in ['check_modern_fx.c','src/compressor_dsp.c','src/compressor_native.c']]
 subprocess.run(['aarch64-linux-gnu-g++',*extra,'-Wl,--gc-sections',*lifecycle_objects,str(lifecycle_object),'-ldl','-lm','-o',str(O/(name+'-lifecycle'))],check=True)
 if name=='normal':
  old_native=(R/'test_baselines/before-master-rebind.c').read_bytes()
  old_source=O/'before-master-rebind.c';old_source.write_bytes(old_native)
  old_object=O/'before-master-rebind.o';old_target=O/'before-master-rebind'
  subprocess.run(['aarch64-linux-gnu-gcc','-std=c11',*common,'-Isrc','-c',str(old_source),'-o',str(old_object)],cwd=R,check=True)
  old_objects=[str(old_object) if x==str(O/'normal-compressor_native.o') else x for x in lifecycle_objects]
  subprocess.run(['aarch64-linux-gnu-g++','-Wl,--gc-sections',*old_objects,str(lifecycle_object),'-ldl','-lm','-o',str(old_target)],check=True)
  old_run=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(old_target),str(stock)],cwd=R,capture_output=True,text=True,timeout=45,preexec_fn=lambda:resource.setrlimit(resource.RLIMIT_CORE,(0,0)))
  (O/'before-master-rebind.log').write_text(old_run.stdout+old_run.stderr)
  assert old_run.returncode!=0 and 'attempts==1&&!compressor_ready()' in old_run.stderr,old_run.stderr
  print('REPRODUCED r26o master replacement rejected while the obsolete master remains bound',flush=True)
 for case in ['dsp','warm','animation','native','gain','lifecycle']:
  command=['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(O/(name+'-'+case))]
  if case in ('native','gain','lifecycle'):command.append(str(stock))
  p=subprocess.run(command,cwd=R,capture_output=True,text=True,timeout=45)
  (O/(name+'-'+case+'.log')).write_text(p.stdout+p.stderr);print(p.stdout+p.stderr,end='',flush=True);p.check_returncode()
  assert 'PASS ' in p.stdout and 'runtime error:' not in p.stderr
  if case=='warm':assert 'WARM_SPECTRUM ' in p.stdout and 'WARM_ALIAS ' in p.stdout
  if case=='native':assert all(x in p.stdout for x in ('PASS Warm B1 edge pairing','PASS shared native Bypass','PASS real Colour master replacement'))
  if case=='gain':assert p.stdout.count('WARM_GAIN_COMPARE ')==4
assert before=={s:sha(R/s) for s in tracked}
proof=dict(passed=True,ubsan=True,animation_verified=True,native_image_blit=True,stereo_meters_verified=True,bypass_meters_hidden_verified=True,bypass_underline_verified=True,bypass_meter_baseline_reproduced=True,sleep_background_hidden_verified=True,sleep_background_baseline_reproduced=True,bypass_meter_baseline_commit='9f4d5a1',bypass_meter_baseline_sha256=hashlib.sha256(baseline_ui).hexdigest(),sleep_background_baseline_sha256=hashlib.sha256(baseline_animation).hexdigest(),shift_controls_verified=True,paw_timing_verified=True,native_gain_compared=True,maximum_gain_db=6,default_response_baseline='152e07c',default_response_baseline_sha256=hashlib.sha256(legacy).hexdigest(),sources_sha256=before,stock_sha256=sha(stock),
 hardware_tested=False,card_written=False,full_vm_verified=False,
 scope='Real pinned native Colour factory/AudioPlugin prepare/process, software Image factory and exact pixel blit; production adapter and Type UI with isolated Component/Graphics/service-owner fixtures; signal-driven animation compositor including Attack/Release paw timing, native Duck/Opto measured gain comparison, stereo PCM, DSP and input lifecycle checks. Native app main not executed.')
assert 'PASS Note Repeat B1/B2/B3' in (O/'normal-native.log').read_text()
proof.update(note_repeat_buttons_verified=True,minimum_gain_db=-6,paw_actual_reduction_verified=True,paw_no_false_strikes_verified=True,paw_dsp_timing_verified=True,master_rebind_verified=True,master_rebind_baseline_reproduced=True,inflight_handoff_verified=True,shared_native_bypass_verified=True,open_active_or_duck_verified=True,warm_verified=True,warm_native_audio_verified=True,warm_default_off=True,
 warm_drive=1.5,warm_coloured_mix=.35,warm_fade_ms=10,warm_antialiasing='first-order ADAA',
 warm_level_note='reciprocal drive compensation; no automatic makeup boost; Gain ceiling +6dB')
(O/'verification.json').write_text(json.dumps(proof,indent=2)+'\n')
