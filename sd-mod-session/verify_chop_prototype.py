"""Offline prototype only: never modifies autorun, archives or SD.

PCM extraction, SamplePool mutation and persistence are still fixture boundaries.
The report distinguishes a limited device trial from hardware validation.
"""
from pathlib import Path
from hashlib import sha256
import json
import subprocess
import tempfile

R = Path(__file__).resolve().parent
ROOT = Path('/home/user/mpc-rootfs-user-20260808')
STOCK = R.parent / 'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
QEMU = ['qemu-aarch64', '-L', str(ROOT)]


def run(label, args):
    result = subprocess.run(args, cwd=R, capture_output=True, text=True)
    (R / ('build/r15-prototype-' + label + '.log')).write_text(result.stdout + result.stderr)
    result.check_returncode()
    return result


cpp = ['aarch64-linux-gnu-g++', '-c', '-fPIC', '-O2', '-std=c++11',
       '-Wall', '-Wextra', '-Werror', '-mbranch-protection=standard']
objects = []
for name in ['step_exceptions', 'step_mute', 'batch_copy', 'batch_events']:
    obj = 'build/prototype-' + name + '.o'
    run('compile-' + name, cpp + ['src/' + name + '.cc', '-o', obj])
    objects.append(obj)
support = 'build/prototype-check_steps_support.o'
run('compile-support', cpp + ['check_steps_support.cc', '-o', support])
cflags = ['aarch64-linux-gnu-gcc', '-O1', '-g', '-Wall', '-Wextra', '-Werror', '-DMPC_EXPERIMENTAL_CHOP_EXPORT=1',
          '-Ibuild', '-mbranch-protection=standard']
exe = 'build/check-chop-prototype'
run('compile-native', cflags + ['check_chop_native.c', 'check_chop_support.S',
    'check_steps_support.S', 'src/step_abi.S', support] + objects +
    ['-lstdc++', '-ldl', '-lpthread', '-lm', '-o', exe])
native = run('native', QEMU + [str(R / exe), str(STOCK)])
assert 'PASS actual native CopyPadCommand' in native.stdout
assert 'PASS native SliceData' in native.stdout
assert 'PASS actual ExtractSliceCommand19aaae0 and SampleEditFunctions209c5b0 mode1 no-output failure' in native.stdout
pcm_exe='build/check-chop-pcm'
run('compile-pcm',cflags+['check_chop_pcm.c','check_chop_pcm_support.S','check_steps_support.S',
    'src/step_abi.S',support]+objects+['-lstdc++','-ldl','-lpthread','-lm','-o',pcm_exe])
pcm=run('pcm',QEMU+[str(R/pcm_exe),str(STOCK)])
assert 'PASS actual native TrimProcessingStream' in pcm.stdout
pipeline_exe='build/check-chop-pipeline'
run('compile-pipeline',cflags+['check_chop_pipeline.c','check_chop_pipeline_support.S','check_chop_pcm_support.S','check_steps_support.S',
    'src/step_abi.S',support]+objects+['-lstdc++','-ldl','-lpthread','-lm','-o',pipeline_exe])
pipeline=run('pipeline',QEMU+[str(R/pipeline_exe),str(STOCK)])
assert 'PASS real ExtractSliceCommand19aaae0' in pipeline.stdout
exe = 'build/check-workflow-prototype'
run('compile-ui', cflags + ['check_workflow.c', 'src/workflow_abi.S', 'src/phone_input.c', 'src/browser_tree.c',
    '-Wl,--no-as-needed', '-lstdc++', '-ldl', '-lpthread', '-lm', '-o', exe])
ui = run('ui', QEMU + [str(R / exe), str(STOCK), str(R / 'build/r15-workflow-draw.txt')])
assert 'PASS actual native File' in ui.stdout
sources = ['phone_input.c', 'phone_events.c', 'phone_native.c', 'note_native.c',
           'note_abi.S', 'music_settings.c', 'music_archive.c', 'music_ui.c',
           'music_lighting.c', 'step_native.c', 'step_abi.S', 'workflow_ui.c', 'workflow_abi.S', 'browser_tree.c']
library = R / 'build/phone-input-r15-prototype.so'
run('compile-library', ['aarch64-linux-gnu-gcc', '-shared', '-fPIC', '-O2', '-std=c11',
    '-Wall', '-Wextra', '-Werror', '-Wcast-function-type', '-Ibuild', '-DMPC_EXPERIMENTAL_CHOP_EXPORT=1',
    '-mbranch-protection=standard', '-Wl,-z,noexecstack,-z,relro,-z,now'] +
    ['src/' + s for s in sources] + objects +
    ['-lstdc++', '-ldl', '-pthread', '-lm', '-o', str(library)])
from verify_callbacks import verify
callbacks = verify(library)
with tempfile.TemporaryDirectory(prefix='mpc-chop-prototype-') as folder:
    lib = Path(folder) / 'phone-input.so'
    lib.write_bytes(library.read_bytes())
    loaded = run('load', QEMU + ['-E', 'LD_DEBUG=libs', '-E', 'LD_PRELOAD=' + str(lib),
        str(ROOT / 'usr/bin/busybox.nosuid'), 'true'])
    assert 'calling init: ' + str(lib) in loaded.stderr
    assert 'cannot be preloaded' not in loaded.stderr
frozen = json.loads((R / 'releases/20261024-r14-browser-batch-mute/build-info.json').read_text())
archive = R.parent / frozen['zip']
assert sha256(archive.read_bytes()).hexdigest() == frozen['zip_sha256']
report = {'prototype': 'r15-chop-batch-trial', 'batch_limit': 16,
          'card_written': False, 'hardware_tested': False,
          'pcm_extraction_tested': 'native_crop_over_fixture_PCM_source', 'ready_for_device_trial': True,
          'native_crop_stream_tested': {'stereo_fixture_source':True,'ranges':['middle','full','one_frame'],
              'first_last_frames':True,'seek_and_EOF':True,'owned_metadata_and_destructor':True,
              'real_SamplePool_and_saved_file':False},
          'native_contracts': ['CopyPadCommand ctor/perform/undo incl 16 items and reject17',
              'SliceData ctor/copy/dtor', 'ExtractAndAssignSampleCommand ctor/dtor ABI',
              'ExtractSliceCommand19aaae0 and SampleEditFunctions209c5b0 mode1 no-output failure returns3'],
          'fixtures': ['instrument cloning and Program/registry/audio notifications',
              'extract perform/undo and SamplePool/file creation', 'GUI, dispatch and device lifecycle'],
          'no_output_probe': {'mode': 1, 'native_perform': True, 'native_sample_edit_pipeline': True,
              'audio_source_and_failed_sample_creation': 'fixtures',
              'crop_mode2_and_pcm_success': False, 'output_assigned': False},
          'checks': ['source markers protected', 'slice mapping and source pad protected',
              'group Undo/Redo', 'rollback after second extract failure',
              'rollback of partial extract before assignment', 'UI cap16 across banks/pages, reject17',
              'Chop host: Shift B1 native Extract, B2 Batch, B3 native Split with paired release',
              'ordinary Sample/Copy B3 unchanged, Cancel and Done return to Chop without Copy exit',
              'shared async queue entry, UI progress before scheduling extraction',
              'native ExtractSlice -> SampleEdit -> stereo Trim -> post-processing closure -> pool adapter -> native Undo removal',
              'silent target-assignment failure detected and rolled back',
              'native rootfs library load', 'r14 archive preserved'],
          'callbacks': callbacks, 'library_sha256': sha256(library.read_bytes()).hexdigest()}
(R / 'build/r15-prototype-verification.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
