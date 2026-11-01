"""Build a separate r24 Chords candidate over the frozen r23a source list."""
from pathlib import Path
import hashlib,json,subprocess
R=Path(__file__).resolve().parent
O=R/'build/chords';O.mkdir(exist_ok=True)
frozen=json.loads((R/'build/r23a/build-info.json').read_text())
sources=frozen['compile_verification']['linked_sources']+['src/chords.c','src/chords_ui.c','src/voice_guard.c','src/values_bipolar.c','src/filter_spectrum.cc','src/takeover_endpoints.c']
sources+=['src/compressor_dsp.c','src/compressor_native.c','src/compressor_ui.c',
          'src/compressor_animation.c','src/compressor_image.cc']
sources=list(dict.fromkeys(sources))
tracked=sources+['src/chords.h','src/chords_ui.h','src/voice_guard.h','src/values_bipolar.h','src/poly_record.h','src/music_ui.h',
                 'src/note_native.h','src/workflow_ui.h','src/resample_ui.h','src/pad_eq.h','src/music_settings.h','src/filter_spectrum.h','src/takeover_endpoints.h','build_chords_candidate.py']
tracked+=['src/compressor.h','src/optocat_atlas.inc']
sha=lambda b:hashlib.sha256(b).hexdigest()
hashes={s:sha((R/s).read_bytes()) for s in tracked}
common=['-O2','-g','-fPIC','-Wall','-Wextra','-Werror','-Ibuild',
 '-DMPC_RESAMPLE_TRIAL=1','-DMPC_STOCK_RESAMPLE=1','-DMPC_EXPERIMENTAL_CHOP_EXPORT=1',
 '-DMPC_PAD_EQ=1','-DMPC_FAST_RESAMPLE=1','-DMPC_SEQUENCE_COLOURS=1',
 '-DMPC_POLY_RECORD=1','-DMPC_POWER_BUTTON=1','-DMPC_CHORDS=1','-DMPC_COMPRESSOR=1','-mbranch-protection=standard']
objects=[]
for source in sources:
 cpp=source.endswith('.cc');obj=O/(Path(source).stem+'.o');objects.append(str(obj))
 subprocess.run(['aarch64-linux-gnu-g++' if cpp else 'aarch64-linux-gnu-gcc',
                 '-std=c++11' if cpp else '-std=c11',*common,'-c',source,'-o',str(obj)],check=True)
library=O/'phone-input.so'
subprocess.run(['aarch64-linux-gnu-g++','-shared','-Wl,-z,noexecstack,-z,relro,-z,now',
 '-Wl,--no-undefined',*objects,'-ldl','-pthread','-lm','-o',str(library)],check=True)
p=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',
 str(R/'build/r22/bootstrap'),str(R.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'),
 str(library),str(R/'build/r22/bootstrap-patches.bin')],capture_output=True,text=True,timeout=45)
(O/'bootstrap.log').write_text(p.stdout+p.stderr);p.check_returncode()
assert 'PASS real full preload constructor' in p.stdout
assert 'PITCH_AWARE_OVERWRITE_INSTALLED' in p.stderr
assert 'VOICE_GUARD RENDER_RELEASE_SERIALISED' in p.stderr
assert 'VALUES_BIPOLAR CENTRE_BAR_INSTALLED' in p.stderr
assert 'TAKEOVER SCALED_ENDPOINTS_INSTALLED' in p.stderr
assert hashes=={s:sha((R/s).read_bytes()) for s in tracked}
(O/'compile-verification.json').write_text(json.dumps(dict(passed=True,
 library_sha256=sha(library.read_bytes()),sources_sha256=hashes,linked_sources=sources,
 full_preload_constructor=True,compressor_included=True,compiler_flags=common,
 hardware_tested=False,card_written=False),indent=2)+'\n')
print(p.stdout);print('Chords candidate:',sha(library.read_bytes()))
