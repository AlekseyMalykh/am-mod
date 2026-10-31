param([switch]$OpenBrowser)
$ErrorActionPreference = 'Stop'
$logs = Join-Path $PSScriptRoot 'logs'
New-Item -ItemType Directory -Force -Path $logs | Out-Null
$socket = [System.Net.Sockets.TcpClient]::new()
try { $socket.Connect('127.0.0.1', 8766); $running = $true }
catch { $running = $false }
finally { $socket.Dispose() }
if (-not $running) {
    Start-Process -FilePath 'wsl.exe' -WindowStyle Hidden -ArgumentList @(
        '-d', 'Ubuntu', '-u', 'root', '--cd', '/opt/mpc-sample-lab/stock-map', '--',
        'python3', '-m', 'http.server', '8766', '--bind', '127.0.0.1'
    ) -RedirectStandardOutput (Join-Path $logs 'stock-map.log') `
      -RedirectStandardError (Join-Path $logs 'stock-map-error.log') | Out-Null
}
Write-Output 'MPC Sample Stock Map: http://127.0.0.1:8766/index.html'
if ($OpenBrowser) { Start-Process 'http://127.0.0.1:8766/index.html' }
