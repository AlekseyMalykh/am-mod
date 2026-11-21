"""PGM preflight, not a native Sample project writer.

The byte parser is src/program_pgm.c (shared with the intended device reader).
This host tool resolves companion audio and produces a loss/ambiguity report.
No archive is extracted and no source/project/card is changed.
"""
from __future__ import annotations
import argparse
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import re
import stat
import subprocess
import wave
import zipfile

ROOT = Path(__file__).resolve().parent
MAX_ENTRY = 64 << 20
MAX_TOTAL = 256 << 20
AUDIO_EXTENSIONS = {'.wav', '.snd', '.aif', '.aiff'}


class PreflightError(ValueError):
    pass


def safe_member(name: str) -> str:
    if (not name or '\\' in name or name.startswith('/') or ':' in name
            or any(ord(c) < 32 for c in name)
            or any(part in ('', '.', '..') for part in name.rstrip('/').split('/'))):
        raise PreflightError(f'Unsafe archive member: {name!r}')
    return name.rstrip('/')


class Sources:
    def __init__(self, path: Path, program: str | None = None):
        self.path = path.resolve(strict=True)
        self.archive = None
        self.paths = {}
        try:
            if zipfile.is_zipfile(self.path):
                self.archive = zipfile.ZipFile(self.path)
                infos = self.archive.infolist()
                if len(infos) > 4096 or sum(i.file_size for i in infos) > MAX_TOTAL:
                    raise PreflightError('Archive exceeds preflight size/entry limits')
                for info in infos:
                    name = safe_member(info.filename)
                    if stat.S_ISLNK(info.external_attr >> 16):
                        raise PreflightError(f'Symlink archive member: {name}')
                    if info.is_dir():
                        continue
                    if info.file_size > MAX_ENTRY or name in self.paths:
                        raise PreflightError(f'Oversized or duplicate member: {name}')
                    self.paths[name] = info
                candidates = [n for n in self.paths if PurePosixPath(n).suffix.lower() == '.pgm']
                if program is not None:
                    program = safe_member(program)
                    if program not in candidates:
                        raise PreflightError('Selected PGM is absent from archive')
                    candidates = [program]
                if len(candidates) != 1:
                    raise PreflightError(f'Select exactly one PGM with --program; found {len(candidates)}')
                self.program = candidates[0]
            else:
                if program is not None:
                    raise PreflightError('--program applies only to ZIP input')
                if self.path.suffix.lower() != '.pgm':
                    raise PreflightError('Expected a PGM or ZIP containing PGM')
                self.program = self.path.name
                for p in self.path.parent.iterdir():
                    if p.is_file() and not p.is_symlink() and p.suffix.lower() in AUDIO_EXTENSIONS | {'.pgm'}:
                        self.paths[p.name] = p
                if self.program not in self.paths:
                    raise PreflightError('PGM symlinks are not accepted')
                if len(self.paths) > 4096:
                    raise PreflightError('Too many companion files')
        except BaseException:
            self.close()
            raise

    def close(self):
        if self.archive is not None:
            self.archive.close()
            self.archive = None

    def read(self, name: str, limit=MAX_ENTRY) -> bytes:
        value = self.paths[name]
        size = value.file_size if self.archive else value.stat().st_size
        if size > limit:
            raise PreflightError(f'File exceeds size limit: {name}')
        if self.archive:
            with self.archive.open(value) as stream:
                data = stream.read(limit + 1)
        else:
            with value.open('rb') as stream:
                data = stream.read(limit + 1)
        if len(data) > limit:
            raise PreflightError(f'File exceeds size limit: {name}')
        return data

    def matches(self, sample: str) -> list[str]:
        matches = [name for name in self.paths
                   if PurePosixPath(name).suffix.lower() in AUDIO_EXTENSIONS
                   and PurePosixPath(name).stem.casefold() == sample.casefold()]
        parent = PurePosixPath(self.program).parent
        local = [name for name in matches if PurePosixPath(name).parent == parent]
        # Ambiguity inside the PGM directory is never resolved by taking the
        # exact-case filename, nor by picking WAV ahead of SND silently.
        return sorted(local or matches)


def pad_number(text: str) -> int:
    match = re.fullmatch(r'([A-Ha-h])(0[1-9]|1[0-6])', text)
    if not match:
        raise PreflightError(f'Expected A01..H16: {text}')
    return (ord(match[1].upper()) - ord('A')) * 16 + int(match[2]) - 1


def pad_name(number: int) -> str:
    return f'{chr(65 + number // 16)}{number % 16 + 1:02}'


def audio_info(sources: Sources, name: str) -> dict:
    data = sources.read(name)
    result = {'path': name, 'sha256': hashlib.sha256(data).hexdigest(), 'bytes': len(data)}
    if PurePosixPath(name).suffix.lower() != '.wav':
        return result | {'status': 'unsupported_audio', 'reason': 'Separate SND/AIFF decoder is not implemented'}
    try:
        # Check the declared RIFF boundary before wave tolerates trailing or
        # truncated data. The current reader supports uncompressed PCM WAV.
        if len(data) < 12 or data[:4] != b'RIFF' or data[8:12] != b'WAVE':
            raise ValueError('Expected RIFF WAVE')
        if int.from_bytes(data[4:8], 'little') + 8 > len(data):
            raise ValueError('Truncated RIFF')
        with wave.open(io.BytesIO(data)) as w:
            channels, width, rate, frames = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
            if channels not in (1, 2) or width not in (1, 2, 3, 4) or not 8000 <= rate <= 192000 or frames <= 0:
                raise ValueError('Unsupported channels/width/rate or empty PCM')
            pcm = w.readframes(frames)
            if len(pcm) != frames * channels * width:
                raise ValueError('Truncated PCM')
            return result | {'status': 'pcm_ready', 'channels': channels, 'bits': width * 8,
                             'sample_rate': rate, 'frames': frames, 'seconds': frames / rate}
    except (wave.Error, EOFError, ValueError) as exc:
        return result | {'status': 'invalid_or_unsupported_wav', 'reason': str(exc)}


def inspect(path: Path, reader: Path, program=None, bank='A', occupied=()) -> dict:
    if bank not in 'ABCDEFGH' or len(bank) != 1:
        raise PreflightError('Destination bank must be A..H')
    busy = {pad_number(p) for p in occupied}
    sources = Sources(path, program)
    try:
        raw = sources.read(sources.program, 10756)
        process = subprocess.run([str(reader.resolve())], input=raw, capture_output=True, timeout=15)
        try:
            parsed = json.loads(process.stdout)
        except (ValueError, UnicodeDecodeError) as exc:
            raise PreflightError('PGM reader did not return JSON') from exc
        if process.returncode:
            raise PreflightError(f'PGM rejected: {parsed}')
        first = (ord(bank) - 65) * 16
        issues, audio = [], {}
        for pad in parsed['pads']:
            target = first + pad['index']
            pad['destination'] = pad_name(target) if target < 128 else None
            if target >= 128:
                issues.append({'kind': 'destination_overflow', 'pad': pad['pad']})
            elif target in busy:
                issues.append({'kind': 'occupied_destination', 'pad': pad['pad'], 'destination': pad_name(target)})
            pad['midi_input_notes'] = [n for n, p in enumerate(parsed['pad_for_note']) if p == pad['index']]
            for layer in pad['layers']:
                matches = sources.matches(layer['name'])
                layer['candidates'] = matches
                if len(matches) != 1:
                    kind = 'missing_sample' if not matches else 'ambiguous_sample'
                    issues.append({'kind': kind, 'pad': pad['pad'], 'layer': layer['index'] + 1, 'name': layer['name'], 'candidates': matches})
                    layer['status'] = kind
                else:
                    name = matches[0]
                    if name not in audio:
                        audio[name] = audio_info(sources, name)
                    layer['resolved_path'] = name
                    layer['status'] = audio[name]['status']
                    if layer['status'] != 'pcm_ready':
                        issues.append({'kind': layer['status'], 'pad': pad['pad'], 'sample': name})
        warnings = [
            'Source parameters retain MPC1000 units. No equivalent Sample scaling is assumed.',
            'Both MIDI tables are preserved. Multiple input notes per pad need a router adapter.',
            'Filters, envelopes, mixer routing, FX sends and sliders require separate native mapping.',
            'No Sample project is generated or modified. This is a preflight report, not a completed import.',
        ]
        return {'schema': 'am-mod.program-preflight.v1', 'source': str(sources.path),
                'program_path': sources.program, 'program_sha256': hashlib.sha256(raw).hexdigest(),
                'destination_bank': bank, 'occupied': sorted(pad_name(p) for p in busy),
                'preflight_status': 'blocked' if issues else 'ready_for_native_adapter',
                'native_import_implemented': False, 'program': parsed,
                'audio': list(audio.values()), 'issues': issues, 'warnings': warnings}
    finally:
        sources.close()


def markdown(report: dict) -> str:
    p = report['program']
    lines = ['# Предварительный разбор PGM', '',
             f"Программа: `{report['program_path']}`. Формат: **{p['format']}**.", '',
             f"Назначений: {len(p['pads'])}; найденных файлов: {len(report['audio'])}; проблем: {len(report['issues'])}.", '',
             '**Это отчёт проверки, а не готовый проект Sample. Прошивка и проекты не изменены.**', '',
             '| Пэд → назначение | Слой | Сэмпл | Уровень слоя | Velocity | Tune, cents | MIDI OUT / IN |',
             '|---|---:|---|---:|---|---:|---|']
    for pad in p['pads']:
        for layer in pad['layers']:
            notes = ', '.join(map(str, pad['midi_input_notes'])) or '—'
            lines.append(f"| {pad['pad']} → {pad['destination'] or 'за пределами'} | {layer['index']+1} | {layer['name']} | {layer['level']} | {layer['velocity_low']}–{layer['velocity_high']} | {layer['tune_cents']} | {pad['midi_note']} / {notes} |")
    lines += ['', 'Все параметры исходной программы и обе MIDI-таблицы сохранены в `report.json`.', '',
              'Панорама в исходных единицах MPC1000: 0 — влево, 50 — центр, 100 — вправо. '
              'Перенос громкости и панорамы требует проверки шкал и прослушивания.', '',
              '## Проблемы', '']
    lines += [f'- `{json.dumps(issue, ensure_ascii=False)}`' for issue in report['issues']] or ['Не обнаружены при предварительной проверке.']
    lines += ['', '## Следующий этап', '',
              'Подключить чтение к браузеру Sample и штатной загрузке сэмплов. '
              'Подготовить все назначения до изменения проекта, проверить занятые пэды, '
              'применить одной отменяемой операцией. Затем проверить звук, сохранение и повторную загрузку.', '',
              'Описание исходного формата: https://mybunnyhug.org/fileformats/pgm/', '']
    return '\n'.join(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('source', type=Path)
    ap.add_argument('--reader', type=Path, default=ROOT / 'build/pgm-import/pgm-reader-host')
    ap.add_argument('--program', help='Full PGM member name when ZIP contains several programs')
    ap.add_argument('--bank', choices=list('ABCDEFGH'), default='A')
    ap.add_argument('--occupied', default='', help='Comma-separated occupied destination pads, e.g. A01,B03')
    ap.add_argument('--out', type=Path, required=True, help='New report directory; existing directory is refused')
    args = ap.parse_args()
    try:
        report = inspect(args.source, args.reader, args.program, args.bank,
                         [p.strip() for p in args.occupied.split(',') if p.strip()])
        args.out.mkdir(parents=True, exist_ok=False)
        (args.out / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
        (args.out / 'README_RU.md').write_text(markdown(report), encoding='utf-8')
        print(json.dumps({'status': report['preflight_status'], 'pads': len(report['program']['pads']),
                          'audio': len(report['audio']), 'issues': report['issues'], 'report': str(args.out)}, ensure_ascii=False))
        return 2 if report['issues'] else 0
    except (PreflightError, OSError, zipfile.BadZipFile, subprocess.SubprocessError) as exc:
        ap.exit(2, f'Preflight failed: {exc}\n')


if __name__ == '__main__':
    raise SystemExit(main())
