"""Sample/overlay UI boundary against pinned native firmware; no hardware."""
from pathlib import Path
import hashlib,json,os,subprocess
R=Path(__file__).resolve().parent;O=R/'build/sample-overlay';O.mkdir(parents=True,exist_ok=True)
stock=R.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
sources=['check_pad_eq_ui.c','check_header_toggle.S','src/step_abi.S','src/music_ui.c','src/phone_input.c','src/pad_eq.c','src/pad_eq_native.c','src/music_settings.c','src/fx_eq.c']
tracked=sources+['check_pad_eq_native.c','src/music_ui.h',Path(__file__).name]
sha=lambda p:hashlib.sha256((R/p).read_bytes()).hexdigest();before={p:sha(p) for p in tracked}
for name,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
 target=O/name
 subprocess.run(['aarch64-linux-gnu-gcc','-O1','-g','-std=c11','-Wall','-Wextra','-Werror','-mbranch-protection=standard','-Ibuild','-DMPC_PAD_EQ=1',*flags,*sources,'-pthread','-ldl','-lm','-o',str(target)],cwd=R,check=True)
 result=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(target),str(stock),str(O/'draw.txt')],cwd=R,env={**os.environ,'AM_SAMPLE_OVERLAY_ONLY':'1'},capture_output=True,text=True,timeout=30)
 (O/(name+'.log')).write_text(result.stdout+result.stderr);print(result.stdout+result.stderr,end='',flush=True);result.check_returncode();assert 'PASS Sample overlay' in result.stdout
assert before=={p:sha(p) for p in tracked}
(O/'verification.json').write_text(json.dumps(dict(passed=True,sources_sha256=before,hardware_tested=False),indent=2)+'\n')
