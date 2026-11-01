"""ARM regression for issue #2; records native parameter calls, not hardware ADC.
Reproduces the old overflow/overtaking before checking production queue fixes.
"""
from pathlib import Path
import hashlib,json,subprocess
R=Path(__file__).resolve().parent;O=R/'build/knob-events';O.mkdir(parents=True,exist_ok=True)
sha=lambda b:hashlib.sha256(b).hexdigest()
names=['src/phone_events.c','src/phone_events.h','src/phone_native.c','src/phone_input.c','check_knob_events.c','check_knob_queue.c','check_resample_routes.c','check_naming_shift.c','verify_knob_events.py']
hashes={n:sha((R/n).read_bytes()) for n in names}
baseline=subprocess.check_output(['git','-c','safe.directory='+str(R.parent),'show','9841eab:sd-mod-session/src/phone_events.c'],cwd=R)
(O/'baseline-phone-events.c').write_bytes(baseline)
q=['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808']
results=[]
def compile_run(label,sources,flags=(),expected=0):
 target=O/label
 subprocess.run(['aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror','-Ibuild','-Isrc','-ffunction-sections','-fdata-sections','-mbranch-protection=standard',*flags,'-Wl,--gc-sections',*sources,'-ldl','-pthread','-o',str(target)],cwd=R,check=True)
 p=subprocess.run([*q,str(target)],cwd=R,capture_output=True,text=True,timeout=45)
 (O/(label+'.log')).write_text(p.stdout+p.stderr)
 assert p.returncode==expected,(label,p.returncode,p.stdout,p.stderr)
 print(p.stdout+p.stderr,end='');return p
before=compile_run('before',['check_knob_events.c',str(O/'baseline-phone-events.c'),'src/phone_input.c'],expected=1)
assert 'REPRODUCED endpoint lost: K1 wanted=0 actual=28' in before.stderr
for sanitized in [False,True]:
 flags=['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'] if sanitized else []
 for label,sources in [('routes',['check_knob_events.c','src/phone_events.c','src/phone_input.c']),('queue',['check_knob_queue.c','src/phone_events.c']),('resample',['check_resample_routes.c','src/phone_events.c','src/phone_input.c']),('naming',['check_naming_shift.c','src/phone_input.c'])]:
  p=compile_run(label+('-ubsan' if sanitized else ''),sources,flags)
  results.append(dict(test=label,ubsan=sanitized,passed=True,output=p.stdout.strip()))
assert hashes=={n:sha((R/n).read_bytes()) for n in names}
(O/'verification.json').write_text(json.dumps(dict(passed=True,issue=2,sources_sha256=hashes,baseline_revision='9841eab',baseline_source_sha256=sha(baseline),baseline_reproduced=True,tests=results,native_receiver='recording fixture; production controller and FIFO execute',physical_knob_tested=False,actual_warp_ui_tested=False),indent=2)+'\n')
