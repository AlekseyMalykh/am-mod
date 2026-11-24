"""Build r27r + Mixer + Song UI as one full-preload candidate (WSL, run from sd-mod-session).

Same source list and flags as the released r27r (build_chords_candidate.py),
plus the Mixer sources with MPC_MIXER_ARCHIVE/LIVE/AUTOBIND. Output only in
build/mixer-r27r. No SD card, no release package, no device.
"""
from pathlib import Path
import hashlib, json, os, subprocess

R = Path(__file__).resolve().parent
O = R / 'build' / os.environ.get('AM_MOD_BUILD_NAME', 'mixer-r27r')
assert O.parent == R / 'build' and O.name not in ('', '.', '..')
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
    'src/rear_audio.cc', 'src/rear_native.cc', 'src/rear_ui.c', 'src/rear_abi.S', 'src/record_hook_abi.S',
    'src/pad_resample_commit.cc', 'src/pad_sample_pool.cc', 'src/pad_assignment.cc',
    'src/pad_assignment_abi.S', 'src/pad_source_abi.S', 'src/pad_memory_abi.S', 'src/pad_sample_source.cc']
mixer = ['src/mixer_model.c', 'src/mixer_meters.c', 'src/fx_chain.c', 'src/fx_own.c', 'src/fx_hs.c', 'src/hs_dsp/hs_heat.c', 'src/hs_dsp/hs_bass_focus.c', 'src/hs_dsp/hs_saturator.c', 'src/compressor_dsp.c', 'src/fx_catalog.c',
         'src/mixer_archive.cc', 'src/mixer_buses.c', 'src/mixer_route_native.c',
         'src/mixer_live.c', 'src/mixer_live_native.c', 'src/mixer_view.c',
         'src/mixer_control.c', 'src/mixer_session.cc', 'src/mixer_input.c',
         'src/mixer_runtime.cc', 'src/mixer_sources.cc', 'src/mixer_leds.c',
         'src/mixer_fx.cc', 'src/input_fx.cc', 'src/fx_native.cc', 'src/song_ui.c', 'src/mixer_stems.c', 'src/seq_mutes.c', 'src/seq_audition.c', 'src/screen_mirror_drm.c', 'src/usb_audio.c', 'src/usb_audio_alsa.c',
         'src/midi_tools.c', 'src/synth.c', 'src/midi_alsa.c', 'src/midi_ui.c',
         'src/midi_event.c', 'src/midi_context.c', 'src/midi_native.c', 'src/sample_live_tuner.cc','src/internal_storage.c']
sources = list(dict.fromkeys(sources + mixer + ['src/piano_roll.c','src/midi_route.c','src/midi_voice.c','src/midi_wire.c','src/midi_file.c','src/midi_file_native.c','src/program_pgm.c','src/program_import.cc','src/program_manifest.cc']))
pgm_probe = os.environ.get('AM_MOD_PGM_NATIVE_PROBE') == '1'
pad_midi_trial = os.environ.get('AM_MOD_PAD_MIDI_RECORDING_TRIAL') == '1'
pad_midi_probe = os.environ.get('AM_MOD_PAD_MIDI_VM_PROBE') == '1'
synth_native_trial = os.environ.get('AM_MOD_SYNTH_NATIVE_PLAYBACK_TRIAL') == '1'
pattern_source_trial = os.environ.get('AM_MOD_PATTERN_SOURCE_TRIAL') == '1'
pattern_tools_trial = os.environ.get('AM_MOD_PATTERN_TOOLS_TRIAL') == '1'
assert not synth_native_trial or (pad_midi_trial and os.environ.get('AM_MOD_MIDI_EVENT_CONTEXT','1')!='0'), 'Synth playback requires pad trial and native MIDI event context'
assert not pad_midi_probe or pad_midi_trial, 'VM probe requires explicit trial build'
if pad_midi_trial:sources += ['src/pad_midi_note.c','src/synth_sequence.c','src/synth_binding.c','src/synth_archive.cc']
if synth_native_trial:sources += ['src/synth_route.c','src/synth_accept_native.cc']
if pattern_source_trial:sources += ['src/pattern_source.cc','src/pattern_lifecycle.cc']
if pattern_tools_trial:sources += ['src/pattern_model.cc','src/pattern_library.cc','src/pattern_codec.cc','src/pattern_tools.cc','src/arrangement.cc','src/arrangement_codec.cc']
if pgm_probe:sources += ['src/program_native_probe.cc']
sha = lambda b: hashlib.sha256(b).hexdigest()
hashes = {s: sha((R / s).read_bytes()) for s in sources + ['build_mixer_r27r.py', 'build/phone_hooks.h', 'src/shift_badge.h', 'src/mod_build.h', 'src/spectrum_plot.h', 'src/screen_mirror_frame.h', 'src/usb_audio.h', 'src/fx_layout.inc', 'src/fx_visual.inc', 'src/fx_catalog.h', 'src/fx_amp_models.inc', 'src/piano_roll.h', 'src/piano_input.inc', 'src/poly_record.h', 'src/batch_copy.h', 'src/piano_roll_native.inc', 'src/midi_event.h', 'src/midi_context.h', 'src/midi_native.h', 'src/sample_live_tuner.h']}
for name in ('src/midi_file.h','src/midi_file_native.h','src/piano_roll_midi.inc'):hashes[name]=sha((R/name).read_bytes())
hashes['src/usb_drive_lights.h']=sha((R/'src/usb_drive_lights.h').read_bytes())
hashes['src/internal_storage.h']=sha((R/'src/internal_storage.h').read_bytes())
hashes['src/pattern_phone_lifecycle.inc']=sha((R/'src/pattern_phone_lifecycle.inc').read_bytes())
for name in ('src/seq_mutes.h','src/seq_audition.h','src/midi_wire.h','src/midi_voice.h','src/midi_route.h','src/midi_live.inc'):hashes[name]=sha((R/name).read_bytes())
for name in ('src/program_pgm.h','src/program_import.h','src/program_manifest.h','src/program_import_ui.inc'):hashes[name]=sha((R/name).read_bytes())
common = ['-O2', '-g', '-fPIC', '-Wall', '-Wextra', '-Werror', '-Ibuild',
          '-DMPC_RESAMPLE_TRIAL=1', '-DMPC_STOCK_RESAMPLE=1', '-DMPC_EXPERIMENTAL_CHOP_EXPORT=1',
          '-DMPC_PAD_EQ=1', '-DMPC_FAST_RESAMPLE=1', '-DMPC_SEQUENCE_COLOURS=1',
          '-DMPC_PROGRAM_IMPORT=1', '-DMPC_POLY_RECORD=1', '-DMPC_POWER_BUTTON=1', '-DMPC_CHORDS=1', '-DMPC_COMPRESSOR=1',
          '-DMPC_PROJECT_RECOVERY=1', '-DMPC_STARTUP_SPLASH=1', '-DMPC_EARLY_STARTUP=1',
          '-DMPC_KNOB_RESET=1', '-DMPC_LOOP_FEEDBACK=1', '-DMPC_REAR_ROUTING=1',
          '-DMPC_SAMPLE_LIVE_TUNER=1', '-DMPC_MIDI_TOOLS=1', '-DMPC_USB_AUDIO=1', '-DMPC_MIXER_ARCHIVE=1', '-DMPC_MIXER_LIVE=1', '-DMPC_MIXER_AUTOBIND=1', '-DMPC_SONG_UI=1',
          '-mbranch-protection=standard']
if os.environ.get('AM_MOD_MIDI_EVENT_CONTEXT','1')!='0':common.append('-DMPC_MIDI_EVENTS=1')
if pattern_source_trial:
    common.append('-DMPC_PATTERN_SOURCE_TRIAL=1')
    for name in ('src/pattern_source.h','src/pattern_request.h','src/pattern_native.h','src/pattern_model.h','src/pattern_lifecycle.h','src/midi_event.h','src/step_native.h'):
        hashes[name]=sha((R/name).read_bytes())
if pattern_tools_trial:
    common.append('-DMPC_PATTERN_TOOLS_TRIAL=1')
    for name in ('src/pattern_model.h','src/pattern_library.h','src/pattern_tools.h','src/pattern_roll.inc','src/arrangement.h','src/arrangement_ui.inc'):
        hashes[name]=sha((R/name).read_bytes())
if pad_midi_trial:
    common.append('-DMPC_PAD_MIDI_RECORDING_TRIAL=1')
    hashes['src/pad_midi_note.h']=sha((R/'src/pad_midi_note.h').read_bytes())
    for name in ('src/synth_pad_record.inc','src/synth.h','src/synth_sequence.h','src/synth_binding.h','src/synth_archive.h','src/workflow_ui.h','src/mixer_control.h'):
        hashes[name]=sha((R/name).read_bytes())
if pad_midi_probe:
    common.append('-DMPC_PAD_MIDI_VM_PROBE=1')
    hashes['src/pad_midi_vm_probe.inc']=sha((R/'src/pad_midi_vm_probe.inc').read_bytes())
if synth_native_trial:
    common.append('-DMPC_SYNTH_NATIVE_PLAYBACK_TRIAL=1')
    for name in ('src/synth_route.h','src/synth_accept_native.h','src/synth_live.inc'):
        hashes[name]=sha((R/name).read_bytes())
if os.environ.get('AM_MOD_RECORD_DIAGNOSTICS')=='1':
    common.append('-DMPC_RECORD_DIAGNOSTICS=1')
    hashes['src/record_probe.inc']=sha((R/'src/record_probe.inc').read_bytes())
if os.environ.get('AM_MOD_RECORD_CHAIN_BYPASS')=='1':
    assert os.environ.get('AM_MOD_RECORD_DIAGNOSTICS')=='1','Input bypass is a diagnostic build only'
    common.append('-DMPC_RECORD_CHAIN_BYPASS=1')
if os.environ.get('AM_MOD_RECORD_KEEP_RECOVERY_PATH')=='1':
    assert os.environ.get('AM_MOD_RECORD_DIAGNOSTICS')=='1','Recovery path A/B experiment only'
    common.append('-DMPC_RECORD_KEEP_RECOVERY_PATH=1')
if pgm_probe:
    common.append('-DMPC_PGM_NATIVE_PROBE=1')
    hashes['src/program_pgm.h'] = sha((R / 'src/program_pgm.h').read_bytes())
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
                    os.environ.get('MPC_STOCK_APP', str(R.parent / 'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample')),
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
assert 'MIXER BANK_LED_INSTALLED' in p.stderr
assert 'MIXER_STEMS INSTALLED' in p.stderr  # native writer close prologue matched
assert ('AM_MIDI_NATIVE SOURCE_CONTEXT_INSTALLED' in p.stderr)==('-DMPC_MIDI_EVENTS=1' in common)
mixer_lines = [l for l in p.stderr.splitlines() if 'MIXER' in l]

route = O / 'check-phone-routes'
subprocess.run(['aarch64-linux-gnu-gcc', '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                '-mbranch-protection=standard', '-ffunction-sections', '-fdata-sections',
                '-Wl,--gc-sections', '-Ibuild', 'check_mixer_phone_routes.c', 'src/phone_events.c',
                'src/mixer_input.c', 'src/mixer_control.c', 'src/phone_input.c', 'src/mixer_model.c', 'src/fx_catalog.c',
                'src/fx_chain.c', 'src/fx_own.c', 'src/fx_hs.c', 'src/hs_dsp/hs_heat.c', 'src/hs_dsp/hs_bass_focus.c', 'src/hs_dsp/hs_saturator.c', 'src/compressor_dsp.c', 'src/fx_eq.c', 'src/mixer_leds.c', 'src/piano_roll.c', '-lm', '-pthread',
                '-o', str(route)], cwd=R, check=True)
routes = subprocess.run(['qemu-aarch64', '-L', sysroot, str(route)], cwd=R,
                        capture_output=True, text=True, timeout=30)
(O / 'phone-routes.log').write_text(routes.stdout + routes.stderr)
routes.check_returncode()
assert 'PASS Mixer through production input hooks' in routes.stdout
assert 'PASS cold MIDI Tools entry through production producer' in routes.stdout
assert hashes == {s: sha((R / s).read_bytes()) for s in hashes}
report = dict(passed=True, base='r27r', library_sha256=sha(library.read_bytes()),
              sources_sha256=hashes, linked_sources=sources, compiler_flags=common,
              full_preload_constructor=True, r27r_markers=True, mixer_bootstrap_lines=mixer_lines,
              mixer_phone_routes=True, hardware_tested=False, card_written=False,
              release_packaged=False)
(O / 'compile-verification.json').write_text(json.dumps(report, indent=2) + '\n')
print('\n'.join(mixer_lines))
print('PASS r27r+Mixer full preload:', report['library_sha256'])
