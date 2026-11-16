"""Knob/display/reset mapping and idle visuals on host/sanitizers/AArch64."""
from pathlib import Path
import hashlib,json,subprocess
R=Path(__file__).resolve().parent
out=R/'build/fx-layout';out.mkdir(parents=True,exist_ok=True)
sources=['check_fx_layout.c','src/mixer_control.c','src/mixer_input.c','src/phone_input.c','src/mixer_model.c','src/mixer_meters.c','src/fx_catalog.c','src/fx_chain.c','src/fx_own.c','src/compressor_dsp.c','src/fx_eq.c','src/compressor_animation.c']
tracked=sources+['src/fx_layout.inc','src/fx_catalog.h','src/fx_visual.inc','src/mixer_view.c','src/mixer_view.h','verify_fx_layout.py']
sha=lambda p:hashlib.sha256((R/p).read_bytes()).hexdigest()
tracked += ['src/fx_amp_models.inc']
before={p:sha(p) for p in tracked};checks=[]
for name,cc,flags,runner in [('host','gcc',[],[]),('host-asan','gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all'],[]),('arm','aarch64-linux-gnu-gcc',[],['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808'])]:
 target=out/name
 subprocess.run([cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*sources,'-lm','-pthread','-o',str(target)],cwd=R,check=True)
 result=subprocess.run([*runner,str(target),str(out)],cwd=R,text=True,capture_output=True,timeout=60)
 (out/(name+'.log')).write_text(result.stdout+result.stderr);print(result.stdout+result.stderr,end='',flush=True);result.check_returncode();checks.append(name)
assert before=={p:sha(p) for p in tracked}
(out/'verification.json').write_text(json.dumps(dict(passed=True,checks=checks,sources_sha256=before,hardware_tested=False),indent=2)+'\n')
