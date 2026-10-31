"""Real systemd integration tests in disposable WSL /run paths.

The hardware app is replaced by the native sleep ELF. The device script itself
is never executed against ac50 or a physical MPC. No firmware code is run here.
"""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time
import sys

SOURCE = Path(__file__).parent / 'src' / 'autorun.sh'
RUN = Path('/run/mpc-clone-lab-tests')
RESULTS = []


def cmd(*args, check=True):
    return subprocess.run(args, text=True, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, check=check)


def state(unit, key):
    return cmd('systemctl', 'show', unit, '-p', key, '--value', check=False).stdout.strip()


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def wait_for(predicate, seconds=20):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        if predicate():
            return
        time.sleep(.15)
    raise AssertionError('Timed out')


def scenario(name, injection='', full_duration=False):
    prefix = 'mpc-clone-lab-' + name
    root = RUN / name
    runtime = Path('/run') / prefix
    stockunit = prefix + '-stock.service'
    stockfile = Path('/run/systemd/system') / stockunit
    root.mkdir(parents=True)
    diskfixture = Path('/var/lib') / prefix
    diskfixture.mkdir()
    sentinel = diskfixture / 'sentinel'
    sentinel.write_text('ORIGINAL\n')
    autorun = root / 'autorun'
    autorun.mkdir()
    native_proxy = SOURCE.parent.parent / 'vendor/xdg-dbus-proxy/amd64/xdg-dbus-proxy'
    shutil.copyfile(native_proxy, autorun / 'xdg-dbus-proxy')
    shutil.copyfile(SOURCE.parent.parent / 'proxy-policy.args', autorun / 'proxy-policy.args')
    app = root / 'stock-app'
    shutil.copyfile('/usr/bin/sleep', app)
    app.chmod(0o500)
    stockfile.write_text('[Service]\nType=exec\nExecStart=' + str(app) + ' 300\nRestart=always\n')
    sha = hashlib.sha256(app.read_bytes()).hexdigest()
    unitsha = hashlib.sha256(stockfile.read_bytes()).hexdigest()
    text = SOURCE.read_text()
    text = text.replace('PROXY_SHA=751a39dd6c4ce06a11839f454607319ea6d90eb334b481233fa6cf6968fc2689',
                        'PROXY_SHA=' + hashlib.sha256(native_proxy.read_bytes()).hexdigest())
    text = text.replace('R=/run/mpc-clone-trial-v1', 'R=' + str(runtime))
    text = text.replace('U=mpc-clone-trial-v1', 'U=' + prefix)
    text = text.replace("STOCK='/usr/bin/MPC Sample'", "STOCK='" + str(app) + "'")
    text = text.replace('STOCK_UNIT=ac50.service', 'STOCK_UNIT=' + stockunit)
    text = text.replace('ac50.service.d', stockunit + '.d')
    text = text.replace('/usr/lib/systemd/system/ac50.service', str(stockfile))
    text = text.replace('b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508', sha)
    text = text.replace('6b2dc52826263daf19c6f1a945c94bd7ba69b15eb7843df68eebdd63ea311e5d', unitsha)
    text = text.replace('case "$DIR" in /media/*/autorun) ;; *) exit 1 ;; esac', ': # lab SD path')
    text = text.replace('awk -v p="$SD"', ': # awk -v p="$SD"')
    text = text.replace('[ "$(uname -m)" = aarch64 ] || fail WRONG_ARCHITECTURE', ': # native lab ELF')
    text = text.replace('cp /usr/bin/busybox.nosuid', 'cp /usr/bin/true')
    text = text.replace('exec setarch -R -- "$STOCK"', 'exec "$STOCK" 300')
    # /run is hidden inside sandbox, so give stockfile a stable visible path.
    text = text.replace('$R/MPC-Sample:"$STOCK"', '$R/MPC-Sample:"/run/mpc-trial-app/Stock Copy"')
    text = text.replace('/usr/bin/launch-ac50', '/usr/bin/true')
    text = text.replace('[ "$(hash "$STOCK")" = "$STOCK_SHA" ] || exit 1\n    echo',
                        '[ "$(hash "/run/mpc-trial-app/Stock Copy")" = "$STOCK_SHA" ] || exit 1\n    echo')
    text = text.replace('echo \'STARTING_UNMODIFIED_COPY (process startup is not UI readiness)\'',
                        'echo \'STARTING_UNMODIFIED_COPY (process startup is not UI readiness)\'\n    STOCK="/run/mpc-trial-app/Stock Copy"')
    text = text.replace('sleep 5\n', 'sleep 1\n').replace('sleep 2\n', 'sleep 0.2\n')
    if not full_duration:
        text = text.replace('CLONE_SECONDS=180', 'CLONE_SECONDS=3')
        text = text.replace('CLONE_RUNTIME_EXPECTED=3min', 'CLONE_RUNTIME_EXPECTED=3s')
        text = text.replace('SUPERVISOR_SECONDS=210', 'SUPERVISOR_SECONDS=8')
        text = text.replace('GUARD_SECONDS=240', 'GUARD_SECONDS=12')
        text = text.replace('PROXY_SECONDS=330', 'PROXY_SECONDS=60')
    text = text.replace('echo PROBE_OK',
                        'if (printf MUTATED > ' + str(sentinel) + ') 2>/dev/null; then exit 99; fi\n'
                        '    echo PERSISTENT_WRITE_DENIED\n    echo PROBE_OK')
    if injection == 'wrong_hash':
        text = text.replace('STOCK_SHA=' + sha, 'STOCK_SHA=' + '0' * 64)
    elif injection == 'probe_fail':
        text = text.replace('echo PROBE_OK', 'exit 44 # forced failure')
    elif injection == 'clone_exit':
        text = text.replace('STOCK="/run/mpc-trial-app/Stock Copy"', 'STOCK="/run/mpc-trial-app/Stock Copy"\n    exit 42')
    elif injection == 'broken_namespace':
        text = text.replace('ProtectSystem=strict', 'ProtectSystem=no')
    elif injection == 'bad_proxy_hash':
        text = text.replace('PROXY_SHA=' + hashlib.sha256(native_proxy.read_bytes()).hexdigest(), 'PROXY_SHA=' + '0' * 64)
    elif injection == 'systemd_leak':
        policy_path = autorun / 'proxy-policy.args'
        old_hash = hashlib.sha256(policy_path.read_bytes()).hexdigest()
        policy_path.write_text(policy_path.read_text() + '--talk=org.freedesktop.systemd1\n')
        text = text.replace('POLICY_SHA=' + old_hash, 'POLICY_SHA=' + hashlib.sha256(policy_path.read_bytes()).hexdigest())
    script = autorun / 'autorun.sh'
    script.write_text(text)
    started = time.monotonic()
    units = [prefix + '-' + s + '.service' for s in ('probe', 'clone', 'supervisor', 'recover', 'bus-proxy')]
    try:
        cmd('systemctl', 'daemon-reload')
        cmd('systemctl', 'start', stockunit)
        original = state(stockunit, 'MainPID')
        result = cmd('sh', str(script), check=False)
        if injection in ('wrong_hash', 'probe_fail', 'broken_namespace', 'bad_proxy_hash', 'systemd_leak'):
            check(result.returncode != 0, 'Failure must refuse activation')
            check(state(stockunit, 'MainPID') == original, 'Preflight failure stopped stock')
            events = (runtime / 'events.txt').read_text()
            check('STOCK_STOP_REQUESTED' not in events, 'Stop requested on rejection')
            check((root / 'MPC_TRIAL_STATUS/STOPPED BEFORE TEST.wav').exists(), 'Missing rejection marker')
            check(not (root / 'MPC_TRIAL_STATUS/TEST ENDED.wav').exists(), 'False completion on rejection')
            if injection == 'probe_fail':
                check('DISK_MOUNTS_READ_ONLY=yes' in (runtime / 'app/probe.txt').read_text(), 'Probe did not reach injected failure')
            if injection == 'systemd_leak':
                check('PROXY_FAILED: systemd_access_allowed' in (runtime / 'app/probe.txt').read_text(), 'Did not detect systemd leakage')
        else:
            check(result.returncode == 0, result.stdout + '\n' + (runtime / 'events.txt').read_text())
            wait_for(lambda: 'STOCK_STOP_REQUESTED' in (runtime / 'events.txt').read_text())
            if injection == 'supervisor_kill':
                cmd('systemctl', 'kill', '--signal=SIGKILL', '--kill-whom=main', prefix + '-supervisor.service')
            elif injection == 'timer_only':
                # Remove ExecStopPost only in the lab, then simulate launcher death.
                servicefile = Path('/run/systemd/system') / (prefix + '-supervisor.service')
                servicefile.write_text('\n'.join(l for l in servicefile.read_text().splitlines() if not l.startswith('ExecStopPost=')) + '\n')
                cmd('systemctl', 'daemon-reload')
                cmd('systemctl', 'kill', '--signal=SIGKILL', '--kill-whom=main', prefix + '-supervisor.service')
            wait_for(lambda: 'DONE:' in (runtime / 'events.txt').read_text(), 270 if full_duration else 24)
            events = (runtime / 'events.txt').read_text()
            check('STOCK_PROCESS_RESTORED' in events, events)
            check(state(stockunit, 'ActiveState') == 'active', 'Stock not active')
            check(state(prefix + '-clone.service', 'MainPID') == '0', 'Clone remains alive')
            wait_for(lambda: state(prefix + '-bus-proxy.service', 'MainPID') == '0')
            check(state(stockunit, 'MainPID') != original, 'No actual process switch')
            wait_for(lambda: (root / 'MPC_TRIAL_STATUS/TEST ENDED.wav').exists())
            check(list((root / 'MPC_TRIAL_STATUS').glob('*.wav')) == [root / 'MPC_TRIAL_STATUS/TEST ENDED.wav'], 'Stale status markers')
            if not injection:
                check('CLONE_PROCESS_VERIFIED' in events, 'Copy inode never observed')
            if full_duration:
                import datetime
                event_lines = events.splitlines()
                def timestamp(marker):
                    line = next(line for line in event_lines if marker in line)
                    return datetime.datetime.fromisoformat(line.split(' ',1)[0].replace('Z','+00:00'))
                elapsed = (timestamp('RECOVERY_BEGIN')-timestamp('CLONE_START_REQUESTED')).total_seconds()
                check(178 <= elapsed <= 195, 'Unexpected clone lifetime: '+str(elapsed))
                check(state(prefix+'-clone.service','Result') == 'timeout', 'Clone did not reach intended time limit')
            # Once-per-boot latch: repeat must not stop restored process.
            restored = state(stockunit, 'MainPID')
            again = cmd('sh', str(script), check=False)
            check(again.returncode != 0 and state(stockunit, 'MainPID') == restored, 'Repeat not refused')
            check('PERSISTENT_WRITE_DENIED' in (runtime / 'app/probe.txt').read_text(), 'Real write refusal not checked')
        check(sentinel.read_text() == 'ORIGINAL\n', 'Disk fixture was modified')
        RESULTS.append(dict(test=name, status='PASS', seconds=round(time.monotonic()-started, 2), events=events))
    except Exception as exc:
        details = cmd('journalctl', '-u', prefix + '-probe.service', '-u', prefix + '-supervisor.service', '-u', prefix + '-recover.service', '-n', '60', '--no-pager', check=False).stdout
        RESULTS.append(dict(test=name, status='FAIL', error=str(exc), journal=details))
        raise
    finally:
        # All cleanup is constrained to this test's explicitly named /run paths.
        cmd('systemctl', 'stop', prefix + '-guard.timer', *units, stockunit, check=False)
        cmd('systemctl', 'stop', prefix + '-recover.service', stockunit, check=False)
        for path in Path('/run/systemd/system').glob(prefix + '*'):
            if path.is_dir():
                shutil.rmtree(path)
            else:
                path.unlink()
        cmd('systemctl', 'daemon-reload')
        cmd('systemctl', 'reset-failed', *units, stockunit, check=False)
        sentinel.unlink()
        diskfixture.rmdir()
        (Path(__file__).parent / 'test-results.json').write_text(json.dumps(RESULTS, indent=2))


if __name__ == '__main__':
    check(os.geteuid() == 0, 'Use disposable WSL root environment')
    # A fresh suffix keeps earlier evidence; never reuse or erase a previous run.
    stamp = str(os.getpid())
    RUN = Path(str(RUN) + '-' + stamp)
    RUN.mkdir()
    cases = [('hash_rejection','wrong_hash'), ('sandbox_rejection','broken_namespace'),
                        ('proxy_hash_rejection','bad_proxy_hash'), ('systemd_leak_rejection','systemd_leak'),
                        ('probe_rejection','probe_fail'), ('normal_return',''),
                        ('early_exit','clone_exit'), ('supervisor_kill','supervisor_kill'),
                        ('independent_timer','timer_only')]
    if '--full-duration' in sys.argv:
        cases = [('full_duration_return','')]
    for name, fault in cases:
        scenario(name + '-' + stamp, fault, full_duration='--full-duration' in sys.argv)
        print(name + ': PASS', flush=True)
