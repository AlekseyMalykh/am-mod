"""Focused audition lifecycle, native private Preview, UI and routing checks."""
from pathlib import Path
import hashlib,json,subprocess
R=Path(__file__).resolve().parent;O=R/'build/browser-audition';O.mkdir(parents=True,exist_ok=True)
stock=R.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
tracked=['src/seq_audition.c','src/seq_audition.h','src/workflow_ui.c','check_seq_audition.c','check_private_preview.c','check_feature_routes.c','check_workflow.c','src/workflow_abi.S','src/browser_tree.c','src/browser_tree.h',Path(__file__).name]
sha=lambda s:hashlib.sha256((R/s).read_bytes()).hexdigest();before={s:sha(s) for s in tracked};checks=[]
for name,cc,flags,runner in [('host','gcc',[],[]),('asan','gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all'],[]),('arm','aarch64-linux-gnu-gcc',[],['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808']),('arm-ubsan','aarch64-linux-gnu-gcc',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'],['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808'])]:
 for case,files in [('lifecycle',['check_seq_audition.c','src/seq_audition.c']),('routes',['check_feature_routes.c'])]:
  binary=O/(case+'-'+name)
  subprocess.run([cc,'-std=c11','-D_POSIX_C_SOURCE=200809L','-Isrc','-O1','-g','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections',*flags,*files,'-Wl,--gc-sections','-lm','-pthread','-o',str(binary)],cwd=R,check=True)
  p=subprocess.run([*runner,str(binary)],cwd=R,capture_output=True,text=True,timeout=45);(O/(case+'-'+name+'.log')).write_text(p.stdout+p.stderr);print(case,name,p.returncode,flush=True);p.check_returncode();checks.append(case+'-'+name)
for name,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
 binary=O/('native-'+name)
 subprocess.run(['aarch64-linux-gnu-gcc','-std=c11','-D_POSIX_C_SOURCE=200809L','-Isrc','-Ibuild','-DMPC_EXPERIMENTAL_CHOP_EXPORT=1','-O1','-g','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections',*flags,'check_private_preview.c','src/workflow_abi.S','src/phone_input.c','src/browser_tree.c','-Wl,--gc-sections','-Wl,--no-as-needed','-lstdc++','-ldl','-pthread','-lm','-o',str(binary)],cwd=R,check=True)
 p=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(binary),str(stock)],cwd=R,capture_output=True,text=True,timeout=45);(O/('native-'+name+'.log')).write_text(p.stdout+p.stderr);print(p.stdout+p.stderr,end='',flush=True);p.check_returncode();checks.append('native-'+name)
assert before=={s:sha(s) for s in tracked}
(O/'verification.json').write_text(json.dumps(dict(passed=True,sources_sha256=before,checks=checks,hardware_tested=False),indent=2)+'\n')
