"""Native queue/lifetime request checks, not an app or preload build."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess
p=argparse.ArgumentParser();p.add_argument('--stock',required=True);a=p.parse_args()
R=Path(__file__).resolve().parent;stock=Path(a.stock).resolve()
expected='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
assert hashlib.sha256(stock.read_bytes()).hexdigest()==expected
O=R/'build/pattern-request';O.mkdir(exist_ok=True);proof=O/'verification.json';proof.unlink(missing_ok=True)
sources=['src/pattern_request.cc','src/pattern_native.cc','src/pattern_model.cc','src/midi_event.c','check_pattern_request_fixture.c','check_pattern_request.cc']
tracked=sources+['src/pattern_request.h','src/pattern_native.h','src/pattern_model.h','src/midi_event.h','src/step_native.h','check_fx_native.c','check_pattern_native_fixture.c','verify_pattern_request.py']
digest=lambda:{f:hashlib.sha256((R/f).read_bytes()).hexdigest() for f in tracked}
before=digest();checks=[]
for name,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
    objects=[]
    for source in sources:
        cpp=source.endswith('.cc');obj=O/(name+'-'+Path(source).name+'.o');objects.append(str(obj))
        subprocess.run(['aarch64-linux-gnu-g++' if cpp else 'aarch64-linux-gnu-gcc','-std=c++11' if cpp else '-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard',*flags,'-c',source,'-o',str(obj)],cwd=R,check=True)
    target=O/name;subprocess.run(['aarch64-linux-gnu-g++',*flags,*objects,'-ldl','-pthread','-lm','-o',str(target)],cwd=R,check=True)
    result=subprocess.run(['qemu-aarch64','-L',os.environ.get('MPC_QEMU_SYSROOT','/home/user/mpc-rootfs-user-20260808'),str(target),str(stock)],cwd=R,text=True,capture_output=True,timeout=60)
    (O/(name+'.log')).write_text(result.stdout+result.stderr);print(name,result.stdout+result.stderr,flush=True);result.check_returncode()
    assert 'PASS native snapshot request:' in result.stdout;checks.append(name)
assert before==digest()
proof.write_text(json.dumps(dict(passed=True,checks=checks,sources_sha256=before,stock_sha256=expected,native_queue='0x2b59300',native_runner='0x2b59200/0x2b59160',native_reclaim='0x2b58c70',native_context_destructor='0x1517c70',native_cancel='0x2b579a0/0x2b590d0/0x2b59000',native_mutex=True,destructor_base_and_other_queue='fixtures',source_owner_lease='fixture; production Project lifetime not proven',application_built=False,ui_connected=False,persistence_connected=False,hardware_tested=False),indent=2)+'\n')
