"""Run inside WSL before testing the SD loader on the slow software ARM CPU.
Only talks to the local QEMU lab SSH port. No device connection or SD package edit.
"""
import sys
sys.path.insert(0, '/opt/mpc-sample-lab')
import lab

script = r'''set -e
mkdir -p /run/systemd/system/mpc-clone-trial-v1-probe.service.d
printf '[Service]\nTimeoutStartSec=180s\n' > /run/systemd/system/mpc-clone-trial-v1-probe.service.d/lab-tcg.conf
cat > /run/mpclab-mod-startup-window.sh <<'SCRIPT'
#!/bin/sh
set -eu
read seconds rest < /proc/uptime
printf '%s\n' "$(( ${seconds%.*} + 600 ))" > /run/mpc-clone-trial-v1/startup-deadline
SCRIPT
mkdir -p /run/systemd/system/mpc-clone-trial-v1-supervisor.service.d
cat > /run/systemd/system/mpc-clone-trial-v1-supervisor.service.d/lab-tcg.conf <<'UNIT'
[Service]
ExecStartPre=/bin/sh /run/mpclab-mod-startup-window.sh
TimeoutStopSec=180s
UNIT
systemctl daemon-reload
'''
print(lab.guest(script).decode())
print('TCG lab only: sandbox probe 180 s; startup recovery window 600 s; runtime guard preserved.')
