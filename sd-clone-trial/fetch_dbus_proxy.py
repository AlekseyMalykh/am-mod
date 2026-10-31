"""Download official Debian packages to the workspace; install nothing."""
from pathlib import Path
import gzip
import hashlib
import io
import json
import tarfile
import urllib.request

target = Path(__file__).parent / 'vendor' / 'xdg-dbus-proxy'
target.mkdir(parents=True, exist_ok=True)
records = []
for arch in ('arm64', 'amd64'):
    index_url = f'https://deb.debian.org/debian/dists/bookworm/main/binary-{arch}/Packages.gz'
    index = gzip.decompress(urllib.request.urlopen(index_url, timeout=60).read()).decode()
    package = next(p for p in index.split('\n\n') if p.startswith('Package: xdg-dbus-proxy\n'))
    fields = dict(line.split(': ', 1) for line in package.splitlines() if ': ' in line and not line.startswith(' '))
    url = 'https://deb.debian.org/debian/' + fields['Filename']
    data = urllib.request.urlopen(url, timeout=30).read()
    assert hashlib.sha256(data).hexdigest() == fields['SHA256']
    (target / Path(fields['Filename']).name).write_bytes(data)
    assert data[:8] == b'!<arch>\n'
    pos = 8
    while pos < len(data):
        header = data[pos:pos+60]
        name = header[:16].decode().strip().rstrip('/')
        size = int(header[48:58])
        payload = data[pos+60:pos+60+size]
        if name.startswith('data.tar'):
            with tarfile.open(fileobj=io.BytesIO(payload), mode='r:*') as tar:
                for suffix, filename in [('usr/bin/xdg-dbus-proxy', arch + '/xdg-dbus-proxy'),
                                         ('usr/share/doc/xdg-dbus-proxy/copyright', 'COPYRIGHT')]:
                    item = next(m for m in tar.getmembers() if m.name.lstrip('./') == suffix)
                    out = target / filename
                    out.parent.mkdir(parents=True, exist_ok=True)
                    out.write_bytes(tar.extractfile(item).read())
                    if suffix.startswith('usr/bin/'):
                        out.chmod(0o755)
            break
        pos += 60 + size + size % 2
    records.append(dict(arch=arch,version=fields['Version'],url=url,
                        package_sha256=fields['SHA256'],
                        executable_sha256=hashlib.sha256((target / arch / 'xdg-dbus-proxy').read_bytes()).hexdigest()))
(target / 'provenance.json').write_text(json.dumps(records, indent=2) + '\n')
print(json.dumps(records, indent=2))
