"""Targeted ARM regressions for browser, pad text and Steps; no card writes."""
from pathlib import Path
from hashlib import sha256
import subprocess,json,sys
R=Path(__file__).resolve().parent
san='--ubsan' in sys.argv
flags=['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'] if san else []
OUT=R/('build/r17a-ubsan' if san else 'build/r17a');OUT.mkdir(exist_ok=True)
stock=R.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
q=['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808']
results=[]
def run(label,args):
 p=subprocess.run(args,cwd=R,capture_output=True,text=True,timeout=90)
 (OUT/(label+'.log')).write_text(p.stdout+p.stderr)
 if p.returncode:print(p.stdout+p.stderr)
 p.check_returncode();results.extend(line for line in p.stdout.splitlines() if line.startswith('PASS'))
c=['aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror','-Ibuild','-mbranch-protection=standard',*flags]
objects=[]
for name in ['check_steps_support','src/step_exceptions','src/step_mute']:
 obj=OUT/(Path(name).name+'.o');objects.append(str(obj))
 run('compile-'+obj.stem,['aarch64-linux-gnu-g++','-c','-O1','-g','-std=c++11','-Wall','-Wextra','-Werror','-mbranch-protection=standard',*flags,name+'.cc','-o',str(obj)])
for name,sources,args in [
 ('music-lights',['check_music_lights.c','src/music_settings.c'],[]),
 ('levels-exit',['check_levels_exit.c','src/music_lighting.c','src/music_settings.c'],[]),
 ('phone',['check_phone_cursor.c','src/phone_input.c'],[]),
 ('naming',['check_naming_shift.c','src/phone_input.c'],[]),
 ('workflow',['check_workflow.c','src/workflow_abi.S','src/phone_input.c','src/browser_tree.c'],[str(stock),str(OUT/'workflow-draw.txt')]),
 ('steps',['check_steps.c','check_steps_support.S','src/step_abi.S',*objects],[str(stock),str(OUT/'steps-draw.txt')])]:
 target=OUT/name
 run('compile-'+name,[*c,'-DMPC_EXPERIMENTAL_CHOP_EXPORT=1','-ffunction-sections','-fdata-sections','-Wl,--gc-sections',*sources,'-Wl,--no-as-needed','-lstdc++','-ldl','-pthread','-lm','-o',str(target)])
 run(name,[*q,str(target),*args])
sources=['src/music_lighting.c','src/music_lighting.h','check_levels_exit.c','src/music_settings.c','src/music_settings.h','check_music_lights.c','src/phone_input.c','src/phone_input.h','src/phone_native.c','src/step_native.c','src/workflow_ui.c','src/browser_tree.c','src/browser_tree.h','src/resample_ui.cc','check_resample_routes.c','check_phone_cursor.c','check_naming_shift.c','check_steps.c','check_workflow.c','verify_r17a.py']
report={'passed':True,'ubsan':san,'hardware_tested':False,'results':results,'fixtures':['GUI and native listing callback','model parameter/audio callbacks'],'actual_native':['BrowserLogic1db1020 directory setter (directory permission, label and JUCE list receivers are fixtures)','native event containers and time conversion'],'sources_sha256':{s:sha256((R/s).read_bytes()).hexdigest() for s in sources}}
(OUT/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'passed':True,'ubsan':san,'results':results},indent=2))
