"""Controller navigation/edits/ownership; actual owner bridge tested separately."""
from pathlib import Path
import hashlib,json,os,subprocess
R=Path(__file__).resolve().parent
out=R/'build/mixer-control';out.mkdir(parents=True,exist_ok=True)
sources=['check_mixer_control.c','src/mixer_control.c','src/mixer_model.c','src/fx_catalog.c','src/fx_chain.c','src/fx_eq.c']
tracked=sources+['src/mixer_control.h','src/mixer_view.h','verify_mixer_control.py']
sha=lambda p:hashlib.sha256((R/p).read_bytes()).hexdigest()
before={p:sha(p) for p in tracked};checks=[]
root=os.environ.get('MPC_QEMU_SYSROOT','/home/user/mpc-rootfs-user-20260808')
for name,compiler,flags,runner in [('host','gcc',[],[]),('host-asan','gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all'],[]),('arm','aarch64-linux-gnu-gcc',['-mbranch-protection=standard'],['qemu-aarch64','-L',root]),('arm-ubsan','aarch64-linux-gnu-gcc',['-mbranch-protection=standard','-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'],['qemu-aarch64','-L',root])]:
    target=out/name
    subprocess.run([compiler,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*sources,'-lm','-o',str(target)],cwd=R,check=True)
    result=subprocess.run([*runner,str(target)],cwd=R,text=True,capture_output=True,timeout=30)
    (out/(name+'.log')).write_text(result.stdout+result.stderr);print(result.stdout+result.stderr,end='',flush=True);result.check_returncode();checks.append(dict(name=name,passed=True))
assert before=={p:sha(p) for p in tracked}
(out/'verification.json').write_text(json.dumps(dict(passed=True,checks=checks,sources_sha256=before,native_input_connected=False,hardware_tested=False),indent=2)+'\n')
