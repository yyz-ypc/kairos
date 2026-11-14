#requires -Version 5.1
# 断网、空工作区验收：随包潮信只做本地校验 + 必要初始化，然后能直接启动。
# 不联网、不跑 npm、不用下载器、不用测试假源，也不碰任何真实 QQ 账号。
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$taskRoot = [IO.Path]::GetFullPath((Join-Path $repo ('.tmp\tide-offline-' + [guid]::NewGuid().ToString('N'))))
New-Item -ItemType Directory -Path $taskRoot -Force | Out-Null
$cases = 0
function Check([bool]$condition, [string]$name) { if (-not $condition) { throw "FAIL: $name" }; $script:cases++; Write-Output "PASS: $name" }
function Put([string]$path, [string]$text) { New-Item -ItemType Directory -Path (Split-Path $path) -Force | Out-Null; [IO.File]::WriteAllText($path, $text, (New-Object Text.UTF8Encoding($false))) }
function Wait-Port([int]$port, [int]$seconds) {
    $deadline = (Get-Date).AddSeconds($seconds)
    while ((Get-Date) -lt $deadline) {
        $c = New-Object Net.Sockets.TcpClient
        try { $c.Connect('127.0.0.1', $port); $c.Close(); return $true } catch { $c.Dispose() }
        Start-Sleep -Milliseconds 300
    }
    return $false
}
$proc = $null
try {
    $stage = Join-Path $taskRoot 'install'
    $ws = Join-Path $taskRoot 'workspace'
    $stateRoot = Join-Path $taskRoot 'state'
    foreach ($d in @((Join-Path $stage 'tools\source-manifests'), (Join-Path $stage 'sidecar'), (Join-Path $ws 'bots'), $stateRoot)) { New-Item -ItemType Directory -Path $d -Force | Out-Null }
    foreach ($f in @('bootstrap.ps1', 'source-manifest.ps1', 'setup-hermes.ps1', 'components.json', 'common.ps1', 'progress.ps1', 'autostart.ps1', 'bundled-assets.ps1')) {
        Copy-Item -LiteralPath (Join-Path $repo ('tools\' + $f)) -Destination (Join-Path $stage 'tools') -Force
    }
    Copy-Item -LiteralPath (Join-Path $repo 'tools\source-manifests\hermes.json') -Destination (Join-Path $stage 'tools\source-manifests') -Force
    Copy-Item -LiteralPath (Join-Path $repo 'sidecar\hermes') -Destination (Join-Path $stage 'sidecar\hermes') -Recurse -Force
    New-Item -ItemType Directory -Path (Join-Path $stage 'sidecar\node') -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $repo 'sidecar\node\node.exe') -Destination (Join-Path $stage 'sidecar\node\node.exe') -Force

    $old = $env:KAIROS_STATE_DIR
    $env:KAIROS_STATE_DIR = $stateRoot
    try {
        $log = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $stage 'tools\bootstrap.ps1') `
            -Base $stage -Workspace $ws -Bots (Join-Path $ws 'bots') -Component hermes -Offline 2>&1 | Out-String
        $code = $LASTEXITCODE
    } finally {
        if ($null -eq $old) { Remove-Item Env:KAIROS_STATE_DIR -ErrorAction SilentlyContinue } else { $env:KAIROS_STATE_DIR = $old }
    }
    [IO.File]::WriteAllText((Join-Path $repo '.tmp\offline-bootstrap.log'),$log,(New-Object Text.UTF8Encoding($false)))
    Check ($code -eq 0) "offline bootstrap initializes hermes (exit $code)"
    Check ($log -match '##RESULT ok=1 fixed=1' -and $log -match 'failed=0') 'offline bootstrap reports one fixed component with no failures'
    Check (-not ($log -match '下载校验|换镜像|检查网络|npm')) 'offline bootstrap never talks about downloads or mirrors'

    $tideHome = Join-Path $stateRoot 'hermes'
    $config = Join-Path $tideHome 'config\onebot.json'
    Check (Test-Path -LiteralPath $config) 'bundled hermes initialized its writable data directory'
    $cfg = [IO.File]::ReadAllText($config) | ConvertFrom-Json
    Check ($cfg.networks.wsClients.Count -eq 1 -and $cfg.networks.wsClients[0].url -eq 'ws://127.0.0.1:6199/ws') 'initialized OneBot template points at AstrBot'
    Check (Test-Path -LiteralPath (Join-Path $stage 'logs\setup')) 'bootstrap kept its local log directory'

    $startCmd = [IO.File]::ReadAllText((Join-Path $stage 'sidecar\hermes\start.cmd'))
    Check (($startCmd -match '\.\.\\node\\node\.exe') -and ($startCmd -match 'HERMES_NODE')) 'bundled start.cmd prefers the bundled Node'
    $nodeVersion = (& (Join-Path $stage 'sidecar\node\node.exe') -v) -join ''
    Check ($nodeVersion -match '^v(2[4-9]|[3-9][0-9])\.') "bundled Node is compatible ($nodeVersion)"

    # 真正用随包 Node 起一次潮信：只读 /api/state，不登录 QQ、不发消息。
    $port = 25187
    $logFile = Join-Path $taskRoot 'hermes.out.log'
    $errFile = Join-Path $taskRoot 'hermes.err.log'
    $env:HERMES_HOME = $tideHome
    $env:HERMES_PORT = "$port"
    $env:HERMES_NODE = (Join-Path $stage 'sidecar\node\node.exe')
    $proc = Start-Process -FilePath $env:HERMES_NODE -ArgumentList 'src\main.mjs' -WorkingDirectory (Join-Path $stage 'sidecar\hermes') -RedirectStandardOutput $logFile -RedirectStandardError $errFile -PassThru -WindowStyle Hidden
    Check (Wait-Port $port 40) 'bundled hermes starts and listens without network'
    $body = ''
    try {
        $resp = Invoke-WebRequest -UseBasicParsing -Uri ("http://127.0.0.1:{0}/api/state" -f $port) -TimeoutSec 10
        $body = [string]$resp.Content
        Check ($resp.StatusCode -eq 200) 'hermes answers its local state API'
    } catch {
        $r = $_.Exception.Response
        $status = if ($r) { [int]$r.StatusCode } else { 0 }
        Check ($status -in 200, 401, 403) 'hermes answers its local state API'
    }
    if ($body) { $parsed = $body | ConvertFrom-Json; Check ($null -ne $parsed) 'hermes state payload is JSON' }
    Start-Sleep -Milliseconds 500
    $outText = ''
    if (Test-Path -LiteralPath $logFile) {
        $fs = [IO.File]::Open($logFile, 'Open', 'Read', 'ReadWrite')
        try { $sr = New-Object IO.StreamReader($fs); $outText = $sr.ReadToEnd(); $sr.Dispose() } finally { $fs.Dispose() }
    }
    Check ($outText -match [regex]::Escape($tideHome)) 'hermes logged the writable data directory, not the install dir'
    Check (Test-Path -LiteralPath (Join-Path $tideHome 'config')) 'runtime created writable config directory'

    taskkill /PID $proc.Id /T /F 2>&1 | Out-Null
    $proc = $null
    Check (-not (Wait-Port $port 5)) 'hermes stopped cleanly'
    Write-Output "SUMMARY total=$cases failed=0"
} finally {
    if ($proc) { taskkill /PID $proc.Id /T /F 2>&1 | Out-Null }
    foreach ($k in @('HERMES_HOME', 'HERMES_PORT', 'HERMES_NODE')) { Remove-Item ("Env:" + $k) -ErrorAction SilentlyContinue }
    $resolved = [IO.Path]::GetFullPath($taskRoot)
    $scope = [IO.Path]::GetFullPath((Join-Path $repo '.tmp')) + [IO.Path]::DirectorySeparatorChar
    if (-not $resolved.StartsWith($scope, [StringComparison]::OrdinalIgnoreCase)) { throw 'cleanup outside fixture scope refused' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
