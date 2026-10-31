param([switch]$OpenBrowser)
$ErrorActionPreference = 'Stop'
$labDirectory = '/opt/mpc-sample-lab'
$logs = Join-Path $PSScriptRoot 'logs'
New-Item -ItemType Directory -Force -Path $logs | Out-Null
$socket = [System.Net.Sockets.TcpClient]::new()
try { $socket.Connect('127.0.0.1', 5554); $running = $true }
catch { $running = $false }
finally { $socket.Dispose() }
if (-not $running) {
    Start-Process -FilePath 'wsl.exe' -WindowStyle Hidden -ArgumentList @(
        '-d', 'Ubuntu', '-u', 'root', '--cd', $labDirectory, '--',
        'env', 'PATH=/opt/mpc-sample-lab/qemu/bin:/usr/bin:/bin',
        'python3', '-u', 'lab.py', 'run', '--headless'
    ) -RedirectStandardOutput (Join-Path $logs 'runtime.log') `
      -RedirectStandardError (Join-Path $logs 'runtime-error.log') | Out-Null
}
Write-Output 'Virtual MPC Sample: http://127.0.0.1:8765/'
Write-Output 'First boot can take several minutes under ARM software emulation.'
if ($OpenBrowser) { Start-Process 'http://127.0.0.1:8765/' }
