"""Production LED interception and pinned native chain/voice lookup; no device."""
from pathlib import Path
import hashlib,json,os,resource,subprocess
R=Path(__file__).resolve().parent
runtime=Path(os.environ.get('MPC_RUNTIME_ROOT',str(R.parent))).resolve()
stock=runtime/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
expected='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
assert sha(stock)==expected
root=os.environ.get('MPC_QEMU_SYSROOT','/home/user/mpc-rootfs-user-20260808')
out=R/'build/mixer-rgb';out.mkdir(parents=True,exist_ok=True)
sources=['check_mixer_rgb.c','src/mixer_leds.c','src/phone_input.c','src/music_settings.c','src/mixer_control.c','src/mixer_model.c','src/fx_catalog.c','src/fx_chain.c','src/fx_eq.c']
tracked=sources+['src/mixer_leds.h','src/phone_native.c','check_fx_native.c','src/mixer_control.h','verify_mixer_rgb.py']
before={p:sha(R/p) for p in tracked};checks=[]
for mode,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
    target=out/mode
    subprocess.run(['aarch64-linux-gnu-gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-I'+str(runtime/'sd-mod-session/build'),'-mbranch-protection=standard','-ffunction-sections','-fdata-sections','-Wl,--gc-sections',*flags,*sources,'-pthread','-ldl','-lm','-o',str(target)],cwd=R,check=True)
    result=subprocess.run(['qemu-aarch64','-L',root,str(target),str(stock)],cwd=R,capture_output=True,text=True,timeout=30,preexec_fn=lambda:resource.setrlimit(resource.RLIMIT_CORE,(0,0)))
    (out/(mode+'.log')).write_text(result.stdout+result.stderr);print(result.stdout+result.stderr,end='',flush=True);result.check_returncode();assert 'PASS Mixer physical RGB path' in result.stdout;checks.append(dict(name=mode,passed=True))
assert before=={p:sha(R/p) for p in tracked} and sha(stock)==expected
(out/'verification.json').write_text(json.dumps(dict(passed=True,checks=checks,sources_sha256=before,firmware_sha256=expected,hardware_tested=False),indent=2)+'\n')
