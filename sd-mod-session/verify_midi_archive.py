"""Pinned native archive ABI and production MIDI metadata integration, ARM64/QEMU."""
from pathlib import Path
import hashlib,json,subprocess
R=Path(__file__).resolve().parent;O=R/'build/midi-archive';O.mkdir(parents=True,exist_ok=True)
sources=['src/mixer_model.c','src/fx_eq.c','src/fx_chain.c','src/fx_own.c','src/compressor_dsp.c','src/mixer_archive.cc','src/music_archive.c','src/music_settings.c','src/midi_tools.c','check_mixer_archive_native.c','check_midi_archive_native.cc']
tracked=sources+['check_mixer_archive_native.cc','src/midi_tools.h','verify_midi_archive.py']
sha=lambda p:hashlib.sha256((R/p).read_bytes()).hexdigest();before={p:sha(p) for p in tracked}
stock=R.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample';assert hashlib.sha256(stock.read_bytes()).hexdigest()=='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
for mode,extra in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
 objects=[]
 for source in sources:
  cpp=source.endswith('.cc');target=O/(mode+'-'+Path(source).name+'.o');objects.append(str(target))
  subprocess.run(['aarch64-linux-gnu-g++' if cpp else 'aarch64-linux-gnu-gcc','-std=c++11' if cpp else '-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard','-DMPC_MIDI_TOOLS=1',*extra,'-c',source,'-o',str(target)],cwd=R,check=True)
 target=O/mode;subprocess.run(['aarch64-linux-gnu-g++',*extra,*objects,'-pthread','-lm','-ldl','-o',str(target)],cwd=R,check=True)
 p=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(target),str(stock)],cwd=R,capture_output=True,text=True,timeout=45)
 (O/(mode+'.log')).write_text(p.stdout+p.stderr);print(p.stdout+p.stderr,end='',flush=True);p.check_returncode();assert 'PASS MIDI extension through production native JSON archive' in p.stdout
assert before=={p:sha(p) for p in tracked}
(O/'verification.json').write_text(json.dumps(dict(passed=True,sources_sha256=before,native_writer_reader=True,hardware_tested=False),indent=2)+'\n')
