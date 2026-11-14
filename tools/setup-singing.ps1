# 冥零本地唱歌：独立环境，安装/启动/健康检查/本机试听，不发送 QQ 消息。
# H-04：源码固定 commit + 归档哈希，模型固定 SHA256，真实 torch.load 强制 weights_only=True。
param(
    [string]$Base = (Split-Path -Parent $PSScriptRoot),
    [string]$Root = '',
    [ValidateSet('install','start','check','test','voice_existing','voice_breeze')][string]$Action = 'check',
    [string]$ModelSha256 = '',
    [string]$ComponentsPath = '',
    [string]$SourceArchive = '',
    [switch]$FunctionOnly
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$script:SoulxSourcePin = [pscustomobject]@{
    Commit = '81aeb3ae772c70093c3de74dc23c92d983801ae4'
    Url = 'https://codeload.github.com/Soul-AILab/SoulX-Singer/tar.gz/81aeb3ae772c70093c3de74dc23c92d983801ae4'
    Sha256 = 'b1a1e9615f8832012396aa306a5de5a6157f4c193c4ce08b3ee35a7983dcee7b'
    Size = 12975795L
    # 对归档内 cli/inference.py 打完 weights_only=True 补丁后的规范哈希。
    # 与 tools/components.json 的 soulx-singer.source.loaderSha256、sing_server.py 的
    # _EXPECTED_LOADER_SHA256 三处必须一致；上游文件一变这里就会拦下来。
    LoaderSha256 = 'b41d688f68827df649b301da10105a4db89e348ad44fc76de280a1cb46d562a2'
}

function Get-SingingComponent([string]$Path = '') {
    if (-not $Path) { $Path = Join-Path (Split-Path -Parent $PSScriptRoot) 'components.json' }
    if (-not (Test-Path -LiteralPath $Path)) { throw "找不到组件清单：$Path" }
    try { $manifest = Get-Content -LiteralPath $Path -Raw -Encoding UTF8 | ConvertFrom-Json }
    catch { throw "组件清单解析失败：$($_.Exception.Message)" }
    $component = @($manifest.components) | Where-Object { $_.id -eq 'soulx-singer' } | Select-Object -First 1
    if (-not $component) { throw "组件清单里没有 soulx-singer" }
    return $component
}

function Resolve-SingingModelPin([object]$Component, [string]$ParameterSha256 = '') {
    $manifestHash = ''
    $manifestSize = 0L
    if ($Component.model -and $Component.model.sha256) {
        $manifestHash = ([string]$Component.model.sha256).Trim().ToLowerInvariant()
        $manifestSize = [long]$Component.model.size
    }
    $parameterHash = ''
    if ($ParameterSha256) { $parameterHash = ([string]$ParameterSha256).Trim().ToLowerInvariant() }
    if ($manifestHash -and $manifestHash -notmatch '^[0-9a-f]{64}$') { throw "组件清单的模型 sha256 格式不对：$manifestHash" }
    if ($parameterHash -and $parameterHash -notmatch '^[0-9a-f]{64}$') { throw "-ModelSha256 格式不对：$parameterHash" }
    if ($manifestHash -and $parameterHash -and $manifestHash -ne $parameterHash) {
        throw "-ModelSha256 与组件清单不一致（清单 $manifestHash，参数 $parameterHash）"
    }
    if ($manifestHash) {
        if ($manifestSize -le 0) { throw "组件清单缺少模型 size" }
        return [pscustomobject]@{ Sha256 = $manifestHash; Size = $manifestSize; Source = 'manifest' }
    }
    if ($parameterHash) {
        return [pscustomobject]@{ Sha256 = $parameterHash; Size = 0L; Source = 'parameter' }
    }
    throw '请提供模型哈希（-ModelSha256 或 components.json 的 model.sha256），没有哈希不会自动下载模型。'
}

function Get-SingingSha256([string]$Path) {
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Assert-SingingFile([string]$Path, [string]$ExpectedHash, [long]$ExpectedSize = 0) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "文件不存在：$Path" }
    $item = Get-Item -LiteralPath $Path
    if ($ExpectedSize -gt 0 -and $item.Length -ne $ExpectedSize) {
        throw "文件大小不对：$Path（期望 $ExpectedSize，实际 $($item.Length)）"
    }
    $actual = Get-SingingSha256 $Path
    if ($actual -ne $ExpectedHash.ToLowerInvariant()) {
        throw "文件哈希不对：$Path（期望 $ExpectedHash，实际 $actual）"
    }
}

function Read-SingingPinFile([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) { return $null }
    try { return Get-Content -LiteralPath $Path -Raw -Encoding UTF8 | ConvertFrom-Json }
    catch { throw "安装记录解析失败：$Path" }
}

function Write-SingingPinFile([string]$Path, [object]$Data) {
    $parent = Split-Path -Parent $Path
    [void][IO.Directory]::CreateDirectory($parent)
    $temporary = "$Path.$([Guid]::NewGuid().ToString('N').Substring(0, 8)).tmp"
    $backup = ''
    try {
        $json = $Data | ConvertTo-Json -Depth 6
        [IO.File]::WriteAllText($temporary, $json, (New-Object Text.UTF8Encoding($false)))
        if (Test-Path -LiteralPath $Path) {
            $backup = "$Path.$([Guid]::NewGuid().ToString('N').Substring(0, 8)).bak"
            [IO.File]::Replace($temporary, $Path, $backup)
        } else {
            [IO.File]::Move($temporary, $Path)
        }
        if ($backup -and (Test-Path -LiteralPath $backup)) { Remove-Item -LiteralPath $backup -Force -ErrorAction SilentlyContinue }
    } finally {
        if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Force -ErrorAction SilentlyContinue }
    }
}

function Test-SingingTarEntryName([string]$EntryName) {
    if ([string]::IsNullOrEmpty($EntryName)) { return $false }
    $normalized = $EntryName -replace '\\', '/'
    if ($normalized.StartsWith('/') -or $normalized -match '^[A-Za-z]:') { return $false }
    $work = $normalized.TrimEnd('/')
    if (-not $work) { return $false }
    if ($work.StartsWith('//')) { return $false }
    # Windows 侧边界：段内盘符/ADS 冒号、尾随点或空格、保留设备名都不接受，
    # 即使当前 bsdtar 会把它们清洗掉，校验器也要自己拒。
    $reserved = '^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(\..*)?$'
    foreach ($segment in @($work -split '/')) {
        if ($segment -eq '' -or $segment -eq '.' -or $segment -eq '..') { return $false }
        if ($segment.Contains(':')) { return $false }
        if ($segment.EndsWith('.') -or $segment.EndsWith(' ')) { return $false }
        if ($segment -match $reserved) { return $false }
    }
    return $true
}

function Invoke-SingingTar([string[]]$Arguments) {
    $output = @(& tar.exe @Arguments 2>$null)
    if ($LASTEXITCODE -ne 0) { throw "tar 执行失败（$LASTEXITCODE）：$($Arguments -join ' ')" }
    return @($output)
}

function Test-SingingArchiveEntries([string]$ArchivePath) {
    $names = Invoke-SingingTar @('-tzf', $ArchivePath)
    $verbose = Invoke-SingingTar @('-tvzf', $ArchivePath)
    if ($names.Count -ne $verbose.Count) { throw 'tar 列表与详细列表不一致，拒绝解压' }
    $expectedTop = 'SoulX-Singer-' + $script:SoulxSourcePin.Commit
    for ($i = 0; $i -lt $names.Count; $i++) {
        $name = [string]$names[$i]
        if (-not (Test-SingingTarEntryName $name)) { throw "tar 条目路径不安全：$name" }
        $work = ($name -replace '\\', '/').TrimEnd('/')
        $top = @($work -split '/')[0]
        if ($top -ne $expectedTop) { throw "tar 必须只有唯一顶层目录 $expectedTop，实际：$top" }
        $type = [string]$verbose[$i]
        if ($type.Length -eq 0 -or ($type[0] -ne '-' -and $type[0] -ne 'd')) {
            throw "tar 含链接或特殊文件，拒绝解压：$name"
        }
    }
    return $expectedTop
}

function Expand-SingingArchive([string]$ArchivePath, [string]$Destination) {
    Assert-SingingFile $ArchivePath $script:SoulxSourcePin.Sha256 $script:SoulxSourcePin.Size
    $expectedTop = Test-SingingArchiveEntries $ArchivePath
    $parent = Split-Path -Parent $Destination
    [void][IO.Directory]::CreateDirectory($parent)
    $stagingRoot = "$Destination.staging-$([Guid]::NewGuid().ToString('N').Substring(0, 8))"
    [void][IO.Directory]::CreateDirectory($stagingRoot)
    try {
        $null = Invoke-SingingTar @('-xzf', $ArchivePath, '-C', $stagingRoot)
        $topPath = Join-Path $stagingRoot $expectedTop
        if (-not (Test-Path -LiteralPath $topPath -PathType Container)) { throw "解压后找不到唯一顶层目录：$topPath" }
        foreach ($required in @('cli\inference.py', 'soulxsinger\config\soulxsinger.yaml')) {
            if (-not (Test-Path -LiteralPath (Join-Path $topPath $required) -PathType Leaf)) { throw "固定源码缺少必需文件：$required" }
        }
        return $topPath
    } catch {
        if (Test-Path -LiteralPath $stagingRoot) { Remove-Item -LiteralPath $stagingRoot -Recurse -Force -ErrorAction SilentlyContinue }
        throw
    }
}

function Patch-SingingLoader([string]$LoaderPath) {
    if (-not (Test-Path -LiteralPath $LoaderPath -PathType Leaf)) { throw "找不到上游加载器：$LoaderPath" }
    $old = 'checkpoint = torch.load(model_path, weights_only=False, map_location="cpu")'
    $new = 'checkpoint = torch.load(model_path, weights_only=True, map_location="cpu")'
    $cliDir = Split-Path -Parent $LoaderPath
    # 服务路径是 cli/inference.py，但归档里 cli/inference_svc.py 也有同一个 torch.load，
    # webui_svc.py / example/infer_svc.sh 会用到它，所以两处都补，再全目录复查。
    foreach ($name in @('inference.py', 'inference_svc.py')) {
        $path = Join-Path $cliDir $name
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            if ($name -eq 'inference.py') { throw "找不到上游加载器：$path" }
            continue
        }
        $text = [IO.File]::ReadAllText($path, [Text.Encoding]::UTF8)
        if ($text.Contains($old)) { $text = $text.Replace($old, $new) }
        elseif (-not $text.Contains($new)) { throw "上游 cli\$name 的 torch.load 原文不匹配，拒绝盲改" }
        if ($text.Contains('weights_only=False')) { throw "上游 cli\$name 仍有 weights_only=False" }
        [IO.File]::WriteAllText($path, $text, (New-Object Text.UTF8Encoding($false)))
    }
    foreach ($file in @(Get-ChildItem -LiteralPath $cliDir -Filter '*.py' -File)) {
        if ([IO.File]::ReadAllText($file.FullName, [Text.Encoding]::UTF8).Contains('weights_only=False')) {
            throw "上游加载器仍有 weights_only=False：$($file.Name)"
        }
    }
    $sha = Get-SingingSha256 $LoaderPath
    if ($sha -ne $script:SoulxSourcePin.LoaderSha256) {
        throw "加载器补丁后哈希不对：期望 $($script:SoulxSourcePin.LoaderSha256)，实际 $sha（上游 cli/inference.py 变了）"
    }
    return [pscustomobject]@{ LoaderSha256 = $sha }
}

function Install-SingingSource {
    param(
        [string]$Source,
        [string]$LocalArchive = '',
        [string]$ComponentsPath = '',
        [scriptblock]$Downloader = $null
    )
    $parent = Split-Path -Parent $Source
    [void][IO.Directory]::CreateDirectory($parent)
    $markerPath = Join-Path $Source '.mingling-source.json'
    if (Test-Path -LiteralPath $Source) {
        $children = @(Get-ChildItem -LiteralPath $Source -Force)
        if (Test-Path -LiteralPath $markerPath) {
            $record = Read-SingingPinFile $markerPath
            if ([string]$record.sourceCommit -ne $script:SoulxSourcePin.Commit) { throw "源码 commit 不对：期望 $($script:SoulxSourcePin.Commit)，实际 $($record.sourceCommit)" }
            if ([string]$record.archiveSha256 -ne $script:SoulxSourcePin.Sha256) { throw "源码归档哈希不对：期望 $($script:SoulxSourcePin.Sha256)，实际 $($record.archiveSha256)" }
            if ([string]$record.loaderSha256 -ne $script:SoulxSourcePin.LoaderSha256) { throw "源码加载器哈希与固定 pin 不一致：$($record.loaderSha256)" }
            $loader = Join-Path $Source 'cli\inference.py'
            Assert-SingingFile $loader ([string]$record.loaderSha256) 0
            if ([IO.File]::ReadAllText($loader, [Text.Encoding]::UTF8).Contains('weights_only=False')) { throw '源码加载器被改回 weights_only=False' }
            return $record
        }
        if (Test-Path -LiteralPath (Join-Path $Source '.git')) {
            $head = (& git.exe -C $Source rev-parse HEAD 2>$null | Select-Object -First 1)
            if ($LASTEXITCODE -ne 0) { throw '无法读取已有源码的 git commit' }
            $head = "$head".Trim()
            if ($head -ne $script:SoulxSourcePin.Commit) { throw "源码 commit 不对：期望 $($script:SoulxSourcePin.Commit)，实际 $head" }
            # rev-parse 只解析 ref，不证明对象存在、也不证明工作树等于该 commit。
            & git.exe -C $Source cat-file -e "$head^{commit}" 2>$null
            if ($LASTEXITCODE -ne 0) { throw "git 对象缺失或伪造：$head" }
            & git.exe -C $Source diff-index --quiet --no-ext-diff HEAD -- 2>$null
            if ($LASTEXITCODE -ne 0) { throw '工作树与固定 commit 不一致（dirty），拒绝复用' }
            foreach ($rel in @('cli\inference.py', 'soulxsinger\config\soulxsinger.yaml')) {
                if (-not (Test-Path -LiteralPath (Join-Path $Source $rel) -PathType Leaf)) { throw "源码缺少必需文件：$rel" }
            }
            $patch = Patch-SingingLoader (Join-Path $Source 'cli\inference.py')
            $record = [pscustomobject]@{ sourceCommit = $script:SoulxSourcePin.Commit; archiveSha256 = $script:SoulxSourcePin.Sha256; loaderSha256 = $patch.LoaderSha256 }
            Write-SingingPinFile $markerPath $record
            return $record
        }
        if ($children.Count -gt 0) { throw "源码目录已存在且没有可信标记：$Source。请人工确认后移走再部署。" }
        Remove-Item -LiteralPath $Source -Force
    }

    if (-not $Downloader) {
        $Downloader = {
            param([string]$Url, [string]$OutFile, [string]$Sha256, [long]$ExpectedBytes)
            Get-File -Urls @($Url) -OutFile $OutFile -MinBytes 1 -Sha256 $Sha256 -ExpectedBytes $ExpectedBytes | Out-Null
        }
    }
    $temporaryArchive = $null
    $stagingRoot = $null
    try {
        if ($LocalArchive) {
            $archivePath = $LocalArchive
        } else {
            $archivePath = Join-Path $parent ("SoulX-Singer-$($script:SoulxSourcePin.Commit).$([Guid]::NewGuid().ToString('N').Substring(0, 8)).tar.gz")
            $temporaryArchive = $archivePath
            & $Downloader -Url $script:SoulxSourcePin.Url -OutFile $archivePath -Sha256 $script:SoulxSourcePin.Sha256 -ExpectedBytes $script:SoulxSourcePin.Size
        }
        Assert-SingingFile $archivePath $script:SoulxSourcePin.Sha256 $script:SoulxSourcePin.Size
        $topPath = Expand-SingingArchive $archivePath $Source
        $stagingRoot = Split-Path -Parent $topPath
        $patch = Patch-SingingLoader (Join-Path $topPath 'cli\inference.py')
        $record = [pscustomobject]@{ sourceCommit = $script:SoulxSourcePin.Commit; archiveSha256 = $script:SoulxSourcePin.Sha256; loaderSha256 = $patch.LoaderSha256 }
        Write-SingingPinFile (Join-Path $topPath '.mingling-source.json') $record
        [IO.Directory]::Move($topPath, $Source)
        return $record
    } finally {
        if ($stagingRoot -and (Test-Path -LiteralPath $stagingRoot)) { Remove-Item -LiteralPath $stagingRoot -Recurse -Force -ErrorAction SilentlyContinue }
        if ($temporaryArchive -and (Test-Path -LiteralPath $temporaryArchive)) { Remove-Item -LiteralPath $temporaryArchive -Force -ErrorAction SilentlyContinue }
    }
}

function Install-SingingModel {
    param(
        [string]$ModelPath,
        [object]$Pin,
        [string[]]$Urls,
        [scriptblock]$Downloader = $null
    )
    if (Test-Path -LiteralPath $ModelPath) {
        Assert-SingingFile $ModelPath $Pin.Sha256 $Pin.Size
        return $true
    }
    if (-not $Pin) { throw '没有有效模型哈希，拒绝下载。' }
    if (-not $Downloader) {
        $Downloader = {
            param([string]$Url, [string]$OutFile, [string]$Sha256, [long]$ExpectedBytes)
            Get-File -Urls @($Url) -OutFile $OutFile -MinBytes 1 -Sha256 $Sha256 -ExpectedBytes $ExpectedBytes | Out-Null
        }
    }
    $parent = Split-Path -Parent $ModelPath
    [void][IO.Directory]::CreateDirectory($parent)
    $failures = @()
    for ($index = 0; $index -lt $Urls.Count; $index++) {
        $url = [string]$Urls[$index]
        $partPath = "$ModelPath.model.$($Pin.Sha256.Substring(0, 12)).$index.part"
        $downloadReturned = $false
        try {
            & $Downloader -Url $url -OutFile $partPath -Sha256 $Pin.Sha256 -ExpectedBytes $Pin.Size
            $downloadReturned = $true
            Assert-SingingFile $partPath $Pin.Sha256 $Pin.Size
        } catch {
            # 下载器没返回 = 网络中断，保留当前源断点；返回后校验失败 = 内容不对，删除该源暂存。
            if ($downloadReturned -and (Test-Path -LiteralPath $partPath)) {
                Remove-Item -LiteralPath $partPath -Force -ErrorAction SilentlyContinue
            }
            $failures += "$url：$($_.Exception.Message)"
            continue
        }
        [IO.File]::Move($partPath, $ModelPath)
        Assert-SingingFile $ModelPath $Pin.Sha256 $Pin.Size
        return $true
    }
    throw ("模型下载未完成：{0}" -f ($failures -join '；'))
}

function Assert-SingingInstallation {
    param(
        [string]$Source,
        [string]$Model,
        [string]$InstalledPath,
        [object]$Component
    )
    $record = Read-SingingPinFile $InstalledPath
    if (-not $record) { throw "缺少安装记录：$InstalledPath。请重新运行部署以校验旧安装并升级元数据。" }
    foreach ($field in @('sourceCommit','modelSha256','modelHashSource','loaderSha256')) {
        if (-not $record.PSObject.Properties[$field] -or -not "$($record.$field)".Trim()) { throw "安装记录缺少 $field，请重新部署以升级元数据。" }
    }
    $marker = Read-SingingPinFile (Join-Path $Source '.mingling-source.json')
    if (-not $marker) { throw "源码缺少固定标记：$Source" }
    if ([string]$marker.sourceCommit -ne [string]$record.sourceCommit) { throw "源码 commit 与安装记录不一致：$($marker.sourceCommit)" }
    if ([string]$marker.loaderSha256 -ne [string]$record.loaderSha256) { throw "源码加载器哈希与安装记录不一致" }
    if ([string]$marker.sourceCommit -ne $script:SoulxSourcePin.Commit) { throw "源码 commit 不是固定值：$($marker.sourceCommit)" }
    $loader = Join-Path $Source 'cli\inference.py'
    Assert-SingingFile $loader ([string]$record.loaderSha256) 0
    if ([IO.File]::ReadAllText($loader, [Text.Encoding]::UTF8).Contains('weights_only=False')) { throw '真实模型加载路径仍是 weights_only=False' }

    $expectedHash = [string]$record.modelSha256
    $expectedSize = 0L
    if ($Component.model -and $Component.model.sha256) {
        $expectedHash = ([string]$Component.model.sha256).ToLowerInvariant()
        $expectedSize = [long]$Component.model.size
        if ([string]$record.modelSha256 -ne $expectedHash) { throw "安装记录的模型哈希与组件清单不一致：$($record.modelSha256)" }
    }
    Assert-SingingFile $Model $expectedHash $expectedSize
    return $record
}

if ($FunctionOnly) { return }

. (Join-Path $PSScriptRoot 'common.ps1')
[Console]::OutputEncoding = New-Object Text.UTF8Encoding($false)
$Base = [IO.Path]::GetFullPath($Base)
$repoRoot = Split-Path -Parent $PSScriptRoot
$assets = Join-Path $repoRoot 'pack\mingling-pack\plugins\astrbot_plugin_dafeiyu_sing\sing_server'
$logDir = Join-Path $Base 'logs'
New-Item -ItemType Directory -Path $logDir -Force | Out-Null
$progressFile = Join-Path $logDir 'singing-progress.json'
$settingsPath = Join-Path $Base 'private\feature-settings.json'
$settings = $null
if (Test-Path -LiteralPath $settingsPath) { $settings = Get-Content -LiteralPath $settingsPath -Raw -Encoding UTF8 | ConvertFrom-Json }
if (-not $Root -and $settings.sing.root) { $Root = [string]$settings.sing.root }
if (-not $Root) { $Root = Join-Path $Base 'sing' }  # 和 components.json 的 detect 路径、说明书里的 -Root 保持一致
$Root = [IO.Path]::GetFullPath($Root)
$source = Join-Path $Root 'SoulX-Singer'
$python = Join-Path $Root 'venv\Scripts\python.exe'
$model = Join-Path $Root 'models\SoulX-Singer\model.pt'
$service = Join-Path $Root 'sing_server.py'
$installedPath = Join-Path $Root 'installed.json'
$url = 'http://127.0.0.1:18620'
if ($settings.sing.server_url) { $url = ([string]$settings.sing.server_url).TrimEnd('/') }
$serviceUri = [Uri]$url
$servicePort = $serviceUri.Port
if (-not $ComponentsPath) { $ComponentsPath = Join-Path $repoRoot 'tools\components.json' }
$component = Get-SingingComponent $ComponentsPath

function Progress-Step([int]$pct, [string]$text) {
    @{pct=$pct; text=$text; action=$Action} | ConvertTo-Json | Set-Content -LiteralPath $progressFile -Encoding UTF8
    Write-Output "##STEP $pct|$text"
}
function Invoke-Native([string]$program, [string[]]$Arguments) {
    # Windows PowerShell 5.1 把 git/curl 的进度 stderr 包装成错误记录；以退出码判断成败。
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        & $program @Arguments 2>&1 | ForEach-Object { "$($_)" | Add-Content -LiteralPath "$logDir\singing-install.log" -Encoding UTF8 }
        $nativeCode = $LASTEXITCODE
    } finally { $ErrorActionPreference = $previousPreference }
    if ($nativeCode -ne 0) { throw "命令执行失败（$nativeCode），详情见 logs\singing-install.log" }
}
function Health {
    try {
        $h = Invoke-RestMethod "$url/health" -TimeoutSec 3
        if ($h.ok -and 'soulx' -in $h.backends) { return $h }
    } catch {}
    return $null
}
try {
    if ($Action -eq 'voice_existing' -or $Action -eq 'voice_breeze') {
        if (-not (Test-Path -LiteralPath $python)) { throw '请先部署本地唱歌服务' }
        Progress-Step 5 '准备现有冥零音色的对齐工具'
        $uv = @("$Base\bots\uv\bin\uv.exe", "$Base\uv\bin\uv.exe") | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
        if (-not $uv) { $uvCommand = Get-Command uv.exe -ErrorAction SilentlyContinue; if ($uvCommand) { $uv = $uvCommand.Source } }
        if (-not $uv) { throw '找不到 uv，请先补齐运行环境' }
        Invoke-Native $uv @('pip','install','--python',$python,'funasr==1.3.0','modelscope','torch==2.7.1+cu128','torchaudio==2.7.1+cu128','--extra-index-url','https://download.pytorch.org/whl/cu128','--index-strategy','unsafe-best-match')
        Progress-Step 30 '制作冥零唱歌参考，不会创建新音色'
        $voiceArguments = @('-X','utf8',"$PSScriptRoot\prepare-singing-voice.py",'--base',$Base,'--root',$Root)
        if ($Action -eq 'voice_breeze') { $voiceArguments += '--from-breeze' }
        Invoke-Native $python $voiceArguments
        Progress-Step 100 '现有冥零音色已用于本地唱歌'
        Write-Output 'RESULT ok=1 why=现有冥零音色已用于本地唱歌'
        exit 0
    }
    if ($Action -eq 'install') {
        Progress-Step 2 '准备独立的本地唱歌环境'
        New-Item -ItemType Directory -Path $Root,"$Root\models\SoulX-Singer" -Force | Out-Null
        Progress-Step 5 '下载并校验固定 commit 的 SoulX-Singer 源码'
        $sourceRecord = Install-SingingSource -Source $source -LocalArchive $SourceArchive -ComponentsPath $ComponentsPath
        $uv = @("$Base\bots\uv\bin\uv.exe", "$Base\bots\AstrBot\bin\uv.exe", "$Base\uv\bin\uv.exe") |
            Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
        if (-not $uv) { $uvCommand = Get-Command uv.exe -ErrorAction SilentlyContinue; if ($uvCommand) { $uv = $uvCommand.Source } }
        if (-not $uv) { throw '找不到 uv，请先运行工作台的一键自动配置补齐运行环境' }
        if (-not (Test-Path -LiteralPath $python)) { Invoke-Native $uv @('venv',"$Root\venv",'--python','3.10') }
        Progress-Step 18 '安装支持 RTX 50 系列的 PyTorch CUDA 12.8 环境'
        Invoke-Native $uv @('pip','install','--python',$python,'torch==2.7.1','torchaudio==2.7.1','--index-url','https://download.pytorch.org/whl/cu128')
        Progress-Step 42 '安装本地唱歌所需依赖'
        Invoke-Native $uv @('pip','install','--python',$python,'-r',"$assets\requirements.txt",'einops','rotary-embedding-torch','beartype','huggingface-hub<1')
        Progress-Step 55 '下载并校验 SoulX-Singer 模型（约 2.8 GB，可断点续传）'
        $modelPin = Resolve-SingingModelPin $component $ModelSha256
        Install-SingingModel -ModelPath $model -Pin $modelPin -Urls @($component.model.urls) | Out-Null
        Progress-Step 86 '安装服务文件与参考声音'
        foreach ($name in @('sing_server.py','melodies.py')) { Copy-Item -LiteralPath "$assets\$name" -Destination "$Root\$name" -Force }
        $hasVoice = Test-Path -LiteralPath "$Root\voice\prompt.wav"
        $legacyVoice = $hasVoice -and (Test-Path -LiteralPath "$Root\voice\prompt.json") -and
            ((Get-FileHash -LiteralPath "$Root\voice\prompt.wav").Hash -eq '7CBC3550CBD6021CD590D9027BC1A1283051D8CDDA9FFF043F577AB74A586AEE') -and
            ((Get-FileHash -LiteralPath "$Root\voice\prompt.json").Hash -eq '3B3FB352BB19F0B0C02F9C9468A8AE4D8121307D576646E8A652CBDDBD2C2DA6')
        if ($legacyVoice) {
            $voiceBackup = Join-Path $Base ('旧版\singing-legacy-voice-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
            New-Item -ItemType Directory -Path $voiceBackup -Force | Out-Null
            Copy-Item -LiteralPath "$Root\voice" -Destination "$voiceBackup\voice" -Recurse
        }
        # 只迁移完整匹配的旧内置参考，保留自定义的音频和标注。
        if (-not $hasVoice -or $legacyVoice) {
            New-Item -ItemType Directory -Path "$Root\voice" -Force | Out-Null
            Get-ChildItem -LiteralPath "$assets\voice" -File | Copy-Item -Destination "$Root\voice" -Force
        }
        # 安装目录自带启动入口，路径始终相对该目录，不依赖 D 盘。服务自身会校验 pin。
        $launcher = '@echo off' + "`r`n" + '"%~dp0venv\Scripts\python.exe" -X utf8 "%~dp0sing_server.py" --soulx "%~dp0SoulX-Singer" --model "%~dp0models\SoulX-Singer\model.pt" --port ' + $servicePort + "`r`npause`r`n"
        [IO.File]::WriteAllText("$Root\start-singing.cmd", $launcher, (New-Object Text.UTF8Encoding($false)))
        Progress-Step 92 '验证显卡计算与安全加载代码'
        $env:PYTHONPATH = $source
        Invoke-Native $python @('-X','utf8',"$PSScriptRoot\check-singing-env.py",'--source',$source)
        $installed = [pscustomobject]@{
            root = $Root; python = $python; model = $model; at = (Get-Date -Format s)
            sourceCommit = $sourceRecord.sourceCommit
            modelSha256 = $modelPin.Sha256
            modelHashSource = $modelPin.Source
            loaderSha256 = $sourceRecord.loaderSha256
        }
        Write-SingingPinFile $installedPath $installed
        Progress-Step 100 '本地唱歌服务部署完成，可启动并试听'
        Write-Output 'RESULT ok=1 why=本地唱歌部署完成 next=启动并试听'
        exit 0
    }
    if ($Action -eq 'start') {
        # Health 早退不能绕过 pin 校验：先确认当前源码、加载器和模型都没被换。
        Assert-SingingInstallation -Source $source -Model $model -InstalledPath $installedPath -Component $component | Out-Null
        if (Health) { Progress-Step 100 '本地唱歌已经在运行'; Write-Output 'RESULT ok=1 why=本地唱歌已运行'; exit 0 }
        if (-not (Test-Path -LiteralPath $python) -or -not (Test-Path -LiteralPath $model) -or -not (Test-Path -LiteralPath $service)) { throw '本地唱歌尚未部署完整，请先点部署本地服务' }
        if (-not $serviceUri.IsLoopback -or $serviceUri.Scheme -ne 'http') { throw '此按钮启动本机 HTTP 服务；远程或 HTTPS 地址请在对应机器启动服务' }
        Progress-Step 10 '加载并校验本地唱歌模型'
        $serverArgs = @('-X','utf8',"`"$service`"",'--soulx',"`"$source`"",'--model',"`"$model`"",'--port',"$servicePort")
        $process = Start-Process -FilePath $python -ArgumentList $serverArgs -WorkingDirectory $Root -WindowStyle Hidden `
            -RedirectStandardOutput "$logDir\singing-server.out.log" -RedirectStandardError "$logDir\singing-server.err.log" -PassThru
        for ($attempt=0; $attempt -lt 120; $attempt++) {
            $h = Health
            if ($h) { Progress-Step 100 ("本地唱歌已运行，设备：" + $h.device); Write-Output 'RESULT ok=1 why=本地唱歌已运行'; exit 0 }
            if ($process.HasExited) { throw '本地唱歌启动失败，请查看 logs\singing-server.err.log' }
            Start-Sleep -Seconds 1
        }
        throw '模型仍未就绪，请查看 logs\singing-server.err.log，稍后检测'
    }
    $health = Health
    if (-not $health) { throw '本地唱歌服务未就绪，请先部署并启动' }
    if ($Action -eq 'test') {
        Progress-Step 15 '正在本机生成试听音频，不发送 QQ 消息'
        $testBody = @{backend='soulx';melody='生日快乐';lyrics=''} | ConvertTo-Json -Compress
        $res = Invoke-RestMethod "$url/sing" -Method Post -Body ([Text.Encoding]::UTF8.GetBytes($testBody)) -ContentType 'application/json' -TimeoutSec 540
        if (-not $res.ok -or -not (Test-Path -LiteralPath $res.path)) { throw '未生成有效的本地试听文件' }
        Progress-Step 100 ('试听文件：' + $res.path)
        Write-Output ('RESULT ok=1 why=本地试听生成成功 file=' + $res.path)
    } else {
        Progress-Step 100 ('本地唱歌正常，设备：' + $health.device)
        Write-Output 'RESULT ok=1 why=本地唱歌健康检查通过'
    }
} catch {
    $message = ([string]$_.Exception.Message) -replace "\r?\n", ' '
    try { Progress-Step 0 $message } catch {}
    Write-Output ('RESULT ok=0 why=' + $message)
    exit 1
}
