"""Pinned native tag-3 event erases and SourceDomain; no application build."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess
p=argparse.ArgumentParser();p.add_argument('--stock',required=True);a=p.parse_args()
R=Path(__file__).resolve().parent;stock=Path(a.stock).resolve()
expected='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
assert hashlib.sha256(stock.read_bytes()).hexdigest()==expected
O=R/'build/pattern-event-writer';O.mkdir(exist_ok=True);proof=O/'verification.json';proof.unlink(missing_ok=True)
sources=['src/pattern_source.cc','src/pattern_native.cc','src/pattern_model.cc','src/midi_event.c','check_pattern_event_writer_fixture.c','check_pattern_event_writer.cc']
tracked=sources+['src/pattern_source.h','src/pattern_request.h','src/pattern_native.h','src/pattern_model.h','src/midi_event.h','src/step_native.h','check_pattern_native_fixture.c','check_fx_native.c','verify_pattern_event_writer.py']
digest=lambda:{f:hashlib.sha256((R/f).read_bytes()).hexdigest() for f in tracked}
before=digest();checks=[]
for name,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
    objects=[]
    for source in sources:
        cpp=source.endswith('.cc');obj=O/(name+'-'+Path(source).name+'.o');objects.append(str(obj))
        subprocess.run(['aarch64-linux-gnu-g++' if cpp else 'aarch64-linux-gnu-gcc','-std=c++11' if cpp else '-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard',*flags,'-c',source,'-o',str(obj)],cwd=R,check=True)
    target=O/name;subprocess.run(['aarch64-linux-gnu-g++',*flags,*objects,'-ldl','-pthread','-o',str(target)],cwd=R,check=True)
    result=subprocess.run(['qemu-aarch64','-L',os.environ.get('MPC_QEMU_SYSROOT','/home/user/mpc-rootfs-user-20260808'),str(target),str(stock)],cwd=R,text=True,capture_output=True,timeout=60)
    (O/(name+'.log')).write_text(result.stdout+result.stderr);print(name,result.stdout+result.stderr,flush=True);result.check_returncode();assert 'PASS native event writers:' in result.stdout and 'PASS native history methods/deltas:' in result.stdout;checks.append(name)
assert before==digest()
proof.write_text(json.dumps(dict(passed=True,checks=checks,sources_sha256=before,stock_sha256=expected,actual_event_entries=['0x27eef10','0x27ef360','0x27ee5d0','0x27f0ee0','0x27f0500','0x19d8f70','0x19d52c0'],event_variant='inline tag 3 only',bulk_scope='actual 27f0ee0 delta replay, multi-remove/restore and single replacement; nested batch counter preserved',history_scope='actual command vtable methods; manually initialized inline command, no native factory or history manager',gui_clear_entry_tested=False,notification_services='fixtures',source_domain=True,immutable_pattern_preserved=True,writer_thread_affinity_certified=False,coverage_complete=False,application_built=False,hardware_tested=False),indent=2)+'\n')
