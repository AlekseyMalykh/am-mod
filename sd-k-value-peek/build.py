"""Build a separate SD trial from the frozen, physically validated loader."""
from pathlib import Path
from hashlib import sha256
import json
import struct
import zipfile

ROOT = Path(__file__).resolve().parent
WORKSPACE = ROOT.parent
BASE = WORKSPACE / 'sd-clone-trial'
STOCK = WORKSPACE / 'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
STOCK_SHA = 'b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
LOADER_SHA = '249de24030e52e0333b50c3803212789e26633381e05a579fc555e393dde9b1c'
CODE_OFFSET = 0x06d3b000
HOOK_OFFSET = 0x016541a4  # Preserve the original entry BTI at 0x16541a0.
ROTATION_CALL = 0x03b7d360
ROTATION_HOOK = CODE_OFFSET + 0x124
COLOUR_CALL = 0x015cdeb4
COLOUR_HOOK = CODE_OFFSET + 0x200


def digest(data):
    return sha256(data).hexdigest()


def build():
    stock = STOCK.read_bytes()
    assert digest(stock) == STOCK_SHA
    source = (BASE / 'src/autorun.sh').read_bytes().replace(b'\r\n', b'\n')
    assert digest(source) == LOADER_SHA, 'Validated loader changed; review required'
    validation = json.loads((BASE / 'hardware-validation.json').read_text(encoding='utf-8'))
    assert validation['payload_sha256']['autorun.sh'] == LOADER_SHA
    code = (ROOT / 'build/peek.bin').read_bytes()
    assert 0 < len(code) <= 0x1000
    assert struct.unpack_from('<I', code, 0x1c)[0] == 0xaa0003f3, 'r2 must use screen directly: mov x19,x0'
    assert not any(stock[0x6d3aff8:0x6d43bb0])
    assert struct.unpack_from('<IIQQQQQQ', stock, 0xb0) == (
        1, 5, 0, 0, 0, 0x6d3aff8, 0x6d3aff8, 0x10000)
    assert stock[0x16541a0:0x16541c4].hex() == (
        '5f2403d541000037c0035fd6018440f9023893d20000028b2108409121804a39b4b4fd17')
    assert struct.unpack_from('<I', stock, ROTATION_CALL)[0] == (
        0x94000000 | (((0xff9370 - ROTATION_CALL) // 4) & 0x3ffffff))
    assert code[0x124:0x128] == bytes.fromhex('5f2403d5')
    assert code[0x200:0x204] == bytes.fromhex('5f2403d5')
    assert struct.unpack_from('<I', stock, COLOUR_CALL)[0] == (
        0x94000000 | (((0x1131fa0 - COLOUR_CALL) // 4) & 0x3ffffff))
    distance = CODE_OFFSET - HOOK_OFFSET
    assert distance % 4 == 0 and -(1 << 27) <= distance < (1 << 27)
    chunks = [
        ('peek-code.bin', CODE_OFFSET, code),
        ('peek-branch.bin', HOOK_OFFSET, struct.pack('<I', 0x14000000 | ((distance // 4) & 0x3ffffff))),
        ('peek-rotation.bin', ROTATION_CALL, struct.pack('<I', 0x94000000 | (((ROTATION_HOOK - ROTATION_CALL) // 4) & 0x3ffffff))),
        ('peek-colour.bin', COLOUR_CALL, struct.pack('<I', 0x94000000 | (((COLOUR_HOOK - COLOUR_CALL) // 4) & 0x3ffffff))),
        ('peek-phdr.bin', 0xd0, struct.pack('<QQ', CODE_OFFSET + len(code), CODE_OFFSET + len(code))),
    ]
    patched = bytearray(stock)
    for _, offset, payload in chunks:
        patched[offset:offset + len(payload)] = payload
    patched_sha = digest(patched)
    out = ROOT / 'autorun'
    out.mkdir(exist_ok=True)
    for name in ('xdg-dbus-proxy', 'proxy-policy.args'):
        data = (BASE / 'autorun' / name).read_bytes()
        assert digest(data) == validation['payload_sha256'][name]
        (out / name).write_bytes(data)
    helper = (ROOT / 'build/apply-peek').read_bytes()
    if (out / 'apply-peek').exists():
        (out / 'apply-peek').chmod(0o700)
    (out / 'apply-peek').write_bytes(helper)
    injection = ['# Apply only to the just-verified copy in /run, before stopping stock.',
                 'chmod 700 "$R/MPC-Sample"',
                 f'[ "$(hash "$DIR/apply-peek")" = "{digest(helper)}" ] || fail WRONG_PEEK_HELPER',
                 'cp "$DIR/apply-peek" "$R/apply-peek"',
                 'chmod 500 "$R/apply-peek"',
                 f'[ "$(hash "$R/apply-peek")" = "{digest(helper)}" ] || fail PEEK_HELPER_COPY_MISMATCH',
                 '"$R/apply-peek" >> "$R/events.txt" 2>&1 || fail PEEK_PATCH_FAILED']
    for name, offset, payload in chunks:
        (out / name).write_bytes(payload)
    injection += ['chmod 500 "$R/MPC-Sample"',
                  '[ "$(hash "$R/MPC-Sample")" = "$PATCH_SHA" ] || fail PEEK_HASH_MISMATCH',
                  'log "K_VALUE_PEEK_PATCH_VERIFIED sha256=$PATCH_SHA"']
    text = source.decode('utf-8')
    text = text.replace('# MPC Sample 1.3.0.12: one-shot, unmodified clone trial. No firmware tool.',
                        '# MPC Sample 1.3.0.12: experimental K-value peek in a RAM copy.')
    text = text.replace('PROXY_SECONDS=330\n', f'PROXY_SECONDS=330\nPATCH_SHA={patched_sha}\n')
    needle = '    [ "$(hash "$STOCK")" = "$STOCK_SHA" ] || exit 1\n'
    assert text.count(needle) == 2
    # The first check belongs to clone mode; the second stock-mode check stays stock.
    text = text.replace(needle, needle.replace('$STOCK_SHA', '$PATCH_SHA'), 1)
    text = text.replace("echo 'STARTING_UNMODIFIED_COPY (process startup is not UI readiness)'",
                        "echo 'STARTING_K_VALUE_PEEK_COPY (experimental UI feature; not hardware validated yet)'")
    text = text.replace('log COPY_HASH_VERIFIED\n', 'log COPY_HASH_VERIFIED\n' + '\n'.join(injection) + '\n')
    text = text.replace('Description=MPC unmodified clone trial', 'Description=MPC K-value peek RAM trial')
    assert '  stock)\n    # Used by a RAM-only ac50 drop-in. Deliberately skips firmware-tool.\n' + needle in text
    assert 'conv=notrunc' not in text
    assert text.count('exec setarch -R -- "$STOCK"') == 2
    (out / 'autorun.sh').write_bytes(text.encode('utf-8'))
    manifest = {'value0': {'scripts': [{
        'path': 'autorun.sh', 'askQuestion': True,
        'message': 'Try K-value peek r3 for 3 minutes? Save first. On Sample: encoder selection shows yellow; editing shows white, for about 2 seconds. Stock returns automatically.',
        'yesButtonText': 'Try K peek', 'noButtonText': 'Cancel',
    }], 'files': []}}
    raw_json = (json.dumps(manifest, indent=2) + '\n').encode('ascii')
    (out / 'autorun.json').write_bytes(raw_json)
    salt = b'f7bed1c9-7e90-4731-9194-c996666b4331'
    (out / 'autorun.check').write_text(digest(raw_json + text.encode() + salt), encoding='ascii')
    archive = WORKSPACE / 'mpc-sample-k-value-peek-trial.zip'
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as z:
        for path in sorted(out.iterdir()):
            z.write(path, 'autorun/' + path.name)
        z.write(ROOT / 'README_RU.md', 'README_RU.md')
        z.write(BASE / 'MPC_TRIAL_STATUS/READY.wav', 'MPC_TRIAL_STATUS/READY.wav')
        for path in sorted((BASE / 'vendor/xdg-dbus-proxy').rglob('*')):
            if path.is_file() and 'arm64' not in path.parts:
                z.write(path, 'licenses/xdg-dbus-proxy/' + path.relative_to(BASE / 'vendor/xdg-dbus-proxy').as_posix())
    summary = {
        'revision': 'r3-yellow-preview-white-edit',
        'feature': 'Sample: focused encoder preview yellow 2000ms; edited value native white 2001ms',
        'hardware_tested': False,
        'base_loader_sha256': LOADER_SHA,
        'stock_sha256': STOCK_SHA,
        'patched_copy_sha256': patched_sha,
        'zip': archive.name,
        'zip_sha256': digest(archive.read_bytes()),
        'patches': [{'file': n, 'offset': hex(o), 'size': len(p), 'sha256': digest(p)} for n, o, p in chunks],
        'payload_sha256': {p.name: digest(p.read_bytes()) for p in sorted(out.iterdir())},
        'limits_seconds': {'clone': 180, 'supervisor': 210, 'guard': 240},
    }
    (ROOT / 'build-info.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    build()
