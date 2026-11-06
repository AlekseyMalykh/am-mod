"""Live native Mixer FX through automatic sessions, real DSP and clock."""
from pathlib import Path
import hashlib,json,os,resource,subprocess
R=Path(__file__).resolve().parent
runtime=Path(os.environ.get('MPC_RUNTIME_ROOT',str(R.parent))).resolve()
stock=runtime/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
expected='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
assert sha(stock)==expected
root=os.environ.get('MPC_QEMU_SYSROOT','/home/user/mpc-rootfs-user-20260808')
out=R/'build/mixer-fx';out.mkdir(parents=True,exist_ok=True)
sources=['check_mixer_fx.c','check_mixer_fx_fixture.cc','src/mixer_session.cc','src/mixer_runtime.cc','src/mixer_fx.cc','src/fx_native.cc','src/mixer_sources.cc','src/resample_queue.cc','src/pad_source_abi.S','src/mixer_control.c','src/mixer_archive.cc','src/mixer_live.c','src/mixer_live_native.c','src/mixer_route_native.c','src/mixer_buses.c','src/mixer_model.c','src/mixer_meters.c','src/fx_chain.c','src/fx_eq.c','src/fx_catalog.c','src/pad_eq.c','src/pad_eq_native.c']
tracked=sources+['check_mixer_session_native.c','check_fx_metadata.h','check_mixer_live_native.c','check_mixer_route_dsp.c','check_mixer_route_native.c','check_fx_native.c','src/mixer_session.h','src/mixer_runtime.h','src/mixer_fx.h','src/fx_native.h','src/mixer_sources.h','src/resample_queue.h','src/mixer_control.h','src/mixer_live.h','src/mixer_archive.h','src/mixer_view.h','src/mixer_buses.h','src/mixer_model.h','src/mixer_meters.h','src/mixer_route_native.h','src/fx_chain.h','src/fx_catalog.h','src/fx_eq.h','src/pad_eq.h','src/pad_eq_native.h','verify_mixer_fx.py']
before={p:sha(R/p) for p in tracked};checks=[]
for mode,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
    objects=[]
    for source in sources:
        cpp=source.endswith('.cc');obj=out/(mode+'-'+Path(source).stem+'.o');objects.append(str(obj))
        subprocess.run(['aarch64-linux-gnu-g++' if cpp else 'aarch64-linux-gnu-gcc','-std=c++11' if cpp else '-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard',*flags,'-c',source,'-o',str(obj)],cwd=R,check=True)
    target=out/mode
    subprocess.run(['aarch64-linux-gnu-g++',*flags,*objects,'-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free','-pthread','-ldl','-lm','-o',str(target)],cwd=R,check=True)
    result=subprocess.run(['qemu-aarch64','-L',root,str(target),str(stock)],cwd=R,capture_output=True,text=True,timeout=60,preexec_fn=lambda:resource.setrlimit(resource.RLIMIT_CORE,(0,0)))
    (out/(mode+'.log')).write_text(result.stdout+result.stderr)
    print(result.stdout if result.returncode==0 else result.stdout+result.stderr,end='',flush=True)
    result.check_returncode();assert 'PASS Mixer native FX' in result.stdout;checks.append(dict(name=mode,passed=True))
assert before=={p:sha(R/p) for p in tracked} and sha(stock)==expected
(out/'verification.json').write_text(json.dumps(dict(passed=True,checks=checks,sources_sha256=before,firmware_sha256=expected,hardware_tested=False,native_ui_connected=False,full_xpj_roundtrip_tested=False),indent=2)+'\n')
