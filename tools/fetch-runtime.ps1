# 抓取安装包需要的两个大件：node.exe 和 ffmpeg.exe。
# 运行时只信任 tools\runtime-pins.json；下载走 .part，哈希/大小通过后原子提交。
param(
    [string]$NodeExe = "",
    [string]$FfmpegExe = "",
    [switch]$Force,
    [switch]$VerifyOnly,
    [switch]$FunctionOnly,
    [string]$PinsPath = "",
    [string]$NodeDestination = "",
    [string]$FfmpegDestination = "",
    [string]$TemporaryRoot = ""
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
Add-Type -AssemblyName System.Net.Http

function Say([string]$Message) { Write-Host "[runtime] $Message" }

function New-RuntimeException([string]$Message, [bool]$Retryable) {
    $exception = New-Object System.Exception($Message)
    $exception.Data["Retryable"] = $Retryable
    return $exception
}

function Test-RetryableException([System.Management.Automation.ErrorRecord]$ErrorRecord) {
    $current = $ErrorRecord.Exception
    while ($null -ne $current) {
        if ($current.Data -and $current.Data.Contains("Retryable")) {
            return [bool]$current.Data["Retryable"]
        }
        if (($current -is [System.Net.Http.HttpRequestException]) -or
            ($current -is [System.OperationCanceledException]) -or
            ($current -is [System.IO.IOException])) {
            return $true
        }
        $current = $current.InnerException
    }
    return $false
}

function Get-RequiredPinProperty([object]$Pin, [string]$Name) {
    $property = $Pin.PSObject.Properties[$Name]
    if ($null -eq $property -or $null -eq $property.Value -or "$($property.Value)".Trim() -eq "") {
        throw "runtime-pins.json 缺少字段：$Name"
    }
    return $property.Value
}

function Assert-RuntimePin([object]$Pin, [string]$Name) {
    $url = [string](Get-RequiredPinProperty $Pin "url")
    $sha256 = [string](Get-RequiredPinProperty $Pin "sha256")
    $size = [long](Get-RequiredPinProperty $Pin "size")
    try { $uri = [Uri]$url } catch { throw "runtime-pins.json 的 $Name URL 无效：$url" }
    if ($uri.Scheme -ne "https") { throw "runtime-pins.json 的 $Name URL 必须是 HTTPS：$url" }
    if ($sha256 -notmatch '^[0-9a-fA-F]{64}$') { throw "runtime-pins.json 的 $Name sha256 不是 64 位十六进制：$sha256" }
    if ($size -le 0) { throw "runtime-pins.json 的 $Name size 必须是正整数：$size" }
    Get-RequiredPinProperty $Pin "version" | Out-Null
    Get-RequiredPinProperty $Pin "fileName" | Out-Null
    Get-RequiredPinProperty $Pin "sourceUrl" | Out-Null
    Get-RequiredPinProperty $Pin "verifiedAt" | Out-Null
}

function Get-RuntimePins([string]$Path) {
    if (-not $Path) { $Path = Join-Path $PSScriptRoot "runtime-pins.json" }
    if (-not (Test-Path -LiteralPath $Path)) { throw "找不到 runtime pins：$Path" }
    try {
        $pins = Get-Content -LiteralPath $Path -Raw -Encoding UTF8 | ConvertFrom-Json
    } catch {
        throw "runtime-pins.json 解析失败：$($_.Exception.Message)"
    }
    if ($null -eq $pins.node -or $null -eq $pins.ffmpeg) { throw "runtime-pins.json 必须包含 node 和 ffmpeg" }
    Assert-RuntimePin $pins.node "node"
    Assert-RuntimePin $pins.ffmpeg "ffmpeg"
    $zipSha256 = [string](Get-RequiredPinProperty $pins.ffmpeg "zipSha256")
    if ($zipSha256 -ne [string]$pins.ffmpeg.sha256) { throw "ffmpeg 的 sha256 与 zipSha256 不一致" }
    $zipSize = [long](Get-RequiredPinProperty $pins.ffmpeg "zipSize")
    if ($zipSize -ne [long]$pins.ffmpeg.size) { throw "ffmpeg 的 size 与 zipSize 不一致" }
    $entryName = [string](Get-RequiredPinProperty $pins.ffmpeg "entryName")
    if ($entryName -notmatch '(?i)/bin/ffmpeg\.exe$') { throw "runtime-pins.json 的 ffmpeg entryName 必须以 /bin/ffmpeg.exe 结尾：$entryName" }
    $exeSha256 = [string](Get-RequiredPinProperty $pins.ffmpeg "ffmpegExeSha256")
    if ($exeSha256 -notmatch '^[0-9a-fA-F]{64}$') { throw "runtime-pins.json 的 ffmpegExeSha256 不是 64 位十六进制：$exeSha256" }
    $exeSize = [long](Get-RequiredPinProperty $pins.ffmpeg "ffmpegExeSize")
    if ($exeSize -le 0) { throw "runtime-pins.json 的 ffmpegExeSize 必须是正整数：$exeSize" }
    return $pins
}

function Get-FileSha256([string]$Path) {
    $hash = Get-FileHash -LiteralPath $Path -Algorithm SHA256
    return $hash.Hash.ToLowerInvariant()
}

function Assert-RuntimeFile([string]$Path, [string]$ExpectedHash = "", [long]$ExpectedSize = 0) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "文件不存在：$Path" }
    $item = Get-Item -LiteralPath $Path
    if ($item.Length -le 0) { throw "文件是空的：$Path" }
    if ($ExpectedSize -gt 0 -and $item.Length -ne $ExpectedSize) {
        throw "文件大小不对：$Path（期望 $ExpectedSize，实际 $($item.Length)）"
    }
    if ($ExpectedHash) {
        $actual = Get-FileSha256 $Path
        if ($actual -ne $ExpectedHash.ToLowerInvariant()) {
            throw "文件哈希不对：$Path（期望 $ExpectedHash，实际 $actual）"
        }
    }
}

function Get-TargetPin([object]$Pin) {
    # ffmpeg 的公共 sha256/size 描述 ZIP；已安装目标是解出的 exe，要用 ffmpegExe* 字段。
    $hashProperty = $Pin.PSObject.Properties["ffmpegExeSha256"]
    if ($null -ne $hashProperty) {
        return [pscustomobject]@{ Hash = [string]$hashProperty.Value; Size = [long]$Pin.ffmpegExeSize }
    }
    return [pscustomobject]@{ Hash = [string]$Pin.sha256; Size = [long]$Pin.size }
}

function Test-PinnedTarget([object]$Pin, [string]$Target) {
    if (-not (Test-Path -LiteralPath $Target -PathType Leaf)) { return $false }
    $targetPin = Get-TargetPin $Pin
    try { Assert-RuntimeFile $Target $targetPin.Hash $targetPin.Size } catch { return $false }
    return $true
}

function Assert-ExistingTarget([object]$Pin, [string]$Target, [string]$Name) {
    $targetPin = Get-TargetPin $Pin
    $actualHash = ""
    $actualSize = 0L
    if (Test-Path -LiteralPath $Target -PathType Leaf) {
        $item = Get-Item -LiteralPath $Target
        $actualSize = $item.Length
        try { $actualHash = Get-FileSha256 $Target } catch { $actualHash = "<无法读取>" }
    }
    throw "$Name 目标已存在但与 pins 不匹配：$Target（期望 SHA256 $($targetPin.Hash)，实际 $actualHash；期望大小 $($targetPin.Size)，实际 $actualSize）。请确认来源，或用 -Force 重下。"
}

function New-UniquePartPath([string]$Target) {
    return "$Target.$([Guid]::NewGuid().ToString('N')).part"
}

function Invoke-PinnedDownloadAttempt {
    param(
        [object]$Pin,
        [string]$Destination,
        [string]$PartPath,
        [int]$TimeoutSeconds,
        [scriptblock]$HttpClientFactory
    )
    $client = $null; $cancellationTokenSource = $null; $response = $null
    $responseStream = $null; $outputStream = $null; $sha256 = $null
    try {
        # PS 5.1 + 旧 .NET 默认可能只到 TLS 1.0；nodejs.org 与 gyan.dev 都要求 TLS 1.2。
        [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
        if ($HttpClientFactory) { $client = & $HttpClientFactory ([string]$Pin.url) }
        else { $client = New-Object System.Net.Http.HttpClient }
        $cancellationTokenSource = New-Object System.Threading.CancellationTokenSource
        $cancellationTokenSource.CancelAfter([TimeSpan]::FromSeconds($TimeoutSeconds))
        $response = $client.GetAsync([Uri][string]$Pin.url, [System.Net.Http.HttpCompletionOption]::ResponseHeadersRead, $cancellationTokenSource.Token).GetAwaiter().GetResult()
        $statusCode = [int]$response.StatusCode
        if ($statusCode -ge 500 -and $statusCode -lt 600) {
            throw (New-RuntimeException "下载返回 $statusCode：$($Pin.url)" $true)
        }
        if (-not $response.IsSuccessStatusCode) {
            throw (New-RuntimeException "下载失败（HTTP $statusCode）：$($Pin.url)" $false)
        }
        $parent = Split-Path -Parent $PartPath
        [void][IO.Directory]::CreateDirectory($parent)
        $responseStream = $response.Content.ReadAsStreamAsync().GetAwaiter().GetResult()
        # 少数实现/测试夹具会返回可查找且已到末尾的流；真实网络流 CanSeek=false，不受影响。
        if ($responseStream.CanSeek -and $responseStream.Position -ne 0) { $responseStream.Position = 0 }
        $outputStream = [IO.File]::Open($PartPath, [IO.FileMode]::Create, [IO.FileAccess]::Write, [IO.FileShare]::None)
        $sha256 = [System.Security.Cryptography.SHA256]::Create()
        $buffer = New-Object byte[] 262144
        while ($true) {
            $read = $responseStream.ReadAsync($buffer, 0, $buffer.Length, $cancellationTokenSource.Token).GetAwaiter().GetResult()
            if ($read -le 0) { break }
            [void]$sha256.TransformBlock($buffer, 0, $read, $buffer, 0)
            $outputStream.Write($buffer, 0, $read)
        }
        [void]$sha256.TransformFinalBlock((New-Object byte[] 0), 0, 0)
        $actualHash = [BitConverter]::ToString($sha256.Hash).Replace("-", "").ToLowerInvariant()
        $actualSize = $outputStream.Length
        if ($actualSize -ne [long]$Pin.size) {
            throw (New-RuntimeException "下载大小不对：$($Pin.url)（期望 $($Pin.size)，实际 $actualSize）" $false)
        }
        if ($actualHash -ne ([string]$Pin.sha256).ToLowerInvariant()) {
            throw (New-RuntimeException "下载哈希不对：$($Pin.url)（期望 $($Pin.sha256)，实际 $actualHash）" $false)
        }
        return $PartPath
    } finally {
        if ($null -ne $outputStream) { $outputStream.Dispose() }
        if ($null -ne $sha256) { $sha256.Dispose() }
        if ($null -ne $responseStream) { $responseStream.Dispose() }
        if ($null -ne $response) { $response.Dispose() }
        if ($null -ne $cancellationTokenSource) { $cancellationTokenSource.Dispose() }
        if ($null -ne $client) { $client.Dispose() }
    }
}

function Invoke-PinnedDownload {
    param(
        [object]$Pin,
        [string]$Destination,
        [int]$TimeoutSeconds = 60,
        [int[]]$RetryDelaysMilliseconds = @(1000, 2000),
        [scriptblock]$HttpClientFactory = $null,
        [scriptblock]$SleepScript = $null
    )
    if (-not $SleepScript) { $SleepScript = { param($Milliseconds) Start-Sleep -Milliseconds $Milliseconds } }
    $partPath = New-UniquePartPath $Destination
    for ($attempt = 0; $attempt -le $RetryDelaysMilliseconds.Count; $attempt++) {
        try {
            return (Invoke-PinnedDownloadAttempt -Pin $Pin -Destination $Destination -PartPath $partPath -TimeoutSeconds $TimeoutSeconds -HttpClientFactory $HttpClientFactory)
        } catch {
            if (Test-Path -LiteralPath $partPath) { Remove-Item -LiteralPath $partPath -Force -ErrorAction SilentlyContinue }
            $retryable = Test-RetryableException $_
            if (-not $retryable -or $attempt -ge $RetryDelaysMilliseconds.Count) {
                throw (New-RuntimeException ("下载失败：$($Pin.url) —— " + $_.Exception.Message) $false)
            }
            & $SleepScript ([int]$RetryDelaysMilliseconds[$attempt])
        }
    }
    throw (New-RuntimeException "下载失败：$($Pin.url)" $false)
}

function Test-ZipEntryName([string]$EntryName) {
    if ([string]::IsNullOrEmpty($EntryName)) { return $false }
    if ($EntryName.Contains(":")) { return $false }
    $normalized = $EntryName -replace '/', '\'
    if ($normalized.StartsWith('\')) { return $false }
    $parts = @($normalized -split '\\')
    if ($parts.Count -gt 0 -and $parts[-1] -eq "") { $parts = @($parts[0..($parts.Count - 2)]) }
    if ($parts.Count -eq 0) { return $false }
    foreach ($part in $parts) {
        if ($part -eq "" -or $part -eq "." -or $part -eq "..") { return $false }
        if ($part.EndsWith(".") -or $part.EndsWith(" ")) { return $false }
        if ($part -match '^(?i:CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(\.|$)') { return $false }
    }
    return $true
}

function Expand-PinnedZipEntry {
    param(
        [string]$ZipPath,
        [string]$EntryName,
        [string]$ExpectedHash,
        [long]$ExpectedSize,
        [string]$Destination
    )
    $archive = $null; $outputStream = $null; $entryStream = $null; $sha256 = $null
    try {
        $archive = [System.IO.Compression.ZipFile]::OpenRead($ZipPath)
        $seen = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)
        foreach ($entry in $archive.Entries) {
            if (-not (Test-ZipEntryName $entry.FullName)) {
                throw "压缩包条目路径不安全：$($entry.FullName)"
            }
            if (-not $seen.Add($entry.FullName)) {
                throw "压缩包条目大小写冲突：$($entry.FullName)"
            }
        }
        $candidates = @($archive.Entries | Where-Object { ($_.FullName -replace '\\', '/') -match '(?i)(^|/)bin/ffmpeg\.exe$' })
        if ($candidates.Count -ne 1 -or $candidates[0].FullName -ne $EntryName) {
            $names = ($candidates | ForEach-Object { $_.FullName }) -join "、"
            throw "压缩包里必须有唯一的 $EntryName，实际候选：$names"
        }
        $selected = $candidates[0]
        if ($selected.Length -le 0) { throw "压缩包条目是空的：$EntryName" }
        # 先按中央目录里的未压缩长度挡一次，免得 pins 被换后先写满磁盘再比对。
        if ($ExpectedSize -gt 0 -and $selected.Length -ne $ExpectedSize) {
            throw "压缩包条目长度不对：$EntryName（期望 $ExpectedSize，中央目录 $($selected.Length)）"
        }
        $parent = Split-Path -Parent $Destination
        [void][IO.Directory]::CreateDirectory($parent)
        $outputStream = [IO.File]::Open($Destination, [IO.FileMode]::Create, [IO.FileAccess]::Write, [IO.FileShare]::None)
        $entryStream = $selected.Open()
        $sha256 = [System.Security.Cryptography.SHA256]::Create()
        $buffer = New-Object byte[] 262144
        while ($true) {
            $read = $entryStream.Read($buffer, 0, $buffer.Length)
            if ($read -le 0) { break }
            [void]$sha256.TransformBlock($buffer, 0, $read, $buffer, 0)
            $outputStream.Write($buffer, 0, $read)
        }
        [void]$sha256.TransformFinalBlock((New-Object byte[] 0), 0, 0)
        $actualHash = [BitConverter]::ToString($sha256.Hash).Replace("-", "").ToLowerInvariant()
        if ($outputStream.Length -ne $ExpectedSize) {
            throw "解出的 ffmpeg.exe 大小不对：$EntryName（期望 $ExpectedSize，实际 $($outputStream.Length)）"
        }
        if ($actualHash -ne $ExpectedHash.ToLowerInvariant()) {
            throw "解出的 ffmpeg.exe 哈希不对：$EntryName（期望 $ExpectedHash，实际 $actualHash）"
        }
        return $Destination
    } catch {
        if (Test-Path -LiteralPath $Destination) { Remove-Item -LiteralPath $Destination -Force -ErrorAction SilentlyContinue }
        throw
    } finally {
        if ($null -ne $sha256) { $sha256.Dispose() }
        if ($null -ne $entryStream) { $entryStream.Dispose() }
        if ($null -ne $outputStream) { $outputStream.Dispose() }
        if ($null -ne $archive) { $archive.Dispose() }
    }
}

function Complete-RuntimeFile {
    param(
        [string]$PartPath,
        [string]$Target,
        [string]$ExpectedHash = "",
        [long]$ExpectedSize = 0,
        [switch]$Force,
        [scriptblock]$PostCommitVerifier = $null
    )
    $targetExisted = Test-Path -LiteralPath $Target -PathType Leaf
    $createdBackup = ""
    try {
        if ($targetExisted -and -not $Force) {
            throw "目标已存在且未给 -Force，不能覆盖：$Target"
        }
        if ($targetExisted) {
            $createdBackup = "$Target.$([Guid]::NewGuid().ToString('N')).bak"
            [IO.File]::Replace($PartPath, $Target, $createdBackup)
        } else {
            [IO.File]::Move($PartPath, $Target)
        }
        Assert-RuntimeFile $Target $ExpectedHash $ExpectedSize
        if ($PostCommitVerifier -and -not (& $PostCommitVerifier $Target)) {
            throw "提交后校验失败：$Target"
        }
        if ($createdBackup -and (Test-Path -LiteralPath $createdBackup)) {
            Remove-Item -LiteralPath $createdBackup -Force -ErrorAction SilentlyContinue
        }
        return $Target
    } catch {
        if ($targetExisted -and $createdBackup -and (Test-Path -LiteralPath $createdBackup) -and (Test-Path -LiteralPath $Target)) {
            # PS 5.1 把 null 备份参数变成空字符串会让 File.Replace 报「路径格式不对」，
            # 这里给回滚也一个唯一备份名，保证替换仍是原子操作，成功后立刻删掉。
            $rollbackBackup = "$Target.$([Guid]::NewGuid().ToString('N')).rollback.bak"
            [IO.File]::Replace($createdBackup, $Target, $rollbackBackup)
            if (Test-Path -LiteralPath $rollbackBackup) { Remove-Item -LiteralPath $rollbackBackup -Force -ErrorAction SilentlyContinue }
        } elseif (-not $targetExisted -and (Test-Path -LiteralPath $Target)) {
            Remove-Item -LiteralPath $Target -Force -ErrorAction SilentlyContinue
        }
        throw
    }
}

function Copy-LocalRuntimeSource {
    param([string]$Source, [string]$PartPath)
    if (-not (Test-Path -LiteralPath $Source -PathType Leaf)) { throw "本地文件不存在：$Source" }
    if ((Get-Item -LiteralPath $Source).Length -le 0) { throw "本地文件是空的：$Source" }
    $parent = Split-Path -Parent $PartPath
    [void][IO.Directory]::CreateDirectory($parent)
    [IO.File]::Copy($Source, $PartPath, $true)
    return $PartPath
}

function Invoke-RuntimeInstall {
    param(
        [object]$Pins,
        [string]$NodeDestination,
        [string]$FfmpegDestination,
        [string]$NodeSource = "",
        [string]$FfmpegSource = "",
        [switch]$Force,
        [switch]$VerifyOnly,
        [string]$TemporaryRoot = "",
        [scriptblock]$HttpClientFactory = $null,
        [int]$TimeoutSeconds = 60,
        [int[]]$RetryDelaysMilliseconds = @(1000, 2000),
        [scriptblock]$SleepScript = $null
    )

    $deleteTemporaryRoot = $false
    if (-not $TemporaryRoot) {
        $TemporaryRoot = Join-Path $env:TEMP ("kairos-runtime-" + [Guid]::NewGuid().ToString("N"))
        $deleteTemporaryRoot = $true
    }
    if (-not $VerifyOnly) { [void][IO.Directory]::CreateDirectory($TemporaryRoot) }

    $nodePart = $null
    try {
        if ((Test-Path -LiteralPath $NodeDestination -PathType Leaf) -and -not $Force) {
            if (Test-PinnedTarget $Pins.node $NodeDestination) {
                Say "node.exe 已存在且与 pins 匹配，跳过下载"
            } else {
                Assert-ExistingTarget $Pins.node $NodeDestination "node"
            }
        } elseif ($VerifyOnly) {
            if (-not (Test-PinnedTarget $Pins.node $NodeDestination)) { throw "VerifyOnly：node.exe 与 pins 不匹配：$NodeDestination" }
            Say "VerifyOnly：node.exe 校验通过"
        } else {
            if ($NodeSource) {
                $nodePart = New-UniquePartPath $NodeDestination
                [void](Copy-LocalRuntimeSource -Source $NodeSource -PartPath $nodePart)
                Say "已准备本地 node：$NodeSource（未校验固定哈希）"
                [void](Complete-RuntimeFile -PartPath $nodePart -Target $NodeDestination -Force:$Force)
            } else {
                Say "下载固定版本 node：$($Pins.node.url)"
                $nodePart = Invoke-PinnedDownload -Pin $Pins.node -Destination $NodeDestination -TimeoutSeconds $TimeoutSeconds -RetryDelaysMilliseconds $RetryDelaysMilliseconds -HttpClientFactory $HttpClientFactory -SleepScript $SleepScript
                [void](Complete-RuntimeFile -PartPath $nodePart -Target $NodeDestination -ExpectedHash ([string]$Pins.node.sha256) -ExpectedSize ([long]$Pins.node.size) -Force:$Force)
            }
            Say "node.exe 已就位：$NodeDestination"
        }
    } finally {
        if ($nodePart -and (Test-Path -LiteralPath $nodePart)) { Remove-Item -LiteralPath $nodePart -Force -ErrorAction SilentlyContinue }
    }

    $ffmpegPart = $null; $zipPart = $null
    try {
        if ((Test-Path -LiteralPath $FfmpegDestination -PathType Leaf) -and -not $Force) {
            if (Test-PinnedTarget $Pins.ffmpeg $FfmpegDestination) {
                Say "ffmpeg.exe 已存在且与 pins 匹配，跳过下载"
            } else {
                Assert-ExistingTarget $Pins.ffmpeg $FfmpegDestination "ffmpeg"
            }
        } elseif ($VerifyOnly) {
            if (-not (Test-PinnedTarget $Pins.ffmpeg $FfmpegDestination)) { throw "VerifyOnly：ffmpeg.exe 与 pins 不匹配：$FfmpegDestination" }
            Say "VerifyOnly：ffmpeg.exe 校验通过"
        } else {
            if ($FfmpegSource) {
                $ffmpegPart = New-UniquePartPath $FfmpegDestination
                [void](Copy-LocalRuntimeSource -Source $FfmpegSource -PartPath $ffmpegPart)
                Say "已准备本地 ffmpeg：$FfmpegSource（未校验固定哈希）"
                [void](Complete-RuntimeFile -PartPath $ffmpegPart -Target $FfmpegDestination -Force:$Force)
            } else {
                $zipDestination = Join-Path $TemporaryRoot "ffmpeg.zip"
                $zipPin = [pscustomobject]@{
                    url = [string]$Pins.ffmpeg.url
                    sha256 = [string]$Pins.ffmpeg.zipSha256
                    size = [long]$Pins.ffmpeg.zipSize
                }
                Say "下载固定版本 ffmpeg ZIP：$($Pins.ffmpeg.url)"
                $zipPart = Invoke-PinnedDownload -Pin $zipPin -Destination $zipDestination -TimeoutSeconds $TimeoutSeconds -RetryDelaysMilliseconds $RetryDelaysMilliseconds -HttpClientFactory $HttpClientFactory -SleepScript $SleepScript
                $ffmpegPart = New-UniquePartPath $FfmpegDestination
                [void](Expand-PinnedZipEntry -ZipPath $zipPart -EntryName ([string]$Pins.ffmpeg.entryName) -ExpectedHash ([string]$Pins.ffmpeg.ffmpegExeSha256) -ExpectedSize ([long]$Pins.ffmpeg.ffmpegExeSize) -Destination $ffmpegPart)
                [void](Complete-RuntimeFile -PartPath $ffmpegPart -Target $FfmpegDestination -ExpectedHash ([string]$Pins.ffmpeg.ffmpegExeSha256) -ExpectedSize ([long]$Pins.ffmpeg.ffmpegExeSize) -Force:$Force)
            }
            Say "ffmpeg.exe 已就位：$FfmpegDestination"
        }
    } finally {
        if ($ffmpegPart -and (Test-Path -LiteralPath $ffmpegPart)) { Remove-Item -LiteralPath $ffmpegPart -Force -ErrorAction SilentlyContinue }
        if ($zipPart -and (Test-Path -LiteralPath $zipPart)) { Remove-Item -LiteralPath $zipPart -Force -ErrorAction SilentlyContinue }
        if ($deleteTemporaryRoot -and (Test-Path -LiteralPath $TemporaryRoot)) {
            Remove-Item -LiteralPath $TemporaryRoot -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
    return $true
}

if ($FunctionOnly) { return }

$repo = Split-Path -Parent $PSScriptRoot
if (-not $NodeDestination) { $NodeDestination = Join-Path $repo "sidecar\node\node.exe" }
if (-not $FfmpegDestination) { $FfmpegDestination = Join-Path $repo "sidecar\rift\bin\ffmpeg.exe" }
$usingDefaultTargets = ($NodeDestination -eq (Join-Path $repo "sidecar\node\node.exe")) -and ($FfmpegDestination -eq (Join-Path $repo "sidecar\rift\bin\ffmpeg.exe"))
if (-not $VerifyOnly -and $usingDefaultTargets -and -not (Test-Path (Join-Path $repo "sidecar\rift\tools\bili.mjs"))) {
    throw "sidecar\rift 不在。请先把 MIT 许可的 dsh-rift 包放到 sidecar\rift（见 docs\THIRD-PARTY.md）。"
}

$pins = Get-RuntimePins $PinsPath
[void](Invoke-RuntimeInstall -Pins $pins -NodeDestination $NodeDestination -FfmpegDestination $FfmpegDestination -NodeSource $NodeExe -FfmpegSource $FfmpegExe -Force:$Force -VerifyOnly:$VerifyOnly -TemporaryRoot $TemporaryRoot)

Say ("node.exe   {0:N0} MB" -f ((Get-Item -LiteralPath $NodeDestination).Length / 1MB))
Say ("ffmpeg.exe {0:N0} MB" -f ((Get-Item -LiteralPath $FfmpegDestination).Length / 1MB))
Say "完成。现在可以运行 build.bat 和 installer\build-installer.bat。"
