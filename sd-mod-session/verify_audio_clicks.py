"""Regression checks for live FX continuity and the sample pitch worker; no device writes."""
from pathlib import Path
import subprocess,json
R=Path(__file__).resolve().parent
out=R/'build/audio-clicks';out.mkdir(parents=True,exist_ok=True)
checks=[]
for name,prefix,runner,flags in [('host','',[],[]),('host-ubsan','',[],['-fsanitize=undefined','-fno-sanitize-recover=all']),('arm','aarch64-linux-gnu-',['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808'],[])]:
 for suite in ['fx','tuner']:
  target=out/(name+'-'+suite)
  if suite=='fx':
   command=[prefix+'gcc','-std=c11','check_fx_continuity.c','src/fx_chain.c','src/fx_own.c','src/fx_eq.c','src/compressor_dsp.c']
  else:
   command=[prefix+'g++','-std=c++11','-ffunction-sections','-fdata-sections','check_sample_tuner_worker.cc','-Wl,--gc-sections']
  subprocess.run([*command,'-O1','-g','-Wall','-Wextra','-Werror',*flags,'-pthread','-lm','-o',str(target)],cwd=R,check=True)
  result=subprocess.run([*runner,str(target)],cwd=R,text=True,capture_output=True,timeout=30)
  print(result.stdout+result.stderr,end='',flush=True);result.check_returncode();checks.append(name+'-'+suite)
(out/'verification.json').write_text(json.dumps(dict(passed=True,checks=checks,hardware_tested=False),indent=2)+'\n')
