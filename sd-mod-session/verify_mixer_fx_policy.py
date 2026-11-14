"""r29l: installed native FX isolation, production input and Stock stems gate."""
from pathlib import Path
import hashlib,json,subprocess,resource
R=Path(__file__).resolve().parent;O=R/'build/mixer-fx-policy';O.mkdir(exist_ok=True)
stock=R.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(stock)=='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
core=['src/mixer_route_native.c','src/mixer_buses.c','src/mixer_model.c','src/mixer_meters.c','src/fx_chain.c','src/fx_own.c','src/compressor_dsp.c','src/fx_eq.c']
audio=['check_mixer_fx_policy.c',*core,'src/pad_eq.c','src/pad_eq_native.c','src/music_settings.c','src/mixer_live.c','src/mixer_live_native.c']
phone=['check_mixer_phone_routes.c','src/phone_events.c','src/mixer_input.c','src/mixer_control.c','src/phone_input.c','src/mixer_model.c','src/fx_catalog.c','src/fx_chain.c','src/fx_own.c','src/compressor_dsp.c','src/fx_eq.c','src/mixer_leds.c']
tracked=list(dict.fromkeys(audio+phone+['src/phone_native.c','src/mixer_stems.c','src/mixer_route_native.h','check_mixer_stock_stems.c','check_mixer_live_native.c','check_mixer_route_native.c','check_mixer_route_dsp.c',Path(__file__).name]))
hashes={n:sha(R/n) for n in tracked};checks=[]
for mode,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
 for suite,sources,extra,cases in [
  ('audio',audio,['-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free'],[(str(stock),'0'),(str(stock),'1')]),
  ('phone',phone,['-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-Ibuild'],[()]),
  ('stems',['check_mixer_stock_stems.c'],['-ffunction-sections','-fdata-sections','-Wl,--gc-sections'],[()])]:
  binary=O/(suite+'-'+mode)
  subprocess.run(['aarch64-linux-gnu-gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard',*flags,*extra,*sources,'-ldl','-lm','-pthread','-o',str(binary)],cwd=R,check=True)
  for i,args in enumerate(cases):
   p=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(binary),*args],cwd=R,capture_output=True,text=True,timeout=45,preexec_fn=lambda:resource.setrlimit(resource.RLIMIT_CORE,(0,0)))
   name=f'{suite}-{mode}-{i}';(O/(name+'.log')).write_text(p.stdout+p.stderr);print(name+':\n'+p.stdout+p.stderr,flush=True);p.check_returncode();assert 'PASS' in p.stdout;checks.append(dict(name=name,passed=True))
assert hashes=={n:sha(R/n) for n in tracked}
(O/'verification.json').write_text(json.dumps(dict(passed=True,revision='r29m',checks=checks,sources_sha256=hashes,hardware_tested=False,card_written=False),indent=2)+'\n')
