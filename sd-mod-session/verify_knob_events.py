"""ARM regression for issue #2; records native parameter calls, not hardware ADC.
Reproduces the old overflow/overtaking before checking production queue fixes.
"""
from pathlib import Path
import hashlib,json,subprocess
R=Path(__file__).resolve().parent;O=R/'build/knob-events';O.mkdir(parents=True,exist_ok=True)
sha=lambda b:hashlib.sha256(b).hexdigest()
names=['src/phone_events.c','src/phone_events.h','src/phone_native.c','src/phone_input.c','src/takeover_endpoints.c','src/takeover_endpoints.h','src/values.S','src/values.ld','src/values_ui.S','src/values_ui_map.inc','check_scaled_display.c','check_knob_events.c','check_knob_queue.c','check_scaled_native.c','check_resample_routes.c','check_naming_shift.c','verify_knob_events.py']
names+=['check_knob_compressor_routes.c','src/compressor.h','check_knob_native_path.c','check_takeover_native.c','src/takeover_native.c','src/takeover_native.h']
hashes={n:sha((R/n).read_bytes()) for n in names}
baseline=subprocess.check_output(['git','-c','safe.directory='+str(R.parent),'show','9841eab:sd-mod-session/src/phone_events.c'],cwd=R)
(O/'baseline-phone-events.c').write_bytes(baseline)
q=['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808']
results=[]
stock=R.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
subprocess.run(['aarch64-linux-gnu-gcc','-DNOTE_UI','-c','src/values.S','-o',str(O/'values.o')],cwd=R,check=True)
subprocess.run(['aarch64-linux-gnu-ld','--defsym=note_footer_bridge=0x6d3c800','-T','src/values.ld',str(O/'values.o'),'-o',str(O/'values.elf')],cwd=R,check=True)
subprocess.run(['aarch64-linux-gnu-objcopy','-O','binary','--only-section=.text',str(O/'values.elf'),str(O/'values.bin')],check=True)
def compile_run(label,sources,flags=(),expected=0,args=()):
 target=O/label
 subprocess.run(['aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror','-Ibuild','-Isrc','-ffunction-sections','-fdata-sections','-mbranch-protection=standard',*flags,'-Wl,--gc-sections',*sources,'-Wl,--no-as-needed','-lstdc++','-ldl','-pthread','-lm','-o',str(target)],cwd=R,check=True)
 p=subprocess.run([*q,str(target),*args],cwd=R,capture_output=True,text=True,timeout=45)
 (O/(label+'.log')).write_text(p.stdout+p.stderr)
 assert p.returncode==expected,(label,p.returncode,p.stdout,p.stderr)
 print(p.stdout+p.stderr,end='');return p
before=compile_run('before',['check_knob_events.c',str(O/'baseline-phone-events.c'),'src/phone_input.c'],expected=1)
assert 'REPRODUCED endpoint lost: K1 wanted=0 actual=27' in before.stderr
native_before=subprocess.check_output(['git','-c','safe.directory='+str(R.parent),'show','b23f35f:sd-mod-session/src/phone_native.c'],cwd=R)
(O/'r26h-phone-native.c').write_bytes(native_before)
p=compile_run('native-path-before',['check_knob_native_path.c','src/phone_events.c','src/phone_input.c'],['-DNATIVE_PATH_BASELINE=1','-DPHONE_NATIVE_SOURCE="'+str(O/'r26h-phone-native.c')+'"'],expected=1)
assert 'REPRODUCED native K input rerouted through UI timer: delivered=0 expected=768' in p.stderr
scaled_before=subprocess.check_output(['git','-c','safe.directory='+str(R.parent),'show','7fa906b:sd-mod-session/src/phone_events.c'],cwd=R)
(O/'r26f-phone-events.c').write_bytes(scaled_before)
p=compile_run('scaled-before',['check_scaled_native.c',str(O/'r26f-phone-events.c')],expected=1,args=[str(stock)])
assert 'REPRODUCED native takeover mismatch mode=1 end=0' in p.stderr
for sanitized in [False,True]:
 flags=['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'] if sanitized else []
 for label,sources in [('routes',['check_knob_events.c','src/phone_events.c','src/phone_input.c']),('queue',['check_knob_queue.c','src/phone_events.c']),('resample',['check_resample_routes.c','src/phone_events.c','src/phone_input.c']),('naming',['check_naming_shift.c','src/phone_input.c'])]:
  p=compile_run(label+('-ubsan' if sanitized else ''),sources,flags)
  results.append(dict(test=label,ubsan=sanitized,passed=True,output=p.stdout.strip()))
 p=compile_run('scaled'+('-ubsan' if sanitized else ''),['check_scaled_native.c','src/phone_events.c'],flags,args=[str(stock)])
 results.append(dict(test='actual native Scaled/Pickup/Jump',ubsan=sanitized,passed=True,output=p.stdout.strip()))
 p=compile_run('native-path'+('-ubsan' if sanitized else ''),['check_knob_native_path.c','src/phone_events.c','src/phone_input.c'],flags)
 results.append(dict(test='production native knob input thread and action boundaries',ubsan=sanitized,passed=True,output=p.stdout.strip()))
 p=compile_run('native-audit'+('-ubsan' if sanitized else ''),['check_takeover_native.c','src/phone_events.c','src/takeover_native.c'],flags,args=[str(stock)])
 results.append(dict(test='production native takeover code preservation',ubsan=sanitized,passed=True,output=p.stdout.strip()))
 p=compile_run('compressor-routes'+('-ubsan' if sanitized else ''),['check_knob_compressor_routes.c','src/phone_events.c','src/phone_input.c'],flags)
 results.append(dict(test='production routing with OptoCat compiled in',ubsan=sanitized,passed=True,output=p.stdout.strip()))
 p=compile_run('scaled-endpoints'+('-ubsan' if sanitized else ''),['check_scaled_native.c','src/phone_events.c','src/takeover_endpoints.c'],[*flags,'-DCHECK_SCALED_ENDPOINTS=1'],args=[str(stock)])
 results.append(dict(test='legacy r26h Scaled helper (not linked into r26i)',ubsan=sanitized,passed=True,output=p.stdout.strip()))
 for corrected in (False,True):
  sources=['check_scaled_display.c','src/phone_events.c']+(['src/takeover_endpoints.c'] if corrected else [])
  extra=['-DCHECK_SCALED_ENDPOINTS=1'] if corrected else []
  p=compile_run('display'+('-corrected' if corrected else '')+('-ubsan' if sanitized else ''),sources,[*flags,*extra],args=[str(stock),str(O/'values.bin')])
  results.append(dict(test='production K display vs native takeover'+(' corrected' if corrected else ''),ubsan=sanitized,passed=True,output=p.stdout.strip()))
assert hashes=={n:sha((R/n).read_bytes()) for n in names}
(O/'verification.json').write_text(json.dumps(dict(passed=True,issue=2,sources_sha256=hashes,baseline_revision='9841eab',baseline_source_sha256=sha(baseline),baseline_reproduced=True,scaled_baseline_revision='7fa906b',scaled_baseline_reproduced=True,scaled_processor='actual stock 0x15f5690, Scaled=1, Pickup=0, Jump=2; parameter/GUI/timer receivers are fixtures',stock_sha256=sha(stock.read_bytes()),tests=results,native_r26h_baseline_reproduced=True,native_r26h_source_sha256=sha(native_before),native_receiver='recording fixture; production controller and FIFO execute',physical_knob_tested=False,actual_warp_ui_tested=False),indent=2)+'\n')
