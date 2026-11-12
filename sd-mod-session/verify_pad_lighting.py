"""Global lighting preferences and actual native Project menu model/dispatch."""
from pathlib import Path
import hashlib,json,subprocess
from phone_build import PROJECT_REQUEST_VA
R=Path(__file__).resolve().parent;O=R/'build/pad-lighting';O.mkdir(exist_ok=True)
subprocess.run(['python3','build_pad_lighting_menu.py'],cwd=R,check=True)
tracked=['src/project.S','src/project.ld','src/music_settings.c','src/music_settings.h','check_lighting_preferences.c','check_project.c','build_pad_lighting_menu.py','verify_pad_lighting.py','build/pad_lighting_project.h']
tracked+=['src/pad_palette.h','src/pad_workflow_colour.h']
sha=lambda n:hashlib.sha256((R/n).read_bytes()).hexdigest();before={n:sha(n) for n in tracked}
syms={v[2]:int(v[0],16) for l in subprocess.check_output(['aarch64-linux-gnu-nm',str(O/'after.elf')],text=True).splitlines() if len(v:=l.split())==3}
for variant,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
 for case,inputs,extra,args in [('preferences',['check_lighting_preferences.c','src/music_settings.c'],[],[]),('project',['check_project.c'],['-DTEST_PHONE=1','-DTEST_LIGHTING_MENU=1','-DTEST_NO_MOD_OFF=1','-DTEST_SCREEN_MIRROR=1','-DTEST_PROJECT_DISPATCH='+str(syms['project_dispatch']),'-DTEST_MUSIC_BRIDGE='+str(PROJECT_REQUEST_VA)],[str(R.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'),str(O/'after.bin')])]:
  target=O/(variant+'-'+case)
  subprocess.run(['aarch64-linux-gnu-gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard',*flags,*extra,*inputs,'-pthread','-lm','-o',str(target)],cwd=R,check=True)
  p=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(target),*args],cwd=R,text=True,capture_output=True,timeout=45)
  (O/(variant+'-'+case+'.log')).write_text(p.stdout+p.stderr);print(p.stdout+p.stderr,end='',flush=True);p.check_returncode()
assert before=={n:sha(n) for n in tracked}
(O/'verification.json').write_text(json.dumps(dict(passed=True,ubsan=True,global_preference_verified=True,six_rows_without_mod_off_verified=True,native_six_row_viewport_verified=True,project_data_preserved=True,sources_sha256=before,hardware_tested=False),indent=2)+'\n')
