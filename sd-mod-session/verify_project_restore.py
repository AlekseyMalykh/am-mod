"""ARM workflow regression and actual native XPJ full/kit branch selection."""
from pathlib import Path
import hashlib,json,subprocess
root=Path(__file__).resolve().parent
out=root/'build/project-restore';out.mkdir(parents=True,exist_ok=True)
sources=['src/workflow_ui.c','src/workflow_ui.h','check_workflow.c','src/browser_tree.c','src/phone_input.c','verify_project_restore.py']
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
before={n:sha(root/n) for n in sources}
stock=root.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
assert sha(stock)=='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
baseline=subprocess.check_output(['git','-c','safe.directory='+str(root.parent),'show','152e07c:sd-mod-session/src/workflow_ui.c'],cwd=root)
(out/'baseline-workflow.c').write_bytes(baseline)
for name,flags in [('before',['-DWORKFLOW_SOURCE="'+str(out/'baseline-workflow.c')+'"']),('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
 target=out/(name+'-workflow')
 subprocess.run(['aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror','-Ibuild','-Isrc','-mbranch-protection=standard',*flags,'-DMPC_EXPERIMENTAL_CHOP_EXPORT=1','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','check_workflow.c','src/workflow_abi.S','src/phone_input.c','src/browser_tree.c','-Wl,--no-as-needed','-lstdc++','-ldl','-pthread','-lm','-o',str(target)],cwd=root,check=True)
 p=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(target),str(root.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'),str(out/(name+'-workflow-draw.txt'))],cwd=root,capture_output=True,text=True,timeout=90)
 (out/(name+'-workflow.log')).write_text(p.stdout+p.stderr)
 print('\n'.join(s for s in (p.stdout+p.stderr).splitlines() if s.startswith(('PASS','FAIL','REPRODUCED'))),flush=True)
 if name=='before':assert p.returncode==1 and 'REPRODUCED autoload started another project while a native modal was open' in p.stderr
 else:p.check_returncode()
assert before=={n:sha(root/n) for n in sources}
(out/'verification.json').write_text(json.dumps(dict(passed=True,ubsan=True,modal_baseline_reproduced=True,baseline_commit='152e07c',baseline_sha256=hashlib.sha256(baseline).hexdigest(),sources_sha256=before,stock_sha256=sha(stock),native='actual XPJ browser branch selection and File callbacks; model/GUI receivers',hardware_tested=False),indent=2)+'\n')
