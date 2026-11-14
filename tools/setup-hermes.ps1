#requires -Version 5.1
<#
  潮信 TideLetter 安装期预配置（离线可跑，不联网）：
  0. 潮信随安装包分发，运行目录是安装目录里的 sidecar\hermes，
     数据目录固定在可写的 %APPDATA%\Kairos\hermes（HERMES_HOME，尊重 KAIROS_STATE_DIR）。
     老装机的 bots\Hermes\state 会被迁移过来，已有用户数据不改写。
  1. 生成/复用 private\napcat-ws-token.txt（AstrBot aiocqhttp 的连接令牌）
  2. 清理老装机留下的潮信明文密码文件 private\hermes-webui.txt（潮信已经没有访问密码）
  3. 写 <DataDir>\config\onebot.json 全局模板：
     wsClients 指向 AstrBot 的 ws://127.0.0.1:6199/ws，使用上面的令牌
     Hermes 的 store.effective() 会把全局模板 merge 进 onebot_<uin>.json，
     所以全新小号首次接入也自动带 AstrBot 节点。
  4. 可选 -Uin：只更新/生成该账号的 onebot_<uin>.json

  用法：
    powershell -File tools\setup-hermes.ps1 -Base <base> -Bots <bots> -DataDir <可写数据目录>
    powershell -File tools\setup-hermes.ps1 -Base <base> -Bots <bots> -DataDir <可写数据目录> -Uin 123456
#>
param(
    [string]$Base = '',
    [string]$Bots = '',
    [string]$DataDir = '',
    [string]$Source = '',
    [string]$Uin = ''
)
$ErrorActionPreference = 'Stop'
if (-not $Base) { $Base = Split-Path -Parent $PSScriptRoot }
$Base = [IO.Path]::GetFullPath($Base)
if (-not $Bots) { $Bots = Join-Path $Base 'bots' }
$Bots = [IO.Path]::GetFullPath($Bots)
if (-not $Source) { $Source = Join-Path $Base 'sidecar\hermes' }
$Source = [IO.Path]::GetFullPath($Source)
if (-not $DataDir) {
    $root = $env:KAIROS_STATE_DIR
    if (-not $root) { $root = Join-Path $env:APPDATA 'Kairos' }
    $DataDir = Join-Path $root 'hermes'
}
$DataDir = [IO.Path]::GetFullPath($DataDir)
$comp = Join-Path $Bots 'Hermes'
$cfgDir = Join-Path $DataDir 'config'
$private = Join-Path $Base 'private'

# 随包副本是首选；老装机的 bots\Hermes 只在随包缺失时兜底。
$bundleOk = (Test-Path -LiteralPath (Join-Path $Source 'start.cmd')) -and
            (Test-Path -LiteralPath (Join-Path $Source 'src\main.mjs'))
$legacyOk = (Test-Path -LiteralPath (Join-Path $comp 'start.cmd')) -and
            (Test-Path -LiteralPath (Join-Path $comp 'src\main.mjs'))
if (-not $bundleOk -and -not $legacyOk) {
    throw "随包的潮信文件不存在：$Source（请点「修复」或重装时序之子）"
}
foreach ($d in @($DataDir, $cfgDir)) { New-Item -ItemType Directory -Force -Path $d | Out-Null }
New-Item -ItemType Directory -Force -Path $private | Out-Null


function Write-TextNoBom([string]$Path, [string]$Text) {
    $dir = Split-Path -Parent $Path
    if ($dir) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
    [IO.File]::WriteAllText($Path, $Text, (New-Object Text.UTF8Encoding($false)))
}

function New-Secret([int]$Length) {
    $upper = 'ABCDEFGHJKLMNPQRSTUVWXYZ'; $lower = 'abcdefghijkmnpqrstuvwxyz'
    $digit = '23456789'; $special = '!@#%+-_'
    $all = $upper + $lower + $digit + $special
    $rng = [Security.Cryptography.RandomNumberGenerator]::Create()
    $pick = {
        param($set)
        $b = New-Object byte[] 4
        $rng.GetBytes($b)
        $set[([BitConverter]::ToUInt32($b, 0) % $set.Length)]
    }
    $chars = New-Object System.Collections.Generic.List[char]
    $chars.Add((& $pick $upper)); $chars.Add((& $pick $lower))
    $chars.Add((& $pick $digit)); $chars.Add((& $pick $special))
    while ($chars.Count -lt $Length) { $chars.Add((& $pick $all)) }
    for ($i = $chars.Count - 1; $i -gt 0; $i--) {
        $b = New-Object byte[] 4; $rng.GetBytes($b)
        $j = [int]([BitConverter]::ToUInt32($b, 0) % ($i + 1))
        $t = $chars[$i]; $chars[$i] = $chars[$j]; $chars[$j] = $t
    }
    $s = -join $chars
    $rng.Dispose()
    return $s
}

# ---- 旧布局数据迁移：只补缺失文件，绝不覆盖已有配置 ----
$legacyState = Join-Path $comp 'state'
if (Test-Path -LiteralPath $legacyState) {
    $legacyCfg = Join-Path $legacyState 'config'
    if (Test-Path -LiteralPath $legacyCfg) {
        foreach ($f in @(Get-ChildItem -LiteralPath $legacyCfg -Filter '*.json' -File -ErrorAction SilentlyContinue)) {
            $dst = Join-Path $cfgDir $f.Name
            if (-not (Test-Path -LiteralPath $dst)) {
                Copy-Item -LiteralPath $f.FullName -Destination $dst -Force
                Write-Output "迁移旧配置：$dst"
            }
        }
    }
    $legacyDb = Join-Path $legacyState 'messages.db'
    $newDb = Join-Path $DataDir 'messages.db'
    if ((Test-Path -LiteralPath $legacyDb) -and -not (Test-Path -LiteralPath $newDb)) {
        Copy-Item -LiteralPath $legacyDb -Destination $newDb -Force
        Write-Output "迁移旧消息库：$newDb"
    }
}

# ---- 连接令牌 ----
$tokFile = Join-Path $private 'napcat-ws-token.txt'
$token = ''
if (Test-Path -LiteralPath $tokFile) { $token = (Get-Content -LiteralPath $tokFile -Raw -Encoding UTF8).Trim() }
if ($token -notmatch '^\S{16,256}$') {
    $token = New-Secret 24
    Write-TextNoBom $tokFile ($token + "`r`n")
    Write-Output "生成连接令牌：$tokFile"
} else {
    Write-Output "沿用连接令牌：$tokFile"
}

# ---- 潮信已经没有访问密码：老装机留下的明文首密码文件收掉 ----
foreach ($legacyDir in @($private, (Join-Path $Base 'secrets'))) {
    $legacyPwd = Join-Path $legacyDir 'hermes-webui.txt'
    if (Test-Path -LiteralPath $legacyPwd) {
        Remove-Item -LiteralPath $legacyPwd -Force -ErrorAction SilentlyContinue
        Write-Output "已清理老的潮信明文密码文件：$legacyPwd"
    }
}

# 新项目只初始化缺失的协议选择；显式回退始终保留，不迁移旧产品配置。
$protoFile = Join-Path $private 'protocol.txt'
if (-not (Test-Path -LiteralPath $protoFile)) { Write-TextNoBom $protoFile "hermes`r`n" }

# ---- onebot.json（全局模板 / 指定账号）----
$url = 'ws://127.0.0.1:6199/ws'
$defaults = [ordered]@{
    mode = 'snapshot'
    networks = [ordered]@{ httpServers = @(); httpClients = @(); wsServers = @(); wsClients = @() }
    statusCommand = [ordered]@{ enabled = $true; trigger = '#sl'; swallow = $false; cooldownSeconds = 5 }
    historySync = [ordered]@{ enabled = $false }
}

function Get-OneBotConfig([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) { return $null }
    try {
        $txt = [IO.File]::ReadAllText($Path, [Text.Encoding]::UTF8)
        if (-not $txt.Trim()) { return $null }
        return ($txt | ConvertFrom-Json)
    } catch {
        $bak = "$Path.bak-" + (Get-Date -Format 'yyyyMMddHHmmss')
        Copy-Item -LiteralPath $Path -Destination $bak -Force
        Write-Host "旧配置无法解析，已备份：$bak"
        return $null
    }
}

function Merge-OneBot([string]$Path) {
    $cfg = Get-OneBotConfig $Path
    if ($null -eq $cfg) {
        $cfg = [pscustomobject]([ordered]@{
            mode = $defaults.mode
            networks = [pscustomobject]([ordered]@{
                httpServers = @(); httpClients = @(); wsServers = @(); wsClients = @()
            })
            statusCommand = [pscustomobject]$defaults.statusCommand
            historySync = [pscustomobject]$defaults.historySync
        })
    }
    if (-not $cfg.PSObject.Properties['mode']) { $cfg | Add-Member -NotePropertyName mode -NotePropertyValue 'snapshot' }
    if (-not $cfg.PSObject.Properties['networks'] -or $null -eq $cfg.networks) {
        $cfg | Add-Member -NotePropertyName networks -NotePropertyValue ([pscustomobject]([ordered]@{
            httpServers = @(); httpClients = @(); wsServers = @(); wsClients = @()
        })) -Force
    }
    $net = $cfg.networks
    foreach ($k in @('httpServers', 'httpClients', 'wsServers', 'wsClients')) {
        if (-not $net.PSObject.Properties[$k] -or $null -eq $net.$k) {
            $net | Add-Member -NotePropertyName $k -NotePropertyValue @() -Force
        }
    }
    if (-not $cfg.PSObject.Properties['statusCommand'] -or $null -eq $cfg.statusCommand) {
        $cfg | Add-Member -NotePropertyName statusCommand -NotePropertyValue ([pscustomobject]$defaults.statusCommand) -Force
    }
    if (-not $cfg.PSObject.Properties['historySync'] -or $null -eq $cfg.historySync) {
        $cfg | Add-Member -NotePropertyName historySync -NotePropertyValue ([pscustomobject]$defaults.historySync) -Force
    }

    $list = @($net.wsClients | Where-Object { $null -ne $_ })
    $hit = @($list | Where-Object { "$($_.name)" -eq 'astrbot' } | Select-Object -First 1)
    if ($hit.Count -gt 0) {
        $n = $hit[0]
        $n.enabled = $true
        $n.url = $url
        $n.role = 'Universal'
        $n.accessToken = $token
        $n.messageFormat = 'array'
        $n.reportSelfMessage = $false
        $n.reconnectIntervalMs = 5000
        if (-not $n.PSObject.Properties['id'] -or -not "$($n.id)") { $n | Add-Member -NotePropertyName id -NotePropertyValue ([guid]::NewGuid().ToString()) -Force }
        if (-not $n.PSObject.Properties['reportEvents']) { $n | Add-Member -NotePropertyName reportEvents -NotePropertyValue $true -Force }
        if ($n.PSObject.Properties['musicSignUrl']) { $n.musicSignUrl = '' } else { $n | Add-Member -NotePropertyName musicSignUrl -NotePropertyValue '' }
    } else {
        $node = [pscustomobject]([ordered]@{
            id = [guid]::NewGuid().ToString()
            name = 'astrbot'; enabled = $true; url = $url; role = 'Universal'
            accessToken = $token; messageFormat = 'array'; reportSelfMessage = $false
            reportEvents = $true; reconnectIntervalMs = 5000; musicSignUrl = ''
        })
        $list = @($list) + $node
    }
    $net.wsClients = @($list)
    $next = ($cfg | ConvertTo-Json -Depth 12) + "`n"
    if (-not (Test-Path -LiteralPath $Path) -or [IO.File]::ReadAllText($Path) -cne $next) { Write-TextNoBom $Path $next }
    Write-Output "写入 OneBot 配置：$Path"
}

$globalCfg = Join-Path $cfgDir 'onebot.json'
Merge-OneBot $globalCfg
if ($Uin) {
    if ($Uin -notmatch '^\d{5,12}$') { throw "-Uin 不像 QQ 号：$Uin" }
    Merge-OneBot (Join-Path $cfgDir ("onebot_{0}.json" -f $Uin))
}
Write-Output "潮信预配置完成（数据目录：$DataDir；AstrBot 反向 WS：ws://127.0.0.1:6199/ws）。"
exit 0
