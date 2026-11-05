"""Build a separate r24 Chords candidate over the frozen r23a source list."""
from pathlib import Path
import hashlib,json,os,subprocess
R=Path(__file__).resolve().parent
O=R/'build/chords';O.mkdir(exist_ok=True)
subprocess.run(['python3','build_pad_lighting_menu.py'],cwd=R,check=True)
frozen=json.loads((R/'build/r23a/build-info.json').read_text())
sources=frozen['compile_verification']['linked_sources']+['src/chords.c','src/chords_ui.c','src/voice_guard.c','src/values_bipolar.c','src/filter_spectrum.cc','src/takeover_native.c']
sources+=['src/compressor_dsp.c','src/compressor_native.c','src/compressor_ui.c',
          'src/compressor_animation.c','src/compressor_image.cc']
sources+=['src/project_recovery.c','src/startup_splash.cc']
sources+=['src/startup_project.cc']
sources+=['src/knob_reset.c']
sources+=['src/sample_load_trace.c']
sources+=['src/sample_playing.c']
sources+=['src/rear_audio.cc','src/rear_native.cc','src/rear_ui.c','src/rear_abi.S','src/pad_resample_commit.cc','src/pad_sample_pool.cc','src/pad_assignment.cc','src/pad_assignment_abi.S','src/pad_source_abi.S','src/pad_memory_abi.S']
sources=list(dict.fromkeys(sources))
tracked=sources+['src/chords.h','src/chords_ui.h','src/voice_guard.h','src/values_bipolar.h','src/poly_record.h','src/music_ui.h',
                 'src/note_native.h','src/workflow_ui.h','src/resample_ui.h','src/pad_eq.h','src/music_settings.h','src/filter_spectrum.h','src/takeover_endpoints.h','build_chords_candidate.py']
tracked+=['src/project.S','src/project.ld','build_pad_lighting_menu.py','build/pad_lighting_project.h']
tracked+=['src/compressor.h','src/optocat_atlas.inc','src/takeover_native.h']
tracked+=['src/project_recovery.h','src/startup_splash.h','src/am_mod_splash.inc','src/am_mod_splash.png']
tracked+=['src/startup_project.h']
tracked+=['src/mod_build.h','src/splash_branding.h','src/branding_font_aa.h']
tracked+=['src/knob_reset.h']
tracked+=['src/sample_load_trace.h','phone_build.py','build/phone_hooks.h']
tracked+=['src/pad_palette.h','src/pad_workflow_colour.h']
tracked+=['src/sample_playing.h','src/sample_mute.h','src/rear_audio.h','src/rear_native.h']
sha=lambda b:hashlib.sha256(b).hexdigest()
hashes={s:sha((R/s).read_bytes()) for s in tracked}
common=['-O2','-g','-fPIC','-Wall','-Wextra','-Werror','-Ibuild',
 '-DMPC_RESAMPLE_TRIAL=1','-DMPC_STOCK_RESAMPLE=1','-DMPC_EXPERIMENTAL_CHOP_EXPORT=1',
 '-DMPC_PAD_EQ=1','-DMPC_FAST_RESAMPLE=1','-DMPC_SEQUENCE_COLOURS=1',
 '-DMPC_POLY_RECORD=1','-DMPC_POWER_BUTTON=1','-DMPC_CHORDS=1','-DMPC_COMPRESSOR=1','-DMPC_PROJECT_RECOVERY=1','-DMPC_STARTUP_SPLASH=1','-DMPC_EARLY_STARTUP=1','-DMPC_KNOB_RESET=1','-DMPC_LOOP_FEEDBACK=1','-DMPC_REAR_ROUTING=1','-mbranch-protection=standard']
objects=[]
for source in sources:
 cpp=source.endswith('.cc');obj=O/(Path(source).stem+'.o');objects.append(str(obj))
 subprocess.run(['aarch64-linux-gnu-g++' if cpp else 'aarch64-linux-gnu-gcc',
                 '-std=c++11' if cpp else '-std=c11',*common,'-c',source,'-o',str(obj)],check=True)
library=O/'phone-input.so'
subprocess.run(['aarch64-linux-gnu-g++','-shared','-Wl,-z,noexecstack,-z,relro,-z,now',
 '-Wl,--no-undefined',*objects,'-ldl','-pthread','-lm','-o',str(library)],check=True)
bootstrap=Path(os.environ.get('AM_MOD_BOOTSTRAP_DIR',str(R/'build/workspace-fix')))
p=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',
 str(bootstrap/'bootstrap'),str(R.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'),
 str(library),str(bootstrap/'bootstrap-patches.bin')],capture_output=True,text=True,timeout=45)
(O/'bootstrap.log').write_text(p.stdout+p.stderr);p.check_returncode()
assert 'PASS real full preload constructor' in p.stdout
assert 'PITCH_AWARE_OVERWRITE_INSTALLED' in p.stderr
assert 'VOICE_GUARD RENDER_RELEASE_SERIALISED' in p.stderr
assert 'VALUES_BIPOLAR CENTRE_BAR_INSTALLED' in p.stderr
assert 'TAKEOVER NATIVE_PROCESSORS_PRESERVED' in p.stderr
assert 'TAKEOVER SCALED_ENDPOINTS_INSTALLED' not in p.stderr
assert hashes=={s:sha((R/s).read_bytes()) for s in tracked}
(O/'compile-verification.json').write_text(json.dumps(dict(passed=True,
 library_sha256=sha(library.read_bytes()),sources_sha256=hashes,linked_sources=sources,
 full_preload_constructor=True,compressor_included=True,native_takeover_preserved=True,compiler_flags=common,
 hardware_tested=False,card_written=False),indent=2)+'\n')
print(p.stdout);print('Chords candidate:',sha(library.read_bytes()))
