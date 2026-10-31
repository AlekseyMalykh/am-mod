"""Run the production host shutdown branch with process/filesystem receivers.
Never calls the real systemctl poweroff, touches an MPC or opens a physical SD.
"""
from pathlib import Path
import os,subprocess,tempfile
R=Path(__file__).resolve().parent
s=(R/'src/autorun.template.sh').read_text()
start=s.index('    if [ -f "$R/data-staged" ]; then',s.index('    native_crash_snapshot\n'))
end=s.index('    if [ -f "$R/switch-intent" ]; then',start)
branch=s[start:end]
assert branch.index('--flush-sd')<branch.index('data_layer_stop')<branch.index('systemctl --no-block poweroff')
assert 'rm -f "$R/poweroff-confirmed"' in branch
with tempfile.TemporaryDirectory(prefix='mpc-power-host-') as folder:
    root=Path(folder)
    fake=root/'systemctl'
    fake.write_text('#!/bin/sh\necho "systemctl $*" >> "$R/order"\ncase "$*" in *poweroff) exit "${POWER_FAIL:-0}";; esac\n')
    fake.chmod(0o700)
    stage=root/'stage-data';stage.write_text('#!/bin/sh\necho flush >> "$R/order"\nexit "${FLUSH_FAIL:-0}"\n');stage.chmod(0o700)
    script=root/'run.sh'
    script.write_text('''#!/bin/sh
set -eu
U=fixture
log(){ echo "log $*" >> "$R/order"; }
status(){ echo "status $*" >> "$R/order"; }
export_report(){ echo report >> "$R/order"; }
data_layer_stop(){ echo unmount >> "$R/order"; }
fail(){ exit 9; }
'''+branch.replace('/usr/bin/systemctl',str(fake))+ '\necho STOCK_FALLBACK >> "$R/order"\n')
    for name,flag,flush,power in [('normal',False,0,0),('confirmed',True,0,0),('flush-failed',True,1,0),('poweroff-failed',True,0,1),('link',False,0,0)]:
        for f in ['poweroff-confirmed','recovered','order']:
            (root/f).unlink(missing_ok=True)
        (root/'data-staged').touch()
        if flag:(root/'poweroff-confirmed').touch()
        if name=='link':(root/'poweroff-confirmed').symlink_to(root/'data-staged')
        env=dict(os.environ,R=str(root),PATH=str(root)+':'+os.environ['PATH'],POWER_FAIL=str(power),FLUSH_FAIL=str(flush))
        subprocess.run(['sh',str(script)],env=env,check=True)
        order=(root/'order').read_text()
        requested='systemctl --no-block poweroff' in order
        assert requested==(flag and not flush),name
        assert ('STOCK_FALLBACK' in order)==(name!='confirmed'),name
        if requested:assert order.index('flush')<order.index('unmount')<order.index('systemctl --no-block poweroff')
        if flush or power:assert not (root/'poweroff-confirmed').exists()
print('PASS production host branch: no shutdown on Mod Off/missing request/link/flush error, completed SD flush and unmount precede poweroff; poweroff failure falls back to stock. Real OS poweroff receiver mocked.')
