"""Isolated native duration regression. Does not build the application/library."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess
p=argparse.ArgumentParser();p.add_argument('--stock',required=True);a=p.parse_args()
R=Path(__file__).resolve().parent;stock=Path(a.stock).resolve()
expected='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
assert hashlib.sha256(stock.read_bytes()).hexdigest()==expected
O=R/'build/record-boundaries';O.mkdir(exist_ok=True)
proof=O/'verification.json';proof.unlink(missing_ok=True)
files=['check_record_boundaries.c','check_overdub_matrix.inc','check_poly_record.c','check_fx_native.c','src/poly_record.c','src/poly_record.h','src/step_native.h','verify_record_boundaries.py']
digest=lambda:{f:hashlib.sha256((R/f).read_bytes()).hexdigest() for f in files}
before=digest();checks=[]
for mode,extra in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
    target=O/mode
    subprocess.run(['aarch64-linux-gnu-gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*extra,'check_record_boundaries.c','-ldl','-pthread','-lm','-o',str(target)],cwd=R,check=True)
    result=subprocess.run(['qemu-aarch64','-L',os.environ.get('MPC_QEMU_SYSROOT','/home/user/mpc-rootfs-user-20260808'),str(target),str(stock)],cwd=R,text=True,capture_output=True,timeout=60)
    (O/(mode+'.log')).write_text(result.stdout+result.stderr)
    print(mode,result.stdout+result.stderr,flush=True);result.check_returncode()
    assert 'PASS 40 native finalizer boundary cases' in result.stdout
    assert 'PASS 144 overdub identity checks' in result.stdout
    assert 'PASS 32 chord boundary releases' in result.stdout;checks.append(mode)
assert before==digest()
proof.write_text(json.dumps(dict(passed=True,checks=checks,cases_per_run=40,overdub_identity_checks=144,chord_boundary_releases=32,native_undo_aggregation=True,sources_sha256=before,stock_sha256=expected,native_finalizer=True,final_commit='capture fixture',pending_timestamps='injected preconditions',transport_recording_tested=False,overdub_transport_tested=False,application_built=False,hardware_tested=False),indent=2)+'\n')
