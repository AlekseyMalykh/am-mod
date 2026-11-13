"""Pinned native input audit in private ARM/QEMU mapping, never on device."""
from pathlib import Path
import hashlib,json,os,resource,subprocess
R=Path(__file__).resolve().parent
runtime=Path(os.environ.get('MPC_RUNTIME_ROOT',str(R.parent)))
stock=runtime/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
root=os.environ.get('MPC_QEMU_SYSROOT','/home/user/mpc-rootfs-user-20260808')
out=R/'build/mixer-input-native';out.mkdir(parents=True,exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
expected='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
assert sha(stock)==expected
sources=['check_mixer_input_native.c','src/mixer_input.c','src/mixer_control.c','src/phone_input.c','src/mixer_model.c','src/fx_catalog.c','src/fx_chain.c','src/fx_own.c', 'src/compressor_dsp.c','src/fx_eq.c']
tracked=sources+['src/mixer_input.h','src/mixer_control.h','src/mixer_view.h','check_fx_native.c','verify_mixer_input_native.py']
before={p:sha(R/p) for p in tracked};checks=[]
for name,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
    target=out/name
    subprocess.run(['aarch64-linux-gnu-gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard',*flags,*sources,'-ldl','-lm','-o',str(target)],cwd=R,check=True)
    result=subprocess.run(['qemu-aarch64','-L',root,str(target),str(stock)],cwd=R,text=True,capture_output=True,timeout=30,preexec_fn=lambda:resource.setrlimit(resource.RLIMIT_CORE,(0,0)))
    (out/(name+'.log')).write_text(result.stdout+result.stderr);print(result.stdout+result.stderr,end='',flush=True);result.check_returncode();checks.append(dict(name=name,passed=True))
assert sha(stock)==expected and before=={p:sha(R/p) for p in tracked}
(out/'verification.json').write_text(json.dumps(dict(passed=True,checks=checks,sources_sha256=before,firmware_sha256=expected,native_ui_connected=False,hardware_tested=False,stock_fader_opens_on_down=True),indent=2)+'\n')
