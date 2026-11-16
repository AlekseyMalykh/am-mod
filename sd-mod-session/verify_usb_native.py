"""Execute the actual installed USB tap against pinned ARM native engine DSP."""
from pathlib import Path
import hashlib,json,os,resource,subprocess
R=Path(__file__).resolve().parent
stock=R.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(stock)=='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
root=os.environ.get('MPC_QEMU_SYSROOT','/home/user/mpc-rootfs-user-20260808')
out=R/'build/usb-native';out.mkdir(parents=True,exist_ok=True)
sources=['check_usb_live_native.c','src/mixer_session.cc','src/mixer_runtime.cc','src/mixer_fx.cc','src/fx_native.cc','src/mixer_sources.cc','src/resample_queue.cc','src/pad_source_abi.S','src/mixer_control.c','src/phone_input.c','src/mixer_archive.cc','src/mixer_live.c','src/mixer_live_native.c','src/mixer_route_native.c','src/mixer_buses.c','src/mixer_model.c','src/mixer_meters.c','src/fx_chain.c','src/fx_own.c','src/compressor_dsp.c','src/fx_eq.c','src/fx_catalog.c','src/pad_eq.c','src/pad_eq_native.c','src/music_settings.c','src/input_fx.cc','src/usb_audio.c']
tracked=sources+['src/usb_audio.h','check_mixer_session_native.c','check_mixer_live_native.c','check_mixer_route_dsp.c','check_mixer_route_native.c','check_fx_native.c','verify_usb_native.py']
tracked += ['src/fx_layout.inc']
tracked += ['src/fx_amp_models.inc']
before={p:sha(R/p) for p in tracked}
for mode,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
 objects=[]
 for source in sources:
  cpp=source.endswith('.cc');obj=out/(mode+'-'+Path(source).stem+'.o');objects.append(str(obj))
  subprocess.run(['aarch64-linux-gnu-g++' if cpp else 'aarch64-linux-gnu-gcc','-std=c++11' if cpp else '-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard','-DMPC_USB_AUDIO=1',*flags,'-c',source,'-o',str(obj)],cwd=R,check=True)
 target=out/mode
 subprocess.run(['aarch64-linux-gnu-g++',*flags,*objects,'-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free','-pthread','-ldl','-lm','-o',str(target)],cwd=R,check=True)
 result=subprocess.run(['qemu-aarch64','-L',root,str(target),str(stock)],cwd=R,capture_output=True,text=True,timeout=30,preexec_fn=lambda:resource.setrlimit(resource.RLIMIT_CORE,(0,0)))
 (out/(mode+'.log')).write_text(result.stdout+result.stderr);print(result.stdout+result.stderr,end='',flush=True);result.check_returncode();assert 'PASS production USB tap:' in result.stdout
assert before=={p:sha(R/p) for p in tracked}
(out/'verification.json').write_text(json.dumps(dict(passed=True,sources_sha256=before,pinned_live_engine=True,all_18_channels_compared=True,offline_excluded=True,project_reset=True,ubsan=True,hardware_tested=False),indent=2)+'\n')
