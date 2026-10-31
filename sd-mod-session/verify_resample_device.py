"""Experimental UI against real firmware routines; never writes an SD card."""
from pathlib import Path
from hashlib import sha256
import subprocess,json,sys,tempfile
sanitizer='--ubsan' in sys.argv
flags=['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'] if sanitizer else []
R=Path(__file__).resolve().parent
OUT=R/('build/resample-device-ubsan' if sanitizer else 'build/resample-device');OUT.mkdir(exist_ok=True)
stock=R.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
digest=lambda p:sha256(p.read_bytes()).hexdigest()
assert digest(stock)=='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
info=json.loads((R/'build-info.json').read_text());assert info['revision']=='r16d-sample-input-preview'
protected=[R/'build-info.json',R.parent/info['zip']]+sorted(p for p in (R/'autorun').rglob('*') if p.is_file())
before={str(p):digest(p) for p in protected}
sources=['check_steps_support.cc','src/step_exceptions.cc','src/step_mute.cc','src/pad_output_stream.cc','src/pad_sample_source.cc','src/pad_memory_sample.cc','src/pad_sample_pool.cc','src/pad_assignment.cc','src/pad_resample_commit.cc','src/pad_resample_history.cc','src/resample_queue.cc','src/resample_native.cc','src/resample_context.cc','src/fx_native.cc','src/resample_runtime.cc','src/resample_ui.cc']
objects=[]
for source in sources:
 obj=OUT/(Path(source).stem+'.o');objects.append(str(obj))
 subprocess.run(['aarch64-linux-gnu-g++','-c','-fPIC','-O1','-g','-std=c++11','-Wall','-Wextra','-Werror',*flags,'-mbranch-protection=standard',source,'-o',str(obj)],cwd=R,check=True)
target=OUT/'ui-bridge'
c_sources=['src/resample_worker.c','src/resample_flow.c','src/resample_view.c','src/fx_catalog.c','src/resample_panel.c','src/pad_render.c','src/fx_chain.c','src/fx_eq.c','src/fx_eq_editor.c']
subprocess.run(['aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-Ibuild','-mbranch-protection=standard','check_resample_ui.c','check_chop_pcm_support.S','check_steps_support.S','src/step_abi.S','src/pad_output_abi.S','src/pad_source_abi.S','src/pad_memory_abi.S','src/pad_assignment_abi.S',*c_sources,*objects,'-lstdc++','-ldl','-pthread','-lm','-o',str(target)],cwd=R,check=True)
result=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(target),str(stock),str(OUT/'eq-draw.txt')],cwd=R,capture_output=True,text=True,timeout=45)
(OUT/'ui-bridge.log').write_text(result.stdout+result.stderr);result.check_returncode()
shutdown_target=OUT/'shutdown-bridge'
subprocess.run(['aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-Ibuild','-mbranch-protection=standard','check_resample_shutdown.c','check_chop_pcm_support.S','check_steps_support.S','src/step_abi.S','src/pad_output_abi.S','src/pad_source_abi.S','src/pad_memory_abi.S','src/pad_assignment_abi.S',*c_sources,*objects,'-lstdc++','-ldl','-pthread','-lm','-o',str(shutdown_target)],cwd=R,check=True)
shutdown=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(shutdown_target),str(stock)],cwd=R,capture_output=True,text=True,timeout=45)
(OUT/'shutdown-bridge.log').write_text(shutdown.stdout+shutdown.stderr);shutdown.check_returncode()
routes=OUT/'routes'
subprocess.run(['aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-Ibuild','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','check_resample_routes.c','src/phone_events.c','src/phone_input.c','-ldl','-pthread','-o',str(routes)],cwd=R,check=True)
routed=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(routes)],cwd=R,capture_output=True,text=True,timeout=30)
(OUT/'routes.log').write_text(routed.stdout+routed.stderr);routed.check_returncode()
library=None;callbacks=None
if not sanitizer:
 production_cpp=['src/batch_copy.cc','src/batch_events.cc']
 for source in production_cpp:
  obj=OUT/(Path(source).stem+'.o');objects.append(str(obj))
  subprocess.run(['aarch64-linux-gnu-g++','-c','-fPIC','-O2','-std=c++11','-Wall','-Wextra','-Werror','-mbranch-protection=standard',source,'-o',str(obj)],cwd=R,check=True)
 production_c=['src/phone_input.c','src/phone_events.c','src/phone_native.c','src/note_native.c','src/note_abi.S','src/music_settings.c','src/music_archive.c','src/music_ui.c','src/music_lighting.c','src/step_native.c','src/step_abi.S','src/workflow_ui.c','src/workflow_abi.S','src/browser_tree.c']
 library=OUT/'phone-input-resample.so'
 # Fixture support is linked only into tests, never into the actual library.
 native_objects=[p for p in objects if Path(p).stem!='check_steps_support']
 subprocess.run(['aarch64-linux-gnu-gcc','-shared','-fPIC','-O2','-std=c11','-Wall','-Wextra','-Werror','-Wcast-function-type','-Ibuild','-DMPC_RESAMPLE_TRIAL=1','-DMPC_EXPERIMENTAL_CHOP_EXPORT=1','-mbranch-protection=standard','-Wl,-z,noexecstack,-z,relro,-z,now','-Wl,--no-undefined',*production_c,*c_sources,'src/pad_output_abi.S','src/pad_source_abi.S','src/pad_memory_abi.S','src/pad_assignment_abi.S',*native_objects,'-lstdc++','-ldl','-pthread','-lm','-o',str(library)],cwd=R,check=True)
 from verify_callbacks import verify
 callbacks=verify(library)
 with tempfile.TemporaryDirectory(prefix='mpc-resample-ui-') as folder:
  preload=Path(folder)/'phone-input.so';preload.write_bytes(library.read_bytes())
  loaded=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808','-E','LD_DEBUG=libs','-E','LD_PRELOAD='+str(preload),'/home/user/mpc-rootfs-user-20260808/usr/bin/busybox.nosuid','true'],cwd=R,capture_output=True,text=True,timeout=30)
  (OUT/'load.log').write_text(loaded.stdout+loaded.stderr);loaded.check_returncode()
  assert 'calling init: '+str(preload) in loaded.stderr and 'cannot be preloaded' not in loaded.stderr
 sources+=production_cpp+production_c
assert before=={str(p):digest(p) for p in protected}
sources+=c_sources+['src/resample_ui.h','src/resample_runtime.h','src/resample_context.h','src/resample_queue.h','src/phone_native.c','src/phone_events.h','src/fx_eq.h','src/fx_eq_editor.h','src/fx_catalog.h','check_fx_eq_editor.c','src/fx_native.h','src/fx_chain.h','src/pad_render.h','src/pad_output_abi.S','src/pad_source_abi.S','src/pad_memory_abi.S','src/pad_assignment_abi.S','check_resample_ui.c','check_resample_routes.c','check_resample_runtime.c','check_resample_shutdown.c','verify_resample_device.py']
report={'prototype':'resample-physical-ui-bridge','passed':True,'ubsan':sanitizer,'production_unchanged':True,'card_written':False,'hardware_tested':False,'device_trial_ready':False,'limited_device_trial_ready':False,'known_hardware_failure':'r18a generated Sample playback SIGSEGV; cause unresolved; local passes do not clear this failure','trial_scope':'local diagnostics only; no new SD trial until generated Sample playback failure is resolved or a separate diagnostic trial is explicitly prepared','experimental_host_connected':['physical pad/button/controller hooks','paintOverChildren on Sample','project replacement and context/screen destruction notifications','lifetime hook prohibits wake/repost before native context destruction'],'cancelled_boot_destructor':'actual CopyContext destructor and ordinary queue; separate priority queue empty fixture; orphan runtime retained until process exit','library_sha256':digest(library) if library else None,'callbacks':callbacks,'not_proven':['full hardware shutdown/native history dependency order; normal trial closes panel before Mod Off','actual device parameter receiver, audio sampler and GUI registry','native Seq gesture handoff on hardware','Save/reopen, live comparison and device service lookup for native FX'],'results':list(dict.fromkeys(x for x in (result.stdout+shutdown.stdout+routed.stdout).splitlines() if x.startswith('PASS'))),'sources_sha256':{s:digest(R/s) for s in sources}}
(OUT/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
