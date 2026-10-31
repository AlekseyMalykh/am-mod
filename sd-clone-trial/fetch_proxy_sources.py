"""Keep the exact Debian source archives alongside the redistributed proxy."""
from pathlib import Path
import hashlib
import re
import urllib.request

root = Path(__file__).parent / 'vendor/xdg-dbus-proxy/source'
root.mkdir(exist_ok=True)
base = 'https://deb.debian.org/debian/pool/main/x/xdg-dbus-proxy/'
name = 'xdg-dbus-proxy_0.1.4-3+deb12u1.dsc'
descriptor = urllib.request.urlopen(base + name, timeout=30).read()
(root/name).write_bytes(descriptor)
checksums = descriptor.decode().split('Checksums-Sha256:\n', 1)[1].split('\n\n', 1)[0]
for digest, size, filename in re.findall(r'^ ([0-9a-f]{64}) (\d+) (\S+)$', checksums, re.M):
    if not filename.endswith('.tar.xz'):
        continue
    data = urllib.request.urlopen(base + filename, timeout=30).read()
    assert len(data) == int(size)
    assert hashlib.sha256(data).hexdigest() == digest
    (root/filename).write_bytes(data)
    print(filename, len(data), digest)
