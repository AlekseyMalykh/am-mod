"""Build a forward-only sample-copy diagnostic over the immutable r27e payload.

Run under WSL in this directory. Does not install or publish anything.
"""
from pathlib import Path
import hashlib
import json
import os
import struct
import subprocess
import sys
import zipfile

R = Path(__file__).resolve().parent
FIX = '--workspace-fix' in sys.argv
O = R / ('build/workspace-fix' if FIX else 'build/sample-load')
O.mkdir(parents=True, exist_ok=True)
REV = 'r27e-workspace-fix1' if FIX else 'r27e-load-diag1'
BASE = R.parent.parent / 'r27e-device-package/am-mod-r27e.zip'
STOCK = R.parent / 'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
SYSROOT = '/home/user/mpc-rootfs-user-20260808'
sha = lambda b: hashlib.sha256(b).hexdigest()


def run(*args, **kwargs):
    return subprocess.run(args, cwd=R, check=True, **kwargs)


def branch(a, b, call=False):
    delta = b - a
    assert delta % 4 == 0 and -(1 << 27) <= delta < (1 << 27)
    return struct.pack('<I', (0x94000000 if call else 0x14000000) | ((delta // 4) & 0x3ffffff))


stock = STOCK.read_bytes()
assert sha(stock) == 'b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
assert sha(BASE.read_bytes()) == 'a6a2cf3e94ee8c0315a11617c6eeab372d05042248fe3bd68efbf1f3ec8e14ec'
with zipfile.ZipFile(BASE) as z:
    old = {n: z.read(n) for n in z.namelist()}
files = dict(old)
manifest = json.loads((R / 'build/r23a/build-info.json').read_text())['patches']
chunks = {}
for p in manifest:
    data = old['autorun/' + p['file']]
    assert len(data) == p['size'] and sha(data) == p['sha256'], p['file']
    chunks[p['file']] = (int(p['offset'], 16), data)

baseline_image = bytearray(stock)
for offset, data in chunks.values():
    baseline_image[offset:offset + len(data)] = data
launcher = old['autorun/mod-launcher.sh'].decode()
assert 'PATCH_SHA=' + sha(baseline_image) + '\n' in launcher

# Append a hook without changing any of the original 50 callback indices.
from phone_build import build
fresh, _ = build(stock, run, branch)
assert next(data for name, _, data in fresh if name == 'phone-code.bin')[:4800] == old['autorun/phone-code.bin'][:4800]
changed_chunks = set()
for name, offset, data in fresh:
    if chunks.get(name) != (offset, data):
        changed_chunks.add(name)
    chunks[name] = (offset, data)
    files['autorun/' + name] = data
assert changed_chunks == {'phone-code.bin', 'phone-table.bin', 'phone-muted-note-gate.bin',
    'phone-muted-sequence-gate.bin', 'phone-muted-sequence_input-gate.bin', 'phone-sample-file-copy.bin'}
ordered = sorted((offset, offset + len(data)) for offset, data in chunks.values())
assert all(a[1] <= b[0] for a, b in zip(ordered, ordered[1:])), 'overlapping native patches'
patched = bytearray(stock)
header = ['#include <stddef.h>', 'struct patch { off_t offset; size_t size; const unsigned char *before,*after; };']
bundle = bytearray()
rows = []
for i, (name, (offset, data)) in enumerate(chunks.items()):
    for prefix, value in [('before', stock[offset:offset + len(data)]), ('after', data)]:
        header.append('static const unsigned char %s%d[]={%s};' % (prefix, i, ','.join('0x%02x' % b for b in value)))
    rows.append('{%d,%d,before%d,after%d}' % (offset, len(data), i, i))
    patched[offset:offset + len(data)] = data
    bundle += struct.pack('<QQ', offset, len(data)) + data
header.append('static const struct patch patches[]={%s};' % ','.join(rows))
assert patched[0x6d43bb0:] == stock[0x6d43bb0:], 'native RW segment must be unchanged'
(O / 'patches.h').write_text('\n'.join(header) + '\n')
(O / 'apply_session.c').write_bytes((R / 'src/apply_session.c').read_bytes())
(O / 'bootstrap-patches.bin').write_bytes(bundle)
(O / 'patched-image-sha256.txt').write_text(sha(patched) + '\n')
run('aarch64-linux-gnu-gcc', '-O2', '-Wall', '-Wextra', '-Werror', '-mbranch-protection=standard',
    '-Wl,-z,noexecstack', str(O / 'apply_session.c'), '-o', str(O / 'apply-session'))
run('aarch64-linux-gnu-gcc', '-O1', '-g', '-Wall', '-Wextra', '-Werror', '-mbranch-protection=standard',
    '-rdynamic', 'check_resample_bootstrap.c', '-ldl', '-lm', '-o', str(O / 'bootstrap'))

tests = {}
for mode, extra in [('normal', []), ('ubsan', ['-fsanitize=undefined', '-static-libubsan', '-fno-sanitize-recover=all'])]:
    exe = O / ('check-trace-' + mode)
    run('aarch64-linux-gnu-gcc', '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
        '-mbranch-protection=standard', *extra, 'check_sample_load_trace.c', 'src/sample_load_trace.c', '-o', str(exe))
    result = run('qemu-aarch64', '-L', SYSROOT, str(exe), capture_output=True, text=True, timeout=30)
    tests[mode] = result.stdout.strip()
    assert result.stdout.startswith('PASS ')
env = dict(os.environ, AM_MOD_BOOTSTRAP_DIR=str(O))
run('python3', 'build_chords_candidate.py', env=env)
from verify_callbacks import verify
library = R / 'build/chords/phone-input.so'
callbacks = verify(library)
assert callbacks['callbacks_verified'] == 51
assert 'SAMPLE_LOAD_TRACE READY ' in (R / 'build/chords/bootstrap.log').read_text()
bridge = run('qemu-aarch64', '-L', SYSROOT, str(O / 'bootstrap'), str(STOCK), str(library),
    str(O / 'bootstrap-patches.bin'), '--sample-copy', capture_output=True, text=True, timeout=45)
(O / 'native-copy-bridge.log').write_text(bridge.stdout + bridge.stderr)
assert 'PASS actual firmware copy bridge' in bridge.stdout
tests['native_copy_bridge'] = bridge.stdout
files['autorun/phone-input.so'] = library.read_bytes()
files['autorun/apply-session'] = (O / 'apply-session').read_bytes()

# Update the original launcher's pinned hashes; leave its storage and boot logic intact.
for previous, current, count in [
    (sha(old['autorun/phone-input.so']), sha(files['autorun/phone-input.so']), 1),
    (sha(old['autorun/apply-session']), sha(files['autorun/apply-session']), 2),
    (sha(baseline_image), sha(patched), 1),
]:
    assert launcher.count(previous) == count
    launcher = launcher.replace(previous, current)
files['autorun/mod-launcher.sh'] = launcher.encode()
entry = old['autorun/autorun.sh'].decode()
assert entry.count(sha(old['autorun/mod-launcher.sh'])) == 1
entry = entry.replace(sha(old['autorun/mod-launcher.sh']), sha(files['autorun/mod-launcher.sh']))
assert entry.count('ENTRY_BEGIN revision=r27e') == 1
entry = entry.replace('ENTRY_BEGIN revision=r27e', 'ENTRY_BEGIN revision=' + REV)
files['autorun/autorun.sh'] = entry.encode()
meta = json.loads(old['autorun/autorun.json'])
meta['value0']['scripts'][0]['message'] = 'AM Mod r27e - sample load diagnostic 1; not a bugfix release'
if FIX:
    meta['value0']['scripts'][0]['message'] = 'AM Mod r27e - temporary sample workspace fix 1; hardware test candidate'
files['autorun/autorun.json'] = (json.dumps(meta, indent=2) + '\n').encode()
files['autorun/autorun.check'] = sha(files['autorun/autorun.json'] + files['autorun/autorun.sh'] + b'f7bed1c9-7e90-4731-9194-c996666b4331').encode()
files['SAMPLE_LOAD_DIAGNOSTIC.ru.md'] = '''# Диагностика загрузки сэмплов — r27e-load-diag1

Это проверочная сборка на основе r27e, не исправление ошибки и не публичный релиз.
Заставка и About сохраняют номер основной версии r27e.

При загрузке сэмпла журнал SAMPLE_LOAD_COPY фиксирует исходный и целевой пути,
результат штатного копирования, наличие файлов и целевого каталога, свободное
место. Первые 4 успешных и первые 32 неуспешных операции за запуск.
Копирование вызывается ровно один раз, его аргументы, результат и errno сохраняются.
Диагностика не создаёт каталоги и не повторяет загрузку.

Проверка на MPC: повторить загрузку того же встроенного сэмпла, записать его имя
и текст ошибки, штатно выключить устройство, подключить карту к ПК для чтения
autorun/MPC_CLONE_TRIAL.txt и autorun/MPC_MOD_EVENTS.txt.
Проверки ARM64 и конструктора не заменяют эту аппаратную проверку.
'''.encode()
if FIX:
    proof = json.loads((R/'build/early-startup/verification.json').read_text())
    assert proof['passed'] and proof['ubsan']
    for name, digest in proof['sources_sha256'].items():
        assert sha((R/name).read_bytes()) == digest, name
    tests['startup_workspace'] = proof
    files['SAMPLE_LOAD_DIAGNOSTIC.ru.md'] = (R.parent/'docs/SAMPLE_WORKSPACE_FIX.ru.md').read_bytes()
allowed = {'autorun/' + n for n in changed_chunks} | {'autorun/phone-input.so', 'autorun/apply-session',
    'autorun/mod-launcher.sh', 'autorun/autorun.sh', 'autorun/autorun.json', 'autorun/autorun.check', 'SAMPLE_LOAD_DIAGNOSTIC.ru.md'}
changed = {n for n in files if files[n] != old.get(n)}
assert changed == allowed, changed ^ allowed
for name in ['autorun.sh', 'mod-launcher.sh']:
    test_script = O / name
    test_script.write_bytes(files['autorun/' + name])
    run('sh', '-n', str(test_script))
package = O / ('am-mod-' + REV + '.zip')
with zipfile.ZipFile(package, 'w', zipfile.ZIP_DEFLATED) as z:
    for name, data in sorted(files.items()):
        z.writestr(name, data)
report = dict(revision=REV, base_revision='r27e', diagnostic_only=not FIX, hardware_tested=False,
    package=package.name, package_sha256=sha(package.read_bytes()),
    payload_sha256={n[8:]: sha(b) for n, b in files.items() if n.startswith('autorun/')},
    changed_files=sorted(changed), tests=tests, callbacks=callbacks,
    native_patch_sha256=sha(patched), source_sha256={n: sha((R/n).read_bytes()) for n in [
        'src/sample_load_trace.c', 'src/sample_load_trace.h', 'check_sample_load_trace.c',
        'phone_build.py', 'build_chords_candidate.py', 'check_resample_bootstrap.c', 'build_sample_load_diagnostic.py',
        'src/startup_project.cc', 'src/startup_project.h', 'src/phone_native.c', 'check_startup_project.cc', 'src/project_recovery.c']})
(O / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(dict(package=str(package), sha256=report['package_sha256'], changed_files=sorted(changed), tests=tests, callbacks=callbacks), indent=2))
