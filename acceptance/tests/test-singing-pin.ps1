# WB-03 offline acceptance tests. Run with Windows PowerShell 5.1.
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $repo 'tools\setup-singing.ps1') -FunctionOnly

$script:CaseCount = 0
$script:FailureCount = 0

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

function Run-Case([string]$Name, [scriptblock]$Body) {
    $script:CaseCount++
    try {
        & $Body | Out-Null
        Write-Host "PASS $Name"
    } catch {
        $script:FailureCount++
        Write-Host "FAIL $Name :: $($_.Exception.Message)"
    }
}

function Get-TestHash([byte[]]$Bytes) {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($Bytes))).Replace('-', '').ToLowerInvariant() }
    finally { $sha.Dispose() }
}

function New-TestRoot([string]$Name) {
    $path = Join-Path $TestRoot ($Name + '-' + [Guid]::NewGuid().ToString('N').Substring(0, 8))
    [void](New-Item -ItemType Directory -Path $path)
    return $path
}

$createTarCode = @'
import io
import sys
import tarfile

path = sys.argv[1]
items = sys.argv[2:]
with tarfile.open(path, 'w:gz') as tar:
    for item in items:
        if item.startswith('SYMLINK:'):
            name, target = item[len('SYMLINK:'):].split('=', 1)
            info = tarfile.TarInfo(name)
            info.type = tarfile.SYMTYPE
            info.linkname = target
            tar.addfile(info)
        elif item.startswith('HARDLINK:'):
            name, target = item[len('HARDLINK:'):].split('=', 1)
            info = tarfile.TarInfo(name)
            info.type = tarfile.LNKTYPE
            info.linkname = target
            tar.addfile(info)
        else:
            data = b'fixture-data'
            info = tarfile.TarInfo(item)
            info.size = len(data)
            tar.addfile(info, io.BytesIO(data))
'@

function New-TarFixture([string]$Path, [string[]]$Items) {
    & python -c $createTarCode $Path @Items
    if ($LASTEXITCODE -ne 0) { throw '生成 tar 夹具失败' }
}

$TestRoot = Join-Path ([IO.Path]::GetTempPath()) ('wb03-tests-' + [Guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $TestRoot)
$realArchive = Join-Path $repo '.workbuddy\acceptance\SoulX-Singer-81aeb3a.tar.gz'
try {
    Run-Case 'model-pin-resolution' {
        $body = [byte[]](1..96)
        $component = [pscustomobject]@{ model = [pscustomobject]@{ sha256 = (Get-TestHash $body); size = $body.Length } }
        $pin = Resolve-SingingModelPin $component ''
        Assert-True ($pin.Source -eq 'manifest') 'manifest pin source mismatch'
        Assert-True ($pin.Sha256 -eq (Get-TestHash $body)) 'manifest pin hash mismatch'
        $same = Resolve-SingingModelPin $component (Get-TestHash $body)
        Assert-True ($same.Source -eq 'manifest') 'matching parameter changed source'
        $thrown = $false
        try { Resolve-SingingModelPin $component ('a' * 64) } catch { $thrown = $true }
        Assert-True $thrown 'parameter mismatch was accepted'
        $thrown = $false
        try { Resolve-SingingModelPin ([pscustomobject]@{}) '' } catch { $thrown = $true }
        Assert-True $thrown 'missing pin was accepted'
    }

    Run-Case 'tar-windows-boundary-names' {
        $top = 'SoulX-Singer-81aeb3ae772c70093c3de74dc23c92d983801ae4'
        foreach ($bad in @("$top/C:/evil.txt", "$top/foo:ads", "$top/CON", "$top/name.", "$top/.. /x", "//server/share/x", '')) {
            Assert-True (-not (Test-SingingTarEntryName $bad)) ("unsafe tar entry was accepted: " + $bad)
        }
        foreach ($good in @("$top/cli/inference.py", "$top/lpt10/x", "$top/console.txt")) {
            Assert-True (Test-SingingTarEntryName $good) ("safe tar entry was rejected: " + $good)
        }
        $root = New-TestRoot 'tar-windows-boundary'
        $tarPath = Join-Path $root 'fixture.tar.gz'
        New-TarFixture $tarPath @("$top/cli/inference.py", "$top/C:/evil.txt")
        $thrown = $false
        try { Test-SingingArchiveEntries $tarPath | Out-Null } catch { $thrown = $true }
        Assert-True $thrown 'tar with in-segment drive letter was accepted'
    }

    Run-Case 'source-fake-git-object-rejected' {
        $root = New-TestRoot 'source-fake-git-object'
        $source = Join-Path $root 'SoulX-Singer'
        [void](New-Item -ItemType Directory -Path (Join-Path $source 'cli') -Force)
        [void](New-Item -ItemType Directory -Path (Join-Path $source 'soulxsinger\config') -Force)
        [IO.File]::WriteAllText((Join-Path $source 'cli\inference.py'), 'checkpoint = torch.load(model_path, weights_only=True, map_location="cpu")\n')
        [IO.File]::WriteAllText((Join-Path $source 'soulxsinger\config\soulxsinger.yaml'), 'fixture: true\n')
        & git.exe -C $source init --quiet 2>$null
        & git.exe -C $source config user.email 'test@example.invalid' 2>$null
        & git.exe -C $source config user.name 'test' 2>$null
        [void](& git.exe -C $source add -A 2>$null)
        [void](& git.exe -C $source commit --quiet -m fixture 2>$null)
        # 把 ref 直接指向固定 commit：rev-parse 会返回它，但对象不存在
        $ref = (& git.exe -C $source rev-parse --abbrev-ref HEAD).Trim()
        [IO.File]::WriteAllText((Join-Path $source ".git\refs\heads\$ref"), $script:SoulxSourcePin.Commit + "\n")
        $thrown = $false
        try { Install-SingingSource -Source $source } catch { $thrown = $true }
        Assert-True $thrown 'fake git object was accepted'
    }

    Run-Case 'source-dirty-git-rejected' {
        $root = New-TestRoot 'source-dirty-git'
        $source = Join-Path $root 'SoulX-Singer'
        [void](New-Item -ItemType Directory -Path (Join-Path $source 'cli') -Force)
        [void](New-Item -ItemType Directory -Path (Join-Path $source 'soulxsinger\config') -Force)
        [IO.File]::WriteAllText((Join-Path $source 'cli\inference.py'), 'checkpoint = torch.load(model_path, weights_only=True, map_location="cpu")\n')
        [IO.File]::WriteAllText((Join-Path $source 'soulxsinger\config\soulxsinger.yaml'), 'fixture: true\n')
        & git.exe -C $source init --quiet 2>$null
        & git.exe -C $source config user.email 'test@example.invalid' 2>$null
        & git.exe -C $source config user.name 'test' 2>$null
        [void](& git.exe -C $source add -A 2>$null)
        [void](& git.exe -C $source commit --quiet -m fixture 2>$null)
        $head = (& git.exe -C $source rev-parse HEAD).Trim()
        [IO.File]::WriteAllText((Join-Path $source 'soulxsinger\extra.py'), 'payload = 1\n')
        $saved = $script:SoulxSourcePin.Commit
        try {
            $script:SoulxSourcePin.Commit = $head
            $thrown = $false
            try { Install-SingingSource -Source $source } catch { $thrown = $true }
            Assert-True $thrown 'dirty work tree was accepted'
        } finally {
            $script:SoulxSourcePin.Commit = $saved
        }
    }

    Run-Case 'source-fixed-archive-success' {
        $root = New-TestRoot 'source-fixed-archive-success'
        $source = Join-Path $root 'SoulX-Singer'
        $record = Install-SingingSource -Source $source -LocalArchive $realArchive
        Assert-True (Test-Path -LiteralPath (Join-Path $source '.mingling-source.json')) 'source marker missing'
        Assert-True ($record.sourceCommit -eq $script:SoulxSourcePin.Commit) 'source commit mismatch'
        $loader = Join-Path $source 'cli\inference.py'
        $text = [IO.File]::ReadAllText($loader, [Text.Encoding]::UTF8)
        Assert-True ($text.Contains('weights_only=True')) 'loader was not patched'
        Assert-True (-not $text.Contains('weights_only=False')) 'loader still unsafe'
        Assert-True ((Get-SingingSha256 $loader) -eq $record.loaderSha256) 'loader hash mismatch'
    }

    Run-Case 'source-valid-install-is-idempotent' {
        $root = New-TestRoot 'source-valid-install-is-idempotent'
        $source = Join-Path $root 'SoulX-Singer'
        [void](Install-SingingSource -Source $source -LocalArchive $realArchive)
        $downloadCalls = New-Object System.Collections.ArrayList
        $downloader = { param([string]$Url, [string]$OutFile, [string]$Sha256, [long]$ExpectedBytes) [void]$downloadCalls.Add(1) }.GetNewClosure()
        $record = Install-SingingSource -Source $source -Downloader $downloader
        Assert-True ($downloadCalls.Count -eq 0) 'valid source caused a download'
        Assert-True ($record.sourceCommit -eq $script:SoulxSourcePin.Commit) 'idempotent record mismatch'
    }

    Run-Case 'source-marker-mismatch-rejected' {
        $root = New-TestRoot 'source-marker-mismatch-rejected'
        $source = Join-Path $root 'SoulX-Singer'
        [void](New-Item -ItemType Directory -Path $source)
        [IO.File]::WriteAllText((Join-Path $source 'keep.txt'), 'old')
        $badMarker = [pscustomobject]@{ sourceCommit = ('0' * 40); archiveSha256 = ('b' * 64); loaderSha256 = ('c' * 64) }
        Write-SingingPinFile (Join-Path $source '.mingling-source.json') $badMarker
        $thrown = $false
        try { Install-SingingSource -Source $source -LocalArchive $realArchive } catch { $thrown = $true }
        Assert-True $thrown 'bad source marker was accepted'
        Assert-True (Test-Path -LiteralPath (Join-Path $source 'keep.txt')) 'old source was overwritten'
    }

    foreach ($attack in @(
        @{ Name = 'tar-path-traversal'; Items = @('SoulX-Singer-81aeb3ae772c70093c3de74dc23c92d983801ae4/cli/inference.py', '../evil.txt') },
        @{ Name = 'tar-absolute-path'; Items = @('SoulX-Singer-81aeb3ae772c70093c3de74dc23c92d983801ae4/cli/inference.py', '/abs.txt') },
        @{ Name = 'tar-drive-path'; Items = @('SoulX-Singer-81aeb3ae772c70093c3de74dc23c92d983801ae4/cli/inference.py', 'C:/abs.txt') },
        @{ Name = 'tar-wrong-top'; Items = @('evil-root/cli/inference.py') },
        @{ Name = 'tar-symlink'; Items = @('SoulX-Singer-81aeb3ae772c70093c3de74dc23c92d983801ae4/cli/inference.py', 'SYMLINK:SoulX-Singer-81aeb3ae772c70093c3de74dc23c92d983801ae4/link=target') },
        @{ Name = 'tar-hardlink'; Items = @('SoulX-Singer-81aeb3ae772c70093c3de74dc23c92d983801ae4/cli/inference.py', 'HARDLINK:SoulX-Singer-81aeb3ae772c70093c3de74dc23c92d983801ae4/link=cli/inference.py') }
    )) {
        Run-Case $attack.Name {
            $root = New-TestRoot $attack.Name
            $tarPath = Join-Path $root 'fixture.tar.gz'
            New-TarFixture $tarPath ([string[]]$attack.Items)
            $thrown = $false
            try { Test-SingingArchiveEntries $tarPath | Out-Null } catch { $thrown = $true }
            Assert-True $thrown 'unsafe tar was accepted'
        }
    }

    Run-Case 'model-download-success' {
        $root = New-TestRoot 'model-download-success'
        $body = [byte[]](5..100)
        $pin = [pscustomobject]@{ Sha256 = (Get-TestHash $body); Size = $body.Length; Source = 'manifest' }
        $model = Join-Path $root 'model.pt'
        $downloader = { param([string]$Url, [string]$OutFile, [string]$Sha256, [long]$ExpectedBytes) [IO.File]::WriteAllBytes($OutFile, $body) }.GetNewClosure()
        Install-SingingModel -ModelPath $model -Pin $pin -Urls @('https://mirror.test/model.pt') -Downloader $downloader | Out-Null
        Assert-True ((Get-SingingSha256 $model) -eq $pin.Sha256) 'model hash mismatch'
        Assert-True (@(Get-ChildItem -LiteralPath $root -Filter '*.part').Count -eq 0) 'model part remains'
    }

    Run-Case 'model-first-source-bad-hash-second-source-success' {
        $root = New-TestRoot 'model-first-source-bad-hash-second-source-success'
        $good = [byte[]](7..90)
        $bad = [byte[]](91..120)
        $pin = [pscustomobject]@{ Sha256 = (Get-TestHash $good); Size = $good.Length; Source = 'manifest' }
        $model = Join-Path $root 'model.pt'
        $downloader = {
            param([string]$Url, [string]$OutFile, [string]$Sha256, [long]$ExpectedBytes)
            if ($Url -like '*bad*') { [IO.File]::WriteAllBytes($OutFile, $bad) }
            else { [IO.File]::WriteAllBytes($OutFile, $good) }
        }.GetNewClosure()
        Install-SingingModel -ModelPath $model -Pin $pin -Urls @('https://bad.test/model.pt', 'https://good.test/model.pt') -Downloader $downloader | Out-Null
        Assert-True ((Get-SingingSha256 $model) -eq $pin.Sha256) 'second-source model hash mismatch'
        Assert-True (@(Get-ChildItem -LiteralPath $root -Filter '*.part').Count -eq 0) 'bad part remains'
    }

    Run-Case 'model-network-interruption-keeps-part' {
        $root = New-TestRoot 'model-network-interruption-keeps-part'
        $partial = [byte[]](1..16)
        $pin = [pscustomobject]@{ Sha256 = ('d' * 64); Size = 64; Source = 'manifest' }
        $model = Join-Path $root 'model.pt'
        $downloader = {
            param([string]$Url, [string]$OutFile, [string]$Sha256, [long]$ExpectedBytes)
            [IO.File]::WriteAllBytes($OutFile, $partial)
            throw 'network interrupted'
        }.GetNewClosure()
        $thrown = $false
        try { Install-SingingModel -ModelPath $model -Pin $pin -Urls @('https://mirror.test/model.pt') -Downloader $downloader | Out-Null }
        catch { $thrown = $true }
        Assert-True $thrown 'network interruption was accepted'
        Assert-True (@(Get-ChildItem -LiteralPath $root -Filter '*.part').Count -eq 1) 'interrupted part was deleted'
        Assert-True (-not (Test-Path -LiteralPath $model)) 'partial model became official'
    }

    Run-Case 'model-existing-correct-is-idempotent-and-wrong-rejected' {
        $root = New-TestRoot 'model-existing-correct-is-idempotent-and-wrong-rejected'
        $body = [byte[]](9..80)
        $pin = [pscustomobject]@{ Sha256 = (Get-TestHash $body); Size = $body.Length; Source = 'manifest' }
        $model = Join-Path $root 'model.pt'
        [IO.File]::WriteAllBytes($model, $body)
        $downloadCalls = New-Object System.Collections.ArrayList
        $downloader = { param([string]$Url, [string]$OutFile, [string]$Sha256, [long]$ExpectedBytes) [void]$downloadCalls.Add(1) }.GetNewClosure()
        Install-SingingModel -ModelPath $model -Pin $pin -Urls @('https://mirror.test/model.pt') -Downloader $downloader | Out-Null
        Assert-True ($downloadCalls.Count -eq 0) 'correct model caused a download'
        [IO.File]::WriteAllBytes($model, [byte[]](81..100))
        $thrown = $false
        try { Install-SingingModel -ModelPath $model -Pin $pin -Urls @('https://mirror.test/model.pt') -Downloader $downloader | Out-Null }
        catch { $thrown = $true }
        Assert-True $thrown 'wrong existing model was accepted'
    }

    Run-Case 'installed-pin-atomic-write-and-read' {
        $root = New-TestRoot 'installed-pin-atomic-write-and-read'
        $path = Join-Path $root 'installed.json'
        $data = [pscustomobject]@{ sourceCommit = $script:SoulxSourcePin.Commit; modelSha256 = ('e' * 64); modelHashSource = 'manifest'; loaderSha256 = ('f' * 64) }
        Write-SingingPinFile $path $data
        $record = Read-SingingPinFile $path
        Assert-True ($record.sourceCommit -eq $data.sourceCommit) 'installed record mismatch'
        Assert-True (@(Get-ChildItem -LiteralPath $root -Filter '*.tmp').Count -eq 0) 'temporary record remains'
        Assert-True (@(Get-ChildItem -LiteralPath $root -Filter '*.bak').Count -eq 0) 'record backup remains'
    }

    Run-Case 'installation-assertion-catches-tampering' {
        $root = New-TestRoot 'installation-assertion-catches-tampering'
        $source = Join-Path $root 'SoulX-Singer'
        [void](Install-SingingSource -Source $source -LocalArchive $realArchive)
        $sourceRecord = Read-SingingPinFile (Join-Path $source '.mingling-source.json')
        $body = [byte[]](11..90)
        $model = Join-Path $root 'model.pt'
        [IO.File]::WriteAllBytes($model, $body)
        $component = [pscustomobject]@{ model = [pscustomobject]@{ sha256 = (Get-TestHash $body); size = $body.Length } }
        $installedPath = Join-Path $root 'installed.json'
        $installed = [pscustomobject]@{
            sourceCommit = $sourceRecord.sourceCommit; modelSha256 = (Get-TestHash $body)
            modelHashSource = 'manifest'; loaderSha256 = $sourceRecord.loaderSha256
        }
        Write-SingingPinFile $installedPath $installed
        Assert-SingingInstallation -Source $source -Model $model -InstalledPath $installedPath -Component $component | Out-Null
        [IO.File]::WriteAllBytes($model, [byte[]](91..120))
        $thrown = $false
        try { Assert-SingingInstallation -Source $source -Model $model -InstalledPath $installedPath -Component $component | Out-Null }
        catch { $thrown = $true }
        Assert-True $thrown 'tampered model passed installation assertion'
        [IO.File]::WriteAllBytes($model, $body)
        $loader = Join-Path $source 'cli\inference.py'
        $text = [IO.File]::ReadAllText($loader, [Text.Encoding]::UTF8).Replace('weights_only=True', 'weights_only=False')
        [IO.File]::WriteAllText($loader, $text, (New-Object Text.UTF8Encoding($false)))
        $thrown = $false
        try { Assert-SingingInstallation -Source $source -Model $model -InstalledPath $installedPath -Component $component | Out-Null }
        catch { $thrown = $true }
        Assert-True $thrown 'unsafe loader passed installation assertion'
    }

    Run-Case 'start-verifies-before-health' {
        $scriptText = Get-Content -Raw -Encoding UTF8 (Join-Path $repo 'tools\setup-singing.ps1')
        $startIndex = $scriptText.IndexOf("if (`$Action -eq 'start')")
        $assertIndex = $scriptText.IndexOf('Assert-SingingInstallation', $startIndex)
        $healthIndex = $scriptText.IndexOf('if (Health)', $startIndex)
        Assert-True ($startIndex -ge 0) 'start block not found'
        Assert-True ($assertIndex -gt $startIndex) 'start pin verification not found'
        Assert-True ($healthIndex -gt $assertIndex) 'start health check happens before pin verification'
    }

    Run-Case 'full-start-failure-result-protocol' {
        $root = New-TestRoot 'full-start-failure-result-protocol'
        $output = @(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $repo 'tools\setup-singing.ps1') -Action start -Base $root -Root $root 2>&1)
        Assert-True ($LASTEXITCODE -eq 1) 'full start failure exit code mismatch'
        $resultLines = @($output | Where-Object { $_ -match '^RESULT ok=' })
        Assert-True ($resultLines.Count -eq 1) 'RESULT line count mismatch'
        Assert-True ("$($resultLines[0])" -match '^RESULT ok=0 why=') 'RESULT failure format mismatch'
    }

    Run-Case 'production-manifest-model-pin' {
        $component = Get-SingingComponent (Join-Path $repo 'tools\components.json')
        $pin = Resolve-SingingModelPin $component ''
        Assert-True ($pin.Sha256 -eq '447eaf41f91a6b6659d55e9ec3c9b809221724fb8592aebaec35a23751a5b500') 'model hash pin mismatch'
        Assert-True ($pin.Size -eq 2818092278) 'model size pin mismatch'
        Assert-True ($pin.Source -eq 'manifest') 'model pin source mismatch'
    }
} finally {
    if (Test-Path -LiteralPath $TestRoot) { Remove-Item -LiteralPath $TestRoot -Recurse -Force -ErrorAction SilentlyContinue }
}

Write-Host ("SUMMARY total={0} failed={1}" -f $script:CaseCount, $script:FailureCount)
if ($script:FailureCount -gt 0) { exit 1 }
exit 0
