"""Execute the launcher's actual bounded proxy wait with a delayed Unix socket."""
from pathlib import Path
import hashlib, json, socket, subprocess, tempfile, threading, time

R = Path(__file__).resolve().parent
O = R / 'build/startup-proxy-wait'
O.mkdir(parents=True, exist_ok=True)
source = (R / 'src/autorun.template.sh').read_text()
wait = source[source.index('proxy_wait=0\n'):source.index('\nstat -Lc', source.index('proxy_wait=0\n'))]
assert '"$proxy_wait" -lt 50' in wait and 'sleep 0.1' in wait
subprocess.run(['sh', '-n', str(R / 'src/autorun.template.sh')], check=True)
with tempfile.TemporaryDirectory() as tmp:
    root = Path(tmp)
    (root / 'proxy').mkdir()
    with socket.socket(socket.AF_UNIX) as server:
        def delayed():
            time.sleep(.12)
            server.bind(str(root / 'proxy/system_bus_socket'))
        worker = threading.Thread(target=delayed)
        worker.start()
        start = time.monotonic()
        subprocess.run(['sh', '-c', 'R="$1"; fail(){ exit 11; };\n' + wait, 'test', tmp], check=True, timeout=2)
        elapsed = time.monotonic() - start
        worker.join()
        assert .12 <= elapsed < .9, elapsed
    (root / 'proxy/system_bus_socket').unlink()
    # Exercise timeout without spending five wall-clock seconds. Only sleep
    # is replaced; the actual launcher loop and failure branch execute.
    p = subprocess.run(['sh', '-c', 'R="$1"; fail(){ echo "$1"; exit 11; }; '
        'sleep(){ echo "$1" >> "$R/delays"; };\n' + wait, 'test', tmp], capture_output=True, text=True, timeout=2)
    assert p.returncode == 11 and p.stdout.strip() == 'PROXY_SOCKET_MISSING'
    assert (root / 'delays').read_text().splitlines() == ['0.1'] * 50
(O / 'verification.json').write_text(json.dumps(dict(passed=True,
    sources_sha256={'src/autorun.template.sh': hashlib.sha256((R / 'src/autorun.template.sh').read_bytes()).hexdigest()},
    delayed_socket_seconds=elapsed, timeout_seconds=5, hardware_tested=False), indent=2) + '\n')
print('PASS real proxy socket readiness and unchanged five-second timeout; launcher shell syntax')
