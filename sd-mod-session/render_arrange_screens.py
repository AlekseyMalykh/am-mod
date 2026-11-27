"""Record the production Arrange painter under ARM QEMU (check_arrange_screens.c) and render
documentation PNGs. Fixture data, substituted font: review pictures, not device photographs.
Usage: render_arrange_screens.py --stock "MPC Sample" --out DIR"""
from pathlib import Path
import argparse,os,subprocess,sys
p=argparse.ArgumentParser();p.add_argument('--stock',required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--no-png',action='store_true');a=p.parse_args()
R=Path(__file__).resolve().parent;O=R/'build/arrange-screens';O.mkdir(parents=True,exist_ok=True);a.out.mkdir(parents=True,exist_ok=True)
core=['src/pattern_tools.cc','src/arrangement.cc','src/arrangement_codec.cc','src/arrangement_native_plan.cc','src/pattern_model.cc','src/pattern_library.cc','src/pattern_codec.cc','src/piano_roll.c','src/midi_event.c','src/phone_input.c','src/arrangement_native_material.cc']
objects=[]
for source in core+['check_arrange_screens.c']:
    cpp=source.endswith('.cc');obj=O/(Path(source).name+'.o');objects.append(str(obj))
    subprocess.run(['aarch64-linux-gnu-g++' if cpp else 'aarch64-linux-gnu-gcc','-std=c++11' if cpp else '-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard','-c',source,'-o',str(obj)],cwd=R,check=True)
target=O/'screens';subprocess.run(['aarch64-linux-gnu-g++',*objects,'-pthread','-ldl','-lm','-o',str(target)],cwd=R,check=True)
sysroot=os.environ.get('MPC_QEMU_SYSROOT','/home/user/mpc-rootfs-user-20260808')
result=subprocess.run(['qemu-aarch64','-L',sysroot,str(target),a.stock,str(O/'draw.txt')],cwd=R,capture_output=True,text=True,timeout=120)
print(result.stdout[-2000:]+result.stderr[-2000:]);result.check_returncode();assert 'PASS Arrange documentation screens recorded' in result.stdout
frames={};name=None
for line in (O/'draw.txt').read_text().splitlines():
    if line.startswith('# '):name=line[2:];frames[name]=[];continue
    if name:frames[name].append(line)
import json
(O/'frames.json').write_text(json.dumps(frames))
if '--no-png' in sys.argv:sys.exit(0)
for name,lines in frames.items():
    # keep only the last full-screen paint of the frame
    starts=[i for i,l in enumerate(lines) if l=='R 0 0 320 240 ff000000']
    if starts:lines=lines[starts[-1]:]
    src=O/(name+'.txt');src.write_text('\n'.join(lines)+'\n')
    subprocess.run([sys.executable,str(R/'render_piano_roll.py'),str(src),str(a.out/('arrange-'+name+'.png'))],check=True)
print('rendered',sorted(frames))
