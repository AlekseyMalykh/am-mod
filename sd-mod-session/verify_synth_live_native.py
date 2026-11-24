"""Isolated ARM native-ELF + DSP trial; no application/preload/SD build."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess
p=argparse.ArgumentParser();p.add_argument('--stock',default=os.environ.get('MPC_STOCK_APP'));a=p.parse_args()
if not a.stock:p.error('pass --stock or MPC_STOCK_APP')
R=Path(__file__).resolve().parent;stock=Path(a.stock).resolve();expected='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
assert hashlib.sha256(stock.read_bytes()).hexdigest()==expected
O=R/'build/synth-live-native';O.mkdir(exist_ok=True);proof=O/'verification.json';proof.unlink(missing_ok=True)
sources=['check_synth_live_native.c','check_synth_drain_exception.cc','src/synth.c','src/synth_sequence.c','src/synth_binding.c','src/synth_route.c','src/synth_accept_native.cc','src/midi_context.c','src/midi_event.c','src/midi_wire.c','src/midi_voice.c','src/midi_route.c','src/midi_tools.c']
tracked=sources+['src/midi_native.c','src/midi_live.inc','src/synth_live.inc','src/synth_accept_native.h','src/synth_route.h','src/synth_binding.h','src/synth_sequence.h','src/synth.h','src/synth_archive.h','check_fx_native.c','verify_synth_live_native.py']
digest=lambda:{f:hashlib.sha256((R/f).read_bytes()).hexdigest() for f in tracked};before=digest();checks=[]
for mode,extra in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
    objects=[]
    for source in sources:
        cpp=source.endswith('.cc');obj=O/(mode+'-'+Path(source).name+'.o');objects.append(str(obj))
        subprocess.run(['aarch64-linux-gnu-g++' if cpp else 'aarch64-linux-gnu-gcc','-std=c++11' if cpp else '-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard','-DMPC_PAD_MIDI_RECORDING_TRIAL=1','-DMPC_SYNTH_NATIVE_PLAYBACK_TRIAL=1',*extra,'-c',source,'-o',str(obj)],cwd=R,check=True)
    target=O/mode;subprocess.run(['aarch64-linux-gnu-g++',*extra,*objects,'-ldl','-pthread','-lm','-o',str(target)],cwd=R,check=True)
    result=subprocess.run(['qemu-aarch64','-L',os.environ.get('MPC_QEMU_SYSROOT','/home/user/mpc-rootfs-user-20260808'),str(target),str(stock)],cwd=R,text=True,capture_output=True,timeout=50)
    (O/(mode+'.log')).write_text(result.stdout+result.stderr);print(mode,result.stdout+result.stderr,flush=True);result.check_returncode();assert 'PASS native Synth:' in result.stdout;checks.append(mode)
assert before==digest()
proof.write_text(json.dumps(dict(passed=True,checks=checks,sources_sha256=before,stock_sha256=expected,stock_track_filters_and_virtual_call=True,stock_due_heap=True,stock_due_paths=['0x21185a0','0x2118c20','0x2118ed0'],stock_heap_drain='0x2119550',stock_track_off_and_midi_backend=True,metadata_and_backend_services='fixtures',midi_queue_insertion='fixture',drain_release_gate_and_tail='fixtures',drain_exception='fixture',probability_engine_tested=False,pcm_frame_alignment_tested=False,full_xpj_roundtrip=False,preload_built=False,hardware_tested=False),indent=2)+'\n')
