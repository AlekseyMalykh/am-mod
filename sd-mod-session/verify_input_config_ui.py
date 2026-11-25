"""Pinned native callsite and monitor indicator, AArch64/QEMU; no card writes."""
from pathlib import Path
import hashlib,json,subprocess,os
R=Path(__file__).resolve().parent;O=R/'build/input-config-ui';O.mkdir(parents=True,exist_ok=True)
stock=Path(os.environ.get('MPC_RUNTIME_ROOT',str(R.parent)))/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
assert hashlib.sha256(stock.read_bytes()).hexdigest()=='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
tracked=['check_rear_ui.c','src/rear_ui.c','src/music_settings.h','check_input_config_ui.c',Path(__file__).name]
sha=lambda p:hashlib.sha256((R/p).read_bytes()).hexdigest();before={p:sha(p) for p in tracked}
for mode,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
 target=O/mode
 subprocess.run(['aarch64-linux-gnu-gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
 '-mbranch-protection=standard','-ffunction-sections','-fdata-sections',*flags,
 'check_input_config_ui.c','-Wl,--gc-sections','-lm','-o',str(target)],cwd=R,check=True)
 p=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(target),str(stock)],capture_output=True,text=True,timeout=30)
 (O/(mode+'.log')).write_text(p.stdout+p.stderr);print(p.stdout+p.stderr,end='',flush=True);p.check_returncode()
assert before=={p:sha(p) for p in tracked}
(O/'verification.json').write_text(json.dumps(dict(passed=True,sources_sha256=before,hardware_tested=False),indent=2)+'\n')
