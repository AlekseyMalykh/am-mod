$ErrorActionPreference = 'Stop'
& wsl.exe -d Ubuntu -u root --cd /opt/mpc-sample-lab -- python3 lab.py stop
if ($LASTEXITCODE -ne 0) { throw 'The virtual MPC did not stop normally; inspect the lab logs.' }
