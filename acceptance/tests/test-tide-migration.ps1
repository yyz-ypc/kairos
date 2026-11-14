#requires -Version 5.1
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $repo 'tools\autostart.ps1')
$taskRoot = [IO.Path]::GetFullPath((Join-Path $repo ('.tmp\tide-migration-' + [guid]::NewGuid().ToString('N'))))
New-Item -ItemType Directory -Path $taskRoot -Force | Out-Null
$cases = 0
function Check([bool]$condition, [string]$name) { if (-not $condition) { throw "FAIL: $name" }; $script:cases++; Write-Output "PASS: $name" }
function Put([string]$path, [string]$text) { New-Item -ItemType Directory -Path (Split-Path $path) -Force | Out-Null; [IO.File]::WriteAllText($path, $text, (New-Object Text.UTF8Encoding($false))) }
$bundleFiles = @(
    'start.cmd',
    'src\main.mjs',
    'vendor\ws\wrapper.mjs',
    'vendor\silk-wasm\silk.wasm',
    'native\build\hermes-qq-hook.dll',
    'native\build\hermes-qq-launcher.exe'
)
function Put-Bundle([string]$basePath) {
    $src = Join-Path $basePath 'sidecar\hermes'
    foreach ($rel in $bundleFiles) { Put (Join-Path $src $rel) ('fixture:' + $rel) }
    return $src
}
function Setup-Tide([string]$basePath, [string]$homePath) {
    $prev = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $out = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $repo 'tools\setup-hermes.ps1') `
            -Base $basePath -Bots (Join-Path $basePath 'fixture-components') -DataDir $homePath 2>&1 | Out-String
        $code = $LASTEXITCODE
    } finally { $ErrorActionPreference = $prev }
    return [pscustomobject]@{ Code = $code; Output = $out }
}
try {
    $startup = Join-Path $taskRoot 'startup'; New-Item -ItemType Directory -Path $startup | Out-Null
    $old = Join-Path $startup ('大肥鱼-{0}{1}.lnk' -f [char]0x51a5,[char]0x6865)
    $new = Join-Path $startup '大肥鱼-潮信.lnk'
    Put $old 'same-shortcut-target'
    Convert-TideStartupName $startup $taskRoot | Out-Null
    Check ((Test-Path -LiteralPath $old) -and -not (Test-Path -LiteralPath $new) -and [IO.File]::ReadAllText($old) -eq 'same-shortcut-target') 'old product shortcut untouched'
    Convert-TideStartupName $startup $taskRoot | Out-Null
    Check (@(Get-ChildItem $startup -Filter '*.lnk').Count -eq 1) 'rename is idempotent'
    Put $old 'duplicate-old-target'; Convert-TideStartupName $startup $taskRoot | Out-Null
    Check ((Test-Path -LiteralPath $old) -and -not (Test-Path -LiteralPath $new) -and [IO.File]::ReadAllText($old) -eq 'duplicate-old-target') 'old shortcuts are never migrated or archived'

    # ---- 随包布局：sidecar\hermes 完整，数据写到可写 Home ----
    $fixture = Join-Path $taskRoot 'workspace'
    $src = Put-Bundle $fixture
    $tideHome = Join-Path $taskRoot 'state\hermes'
    $protocol = Join-Path $fixture 'private\protocol.txt'
    $config = Join-Path $tideHome 'config\onebot.json'
    $r = Setup-Tide $fixture $tideHome
    Check ($r.Code -eq 0) 'clean workspace initializes hermes from bundled sidecar'
    Check ([IO.File]::ReadAllText($protocol).Trim() -eq 'hermes') 'protocol defaults to hermes'
    Check (Test-Path -LiteralPath $config) 'OneBot template written to writable home'
    $cfg = [IO.File]::ReadAllText($config) | ConvertFrom-Json
    $token = [IO.File]::ReadAllText((Join-Path $fixture 'private\napcat-ws-token.txt')).Trim()
    Check ($cfg.networks.wsClients.Count -eq 1 -and $cfg.networks.wsClients[0].url -eq 'ws://127.0.0.1:6199/ws' -and $cfg.networks.wsClients[0].accessToken -eq $token) 'OneBot node points at AstrBot with the shared token'
    Check (-not (Test-Path -LiteralPath (Join-Path $fixture 'fixture-components\Hermes\state\config\onebot.json'))) 'bundled run no longer writes into bots\Hermes'

    $bytes = [IO.File]::ReadAllText($config); $modified = (Get-Item $config).LastWriteTimeUtc
    $r = Setup-Tide $fixture $tideHome
    Check ($r.Code -eq 0 -and [IO.File]::ReadAllText($config) -ceq $bytes -and (Get-Item $config).LastWriteTimeUtc -eq $modified) 'repeat setup leaves correct OneBot config unchanged'

    Put $protocol 'snowluma'; $r = Setup-Tide $fixture $tideHome
    Check ([IO.File]::ReadAllText($protocol).Trim() -eq 'snowluma') 'explicit fallback survives later upgrade'
    Put $protocol 'napcat'; $r = Setup-Tide $fixture $tideHome
    Check ([IO.File]::ReadAllText($protocol).Trim() -eq 'napcat') 'NapCat choice preserved'

    Put $config '{broken'; $r = Setup-Tide $fixture $tideHome
    Check (([IO.File]::ReadAllText($config) | ConvertFrom-Json).networks.wsClients.Count -eq 1 -and @(Get-ChildItem ($config + '.bak-*')).Count -eq 1) 'broken configuration backed up and repaired without pipeline pollution'

    # ---- 老装机数据迁移：只补缺失，不覆盖已有 ----
    $fixture2 = Join-Path $taskRoot 'legacy'
    Put-Bundle $fixture2 | Out-Null
    $tideHome2 = Join-Path $taskRoot 'state2\hermes'
    $legacyCfgDir = Join-Path $fixture2 'fixture-components\Hermes\state\config'
    Put (Join-Path $legacyCfgDir 'onebot_12345.json') '{"legacy":true}'
    Put (Join-Path $fixture2 'fixture-components\Hermes\state\messages.db') 'legacy-db'
    New-Item -ItemType Directory -Path (Join-Path $tideHome2 'config') -Force | Out-Null
    Put (Join-Path $tideHome2 'config\onebot_12345.json') '{"keep":true}'
    $r = Setup-Tide $fixture2 $tideHome2
    Check ($r.Code -eq 0) 'setup succeeds with legacy bots data present'
    Check ([IO.File]::ReadAllText((Join-Path $tideHome2 'config\onebot_12345.json')).Trim() -eq '{"keep":true}') 'existing home account config is never overwritten'
    Check ([IO.File]::ReadAllText((Join-Path $tideHome2 'messages.db')).Trim() -eq 'legacy-db') 'legacy message database migrated once'

    # ---- 老布局兜底：没有随包副本也能初始化 ----
    $fixture3 = Join-Path $taskRoot 'legacy-only'
    Put (Join-Path $fixture3 'fixture-components\Hermes\start.cmd') 'legacy-start'
    Put (Join-Path $fixture3 'fixture-components\Hermes\src\main.mjs') 'legacy-main'
    $tideHome3 = Join-Path $taskRoot 'state3\hermes'
    $r = Setup-Tide $fixture3 $tideHome3
    Check ($r.Code -eq 0 -and (Test-Path -LiteralPath (Join-Path $tideHome3 'config\onebot.json'))) 'legacy bots\Hermes remains a working fallback'

    # ---- 两份都不完整：明确修复/重装，不出现网络措辞 ----
    $fixture4 = Join-Path $taskRoot 'broken'
    New-Item -ItemType Directory -Path (Join-Path $fixture4 'fixture-components\Hermes') -Force | Out-Null
    $r = Setup-Tide $fixture4 (Join-Path $taskRoot 'state4\hermes')
    Check ($r.Code -ne 0) 'missing bundle is rejected instead of half-initialized'
    Check ($r.Output -match '随包' -and $r.Output -match '修复') 'missing bundle points at repair/reinstall'
    Check (-not ($r.Output -match '网络|下载|镜像|npm|在线')) 'missing bundle never blames the network'

    Write-Output "SUMMARY total=$cases failed=0"
} finally {
    $resolved = [IO.Path]::GetFullPath($taskRoot)
    $scope = [IO.Path]::GetFullPath((Join-Path $repo '.tmp')) + [IO.Path]::DirectorySeparatorChar
    if (-not $resolved.StartsWith($scope, [StringComparison]::OrdinalIgnoreCase)) { throw 'cleanup outside fixture scope refused' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
