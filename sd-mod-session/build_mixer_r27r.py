"""Build r27r + Mixer + Song UI as one full-preload candidate (WSL, run from sd-mod-session).

Same source list and flags as the released r27r (build_chords_candidate.py),
plus the Mixer sources with MPC_MIXER_ARCHIVE/LIVE/AUTOBIND. Output only in
build/mixer-r27r. No SD card, no release package, no device.
"""
from pathlib import Path
import hashlib, json, os, subprocess

R = Path(__file__).resolve().parent
O = R / 'build/mixer-r27r'
O.mkdir(parents=True, exist_ok=True)
subprocess.run(['python3', 'build_pad_lighting_menu.py'], cwd=R, check=True)
frozen = json.loads((R / 'build/r23a/build-info.json').read_text())
sources = frozen['compile_verification']['linked_sources'] + [
    'src/chords.c', 'src/chords_ui.c', 'src/voice_guard.c', 'src/values_bipolar.c',
    'src/filter_spectrum.cc', 'src/takeover_native.c',
    'src/compressor_dsp.c', 'src/compressor_native.c', 'src/compressor_ui.c',
    'src/compressor_animation.c', 'src/compressor_image.cc',
    'src/project_recovery.c', 'src/startup_splash.cc', 'src/startup_project.cc',
    'src/knob_reset.c', 'src/sample_load_trace.c', 'src/sample_playing.c',
    'src/rear_audio.cc', 'src/rear_native.cc', 'src/rear_ui.c', 'src/rear_abi.S',
    'src/pad_resample_commit.cc', 'src/pad_sample_pool.cc', 'src/pad_assignment.cc',
    'src/pad_assignment_abi.S', 'src/pad_source_abi.S', 'src/pad_memory_abi.S']
mixer = ['src/mixer_model.c', 'src/mixer_meters.c', 'src/fx_chain.c', 'src/fx_catalog.c',
         'src/mixer_archive.cc', 'src/mixer_buses.c', 'src/mixer_route_native.c',
         'src/mixer_live.c', 'src/mixer_live_native.c', 'src/mixer_view.c',
         'src/mixer_control.c', 'src/mixer_session.cc', 'src/mixer_input.c',
         'src/mixer_runtime.cc', 'src/mixer_sources.cc', 'src/mixer_leds.c',
         'src/mixer_fx.cc', 'src/fx_native.cc', 'src/song_ui.c', 'src/mixer_stems.c']
sources = list(dict.fromkeys(sources + mixer))
sha = lambda b: hashlib.sha256(b).hexdigest()
hashes = {s: sha((R / s).read_bytes()) for s in sources + ['build_mixer_r27r.py', 'build/phone_hooks.h', 'src/shift_badge.h', 'src/mod_build.h', 'src/spectrum_plot.h']}
common = ['-O2', '-g', '-fPIC', '-Wall', '-Wextra', '-Werror', '-Ibuild',
          '-DMPC_RESAMPLE_TRIAL=1', '-DMPC_STOCK_RESAMPLE=1', '-DMPC_EXPERIMENTAL_CHOP_EXPORT=1',
          '-DMPC_PAD_EQ=1', '-DMPC_FAST_RESAMPLE=1', '-DMPC_SEQUENCE_COLOURS=1',
          '-DMPC_POLY_RECORD=1', '-DMPC_POWER_BUTTON=1', '-DMPC_CHORDS=1', '-DMPC_COMPRESSOR=1',
          '-DMPC_PROJECT_RECOVERY=1', '-DMPC_STARTUP_SPLASH=1', '-DMPC_EARLY_STARTUP=1',
          '-DMPC_KNOB_RESET=1', '-DMPC_LOOP_FEEDBACK=1', '-DMPC_REAR_ROUTING=1',
          '-DMPC_MIXER_ARCHIVE=1', '-DMPC_MIXER_LIVE=1', '-DMPC_MIXER_AUTOBIND=1', '-DMPC_SONG_UI=1',
          '-mbranch-protection=standard']
objects = []
for source in sources:
    cpp = source.endswith('.cc')
    obj = O / (Path(source).stem + '.o')
    objects.append(str(obj))
    subprocess.run(['aarch64-linux-gnu-g++' if cpp else 'aarch64-linux-gnu-gcc',
                    '-std=c++11' if cpp else '-std=c11', *common, '-c', source, '-o', str(obj)],
                   cwd=R, check=True)
library = O / 'phone-input.so'
subprocess.run(['aarch64-linux-gnu-g++', '-shared', '-Wl,-z,noexecstack,-z,relro,-z,now',
                '-Wl,--no-undefined', *objects, '-ldl', '-pthread', '-lm', '-o', str(library)],
               cwd=R, check=True)
sysroot = os.environ.get('MPC_QEMU_SYSROOT', '/home/user/mpc-rootfs-user-20260808')
bootstrap = Path(os.environ.get('AM_MOD_BOOTSTRAP_DIR', str(R / 'build/workspace-fix')))
p = subprocess.run(['qemu-aarch64', '-L', sysroot, str(bootstrap / 'bootstrap'),
                    str(R.parent / 'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'),
                    str(library), str(bootstrap / 'bootstrap-patches.bin')],
                   capture_output=True, text=True, timeout=60)
(O / 'bootstrap.log').write_text(p.stdout + p.stderr)
p.check_returncode()
# The released r27r checks must still hold with the Mixer linked in.
assert 'PASS real full preload constructor' in p.stdout
assert 'PITCH_AWARE_OVERWRITE_INSTALLED' in p.stderr
assert 'VOICE_GUARD RENDER_RELEASE_SERIALISED' in p.stderr
assert 'VALUES_BIPOLAR CENTRE_BAR_INSTALLED' in p.stderr
assert 'VALUES_SHIFT_ARROW INSTALLED' in p.stderr
assert 'TAKEOVER NATIVE_PROCESSORS_PRESERVED' in p.stderr
assert 'TAKEOVER SCALED_ENDPOINTS_INSTALLED' not in p.stderr
assert 'SONG_UI INSTALLED' in p.stderr  # SongScreen slots matched the pinned ELF
assert 'MIXER_STEMS INSTALLED' in p.stderr  # native writer close prologue matched
mixer_lines = [l for l in p.stderr.splitlines() if 'MIXER' in l]

route = O / 'check-phone-routes'
subprocess.run(['aarch64-linux-gnu-gcc', '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                '-mbranch-protection=standard', '-ffunction-sections', '-fdata-sections',
                '-Wl,--gc-sections', '-Ibuild', 'check_mixer_phone_routes.c', 'src/phone_events.c',
                'src/mixer_input.c', 'src/mixer_control.c', 'src/phone_input.c', 'src/mixer_model.c', 'src/fx_catalog.c',
                'src/fx_chain.c', 'src/fx_eq.c', 'src/mixer_leds.c', '-lm', '-pthread',
                '-o', str(route)], cwd=R, check=True)
routes = subprocess.run(['qemu-aarch64', '-L', sysroot, str(route)], cwd=R,
                        capture_output=True, text=True, timeout=30)
(O / 'phone-routes.log').write_text(routes.stdout + routes.stderr)
routes.check_returncode()
assert 'PASS Mixer through production input hooks' in routes.stdout
assert hashes == {s: sha((R / s).read_bytes()) for s in hashes}
report = dict(passed=True, base='r27r', library_sha256=sha(library.read_bytes()),
              sources_sha256=hashes, linked_sources=sources, compiler_flags=common,
              full_preload_constructor=True, r27r_markers=True, mixer_bootstrap_lines=mixer_lines,
              mixer_phone_routes=True, hardware_tested=False, card_written=False,
              release_packaged=False)
(O / 'compile-verification.json').write_text(json.dumps(report, indent=2) + '\n')
print('\n'.join(mixer_lines))
print('PASS r27r+Mixer full preload:', report['library_sha256'])
