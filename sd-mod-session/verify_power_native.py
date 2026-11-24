"""Actual pinned native Power function, options, strings and hook under QEMU."""
from pathlib import Path
import argparse,hashlib,json,subprocess
p=argparse.ArgumentParser();p.add_argument('--stock',type=Path,required=True);a=p.parse_args()
R=Path(__file__).resolve().parent;O=R/'build/power-native';O.mkdir(exist_ok=True)
stock_sha=hashlib.sha256(a.stock.read_bytes()).hexdigest()
assert stock_sha=='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
names=['check_power_native.c','check_power_native.cc','check_fx_native.c','src/power_control.cc','src/power_control.h','verify_power_native.py']
hashes=lambda:{n:hashlib.sha256((R/n).read_bytes()).hexdigest() for n in names}
before=hashes()
for mode,extra in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
 objs=[]
 for src in names[:2]:
  cpp=src.endswith('.cc');obj=O/(mode+src+'.o');objs.append(str(obj))
  subprocess.run(['aarch64-linux-gnu-g++' if cpp else 'aarch64-linux-gnu-gcc','-std=c++11' if cpp else '-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard',*extra,'-c',src,'-o',str(obj)],cwd=R,check=True)
 exe=O/mode
 subprocess.run(['aarch64-linux-gnu-g++',*extra,*objs,'-ldl','-lm','-o',str(exe)],cwd=R,check=True)
 r=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(exe),str(a.stock)],cwd=R,capture_output=True,text=True,timeout=30)
 (O/(mode+'.log')).write_text(r.stdout+r.stderr);print(r.stdout+r.stderr,flush=True);r.check_returncode();assert 'PASS actual pinned native Power question' in r.stdout
assert before==hashes()
(O/'verification.json').write_text(json.dumps(dict(passed=True,stock_sha256=stock_sha,sources_sha256=before,actual_native_question=True,actual_native_strings=True,gui_manager='fixture',save='fixture',full_vm_verified=False,hardware_tested=False),indent=2)+'\n')
