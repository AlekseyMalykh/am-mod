"""Occupied-source masks through pinned native getters and owner queue; no device."""
from pathlib import Path
import hashlib,json,os,resource,subprocess
R=Path(__file__).resolve().parent
runtime=Path(os.environ.get('MPC_RUNTIME_ROOT',str(R.parent))).resolve()
stock=runtime/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
expected='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
assert sha(stock)==expected
root=os.environ.get('MPC_QEMU_SYSROOT','/home/user/mpc-rootfs-user-20260808')
out=R/'build/mixer-sources';out.mkdir(parents=True,exist_ok=True)
sources=['check_mixer_sources_native.c','src/mixer_sources.cc','src/resample_queue.cc','src/pad_source_abi.S','src/mixer_archive.cc','src/mixer_model.c','src/fx_chain.c','src/fx_eq.c']
tracked=sources+['check_fx_native.c','src/mixer_sources.h','src/resample_queue.h','src/mixer_archive.h','verify_mixer_sources.py']
before={p:sha(R/p) for p in tracked};checks=[]
for mode,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
    objects=[]
    for source in sources:
        cpp=source.endswith('.cc');obj=out/(mode+'-'+Path(source).stem+'.o');objects.append(str(obj))
        subprocess.run(['aarch64-linux-gnu-g++' if cpp else 'aarch64-linux-gnu-gcc','-std=c++11' if cpp else '-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard',*flags,'-c',source,'-o',str(obj)],cwd=R,check=True)
    target=out/mode
    subprocess.run(['aarch64-linux-gnu-g++',*flags,*objects,'-pthread','-ldl','-lm','-o',str(target)],cwd=R,check=True)
    result=subprocess.run(['qemu-aarch64','-L',root,str(target),str(stock)],cwd=R,capture_output=True,text=True,timeout=30,preexec_fn=lambda:resource.setrlimit(resource.RLIMIT_CORE,(0,0)))
    (out/(mode+'.log')).write_text(result.stdout+result.stderr);print(result.stdout+result.stderr,end='',flush=True);result.check_returncode();assert 'PASS native Mixer sources' in result.stdout;checks.append(dict(name=mode,passed=True))
assert before=={p:sha(R/p) for p in tracked} and sha(stock)==expected
(out/'verification.json').write_text(json.dumps(dict(passed=True,checks=checks,sources_sha256=before,firmware_sha256=expected,hardware_tested=False),indent=2)+'\n')
