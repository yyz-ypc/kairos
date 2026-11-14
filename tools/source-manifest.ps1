#requires -Version 5.1
<#
  为随安装包提供（不联网下载）的真实源码目录生成/校验确定性 SHA256 清单。
  用法：
    powershell -File tools\source-manifest.ps1 -Root sidecar\rift -Create -Manifest tools\source-manifests\rift.json
    powershell -File tools\source-manifest.ps1 -Root sidecar\rift -Verify -Manifest tools\source-manifests\rift.json
  清单按相对路径排序，摘要覆盖路径、文件长度和文件 SHA256；排除 .git / node_modules / __pycache__ / *.pyc / 临时文件。
#>
param(
    [Parameter(Mandatory)][string]$Root,
    [string]$Manifest = '',
    [switch]$Create,
    [switch]$Verify,
    [string[]]$Exclude = @()
)
$ErrorActionPreference = 'Stop'
$Root = [IO.Path]::GetFullPath($Root)
if (-not (Test-Path -LiteralPath $Root -PathType Container)) { throw "源码目录不存在：$Root" }

# 额外排除项：目录名、相对路径前缀或通配（不区分大小写），逗号/分号分隔。
# 用于源码树里带运行时状态（state/、.selftest/、logs/）的组件，保证摘要稳定。
$script:ExcludeList = @($Exclude |
    ForEach-Object { $_ -split '[,;]' } |
    ForEach-Object { $_.Trim() } |
    Where-Object { $_ })
function Test-SourceExcluded([string]$Rel) {
    foreach ($ex in $script:ExcludeList) {
        $p = $ex.TrimEnd('/', '\')
        if (-not $p) { continue }
        if ($Rel -eq $p -or $Rel.StartsWith($p + '/', [StringComparison]::OrdinalIgnoreCase) -or $Rel -like $p) { return $true }
    }
    return $false
}

function Get-Relative-Path([string]$Base, [string]$Path) {
    $b = $Base.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    $full = [IO.Path]::GetFullPath($Path)
    if (-not $full.StartsWith($b, [StringComparison]::OrdinalIgnoreCase)) { throw "文件不在源码根下：$Path" }
    return $full.Substring($b.Length).Replace('\', '/')
}

function Get-SourceFiles([string]$Base) {
    Get-ChildItem -LiteralPath $Base -File -Recurse -Force -ErrorAction Stop |
        Where-Object {
            $rel = Get-Relative-Path $Base $_.FullName
            -not (Test-SourceExcluded $rel) -and
            $rel -notmatch '(^|/)(\.git|node_modules|__pycache__)(/|$)' -and
            $rel -notmatch '\.py[co]$' -and
            $rel -notmatch '(^|/)(Thumbs\.db|\.DS_Store)$' -and
            $rel -notmatch '\.(tmp|temp|log)$'
        } | ForEach-Object {
            [pscustomobject]@{
                path = (Get-Relative-Path $Base $_.FullName)
                size = [long]$_.Length
                sha256 = (Get-SourceFileDigest $_.FullName)
            }
        } | Sort-Object path
}

# Do not depend on a lazily imported Get-FileHash function in nested build shells.
function Get-SourceFileDigest([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-', '').ToLowerInvariant() }
    finally { $stream.Dispose(); $sha.Dispose() }
}

function Get-TreeDigest($Files) {
    $sb = New-Object Text.StringBuilder
    foreach ($f in $Files) {
        [void]$sb.Append($f.path).Append([char]0).Append($f.size).Append([char]0).Append($f.sha256).Append([char]10)
    }
    $bytes = [Text.Encoding]::UTF8.GetBytes($sb.ToString())
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant() }
    finally { $sha.Dispose() }
}

$files = @(Get-SourceFiles $Root)
$digest = Get-TreeDigest $files
if ($Create) {
    if (-not $Manifest) { throw '-Create 需要 -Manifest 输出路径' }    $parent = Split-Path -Parent $Manifest
    if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
    $obj = [ordered]@{ schema = 1; algorithm = 'sha256(path\0size\0file_sha256, sorted, UTF-8)'; root = (Split-Path -Leaf $Root); digest = $digest; files = $files }
    [IO.File]::WriteAllText($Manifest, (($obj | ConvertTo-Json -Depth 6) + "`n"), (New-Object Text.UTF8Encoding($false)))
    Write-Output $digest
    exit 0
}
if (-not $Manifest -or -not (Test-Path -LiteralPath $Manifest)) { throw '-Verify 需要存在的 -Manifest 清单' }
$saved = [IO.File]::ReadAllText($Manifest, [Text.Encoding]::UTF8) | ConvertFrom-Json
if ([string]$saved.digest -ne $digest) { throw "源码树摘要不符：清单 $($saved.digest)，实际 $digest" }
$map = @{}
foreach ($f in @($saved.files)) { $map[[string]$f.path] = $f }
foreach ($f in $files) {
    if (-not $map.ContainsKey($f.path)) { throw "源码清单外新增文件：$($f.path)" }
    if ([long]$map[$f.path].size -ne $f.size -or [string]$map[$f.path].sha256 -ne $f.sha256) { throw "源码文件哈希不符：$($f.path)" }
    $map.Remove($f.path)
}
if ($map.Count -gt 0) { throw ('源码文件缺失：' + (($map.Keys | Sort-Object) -join ', ')) }
Write-Output $digest
