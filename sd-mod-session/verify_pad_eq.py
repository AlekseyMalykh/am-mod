"""One-point per-pad EQ, native render ABI, persistence and layout."""
from pathlib import Path
import subprocess,json,hashlib
root=Path(__file__).resolve().parent;out=root/'build/pad-eq';out.mkdir(exist_ok=True)
stock=root.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
assert hashlib.sha256(stock.read_bytes()).hexdigest()=='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
sources=['build/pad_lighting_project.h','src/project.S','src/pad_eq.c','src/pad_eq.h','src/pad_eq_native.c','src/pad_eq_native.h','src/pad_eq_history.cc','src/pad_eq_history.h','src/music_ui.c','src/fx_eq.c','src/fx_eq.h','src/music_settings.c','src/music_settings.h','check_pad_eq_native.c','check_pad_eq_ui.c','check_pad_eq_history.c','src/step_abi.S','check_header_toggle.S','verify_pad_eq.py']
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
before={s:digest(root/s) for s in sources}
for name,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
 target=out/name
 subprocess.run(['aarch64-linux-gnu-gcc','-O1','-g','-std=c11','-Wall','-Wextra','-Werror','-mbranch-protection=standard','-DMPC_PAD_EQ=1',*flags,'check_pad_eq_native.c','src/pad_eq.c','src/pad_eq_native.c','src/music_settings.c','src/fx_eq.c','-pthread','-ldl','-lm','-o',str(target)],cwd=root,check=True)
 r=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(target),str(stock),str(out/'draw.txt')],cwd=root,capture_output=True,text=True,timeout=30)
 (out/(name+'.log')).write_text(r.stdout+r.stderr);print(r.stdout+r.stderr,end='');r.check_returncode()
 target=out/(name+'-ui')
 subprocess.run(['aarch64-linux-gnu-gcc','-O1','-g','-std=c11','-Wall','-Wextra','-Werror','-mbranch-protection=standard','-Ibuild','-DMPC_PAD_EQ=1',*flags,'check_pad_eq_ui.c','check_header_toggle.S','src/step_abi.S','src/music_ui.c','src/pad_eq.c','src/pad_eq_native.c','src/music_settings.c','src/fx_eq.c','-pthread','-ldl','-lm','-o',str(target)],cwd=root,check=True)
 r=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(target),str(stock),str(out/'draw.txt')],cwd=root,capture_output=True,text=True,timeout=30)
 (out/(name+'-ui.log')).write_text(r.stdout+r.stderr);print(r.stdout+r.stderr,end='');r.check_returncode()
 history=out/(name+'-history.o');target=out/(name+'-history')
 subprocess.run(['aarch64-linux-gnu-g++','-O1','-g','-std=c++11','-Wall','-Wextra','-Werror','-mbranch-protection=standard',*flags,'-c','src/pad_eq_history.cc','-o',str(history)],cwd=root,check=True)
 subprocess.run(['aarch64-linux-gnu-gcc','-O1','-g','-std=c11','-Wall','-Wextra','-Werror','-mbranch-protection=standard','-DMPC_PAD_EQ=1',*flags,'check_pad_eq_history.c','src/pad_eq.c','src/pad_eq_native.c','src/music_settings.c','src/fx_eq.c',str(history),'-lstdc++','-pthread','-ldl','-lm','-o',str(target)],cwd=root,check=True)
 r=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(target),str(stock),str(out/'draw.txt')],cwd=root,capture_output=True,text=True,timeout=30)
 (out/(name+'-history.log')).write_text(r.stdout+r.stderr);print(r.stdout+r.stderr,end='');r.check_returncode()
assert before=={s:digest(root/s) for s in sources}
for name in ['normal','ubsan']:
 assert 'PASS Standard retains Lights mode/key/scale and Transpose16 controls' in (out/(name+'-ui.log')).read_text()
(out/'verification.json').write_text(json.dumps({'passed':True,'ubsan':True,'native_sidebars_preserved':True,'hardware_tested':False,'card_written':False,'scope':'single-point DSP/native common instrument render with generator and insert boundaries; project extension and layout','sources_sha256':before},indent=2)+'\n')
