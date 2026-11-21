"""Host/ARM PGM parser and import-preflight checks; no firmware/SD mutation."""
from pathlib import Path
import argparse
import hashlib
import io
import json
import subprocess
import tempfile
import wave
import zipfile
import inspect_pgm as preflight

R = Path(__file__).resolve().parent
OUT = R / 'build/pgm-import'


def run(args, **kwargs):
    p = subprocess.run(list(map(str, args)), capture_output=True, timeout=90, **kwargs)
    if p.returncode:
        raise RuntimeError(f'{args}: {p.returncode}\n{p.stdout!r}\n{p.stderr!r}')
    return p


def fixture():
    b = bytearray(10756)
    b[:2] = len(b).to_bytes(2, 'little')
    b[4:20] = b'MPC1000 PGM 1.00'
    b[24:28] = b'Kick'
    b[41], b[43], b[24+0x8f], b[24+0x90] = 85, 127, 100, 50
    b[0x2958:0x29d8] = bytes([64]) * 128
    b[0x2918] = 37
    b[0x2958+37] = 0
    b[0x2958+50] = 0
    return bytes(b)


def wav():
    stream = io.BytesIO()
    with wave.open(stream, 'wb') as w:
        w.setparams((1, 2, 44100, 0, 'NONE', 'not compressed'))
        w.writeframes(b'\0\x10' * 441)
    return stream.getvalue()


def check_preflight(reader):
    checks = []
    with tempfile.TemporaryDirectory(prefix='pgm-preflight-') as temporary:
        root = Path(temporary)

        def report(entries, **kwargs):
            archive = root / 'kit.zip'
            with zipfile.ZipFile(archive, 'w') as z:
                for name, data in entries:
                    z.writestr(name, data)
            before = hashlib.sha256(archive.read_bytes()).hexdigest()
            result = preflight.inspect(archive, reader, **kwargs)
            assert hashlib.sha256(archive.read_bytes()).hexdigest() == before
            return result

        normal = [('Kit/kit.PGM', fixture()), ('Kit/KICK.WAV', wav())]
        result = report(normal, bank='H')
        pad = result['program']['pads'][0]
        assert not result['issues'] and pad['destination'] == 'H01' and pad['midi_input_notes'] == [37, 50]
        assert result['audio'][0]['frames'] == 441 and not result['native_import_implemented']
        checks.append('ZIP names differing in case resolve; audio frames and both MIDI aliases retained; no source writes')
        result = report(normal, occupied=['A01'])
        assert result['issues'][0]['kind'] == 'occupied_destination'
        result = report(normal[:1])
        assert result['issues'][0]['kind'] == 'missing_sample'
        result = report(normal + [('Kit/Kick.wav', wav())])
        assert result['issues'][0]['kind'] == 'ambiguous_sample'
        result = report(normal + [('Elsewhere/Kick.wav', wav())])
        assert not result['issues']  # local directory takes precedence
        result = report([normal[0], ('Nested/Kick.wav', wav())])
        assert not result['issues']  # unique archive-wide fallback
        result = report([normal[0], ('Kit/Kick.snd', b'not a WAV')])
        assert result['issues'][0]['kind'] == 'unsupported_audio'
        result = report([normal[0], ('Kit/Kick.wav', wav()[:-10])])
        assert result['issues'][0]['kind'] == 'invalid_or_unsupported_wav'
        checks.append('Occupied pads, missing and ambiguous files, unsupported SND and truncated WAV block preflight')
        for entries in [normal + [('../escaped.wav', wav())],
                        normal + [('/absolute.wav', wav())],
                        normal + [('Kit/other.PGM', fixture())]]:
            try:
                report(entries)
            except preflight.PreflightError:
                pass
            else:
                raise AssertionError('Unsafe/ambiguous archive accepted')
        result = report(normal + [('Kit/other.PGM', fixture())], program='Kit/other.PGM')
        assert not result['issues']
        checks.append('Traversal, absolute paths and multiple PGM selection refused; explicit program selection works')
        # Sparse programs can start at H if their occupied pads fit. A later
        # source pad must block, rather than wrap into an existing bank.
        sparse = bytearray(fixture());sparse[24+16*164:24+16*164+4] = b'Snar'
        o = 24+16*164;sparse[o+17], sparse[o+19] = 100, 127
        result = report([('Kit.pgm', sparse), ('Kick.wav', wav()), ('Snar.wav', wav())], bank='H')
        assert any(i['kind'] == 'destination_overflow' for i in result['issues'])
        (root/'Kit.PGM').write_bytes(fixture());(root/'Kick.WAV').write_bytes(wav())
        result = preflight.inspect(root/'Kit.PGM', reader)
        assert not result['issues']
        checks.append('Sparse destination bounds and loose PGM with companion WAV checked')
    return checks


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--fixture', type=Path, help='Optional user ZIP; never copied into source repository')
    args = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    sources = ['src/program_pgm.c', 'src/program_pgm.h', 'check_program_pgm.c',
               'pgm_reader.c', 'inspect_pgm.py', 'verify_program_pgm.py']
    results = []
    for arch, cc, prefix in [('host', 'gcc', []), ('arm', 'aarch64-linux-gnu-gcc',
        ['qemu-aarch64', '-L', '/usr/aarch64-linux-gnu'])]:
        for sanitize in (False, True):
            target = OUT / f'check-{arch}{"-ubsan" if sanitize else ""}'
            flags = ['-fsanitize=undefined', '-fno-sanitize-recover=all'] if sanitize else []
            run([cc, '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', *flags,
                 'check_program_pgm.c', 'src/program_pgm.c', '-o', target], cwd=R)
            output = run([*prefix, target]).stdout.decode()
            assert output.startswith('PASS')
            (OUT / (target.name + '.log')).write_text(output, encoding='utf-8')
            results.append({'check': target.name, 'passed': True})
        reader = OUT / f'pgm-reader-{arch}'
        run([cc, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', 'pgm_reader.c',
             'src/program_pgm.c', '-o', reader], cwd=R)
        decoded = json.loads(run([*prefix, reader], input=fixture()).stdout)
        assert decoded['pads'][0]['midi_note'] == 37
    results += [{'check': s, 'passed': True} for s in check_preflight(OUT/'pgm-reader-host')]
    fixture_report = None
    if args.fixture:
        report = preflight.inspect(args.fixture, OUT/'pgm-reader-host')
        assert not report['issues'], report['issues']
        src = preflight.Sources(args.fixture)
        try:
            raw = src.read(src.program)
        finally:
            src.close()
        host = run([OUT/'pgm-reader-host'], input=raw).stdout
        arm = run(['qemu-aarch64', '-L', '/usr/aarch64-linux-gnu', OUT/'pgm-reader-arm'], input=raw).stdout
        assert host == arm
        fixture_report = {'archive_sha256': hashlib.sha256(args.fixture.read_bytes()).hexdigest(),
                          'program_sha256': report['program_sha256'],
                          'pads': len(report['program']['pads']), 'audio': len(report['audio']),
                          'host_arm_identical': True}
        (OUT/'503-BD-Tape-report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
        (OUT/'503-BD-Tape-report_RU.md').write_text(preflight.markdown(report), encoding='utf-8')
        results.append({'check': 'User PGM: host/ARM identical and all companion PCM WAV validated', 'passed': True})
    result = {'passed': True, 'checks': results, 'fixture': fixture_report,
              'firmware_modified': False, 'hardware_tested': False,
              'native_import_implemented': False,
              'sources_sha256': {s: hashlib.sha256((R/s).read_bytes()).hexdigest() for s in sources}}
    (OUT/'verification.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
