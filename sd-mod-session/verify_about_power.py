"""Target current About through the existing native UI fixture.
The full legacy settings assertion uses an obsolete 'Pad lighting' first tab;
this explicit scope exits before that assertion, and does not claim its suite.
"""
from pathlib import Path
import argparse,hashlib,json,os,subprocess
p=argparse.ArgumentParser();p.add_argument('--stock',type=Path,required=True);a=p.parse_args()
R=Path(__file__).resolve().parent;O=R/'build/about-power';O.mkdir(exist_ok=True)
assert hashlib.sha256(a.stock.read_bytes()).hexdigest()=='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
sources=['check_pad_eq_ui.c','check_header_toggle.S','src/step_abi.S','src/music_ui.c','src/phone_input.c','src/pad_eq.c','src/pad_eq_native.c','src/music_settings.c','src/fx_eq.c']
tracked=sources+['src/mod_build.h','check_pad_eq_native.c','check_fx_native.c','build/pad_lighting_project.h','verify_about_power.py']
digest=lambda:{n:hashlib.sha256((R/n).read_bytes()).hexdigest() for n in tracked};before=digest()
for mode,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
 exe=O/mode
 subprocess.run(['aarch64-linux-gnu-gcc','-O1','-g','-std=c11','-Wall','-Wextra','-Werror','-mbranch-protection=standard','-Ibuild','-DMPC_PAD_EQ=1',*flags,*sources,'-pthread','-ldl','-lm','-o',str(exe)],cwd=R,check=True)
 result=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(exe),str(a.stock),str(O/'draw.txt')],cwd=R,env=dict(os.environ,AM_ABOUT_ONLY='1'),capture_output=True,text=True,timeout=30)
 (O/(mode+'.log')).write_text(result.stdout+result.stderr);print(result.stdout+result.stderr,flush=True);result.check_returncode();assert 'PASS About retains version/repository' in result.stdout
assert before==digest()
(O/'verification.json').write_text(json.dumps(dict(passed=True,sources_sha256=before,about_no_exit_action=True,checks=['normal','ubsan'],legacy_full_settings_suite=False,hardware_tested=False),indent=2)+'\n')
