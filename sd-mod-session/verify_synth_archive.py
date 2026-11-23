"""Isolated model/archive checks; no preload, XPJ file or SD package built."""
from pathlib import Path
import hashlib,json,os,subprocess
R=Path(__file__).resolve().parent;O=R/'build/synth-archive';O.mkdir(exist_ok=True)
sources=['src/synth.c','src/synth_binding.c','src/synth_route.c','src/synth_archive.cc','check_synth_archive.cc','check_synth_binding_ui.c','check_synth_route.c']
tracked=sources+['src/synth.h','src/synth_route.h','src/synth_binding.h','src/synth_archive.h','src/synth_pad_record.inc','src/batch_copy.h','src/pad_midi_note.h','verify_synth_archive.py']
sha=lambda p:hashlib.sha256((R/p).read_bytes()).hexdigest()
before={p:sha(p) for p in tracked};proof=O/'verification.json';proof.unlink(missing_ok=True);checks=[]
for name,prefix,flags,runner in [('host','',[],[]),('host-asan','',['-fsanitize=address,undefined','-fno-sanitize-recover=all'],[]),('arm','aarch64-linux-gnu-',[],['qemu-aarch64','-L',os.environ.get('MPC_QEMU_SYSROOT','/home/user/mpc-rootfs-user-20260808')]),('arm-ubsan','aarch64-linux-gnu-',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'],['qemu-aarch64','-L',os.environ.get('MPC_QEMU_SYSROOT','/home/user/mpc-rootfs-user-20260808')])]:
    objects=[]
    for source in sources:
        cpp=source.endswith('.cc');obj=O/(name+'-'+Path(source).name+'.o');objects.append(str(obj))
        subprocess.run([prefix+('g++' if cpp else 'gcc'),'-std=c++11' if cpp else '-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-DMPC_PAD_MIDI_RECORDING_TRIAL=1','-DMPC_SYNTH_NATIVE_PLAYBACK_TRIAL=1','-ffunction-sections','-fdata-sections',*flags,'-c',source,'-o',str(obj)],cwd=R,check=True)
    target=O/name;subprocess.run([prefix+'g++',*flags,*objects,'-Wl,--gc-sections','-pthread','-lm','-o',str(target)],cwd=R,check=True)
    result=subprocess.run([*runner,str(target)],cwd=R,capture_output=True,text=True,timeout=45)
    (O/(name+'.log')).write_text(result.stdout+result.stderr);print(name,result.stdout+result.stderr,flush=True);result.check_returncode()
    assert 'PASS Synth archive:' in result.stdout;checks.append(name)
assert before=={p:sha(p) for p in tracked}
proof.write_text(json.dumps(dict(passed=True,checks=checks,sources_sha256=before,hardware_tested=False,full_xpj_roundtrip=False,native_frames_writer='fixtures',preload_built=False),indent=2)+'\n')
