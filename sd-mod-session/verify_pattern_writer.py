"""Actual pinned Program writer and Clip-selector probes; no app/preload build."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess
p=argparse.ArgumentParser();p.add_argument('--stock',required=True);a=p.parse_args()
R=Path(__file__).resolve().parent;stock=Path(a.stock).resolve()
expected='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
assert hashlib.sha256(stock.read_bytes()).hexdigest()==expected
O=R/'build/pattern-writer';O.mkdir(exist_ok=True);proof=O/'verification.json';proof.unlink(missing_ok=True)
sources=['src/pattern_source.cc','check_pattern_writer_fixture.c','check_pattern_writer.cc']
tracked=sources+['src/pattern_source.h','src/pattern_request.h','src/pattern_native.h','src/pattern_model.h','src/midi_event.h','src/step_native.h','check_pattern_native_fixture.c','check_fx_native.c','verify_pattern_writer.py']
digest=lambda:{f:hashlib.sha256((R/f).read_bytes()).hexdigest() for f in tracked}
before=digest();checks=[]
for name,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
    objects=[]
    for source in sources:
        cpp=source.endswith('.cc');obj=O/(name+'-'+Path(source).name+'.o');objects.append(str(obj))
        subprocess.run(['aarch64-linux-gnu-g++' if cpp else 'aarch64-linux-gnu-gcc','-std=c++11' if cpp else '-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard',*flags,'-c',source,'-o',str(obj)],cwd=R,check=True)
    target=O/name;subprocess.run(['aarch64-linux-gnu-g++',*flags,*objects,'-ldl','-pthread','-o',str(target)],cwd=R,check=True)
    result=subprocess.run(['qemu-aarch64','-L',os.environ.get('MPC_QEMU_SYSROOT','/home/user/mpc-rootfs-user-20260808'),str(target),str(stock)],cwd=R,text=True,capture_output=True,timeout=60)
    (O/(name+'.log')).write_text(result.stdout+result.stderr);print(name,result.stdout+result.stderr,flush=True);result.check_returncode()
    assert 'PASS native Clip map selector:' in result.stdout and 'PASS native Program setter' in result.stdout and 'PASS native Program replacement 29c9670 base path:' in result.stdout and 'PASS native Program replacement subtype branches:' in result.stdout;checks.append(name)
assert before==digest()
proof.write_text(json.dumps(dict(passed=True,checks=checks,sources_sha256=before,stock_sha256=expected,actual_program_setter='0x29c9490',actual_program_disappearance=['0x29c93a0(Track)','0x29c7a80(Track+0x910)'],actual_program_replacement='0x29c9670',replacement_scope='base and subtype branches; RTTI/token provider/notification/property services are fixtures',replacement_store_sites=['0x29c9884','0x29c98e0'],replacement_subtype_executed=True,native_connection_functions=['0x236efe0','0x236f060'],independent_subscriber_preserved=True,actual_clip_lookup='0x29579e0/0x2957ae0',clip_third_argument='map selector, not create',observer_services='fixtures; native 29c9210 rebuild captured',source_domain=True,writer_thread_affinity_certified=False,coverage_complete=False,application_built=False,hardware_tested=False),indent=2)+'\n')
