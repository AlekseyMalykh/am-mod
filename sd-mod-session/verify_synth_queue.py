"""AArch64 trial MPSC queue stress and wrap/reservation checks under QEMU."""
from pathlib import Path
import hashlib,json,os,subprocess
R=Path(__file__).resolve().parent;O=R/'build/synth-queue';O.mkdir(exist_ok=True)
files=['src/synth.c','src/synth.h','check_synth_queue.c','verify_synth_queue.py']
hashes={f:hashlib.sha256((R/f).read_bytes()).hexdigest() for f in files}
proof=O/'verification.json';proof.unlink(missing_ok=True)
checks=[]
for name,extra in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
    target=O/name
    subprocess.run(['aarch64-linux-gnu-gcc','-O2','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections',*extra,'check_synth_queue.c','-pthread','-lm','-Wl,--gc-sections','-o',str(target)],cwd=R,check=True)
    result=subprocess.run(['qemu-aarch64','-L',os.environ.get('MPC_QEMU_SYSROOT','/home/user/mpc-rootfs-user-20260808'),str(target)],cwd=R,check=True,capture_output=True,text=True,timeout=45)
    assert result.stdout.startswith('PASS Synth MPSC:')
    (O/(name+'.log')).write_text(result.stdout+result.stderr);checks.append(name);print(name,result.stdout.strip(),flush=True)
assert hashes=={f:hashlib.sha256((R/f).read_bytes()).hexdigest() for f in files}
proof.write_text(json.dumps(dict(passed=True,sources_sha256=hashes,checks=checks,commands_per_run=80000,hardware_tested=False),indent=2)+'\n')
