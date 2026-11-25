"""Loopback tests of the production bounded HTTP server; no device FPS claim."""
from pathlib import Path
import hashlib, json, socket, subprocess, tempfile, time, urllib.request

R = Path(__file__).resolve().parent
O = R / 'build/mirror-http'
O.mkdir(parents=True, exist_ok=True)
tracked = ['src/screen_mirror.c', 'src/screen_mirror_frame.h', __file__.split('/')[-1]]
before = {s: hashlib.sha256((R / s).read_bytes()).hexdigest() for s in tracked}
SIZE = 54 + 320 * 240 * 3


def record(n):
    return b'BM' + bytes([n]) * (SIZE - 2)


def exact(s, n):
    out = bytearray()
    while len(out) < n:
        b = s.recv(min(997, n - len(out)))
        assert b, ('premature close', len(out), n)
        out.extend(b)
    return bytes(out)


with tempfile.TemporaryDirectory() as tmp:
    root = Path(tmp)
    flag, frame = root / 'on', root / 'frame.bmp'
    flag.touch()

    def publish(n):
        staging = root / 'tmp'
        staging.write_bytes(record(n))
        staging.replace(frame)

    publish(1)
    with socket.socket() as reserve:
        reserve.bind(('127.0.0.1', 0))
        port = reserve.getsockname()[1]
    exe = O / 'server'
    subprocess.run(['gcc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-DMIRROR_ADDRESS="127.0.0.1"', f'-DMIRROR_PORT={port}',
                    'src/screen_mirror.c', '-o', str(exe)], cwd=R, check=True)
    process = subprocess.Popen([str(exe), str(flag), str(frame)])
    base = f'http://127.0.0.1:{port}'

    def get(path):
        with urllib.request.urlopen(base + path, timeout=2) as r:
            return r.read()

    def stream():
        s = socket.create_connection(('127.0.0.1', port), timeout=2)
        # Request-line fragmentation is intentional.
        s.sendall(b'GET /str')
        time.sleep(.02)
        s.sendall(b'eam HTTP/1.1\r\nHost: localhost\r\n\r\n')
        header = bytearray()
        while not header.endswith(b'\r\n\r\n'):
            header.extend(exact(s, 1))
        assert b'200 OK' in header and b'application/octet-stream' in header
        return s

    try:
        deadline = time.monotonic() + 3
        while True:
            try:
                page = get('/')
                break
            except OSError:
                assert time.monotonic() < deadline
                time.sleep(.02)
        assert b"fetch('/stream'" in page and b'setTimeout(update,80)' in page
        (O / 'page.html').write_bytes(page)
        assert get('/frame.bmp') == record(1)
        s = stream()
        assert exact(s, SIZE) == record(1)
        publish(2)
        assert exact(s, SIZE) == record(2)
        # Quiet screen heartbeat keeps JS connection watchdog alive.
        assert exact(s, SIZE) == record(2)
        slow = stream()  # Do not read its body.
        changed_start = time.monotonic()
        for n in range(3, 33):
            publish(n)
            assert get('/frame.bmp') == record(n)
            # No partial-frame overwrite even while a second browser stalls.
            received = exact(s, SIZE)
            assert received == record(n)
        changed_seconds = time.monotonic() - changed_start
        partial = socket.create_connection(('127.0.0.1', port), timeout=2)
        partial.sendall(b'GET /')
        time.sleep(.35)
        assert partial.recv(1) == b''
        partial.close()
        assert get('/') == page
        # Turning Off closes streams and listener; next On starts cleanly.
        flag.unlink()
        time.sleep(.15)
        for client in (s, slow):
            while client.recv(SIZE):
                pass
            client.close()
        flag.touch()
        time.sleep(.3)
        assert get('/frame.bmp') == record(32)
        frame.write_bytes(b'BMbroken')
        try:
            get('/frame.bmp')
            raise AssertionError('truncated frame accepted')
        except urllib.error.HTTPError as e:
            assert e.code == 503
    finally:
        process.terminate()
        process.wait(timeout=3)
subprocess.run(['aarch64-linux-gnu-gcc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                '-c', 'src/screen_mirror.c', '-o', str(O / 'server-arm.o')], cwd=R, check=True)
assert before == {s: hashlib.sha256((R / s).read_bytes()).hexdigest() for s in tracked}
(O / 'verification.json').write_text(json.dumps(dict(passed=True, sources_sha256=before,
    concurrent_clients=True, fragmented_requests=True, slow_client=True,
    disable_reenable=True, changed_frames=30, changed_seconds=changed_seconds,
    loopback_fps=30/changed_seconds, hardware_tested=False, browser_tested=False), indent=2) + '\n')
print('PASS stream framing, heartbeat, simultaneous legacy HTTP, slow/partial clients, Off/On, truncated frame and ARM codegen')
