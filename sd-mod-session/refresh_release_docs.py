"""Refresh release documentation without changing a verified autorun payload."""
from pathlib import Path, PurePosixPath
import argparse
import hashlib
import json
import sys
import zipfile

R = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(R / 'installer'))
from mpc_mod_installer import Package


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--base-sha256', required=True)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    assert args.base.resolve() != args.output.resolve()
    assert sha(args.base.read_bytes()) == args.base_sha256, 'Unexpected base package'
    original = Package.from_file(args.base)
    with zipfile.ZipFile(args.base) as archive:
        assert archive.testzip() is None
        old = {n: archive.read(n) for n in archive.namelist() if not n.endswith('/')}
    for name in old:
        path = PurePosixPath(name)
        assert not path.is_absolute() and '..' not in path.parts and '\\' not in name
    files = dict(old)
    documents = [R / name for name in ('README.md', 'README.ru.md', 'CHANGELOG.md')]
    documents += sorted((R / 'docs').rglob('*'))
    for path in documents:
        if not path.is_file() or path.suffix.lower() not in ('.md', '.png', '.gif', '.svg'):
            continue
        assert not path.is_symlink() and path.resolve().is_relative_to(R.resolve())
        files[path.relative_to(R).as_posix()] = path.read_bytes()
    files['README_RU.md'] = files['README.ru.md']
    assert {n: b for n, b in files.items() if n.startswith('autorun/')} == {
        n: b for n, b in old.items() if n.startswith('autorun/')
    }
    proof = json.loads((R / 'sd-mod-session/build/r27e/package-verification.json').read_text())
    assert proof['passed']
    for result in proof['proofs'].values():
        assert result['passed']
        for name, expected in result['sources_sha256'].items():
            assert sha((R / 'sd-mod-session' / name).read_bytes()) == expected, name
    assert sha(files['autorun/phone-input.so']) == proof['library_sha256']
    assert sha(files['autorun/boot-splash']) == proof['splash_sha256']
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.output, 'x', zipfile.ZIP_DEFLATED) as archive:
        for name, data in sorted(files.items()):
            archive.writestr(name, data)
    with zipfile.ZipFile(args.output) as archive:
        assert archive.testzip() is None
        for name, data in files.items():
            assert archive.read(name) == data, name
    refreshed = Package.from_file(args.output)
    assert refreshed.revision == original.revision == 'r27e'
    assert refreshed.files == original.files
    report = dict(
        passed=True, revision=refreshed.revision,
        package_sha256=refreshed.sha256, base_package_sha256=original.sha256,
        documentation_only=True, autorun_byte_identical=True,
        payload_files=len(refreshed.files),
        payload_sha256={n: sha(b) for n, b in sorted(refreshed.files.items())},
        library_sha256=proof['library_sha256'], splash_sha256=proof['splash_sha256'],
        documents_sha256={n: sha(b) for n, b in sorted(files.items()) if n.endswith('.md')},
        exact_package_full_vm_boot_tested=False, published=False,
        release_channel='prerelease', compile_and_test_verification=proof,
    )
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: report[k] for k in (
        'passed', 'revision', 'package_sha256', 'documentation_only',
        'autorun_byte_identical', 'payload_files', 'library_sha256')}, indent=2))


if __name__ == '__main__':
    main()
