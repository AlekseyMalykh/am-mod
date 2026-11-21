"""Run real firmware event factory and packed modifiers against the new snapshot."""
from pathlib import Path
import hashlib,json,subprocess
R=Path(__file__).resolve().parent;O=R/'build/midi-events';O.mkdir(parents=True,exist_ok=True)
proof=O/'snapshot-verification.json'
proof.unlink(missing_ok=True)
sources=['check_midi_wire.c','check_midi_live_native.c','src/midi_wire.c','src/midi_wire.h','src/midi_live.inc','src/midi_tools.c','src/midi_tools.h','src/midi_route.c','src/midi_route.h','src/midi_event.c','src/midi_event.h','src/midi_context.c','src/midi_context.h','src/midi_native.c','src/midi_native.h','src/midi_voice.c','src/midi_voice.h','check_midi_context.c','check_midi_context_native.c','check_midi_voice.c','check_midi_event_native.c','check_midi_backend_native.c','check_fx_native.c','verify_midi_event.py']
digest=lambda p:hashlib.sha256((R/p).read_bytes()).hexdigest()
before={p:digest(p) for p in sources}
stock=R.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
assert hashlib.sha256(stock.read_bytes()).hexdigest()=='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
for mode,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
    target=O/('native-'+mode)
    subprocess.run(['aarch64-linux-gnu-gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard',*flags,'check_midi_event_native.c','src/midi_event.c','-ldl','-lm','-o',str(target)],cwd=R,check=True)
    p=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(target),str(stock)],cwd=R,capture_output=True,text=True,timeout=45)
    (O/(mode+'.log')).write_text(p.stdout+p.stderr);print(p.stdout+p.stderr,end='',flush=True);p.check_returncode()
assert before=={p:digest(p) for p in sources}
for mode,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
    target=O/('hook-'+mode)
    subprocess.run(['aarch64-linux-gnu-gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard',*flags,'check_midi_context_native.c','src/midi_event.c','src/midi_context.c','src/midi_native.c','src/midi_wire.c','src/midi_voice.c','src/midi_route.c','src/midi_tools.c','-pthread','-ldl','-lm','-o',str(target)],cwd=R,check=True)
    p=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(target),str(stock)],cwd=R,capture_output=True,text=True,timeout=45)
    (O/('hook-'+mode+'.log')).write_text(p.stdout+p.stderr);print(p.stdout+p.stderr,end='',flush=True);p.check_returncode()
assert before=={p:digest(p) for p in sources}
for mode,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
    target=O/('backend-'+mode)
    subprocess.run(['aarch64-linux-gnu-gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard',*flags,'check_midi_backend_native.c','src/midi_event.c','-ldl','-lm','-o',str(target)],cwd=R,check=True)
    p=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(target),str(stock)],cwd=R,capture_output=True,text=True,timeout=45)
    (O/('backend-'+mode+'.log')).write_text(p.stdout+p.stderr);print(p.stdout+p.stderr,end='',flush=True);p.check_returncode()
assert before=={p:digest(p) for p in sources}
for architecture,cc,prefix in [('host','gcc',[]),('arm64','aarch64-linux-gnu-gcc',['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808'])]:
    for mode,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
        target=O/('voices-'+architecture+'-'+mode)
        subprocess.run([cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'check_midi_voice.c','src/midi_event.c','src/midi_voice.c','-lm','-o',str(target)],cwd=R,check=True)
        p=subprocess.run([*prefix,str(target)],cwd=R,capture_output=True,text=True,timeout=30)
        (O/('voices-'+architecture+'-'+mode+'.log')).write_text(p.stdout+p.stderr);print(p.stdout+p.stderr,end='',flush=True);p.check_returncode()
        target=O/('context-'+architecture+'-'+mode)
        subprocess.run([cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'check_midi_context.c','src/midi_event.c','src/midi_context.c','-pthread','-lm','-o',str(target)],cwd=R,check=True)
        p=subprocess.run([*prefix,str(target)],cwd=R,capture_output=True,text=True,timeout=30)
        (O/('context-'+architecture+'-'+mode+'.log')).write_text(p.stdout+p.stderr);print(p.stdout+p.stderr,end='',flush=True);p.check_returncode()
assert before=={p:digest(p) for p in sources}
for architecture,cc,prefix in [('host','gcc',[]),('arm64','aarch64-linux-gnu-gcc',['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808'])]:
    for mode,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
        target=O/('wire-'+architecture+'-'+mode)
        subprocess.run([cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'check_midi_wire.c','src/midi_wire.c','src/midi_voice.c','src/midi_route.c','src/midi_event.c','src/midi_tools.c','-pthread','-lm','-o',str(target)],cwd=R,check=True)
        p=subprocess.run([*prefix,str(target)],cwd=R,capture_output=True,text=True,timeout=30)
        (O/('wire-'+architecture+'-'+mode+'.log')).write_text(p.stdout+p.stderr);print(p.stdout+p.stderr,end='',flush=True);p.check_returncode()
for mode,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
    target=O/('live-'+mode)
    subprocess.run(['aarch64-linux-gnu-gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard',*flags,'check_midi_live_native.c','src/midi_context.c','src/midi_event.c','src/midi_wire.c','src/midi_voice.c','src/midi_route.c','src/midi_tools.c','-pthread','-ldl','-lm','-o',str(target)],cwd=R,check=True)
    p=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(target),str(stock)],cwd=R,capture_output=True,text=True,timeout=45)
    (O/('live-'+mode+'.log')).write_text(p.stdout+p.stderr);print(p.stdout+p.stderr,end='',flush=True);p.check_returncode()
assert before=={p:digest(p) for p in sources}
proof.write_text(json.dumps(dict(passed=True,sources_sha256=before,hardware_tested=False,native_live_integration=False,
    native_backend_boundary_executed=True,native_release_heap_executed=True,
    chop_policy='base + slice - 1; Tune adds semitones; out-of-range notes are rejected',
    native_chop_cases=128*128,checks=['wire-host-normal','wire-host-ubsan','wire-arm64-normal','wire-arm64-ubsan','live-normal','live-ubsan','native-normal','native-ubsan','backend-normal','backend-ubsan','hook-normal','hook-ubsan','voices-host-normal','voices-host-ubsan','voices-arm64-normal','voices-arm64-ubsan','context-host-normal','context-host-ubsan','context-arm64-normal','context-arm64-ubsan']),indent=2)+'\n')
