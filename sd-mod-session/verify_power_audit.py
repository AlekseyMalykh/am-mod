"""Exercise one-shot read-only inventory against a fake sysfs, never real hardware."""
from pathlib import Path
import tempfile,subprocess,hashlib,json,os
R=Path(__file__).resolve().parent;script=(R/'src/power_audit.sh').read_text()
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
subprocess.run(['sh','-n',str(R/'src/power_audit.sh')],check=True)
with tempfile.TemporaryDirectory(prefix='mpc-power-audit-') as folder:
 root=Path(folder);reader=root/'power-read.sh';binary=root/'power-read';subprocess.run(['aarch64-linux-gnu-gcc','-std=c11','-Wall','-Wextra','-Werror','-DPOWER_AUDIT_TEST','src/power_read.c','-o',str(binary)],check=True);reader.write_text('#!/bin/sh\nexec qemu-aarch64 -L /home/user/mpc-rootfs-user-20260808 '+str(binary)+' "$@"\n');reader.chmod(0o700);os.environ['MPC_POWER_READER']=str(reader);sd=root/'media/MPCMOD';autorun=sd/'autorun';autorun.mkdir(parents=True)
 def put(path,body):
  p=root/path;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(body);return p
 put('proc/mounts',f'device {sd} exfat rw 0 0\n');put('proc/uptime','123.4 234.5\n')
 put('proc/device-tree/model','AC50\0');put('proc/device-tree/compatible','inmusic,ac50\0')
 for name,value in [('uevent','POWER_SUPPLY_CAPACITY=63\n'),('capacity','63\n'),('status','Charging\n'),('charging_enabled','1\n')]:put('sys/class/power_supply/test-battery/'+name,value)
 put('sys/bus/i2c/devices/6-000b/name','sbs-battery\n')
 # Only fixture path translation; command/control flow is the production script.
 body=script.replace('/sys/',str(root)+'/sys/').replace('/proc/',str(root)+'/proc/').replace('/media/',str(root)+'/media/')
 runner=autorun/'power-audit.sh';runner.write_text(body)
 before={str(p):sha(p) for base in [root/'sys',root/'proc'] for p in base.rglob('*') if p.is_file()}
 subprocess.run(['sh',str(runner)],check=True,timeout=15)
 report=(sd/'MPC_POWER_AUDIT.txt').read_text();assert 'READ_ONLY=1' in report and 'CAPACITY=63' in report and 'Charging' in report and 'charging_enabled' in report and 'MPC_POWER_AUDIT_END' in report
 assert before=={str(p):sha(p) for base in [root/'sys',root/'proc'] for p in base.rglob('*') if p.is_file()}
 # Repeated capture replaces its own report only; symlink reports are refused.
 put('proc/mounts',f'device {sd} vfat rw 0 0\n')
 before={str(p):sha(p) for base in [root/'sys',root/'proc'] for p in base.rglob('*') if p.is_file()}
 subprocess.run(['sh',str(runner)],check=True,timeout=15)
 report=(sd/'MPC_POWER_AUDIT.txt').read_text();assert 'READ_ONLY=1' in report and 'MPC_POWER_AUDIT_END' in report
 (sd/'MPC_POWER_AUDIT.txt').unlink();(sd/'MPC_POWER_AUDIT.txt').symlink_to(root/'sys/class/power_supply/test-battery/charging_enabled')
 assert subprocess.run(['sh',str(runner)],capture_output=True).returncode!=0
 assert before=={str(p):sha(p) for base in [root/'sys',root/'proc'] for p in base.rglob('*') if p.is_file()}
O=R/'build/power-audit';O.mkdir(exist_ok=True)
report={'passed':True,'filesystems_tested':['exfat','vfat'],'hardware_tested':False,'charger_writes':False,'sources_sha256':{s:sha(R/s) for s in ['src/power_audit.sh','src/power_read.c','verify_power_audit.py']}}
(O/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS bounded read-only power inventory, repeated capture, unchanged fixture properties and symlink refusal')
