"""Build an isolated Type/Opto candidate; never writes a physical SD."""
from pathlib import Path
import hashlib,json,os,subprocess
R=Path(__file__).resolve().parent
runtime=Path(os.environ.get('MPC_RUNTIME_ROOT',str(R.parent))).resolve()
O=R/'build/compressor';O.mkdir(parents=True,exist_ok=True)
frozen=json.loads((runtime/'sd-mod-session/build/r23a/build-info.json').read_text())
sources=list(dict.fromkeys(frozen['compile_verification']['linked_sources']+
 ['src/chords.c','src/chords_ui.c','src/voice_guard.c','src/values_bipolar.c','src/filter_spectrum.cc']+
 ['src/compressor_dsp.c','src/compressor_native.c','src/compressor_ui.c','src/compressor_animation.c','src/compressor_image.cc']))
tracked=sources+[str(p.relative_to(R)) for p in (R/'src').glob('*.h')]+['src/optocat_atlas.inc','graphics/build_optocat.cjs','graphics/optocat_shapes.js','build_compressor_candidate.py']
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
before={s:sha(R/s) for s in tracked}
common=['-O2','-g','-fPIC','-Wall','-Wextra','-Werror','-Ibuild','-I'+str(runtime/'sd-mod-session/build'),
 '-DMPC_RESAMPLE_TRIAL=1','-DMPC_STOCK_RESAMPLE=1','-DMPC_EXPERIMENTAL_CHOP_EXPORT=1',
 '-DMPC_PAD_EQ=1','-DMPC_FAST_RESAMPLE=1','-DMPC_SEQUENCE_COLOURS=1',
 '-DMPC_POLY_RECORD=1','-DMPC_POWER_BUTTON=1','-DMPC_CHORDS=1',
 '-DMPC_COMPRESSOR=1','-mbranch-protection=standard']
objects=[]
for source in sources:
 cpp=source.endswith('.cc');obj=O/(Path(source).stem+'.o');objects.append(str(obj))
 subprocess.run(['aarch64-linux-gnu-g++' if cpp else 'aarch64-linux-gnu-gcc',
  '-std=c++11' if cpp else '-std=c11',*common,'-c',source,'-o',str(obj)],cwd=R,check=True)
library=O/'phone-input.so'
subprocess.run(['aarch64-linux-gnu-g++','-shared','-Wl,-z,noexecstack,-z,relro,-z,now',
 '-Wl,--no-undefined',*objects,'-ldl','-pthread','-lm','-o',str(library)],check=True)
p=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',
 str(runtime/'sd-mod-session/build/r22/bootstrap'),str(runtime/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'),
 str(library),str(runtime/'sd-mod-session/build/r22/bootstrap-patches.bin')],capture_output=True,text=True,timeout=45)
(O/'bootstrap.log').write_text(p.stdout+p.stderr);p.check_returncode()
assert 'PASS real full preload constructor' in p.stdout
for marker in ('PITCH_AWARE_OVERWRITE_INSTALLED','VOICE_GUARD RENDER_RELEASE_SERIALISED','VALUES_BIPOLAR CENTRE_BAR_INSTALLED'):
 assert marker in p.stderr,marker
assert before=={s:sha(R/s) for s in tracked}
proof=dict(passed=True,base_revision='r26d',base_commit='9841eab7dff0f7d3230f8e88b9a09886f240b400',library_sha256=sha(library),sources_sha256=before,
 linked_sources=sources,full_preload_constructor=True,hardware_tested=False,card_written=False)
(O/'compile-verification.json').write_text(json.dumps(proof,indent=2)+'\n')
print(p.stdout);print('Compressor candidate:',proof['library_sha256'])
