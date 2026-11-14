"""Rear Mix + independent all-source Input FX, host/ARM/UBSan; no device."""
from pathlib import Path
import hashlib,json,subprocess,resource
R=Path(__file__).resolve().parent;O=R/'build/rear-mix-input-fx';O.mkdir(exist_ok=True)
core=['src/mixer_model.c','src/fx_chain.c','src/fx_own.c','src/compressor_dsp.c','src/fx_eq.c']
tracked=core+['src/rear_audio.h','src/rear_audio.cc','src/rear_native.cc','src/rear_ui.c','src/input_fx.cc','src/input_fx.h','check_rear_audio.cc','check_rear_ui.c','check_input_fx.cc',Path(__file__).name]
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();before={p:sha(R/p) for p in tracked};checks=[]
for mode,flags in [('host',[]),('arm',[]),('arm-ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
 arm=mode.startswith('arm');cc='aarch64-linux-gnu-gcc' if arm else 'gcc';cpp='aarch64-linux-gnu-g++' if arm else 'g++';runner=['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808'] if arm else []
 common=['-O1','-g','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-I.',*flags]+(['-mbranch-protection=standard'] if arm else [])
 for suite in ['audio','ui','input-fx']:
  binary=O/(suite+'-'+mode)
  if suite=='ui':cmd=[cc,'-std=c11',*common,'check_rear_ui.c','-Wl,--gc-sections','-lm','-o',str(binary)]
  elif suite=='audio':cmd=[cpp,'-std=c++11',*common,'check_rear_audio.cc','src/rear_audio.cc','-pthread','-lm','-o',str(binary)]
  else:
   objects=[]
   for src in core:
    obj=O/(Path(src).stem+'-'+mode+'.o');subprocess.run([cc,'-std=c11',*common,'-c',src,'-o',str(obj)],cwd=R,check=True);objects.append(str(obj))
   cmd=[cpp,'-std=c++11',*common,'check_input_fx.cc','src/rear_audio.cc',*objects,'-pthread','-lm','-Wl,--gc-sections,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free','-o',str(binary)]
  subprocess.run(cmd,cwd=R,check=True)
  p=subprocess.run([*runner,str(binary)],cwd=R,capture_output=True,text=True,timeout=45,preexec_fn=lambda:resource.setrlimit(resource.RLIMIT_CORE,(0,0)))
  name=suite+'-'+mode;(O/(name+'.log')).write_text(p.stdout+p.stderr);print(name+':\n'+p.stdout+p.stderr,flush=True);p.check_returncode();assert 'PASS' in p.stdout;checks.append(dict(name=name,passed=True))
assert before=={p:sha(R/p) for p in tracked}
(O/'verification.json').write_text(json.dumps(dict(passed=True,revision='r29m',checks=checks,sources_sha256=before,hardware_tested=False,native_voices_and_effects_fixture=True,card_written=False),indent=2)+'\n')
