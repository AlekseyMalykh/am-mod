"""Offline r13 contracts; execute in the existing WSL ARM toolchain.

This does not connect to MPC. Native event/container/ABI code runs under QEMU;
the command executor and GUI remain fixture boundaries in check_steps.c.
"""
from pathlib import Path
from hashlib import sha256
import json
import subprocess
import tempfile
import zipfile

R=Path(__file__).resolve().parent
W=R.parent
ROOT=Path('/home/user/mpc-rootfs-user-20260808')
q=['qemu-aarch64','-L',str(ROOT)]
def run(*args,**kw):return subprocess.run(args,cwd=R,check=True,**kw)
def digest(b):return sha256(b).hexdigest()
info=json.loads((R/'build-info.json').read_text())
assert info['revision']=='r16d-sample-input-preview'
assert info['workflows']['batch_copy_limit']==16 and info['workflows']['chop_export_enabled']
checks=[]
probe=run('python3','verify_picker_probe.py',capture_output=True,text=True)
(R/'build/r16b-picker-probe.log').write_text(probe.stdout+probe.stderr)
checks.append(probe.stdout.strip())
for name,sources in [('music-lights',['check_music_lights.c','src/music_settings.c']),('naming-shift',['check_naming_shift.c','src/phone_input.c']),('private-preview',['check_private_preview.c','src/workflow_abi.S','src/phone_input.c','src/browser_tree.c'])]:
    run('aarch64-linux-gnu-gcc','-O1','-Wall','-Wextra','-Werror','-Ibuild','-DMPC_EXPERIMENTAL_CHOP_EXPORT=1','-ffunction-sections','-fdata-sections','-Wl,--gc-sections',*sources,'-Wl,--no-as-needed','-lstdc++','-ldl','-lpthread','-lm','-o','build/check-'+name)
    result=subprocess.run(q+[str(R/('build/check-'+name)),str(W/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample')],cwd=R,capture_output=True,text=True)
    (R/('build/r15f-'+name+'.log')).write_text(result.stdout+result.stderr)
    result.check_returncode()
    checks.append(result.stdout.strip())
from verify_callbacks import verify as verify_callback_vector
callback_result=verify_callback_vector(R/'autorun/phone-input.so')
assert callback_result['swapped_vector_rejected']
checks.append('Compiled ARM callback vector: all hook/function ELF relocations match; swapped Chop/footer vector rejected')
run('aarch64-linux-gnu-gcc','-O1','-Wall','-Wextra','-Werror','-Ibuild',
    '-ffunction-sections','-fdata-sections','-Wl,--gc-sections',
    'check_headers.c','src/music_settings.c','-pthread','-o','build/check-headers')
headers=run(*q,str(R/'build/check-headers'),capture_output=True,text=True)
(R/'build/r15-header-check.log').write_text(headers.stdout+headers.stderr)
checks.append('Native question captions and compact tabs retained; no Project menu overlay, native white last row, tagged confirmation, Cancel no exit, explicit B3 RAM request0600 and O_NOFOLLOW, Play passthrough (GUI fixture)')
run('aarch64-linux-gnu-gcc','-O1','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-misleading-indentation','-DTEST_PHONE','-DTEST_PROJECT_DISPATCH='+info['symbols']['project_dispatch'],'-DTEST_MUSIC_BRIDGE='+info['symbols']['phone_music_project'],'check_project.c','-o','build/check-project-phone')
project_rows=run(*q,str(R/'build/check-project-phone'),str(W/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'),str(R/'build/project.bin'),capture_output=True,text=True)
(R/'build/r15g-project-rows.log').write_text(project_rows.stdout+project_rows.stderr)
checks.append('Actual native Project vector deep-copy/rename/destruction128 cycles at two PIE bases, six rows: Key4 then Off5; stock commands0..3 retained; injected dispatch emits tagged confirmation only')
run('aarch64-linux-gnu-g++','-c','-O2','-std=c++11','-Wall','-Wextra','-Werror',
    '-mbranch-protection=standard','check_steps_support.cc','-o','build/check_steps_support.o')
run('aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror','-Ibuild','-mbranch-protection=standard',
    'check_steps.c','check_steps_support.S','src/step_abi.S','build/check_steps_support.o',
    'build/step_exceptions.o','build/step_mute.o','-lstdc++','-ldl','-lpthread','-lm','-o','build/check-steps')
stock_path=W/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
native=subprocess.run(q+[str(R/'build/check-steps'),str(stock_path),str(R/'build/r15-step-draw.txt')],cwd=R,capture_output=True,text=True)
(R/'build/r15-native-check.log').write_text(native.stdout+native.stderr)
native.check_returncode()
assert 'PASS actual ARM' in native.stdout
checks.append('Actual ARM factories, containers, 10-mode 16 Levels processor, staged event picker/cancel, group Erase scope/duplicates, ordinary-mode playback RGB, time signatures and ABI; unmocked independent isPlaying and record predicates, native bank setter, native LED MIDI message; nonidentity pad-to-MIDI sample lookup; module event commands, tickets, RGB and production paint calls')
checks.append('r14: actual all-layer HasSample sound palette; native mute bit9 group replacement with other payload bytes unchanged; mixed/empty groups, latch/hold and producer timestamps across UI delay; stale held ticket after release ignored; K1 staged picker type; known slice-count bounds; C/L source-data markers and generated x24/x8 ARM label bridge/fallback')
# Execute the actual command normalization/perform/undo which the older
# fixture did not cover. Program/registry/audio notification adapters remain
# isolated fixtures for the copy command; no real device/audio claim.
for check in ['mute_native','batch_native','batch_dispatch','batch_events']:
    run('aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror','-Ibuild',
        '-mbranch-protection=standard','check_'+check+'.c','check_steps_support.S',
        'src/step_abi.S','build/check_steps_support.o','build/step_mute.o',
        'build/batch_copy.o','build/batch_events.o','build/step_exceptions.o','-lstdc++','-ldl','-lpthread','-lm',
        '-o','build/check-'+check)
    result=run(*q,str(R/('build/check-'+check)),str(stock_path),capture_output=True,text=True)
    assert 'PASS actual native' in result.stdout
    (R/('build/r15-'+check+'.log')).write_text(result.stdout+result.stderr)
checks.append('r15e: actual Project2909be0 and Sequence2957ae0 across three sequences, native bulk command19d79d0/perform/undo plus CopyPadCommand in one outer group; distinct patterns, time/type and exact40-byte payload except destination note (native alignment padding ignored), duplicate/C/L/mute and existing target preservation, sound-only no event writes, overlapping source snapshots and fail-before-write; native list notifications/audio remain fixtures')
checks.append('Actual native replacement command Mute/Unmute with duplicate notes and mixed groups: counts, Undo/Redo; native CopyPadCommand ctor/perform/undo: overlapping 3-pad cycle, original source snapshots and key mapping restored, one group Undo/Redo; source getter production callback; Program/registry/audio adapters are fixtures')
run('aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror',
    '-mbranch-protection=standard','-DMPC_EXPERIMENTAL_CHOP_EXPORT=1','check_workflow.c','src/workflow_abi.S','src/phone_input.c','src/browser_tree.c',
    '-Wl,--no-as-needed','-lstdc++','-ldl','-lpthread','-lm','-o','build/check-workflow')
result=run(*q,str(R/'build/check-workflow'),str(stock_path),str(R/'build/r15-workflow-draw.txt'),capture_output=True,text=True)
assert 'PASS actual native File' in result.stdout
(R/'build/r15-workflow-check.log').write_text(result.stdout+result.stderr)
checks.append('Actual native optional File getter, isFile and primary-load callback; phone query/filter/zero results/stale tickets; batch source banks, free auto targets, pinned targets, duplicate/self check, separate overwrite confirmation; production paint bounds')
checks.append('r15g: Copy/Chop Sources/Targets use private SampleFileAudition, exact trim/slice ranges, one-shot/no Loop, click toggle, natural completion replay, navigation Stop; native worker replaces previous voice before next; Play/Stop down/up bypass custom modes; storage caption from actual directory and bounded selector/path paint; source/project mutation absent from preview')
checks.append('r15c: device curve3 (+6dB squared) inverse verified by actual native instrument gain; native selected File getter hides K3 for folders/empty; physical Erase invokes actual stock B3 Delete entry with final dialog fixture; Shift+B2 swallowed; Chop Sources/Targets encoder audition down/up and native EventAudition slice+MIDI mapping')
checks.append('r15b: actual native preview closure constructor; no Copy context/preview DI service; Main binding absent/NaN/wrong preview curve blocks sound; native Main physical property to actual instrument amplitude, mute at K3=100 and live decrease; Chop same Program through distinct wrappers and nonSample mode accepted')
checks.append('r14a: actual AC50CopyMode ctor15173c0 changes captured base-subobject vptr at1517478; B2 opens Batch after final vptr6e0dfd8; 16 sources accepted and 17th rejected; stock B3 retained with experimental Chop export disabled')
checks.append('r14b: actual CommandManager ctor19cf2b0/dispatch19d0a00/transaction19cc1f0/command19c9a50 receive exact context68 self, not field60; four copies and Undo; actual native queue2b59300 closure copy and runner2b59200/2b59160, reference lifetime, sequential progress, rejected duplicate start/stopped queue and failed snapshot. History storage/notifications, Program/instrument/audio adapters remain fixtures')
checks.append('r14b GUI: green sources select row; other pads assign target; occupied red/free violet/current amber; no self or duplicate pinned target; K2 Auto/unpin, bank browsing preserves targets; initial progress paints before worker scheduling; busy blocks mutations/duplicate apply; Done persists until acknowledgement')
run('aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror','-Ibuild',
    '-ffunction-sections','-fdata-sections','-Wl,--gc-sections',
    'check_workflow_rgb.c','src/phone_input.c','-ldl','-lpthread','-o','build/check-workflow-rgb')
rgb=run(*q,str(R/'build/check-workflow-rgb'),capture_output=True,text=True)
assert 'PASS production RGB sender' in rgb.stdout
(R/'build/r15-rgb-check.log').write_text(rgb.stdout+rgb.stderr)
checks.append('r14c production RGB sender: workflow palette priority over stale naming state, generation reassert after bank change even with identical colours, target delta and native colour restore; actual device receiver replaced with recording fixture')
checks.append('r14c browser: actual native14fc6a0 sample callback distinct from project14fb1c0; silent destination selection until B3; bounded recursive Subfolders/Library with Samples/Projects roots, audio/project filtering and symlink exclusion; repeated cancel/Shift invalidation and pad8 #; independent browser tests retained')
old14b=json.loads((R/'releases/20261025-r14b-batch-targets-progress/build-info.json').read_text())
assert digest((W/old14b['zip']).read_bytes())==old14b['zip_sha256']
checks.append('Frozen r14b archive preserved; four-copy plus Undo hardware confirmation retained separately from unverified r14c')
old14c=json.loads((R/'releases/20261025-r14c-browser-load-rgb/build-info.json').read_text())
assert digest((W/old14c['zip']).read_bytes())==old14c['zip_sha256']
checks.append('Frozen r14c archive preserved')
checks.append('r14d: queued Copy exit runs actual native modal removal1c0d990, mode handoff1c0d420 and property1c09870; deactivates Copy and activates Sample (recording receiver adapters), releases context guard, no Undo entry, duplicate/stopped queue rejected; UI waits for exit completion on Cancel and Done, forwards Shift release while busy; all assigned targets cyan/current amber; K3 bank-only free refuses overflow and retains pinned rows, explicit all-bank fallback')
old14d=json.loads((R/'releases/20261025-r14d-batch-exit-bank-targets/build-info.json').read_text())
assert digest((W/old14d['zip']).read_bytes())==old14d['zip_sha256']
checks.append('Frozen r14d archive preserved')
run('aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror','-Ibuild',
    '-ffunction-sections','-fdata-sections','-Wl,--gc-sections',
    'check_button_routes.c','src/phone_input.c','-ldl','-lpthread','-o','build/check-button-routes')
routes=run(*q,str(R/'build/check-button-routes'),capture_output=True,text=True)
assert 'PASS production button routing' in routes.stdout
(R/'build/r15-button-routes.log').write_text(routes.stdout+routes.stderr)
checks.append('r14e production button routing: native down/up pairing across workflow tickets, orphan B3 release rejected, B2 immediate native Shift/Erase release, one-time suppression of delayed physical releases')
checks.append('r14e actual queued native Shift decoder159f2f0 and BoolProperty13904f0: stale Shift-up updates GlobalControls22a0 after B2 epoch change; ordinary stale actions still rejected')
checks.append('r14e both actual Erase release closures14c0f60/14c0fb0 with exact BL guards preserve Copy only during Batch, normal native exit outside Batch and lifetime release retained; all45 current callback bindings verified')
checks.append('r14e worker audition: native record-None predicate checked on UI and worker; ordinary source-pad audio on/off ABI with audio receiver fixtures, no copy/Undo/current-pad setter; blocked audition cannot release another note')
checks.append('r14e UI: First free restored, Same pad preserved, additional Bank free strictly bounded; Auto bank label distinct from View bank; +/- rows in Targets, correct native +/- direction in Sources/browser; persistent red missing-target warning and No space caption; encoder auditions and never confirms Copy')
assert 'ffff4040' in (R/'build/r15-workflow-draw.txt.no-space').read_text()
assert 'No space' in (R/'build/r15-workflow-draw.txt.no-space').read_text()
assert 'targets missing' in (R/'build/r15-workflow-draw.txt.no-space').read_text()
old14e=json.loads((R/'releases/20261025-r14e-batch-gesture-audition/build-info.json').read_text())
assert digest((W/old14e['zip']).read_bytes())==old14e['zip_sha256']
run('aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror','-Ibuild','-mbranch-protection=standard',
    'check_mute_playback.c','check_steps_support.S','src/step_abi.S','build/check_steps_support.o',
    'build/step_exceptions.o','build/step_mute.o','-lstdc++','-ldl','-lpthread','-lm','-o','build/check-mute-playback')
played=run(*q,str(R/'build/check-mute-playback'),str(stock_path),capture_output=True,text=True)
assert 'PASS actual native213ba00' in played.stdout
(R/'build/r15-mute-playback.log').write_text(played.stdout+played.stderr)
checks.append('r14f actual213ba00 emits muted note-ons before patch; production ARM gate blocks muted note-ons and retains unmuted/NoteOff dispatch with final audio receiver fixture')
checks.append('Whole-sound Unmute and Shift+encoder move retain native commands/Undo; r15f tests green/yellow/red/orange semantics, white selection in both phases, cyan playhead and mixed groups; Mute hold clears previously latched state; Shift preserves Steps RGB; KnobFX underlay fixture retained')
run('aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror','-Ibuild','-mbranch-protection=standard',
    'check_picker_audition.c','check_steps_support.S','src/step_abi.S','build/check_steps_support.o',
    'build/step_exceptions.o','build/step_mute.o','-lstdc++','-ldl','-lpthread','-lm','-o','build/check-picker-audition')
audition=run(*q,str(R/'build/check-picker-audition'),str(stock_path),capture_output=True,text=True)
(R/'build/r15-picker-audition.log').write_text(audition.stdout+audition.stderr)
assert 'PASS r16d frontend' in audition.stdout
checks.append('r16d frontend: all16 pads/all10 modes Events/Steps, staged Sample input adapter, original amber/choice cyan, Apply/Cancel and paired release; legacy EventAudition entries poisoned')
run('aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror','-Ibuild','-mbranch-protection=standard',
    'check_event_sample_input.c','check_steps_support.S','src/step_abi.S','build/check_steps_support.o',
    'build/step_exceptions.o','build/step_mute.o','build/batch_copy.o','build/batch_events.o',
    '-lstdc++','-ldl','-lpthread','-lm','-o','build/check-event-sample-input')
sample_input=run(*q,str(R/'build/check-event-sample-input'),str(stock_path),capture_output=True,text=True)
(R/'build/r16d-event-sample-input.log').write_text(sample_input.stdout+sample_input.stderr)
assert 'PASS actual native Sample1e27360' in sample_input.stdout
checks.append('r16d real native queue, Sample1e27360 MIDI construction, shipped Levels processor bridge/TLS override and native1e270d0 release; staged full C/L note, latest-only/cancel/record guards/refcounts; C executes actual211b090/211aa30/2115070 with16 slices and other-bank source; registered-track/MIDI policy and final sampler remain adapters')
run('aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror','-Ibuild','-mbranch-protection=standard',
    'check_preview_volume.c','check_steps_support.S','src/step_abi.S','build/check_steps_support.o',
    'build/step_exceptions.o','build/step_mute.o','-lstdc++','-ldl','-lpthread','-lm','-o','build/check-preview-volume')
preview_gain=run(*q,str(R/'build/check-preview-volume'),str(stock_path),capture_output=True,text=True)
(R/'build/r15a-preview-volume.log').write_text(preview_gain.stdout+preview_gain.stderr)
assert 'PASS actual SampleFileAudition' in preview_gain.stdout
checks.append('r15a native SampleFileAudition volume listener20bc210/worker20bc160 only requests preview instrument20, zero/half/full gain and queue refcounts; instrument lookup/final audio gain receiver remain fixtures; workflow uses real float-property setter')
checks.append('r15a workflow: actual RTTI Samples0/Projects1, native sample/project/preview callbacks, sequential destination loads retaining search and project confirmation/cancel; root search then native-directory transition/exact row selection for Samples/User and Projects; actual float-property preview volume setter/address-point guard; stale directory cache, linked/hidden folders, bounded query after4100 files; final GUI/service/audio callbacks remain fixtures')
checks.append('r14f Steps Chop/Levels physical button availability follows selected step even when playback Follow displays a different page; both C/L available for mixed step, neither for plain event')
run('aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror','-Ibuild','-mbranch-protection=standard',
    'check_mute_sequence.c','check_steps_support.S','src/step_abi.S','build/check_steps_support.o',
    'build/step_exceptions.o','build/step_mute.o','-lstdc++','-ldl','-lpthread','-lm','-o','build/check-mute-sequence')
sequence=run(*q,str(R/'build/check-mute-sequence'),str(stock_path),capture_output=True,text=True)
(R/'build/r15-mute-sequence.log').write_text(sequence.stdout+sequence.stderr)
assert 'PASS actual sampler2119d00' in sequence.stdout
checks.append('r15 actual sequence input211a230 and sampler route2119d00 reproduce r14f bypass; production gates block muted note-on before sampler/event bus/buffer while ordinary notes and NoteOff pass; final output receivers are fixtures')
old14f=json.loads((R/'releases/20261025-r14f-steps-mute-move-fx/build-info.json').read_text())
assert digest((W/old14f['zip']).read_bytes())==old14f['zip_sha256']
checks.append('Frozen r14f archive preserved; audible mute failure on device retained as regression evidence')
# The loader splits LD_PRELOAD on spaces; use a controlled temporary path.
with tempfile.TemporaryDirectory(prefix='mpc-r13-loader-') as folder:
    lib=Path(folder)/'phone-input.so';lib.write_bytes((R/'autorun/phone-input.so').read_bytes())
    loaded=run(*q,'-E','LD_DEBUG=libs','-E','LD_PRELOAD='+str(lib),
               str(ROOT/'usr/bin/busybox.nosuid'),'true',capture_output=True,text=True)
    (R/'build/r15-library-load.log').write_text(loaded.stdout+loaded.stderr)
    assert 'calling init: '+str(lib) in loaded.stderr
    assert 'cannot be preloaded' not in loaded.stderr
checks.append('Shipped ARM library loads against firmware rootfs, including libstdc++ CXXABI_1.3')
archive=W/info['zip']
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
    for name,h in info['payload_sha256'].items():
        assert digest(z.read('autorun/'+name))==h==digest((R/'autorun'/name).read_bytes())
    assert z.read('autorun/autorun.check').decode()==digest(z.read('autorun/autorun.json')+
        z.read('autorun/autorun.sh')+b'f7bed1c9-7e90-4731-9194-c996666b4331')
assert digest(archive.read_bytes())==info['zip_sha256']
checks.append('Archive, every payload digest, native autorun checksum')
checks.append('r16 drawing fixtures: larger Steps identity/page/edit/play/status groups; three aligned mapping rows, busy/missing target warnings, query cursor viewport, whole UTF8 visual ellipsis and result parent folder; input/audio command contracts unchanged')
stock=stock_path.read_bytes();assert digest(stock)==info['stock_sha256']
target=Path('/run/mpc-clone-trial-v1')
assert not target.exists(),'Refuse to touch an existing runtime directory'
target.mkdir(mode=0o700)
path=target/'MPC-Sample'
try:
    # The new44th bridge takes this patch beyond4096 bytes. Verify that the
    # fixed-size compare buffer still checks its tail before any write.
    bridge=next(p for p in info['patches'] if p['file']=='phone-code.bin')
    assert bridge['size']>4096
    corrupt=bytearray(stock);corrupt[int(bridge['offset'],16)+4096]^=1
    path.write_bytes(corrupt)
    refused=subprocess.run(q+[str(R/'autorun/apply-session')],capture_output=True,cwd=R)
    assert refused.returncode!=0 and b'original bytes' in refused.stderr and path.read_bytes()==corrupt
    path.write_bytes(stock)
    patched=run(*q,str(R/'autorun/apply-session'),capture_output=True,text=True)
    assert patched.stdout.strip()=='SESSION_RAM_PATCH_OK'
    actual=path.read_bytes();assert digest(actual)==info['patched_copy_sha256']
    assert actual[0x6d43bb0:]==stock[0x6d43bb0:]
    repeat=subprocess.run(q+[str(R/'autorun/apply-session')],capture_output=True,cwd=R)
    assert repeat.returncode!=0 and path.read_bytes()==actual
finally:
    path.unlink(missing_ok=True);target.rmdir()
checks.append('Actual ARM patcher: full expected hash, RW data unchanged, repeat rejected')
for name in ['autorun.sh','mod-launcher.sh']:
    run(*q,str(ROOT/'usr/bin/busybox.nosuid'),'sh','-n',str(R/'autorun'/name))
checks.append('Both loader scripts parse with firmware BusyBox')
old=json.loads((R/'releases/20261024-r12b-compact-header-tabs/build-info.json').read_text())
assert digest((W/old['zip']).read_bytes())==old['zip_sha256']
checks.append('Frozen r12b archive preserved')
frozen15f=json.loads((R/'releases/20261025-r15f-lights-preview-steps/build-info.json').read_text())
assert digest((W/frozen15f['zip']).read_bytes())==frozen15f['zip_sha256']
checks.append('Frozen r15f archive preserved; hardware confirmation and reported replay/Loop/transport regressions retained separately')
old13=json.loads((R/'releases/20261024-r13-pad-steps/build-info.json').read_text())
assert digest((W/old13['zip']).read_bytes())==old13['zip_sha256']
checks.append('Frozen r13 archive preserved')
old13b=json.loads((R/'releases/20261024-r13b-steps-controls/build-info.json').read_text())
assert digest((W/old13b['zip']).read_bytes())==old13b['zip_sha256']
checks.append('Frozen r13b archive preserved')
old13c=json.loads((R/'releases/20261024-r13c-playhead-bank-sample/build-info.json').read_text())
assert digest((W/old13c['zip']).read_bytes())==old13c['zip_sha256']
checks.append('Frozen r13c archive preserved')
old13d=json.loads((R/'releases/20261024-r13d-event-choice-playback/build-info.json').read_text())
assert digest((W/old13d['zip']).read_bytes())==old13d['zip_sha256']
checks.append('Frozen r13d archive preserved')
old13e=json.loads((R/'releases/20261024-r13e-sound-mute-text-picker/build-info.json').read_text())
assert digest((W/old13e['zip']).read_bytes())==old13e['zip_sha256']
checks.append('Frozen r13e archive preserved')
checks.append('r14: real native CC4b Chop input distinct from CC24 Save; cold ordinary Sample UI init; hidden Steps does not block playback; locked instrument668; outer bank source pulses; 10 native Levels modes over sequence wrap, muted-event exclusion; compact C/L/CL markers')
old13f1=json.loads((R/'releases/20261024-r13f1-playback-chop-grouped-steps/build-info.json').read_text())
assert digest((W/old13f1['zip']).read_bytes())==old13f1['zip_sha256']
checks.append('Frozen r13f1 archive preserved')
old13g=json.loads((R/'releases/20261024-r13g-context-event-pickers/build-info.json').read_text())
assert digest((W/old13g['zip']).read_bytes())==old13g['zip_sha256']
checks.append('Frozen r13g archive preserved')
old14=json.loads((R/'releases/20261024-r14-browser-batch-mute/build-info.json').read_text())
assert digest((W/old14['zip']).read_bytes())==old14['zip_sha256']
checks.append('Frozen r14 archive preserved')
old14a=json.loads((R/'releases/20261024-r14a-copy-entry-limit16/build-info.json').read_text())
assert digest((W/old14a['zip']).read_bytes())==old14a['zip_sha256']
checks.append('Frozen r14a archive preserved')
checks.append('r14: C/L strict matching shared with row labels; unmarked sliced source has no editor; Steps single event Apply and cancel retain view; mixed groups filter by kind; multiple-event chooser; stale source rejected; 64 cap rejects overflow; final off-grid tail included')
result={'revision':info['revision'],'zip_sha256':info['zip_sha256'],'checks':checks,
        'hardware_tested':False,'audio_engine_tested':False,
        'native_contract_fixture':'Existing Steps tests isolate GUI/executor; added tests execute native Mute command ctor/perform/undo and native CopyPadCommand ctor/perform/undo; Program registry, instrument audio clone and notifications remain fixtures; device timing/LED/lifecycle pending'}
(R/'build/r15-verification.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
