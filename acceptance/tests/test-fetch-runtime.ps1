# WB-01 offline acceptance tests. Run with Windows PowerShell 5.1.
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $repo 'tools\fetch-runtime.ps1') -FunctionOnly

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
Add-Type -AssemblyName System.Net.Http
$fakeHttpType = @'
using System;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Threading;
using System.Threading.Tasks;

public sealed class StalledStream : Stream
{
    public override bool CanRead { get { return true; } }
    public override bool CanSeek { get { return false; } }
    public override bool CanWrite { get { return false; } }
    public override long Length { get { throw new NotSupportedException(); } }
    public override long Position { get { throw new NotSupportedException(); } set { throw new NotSupportedException(); } }
    public override void Flush() { }
    public override int Read(byte[] buffer, int offset, int count) { throw new NotSupportedException(); }
    public override long Seek(long offset, SeekOrigin origin) { throw new NotSupportedException(); }
    public override void SetLength(long value) { throw new NotSupportedException(); }
    public override void Write(byte[] buffer, int offset, int count) { throw new NotSupportedException(); }
    public override Task<int> ReadAsync(byte[] buffer, int offset, int count, CancellationToken cancellationToken)
    {
        TaskCompletionSource<int> source = new TaskCompletionSource<int>();
        cancellationToken.Register(delegate { source.TrySetCanceled(); });
        return source.Task;
    }
}

public sealed class FakeHttpHandler : HttpMessageHandler
{
    public int Calls;
    public int Failures;
    public int DelayMilliseconds;
    public bool StallBody;
    public byte[] Body;

    protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken cancellationToken)
    {
        Calls++;
        if (Calls <= Failures) { throw new HttpRequestException("fake network failure"); }
        if (DelayMilliseconds > 0) { Task.Delay(DelayMilliseconds, cancellationToken).Wait(cancellationToken); }
        HttpResponseMessage response = new HttpResponseMessage(HttpStatusCode.OK);
        if (StallBody) { response.Content = new StreamContent(new StalledStream()); }
        else { response.Content = new ByteArrayContent(Body ?? new byte[0]); }
        return Task.FromResult(response);
    }
}
'@
if (-not ('FakeHttpHandler' -as [type])) {
    Add-Type -TypeDefinition $fakeHttpType -ReferencedAssemblies @('System.Net.Http', 'System.dll', 'System.Core.dll')
}

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
    try {
        return ([BitConverter]::ToString($sha.ComputeHash($Bytes))).Replace('-', '').ToLowerInvariant()
    } finally { $sha.Dispose() }
}

function New-NodePin([byte[]]$Body) {
    [pscustomobject]@{
        version = 'test-node'; url = 'https://node.test/win-x64/node.exe'; fileName = 'node.exe'
        sha256 = (Get-TestHash $Body); size = $Body.Length
        sourceUrl = 'https://node.test/SHASUMS256.txt'; verifiedAt = '2026-11-12'
    }
}

function New-FfmpegPin([byte[]]$Zip, [byte[]]$Exe, [string]$EntryName = 'ffmpeg-test-essentials_build/bin/ffmpeg.exe') {
    [pscustomobject]@{
        version = 'test-ffmpeg'; url = 'https://ffmpeg.test/ffmpeg-test.zip'; fileName = 'ffmpeg-test.zip'
        sha256 = (Get-TestHash $Zip); zipSha256 = (Get-TestHash $Zip)
        size = $Zip.Length; zipSize = $Zip.Length
        entryName = $EntryName; ffmpegExeSha256 = (Get-TestHash $Exe); ffmpegExeSize = $Exe.Length
        sourceUrl = 'https://ffmpeg.test/ffmpeg-test.zip.sha256'; verifiedAt = '2026-11-12'
    }
}

function New-ZipBytes([System.Collections.Specialized.OrderedDictionary]$Entries) {
    $memory = New-Object System.IO.MemoryStream
    $archive = $null
    try {
        $archive = New-Object System.IO.Compression.ZipArchive($memory, [System.IO.Compression.ZipArchiveMode]::Create)
        foreach ($key in $Entries.Keys) {
            $entry = $archive.CreateEntry([string]$key)
            $stream = $entry.Open()
            try { $stream.Write([byte[]]$Entries[$key], 0, $Entries[$key].Length) } finally { $stream.Dispose() }
        }
    } finally {
        if ($null -ne $archive) { $archive.Dispose() }
    }
    return $memory.ToArray()
}

function New-FakeFactory([FakeHttpHandler]$Handler) {
    return { param([string]$Url) New-Object System.Net.Http.HttpClient($Handler) }.GetNewClosure()
}

function New-TestRoot([string]$Name) {
    $path = Join-Path $TestRoot ($Name + '-' + [Guid]::NewGuid().ToString('N'))
    [void](New-Item -ItemType Directory -Path $path)
    return $path
}

$TestRoot = Join-Path ([IO.Path]::GetTempPath()) ('wb01-tests-' + [Guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $TestRoot)
try {
    Run-Case 'node-success' {
        $root = New-TestRoot 'node-success'
        $body = [byte[]](1..160)
        $nodePin = New-NodePin $body
        $exe = [byte[]](7..70)
        $ffmpegPin = New-FfmpegPin (New-ZipBytes (New-Object System.Collections.Specialized.OrderedDictionary)) $exe
        $pins = [pscustomobject]@{ node = $nodePin; ffmpeg = $ffmpegPin }
        $nodeDst = Join-Path $root 'node.exe'
        $ffmpegDst = Join-Path $root 'ffmpeg.exe'
        [IO.File]::WriteAllBytes($ffmpegDst, $exe)
        $handler = New-Object FakeHttpHandler
        $handler.Body = $body
        $factory = New-FakeFactory $handler
        Invoke-RuntimeInstall -Pins $pins -NodeDestination $nodeDst -FfmpegDestination $ffmpegDst -TemporaryRoot (Join-Path $root 'tmp') -HttpClientFactory $factory -RetryDelaysMilliseconds @(10, 10) | Out-Null
        Assert-True (Test-Path -LiteralPath $nodeDst) 'node target missing'
        Assert-True ((Get-FileSha256 $nodeDst) -eq $nodePin.sha256) 'node target hash mismatch'
        Assert-True (@(Get-ChildItem -LiteralPath $root -Filter '*.part').Count -eq 0) 'part file remains'
        Assert-True ($handler.Calls -eq 1) 'unexpected request count'
    }

    Run-Case 'ffmpeg-success' {
        $root = New-TestRoot 'ffmpeg-success'
        $nodeBody = [byte[]](3..66)
        $exe = [byte[]](11..90)
        $entries = New-Object System.Collections.Specialized.OrderedDictionary
        $entries['ffmpeg-test-essentials_build/bin/ffmpeg.exe'] = $exe
        $zip = New-ZipBytes $entries
        $pins = [pscustomobject]@{ node = (New-NodePin $nodeBody); ffmpeg = (New-FfmpegPin $zip $exe) }
        $nodeDst = Join-Path $root 'node.exe'
        $ffmpegDst = Join-Path $root 'ffmpeg.exe'
        [IO.File]::WriteAllBytes($nodeDst, $nodeBody)
        $handler = New-Object FakeHttpHandler
        $handler.Body = $zip
        $factory = New-FakeFactory $handler
        Invoke-RuntimeInstall -Pins $pins -NodeDestination $nodeDst -FfmpegDestination $ffmpegDst -TemporaryRoot (Join-Path $root 'tmp') -HttpClientFactory $factory -RetryDelaysMilliseconds @(10, 10) | Out-Null
        Assert-True (Test-Path -LiteralPath $ffmpegDst) 'ffmpeg target missing'
        Assert-True ((Get-FileSha256 $ffmpegDst) -eq $pins.ffmpeg.ffmpegExeSha256) 'ffmpeg target hash mismatch'
        Assert-True (@(Get-ChildItem -LiteralPath $root -Filter '*.part').Count -eq 0) 'part file remains'
        Assert-True ($handler.Calls -eq 1) 'unexpected request count'
    }

    Run-Case 'node-bad-hash' {
        $root = New-TestRoot 'node-bad-hash'
        $body = [byte[]](1..96)
        $badPin = New-NodePin $body
        $badPin.sha256 = ('0' * 64)
        $exe = [byte[]](5..44)
        $pins = [pscustomobject]@{ node = $badPin; ffmpeg = (New-FfmpegPin (New-ZipBytes (New-Object System.Collections.Specialized.OrderedDictionary)) $exe) }
        $nodeDst = Join-Path $root 'node.exe'
        $ffmpegDst = Join-Path $root 'ffmpeg.exe'
        [IO.File]::WriteAllBytes($ffmpegDst, $exe)
        $handler = New-Object FakeHttpHandler
        $handler.Body = $body
        $thrown = $false
        try { Invoke-RuntimeInstall -Pins $pins -NodeDestination $nodeDst -FfmpegDestination $ffmpegDst -TemporaryRoot (Join-Path $root 'tmp') -HttpClientFactory (New-FakeFactory $handler) -RetryDelaysMilliseconds @(10, 10) | Out-Null }
        catch { $thrown = $true }
        Assert-True $thrown 'bad node hash was accepted'
        Assert-True (-not (Test-Path -LiteralPath $nodeDst)) 'bad node target exists'
        Assert-True (@(Get-ChildItem -LiteralPath $root -Filter '*.part').Count -eq 0) 'part file remains'
    }

    Run-Case 'ffmpeg-zip-bad-hash' {
        $root = New-TestRoot 'ffmpeg-zip-bad-hash'
        $nodeBody = [byte[]](2..33)
        $exe = [byte[]](9..88)
        $entries = New-Object System.Collections.Specialized.OrderedDictionary
        $entries['ffmpeg-test-essentials_build/bin/ffmpeg.exe'] = $exe
        $zip = New-ZipBytes $entries
        $badPin = New-FfmpegPin $zip $exe
        $badPin.sha256 = 'f' * 64
        $badPin.zipSha256 = $badPin.sha256
        $pins = [pscustomobject]@{ node = (New-NodePin $nodeBody); ffmpeg = $badPin }
        $nodeDst = Join-Path $root 'node.exe'
        $ffmpegDst = Join-Path $root 'ffmpeg.exe'
        [IO.File]::WriteAllBytes($nodeDst, $nodeBody)
        $handler = New-Object FakeHttpHandler
        $handler.Body = $zip
        $thrown = $false
        try { Invoke-RuntimeInstall -Pins $pins -NodeDestination $nodeDst -FfmpegDestination $ffmpegDst -TemporaryRoot (Join-Path $root 'tmp') -HttpClientFactory (New-FakeFactory $handler) -RetryDelaysMilliseconds @(10, 10) | Out-Null }
        catch { $thrown = $true }
        Assert-True $thrown 'bad ffmpeg zip hash was accepted'
        Assert-True (-not (Test-Path -LiteralPath $ffmpegDst)) 'bad ffmpeg target exists'
    }

    Run-Case 'ffmpeg-inner-bad-hash' {
        $root = New-TestRoot 'ffmpeg-inner-bad-hash'
        $nodeBody = [byte[]](4..55)
        $exe = [byte[]](13..92)
        $entries = New-Object System.Collections.Specialized.OrderedDictionary
        $entries['ffmpeg-test-essentials_build/bin/ffmpeg.exe'] = $exe
        $zip = New-ZipBytes $entries
        $badPin = New-FfmpegPin $zip $exe
        $badPin.ffmpegExeSha256 = 'a' * 64
        $pins = [pscustomobject]@{ node = (New-NodePin $nodeBody); ffmpeg = $badPin }
        $nodeDst = Join-Path $root 'node.exe'
        $ffmpegDst = Join-Path $root 'ffmpeg.exe'
        [IO.File]::WriteAllBytes($nodeDst, $nodeBody)
        $handler = New-Object FakeHttpHandler
        $handler.Body = $zip
        $thrown = $false
        try { Invoke-RuntimeInstall -Pins $pins -NodeDestination $nodeDst -FfmpegDestination $ffmpegDst -TemporaryRoot (Join-Path $root 'tmp') -HttpClientFactory (New-FakeFactory $handler) -RetryDelaysMilliseconds @(10, 10) | Out-Null }
        catch { $thrown = $true }
        Assert-True $thrown 'bad inner ffmpeg hash was accepted'
        Assert-True (-not (Test-Path -LiteralPath $ffmpegDst)) 'bad inner ffmpeg target exists'
    }

    foreach ($badEntry in @('..\evil.txt', '/abs.txt', 'C:\abs.txt', 'stream:ads', 'bad.', 'CON')) {
        $caseName = 'zip-reject-' + ($badEntry -replace '[^A-Za-z0-9]+', '-')
        Run-Case $caseName {
            $root = New-TestRoot $caseName
            $exe = [byte[]](21..80)
            $entries = New-Object System.Collections.Specialized.OrderedDictionary
            $entries[$badEntry] = [byte[]](1..8)
            $entries['ffmpeg-test-essentials_build/bin/ffmpeg.exe'] = $exe
            $zip = New-ZipBytes $entries
            $zipPath = Join-Path $root 'fixture.zip'
            $destination = Join-Path $root 'ffmpeg.part'
            [IO.File]::WriteAllBytes($zipPath, $zip)
            $thrown = $false
            try { Expand-PinnedZipEntry -ZipPath $zipPath -EntryName 'ffmpeg-test-essentials_build/bin/ffmpeg.exe' -ExpectedHash (Get-TestHash $exe) -ExpectedSize $exe.Length -Destination $destination | Out-Null }
            catch { $thrown = $true }
            Assert-True $thrown 'unsafe zip entry was accepted'
            Assert-True (-not (Test-Path -LiteralPath $destination)) 'destination remains'
            Assert-True (-not (Test-Path -LiteralPath (Join-Path $root 'evil.txt'))) 'evil file escaped'
        }
    }

    Run-Case 'zip-two-ffmpeg-candidates' {
        $root = New-TestRoot 'zip-two-ffmpeg-candidates'
        $exe = [byte[]](31..90)
        $entries = New-Object System.Collections.Specialized.OrderedDictionary
        $entries['ffmpeg-a-essentials_build/bin/ffmpeg.exe'] = $exe
        $entries['ffmpeg-b-essentials_build/bin/ffmpeg.exe'] = $exe
        $zip = New-ZipBytes $entries
        $zipPath = Join-Path $root 'fixture.zip'
        $destination = Join-Path $root 'ffmpeg.part'
        [IO.File]::WriteAllBytes($zipPath, $zip)
        $thrown = $false
        try { Expand-PinnedZipEntry -ZipPath $zipPath -EntryName 'ffmpeg-a-essentials_build/bin/ffmpeg.exe' -ExpectedHash (Get-TestHash $exe) -ExpectedSize $exe.Length -Destination $destination | Out-Null }
        catch { $thrown = $true }
        Assert-True $thrown 'two ffmpeg candidates were accepted'
        Assert-True (-not (Test-Path -LiteralPath $destination)) 'destination remains'
    }

    Run-Case 'zip-backslash-layout' {
        $root = New-TestRoot 'zip-backslash-layout'
        $exe = [byte[]](41..100)
        $entryName = 'ffmpeg-test-essentials_build\bin\ffmpeg.exe'
        $entries = New-Object System.Collections.Specialized.OrderedDictionary
        $entries[$entryName] = $exe
        $zip = New-ZipBytes $entries
        $zipPath = Join-Path $root 'fixture.zip'
        $destination = Join-Path $root 'ffmpeg.part'
        [IO.File]::WriteAllBytes($zipPath, $zip)
        Expand-PinnedZipEntry -ZipPath $zipPath -EntryName $entryName -ExpectedHash (Get-TestHash $exe) -ExpectedSize $exe.Length -Destination $destination | Out-Null
        Assert-True ((Get-FileSha256 $destination) -eq (Get-TestHash $exe)) 'backslash layout extraction failed'
    }

    Run-Case 'headers-timeout' {
        $root = New-TestRoot 'headers-timeout'
        $body = [byte[]](1..32)
        $pin = New-NodePin $body
        $destination = Join-Path $root 'node.exe'
        $handler = New-Object FakeHttpHandler
        $handler.Body = $body
        $handler.DelayMilliseconds = 1500
        $thrown = $false
        $started = Get-Date
        try { Invoke-PinnedDownload -Pin $pin -Destination $destination -TimeoutSeconds 1 -RetryDelaysMilliseconds @() -HttpClientFactory (New-FakeFactory $handler) | Out-Null }
        catch { $thrown = $true }
        Assert-True $thrown 'headers timeout was accepted'
        Assert-True (((Get-Date) - $started).TotalSeconds -lt 4) 'headers timeout took too long'
        Assert-True (@(Get-ChildItem -LiteralPath $root -Filter '*.part').Count -eq 0) 'part file remains'
    }

    Run-Case 'body-stall-timeout' {
        $root = New-TestRoot 'body-stall-timeout'
        $body = [byte[]](1..32)
        $pin = New-NodePin $body
        $destination = Join-Path $root 'node.exe'
        $handler = New-Object FakeHttpHandler
        $handler.Body = $body
        $handler.StallBody = $true
        $thrown = $false
        $started = Get-Date
        try { Invoke-PinnedDownload -Pin $pin -Destination $destination -TimeoutSeconds 1 -RetryDelaysMilliseconds @() -HttpClientFactory (New-FakeFactory $handler) | Out-Null }
        catch { $thrown = $true }
        Assert-True $thrown 'body stall was accepted'
        Assert-True (((Get-Date) - $started).TotalSeconds -lt 4) 'body stall took too long'
        Assert-True (@(Get-ChildItem -LiteralPath $root -Filter '*.part').Count -eq 0) 'part file remains'
    }

    Run-Case 'retry-success-third-attempt' {
        $root = New-TestRoot 'retry-success-third-attempt'
        $body = [byte[]](1..64)
        $pin = New-NodePin $body
        $destination = Join-Path $root 'node.exe'
        $handler = New-Object FakeHttpHandler
        $handler.Body = $body
        $handler.Failures = 2
        $delayList = New-Object System.Collections.ArrayList
        $sleep = { param([int]$Milliseconds) [void]$delayList.Add($Milliseconds) }.GetNewClosure()
        $part = Invoke-PinnedDownload -Pin $pin -Destination $destination -RetryDelaysMilliseconds @(11, 22) -HttpClientFactory (New-FakeFactory $handler) -SleepScript $sleep
        Assert-True ($handler.Calls -eq 3) 'retry request count mismatch'
        Assert-True (($delayList -join ',') -eq '11,22') 'retry delay sequence mismatch'
        Assert-True ((Get-FileSha256 $part) -eq $pin.sha256) 'retry result hash mismatch'
        Remove-Item -LiteralPath $part -Force
    }

    Run-Case 'retry-all-fail' {
        $root = New-TestRoot 'retry-all-fail'
        $body = [byte[]](1..64)
        $pin = New-NodePin $body
        $destination = Join-Path $root 'node.exe'
        $handler = New-Object FakeHttpHandler
        $handler.Body = $body
        $handler.Failures = 3
        $thrown = $false
        try { Invoke-PinnedDownload -Pin $pin -Destination $destination -RetryDelaysMilliseconds @(10, 10) -HttpClientFactory (New-FakeFactory $handler) | Out-Null }
        catch { $thrown = $true }
        Assert-True $thrown 'all retries failing was accepted'
        Assert-True ($handler.Calls -eq 3) 'retry-all request count mismatch'
        Assert-True (@(Get-ChildItem -LiteralPath $root -Filter '*.part').Count -eq 0) 'part file remains'
    }

    Run-Case 'idempotent-valid-targets' {
        $root = New-TestRoot 'idempotent-valid-targets'
        $nodeBody = [byte[]](1..80)
        $exe = [byte[]](2..81)
        $pins = [pscustomobject]@{ node = (New-NodePin $nodeBody); ffmpeg = (New-FfmpegPin (New-ZipBytes (New-Object System.Collections.Specialized.OrderedDictionary)) $exe) }
        $nodeDst = Join-Path $root 'node.exe'
        $ffmpegDst = Join-Path $root 'ffmpeg.exe'
        [IO.File]::WriteAllBytes($nodeDst, $nodeBody)
        [IO.File]::WriteAllBytes($ffmpegDst, $exe)
        $handler = New-Object FakeHttpHandler
        Invoke-RuntimeInstall -Pins $pins -NodeDestination $nodeDst -FfmpegDestination $ffmpegDst -TemporaryRoot (Join-Path $root 'tmp') -HttpClientFactory (New-FakeFactory $handler) | Out-Null
        Assert-True ($handler.Calls -eq 0) 'valid targets caused a request'
    }

    Run-Case 'zero-byte-target-is-invalid' {
        $root = New-TestRoot 'zero-byte-target-is-invalid'
        $nodeBody = [byte[]](1..80)
        $exe = [byte[]](2..81)
        $pins = [pscustomobject]@{ node = (New-NodePin $nodeBody); ffmpeg = (New-FfmpegPin (New-ZipBytes (New-Object System.Collections.Specialized.OrderedDictionary)) $exe) }
        $nodeDst = Join-Path $root 'node.exe'
        $ffmpegDst = Join-Path $root 'ffmpeg.exe'
        [IO.File]::WriteAllBytes($nodeDst, [byte[]]@())
        [IO.File]::WriteAllBytes($ffmpegDst, $exe)
        $handler = New-Object FakeHttpHandler
        $thrown = $false
        try { Invoke-RuntimeInstall -Pins $pins -NodeDestination $nodeDst -FfmpegDestination $ffmpegDst -TemporaryRoot (Join-Path $root 'tmp') -HttpClientFactory (New-FakeFactory $handler) | Out-Null }
        catch { $thrown = $true }
        Assert-True $thrown 'zero-byte target was accepted'
        Assert-True ($handler.Calls -eq 0) 'zero-byte target caused a request'
    }

    Run-Case 'force-replaces-and-cleans-backup' {
        $root = New-TestRoot 'force-replaces-and-cleans-backup'
        $old = [byte[]](1..64)
        $new = [byte[]](65..128)
        $pin = New-NodePin $new
        $target = Join-Path $root 'node.exe'
        $keepBackup = "$target.keep.bak"
        [IO.File]::WriteAllBytes($target, $old)
        [IO.File]::WriteAllBytes($keepBackup, $old)
        $part = Join-Path $root 'node.part'
        [IO.File]::WriteAllBytes($part, $new)
        Complete-RuntimeFile -PartPath $part -Target $target -ExpectedHash $pin.sha256 -ExpectedSize $pin.size -Force | Out-Null
        Assert-True ((Get-FileSha256 $target) -eq $pin.sha256) 'force replacement hash mismatch'
        Assert-True (Test-Path -LiteralPath $keepBackup) 'preexisting backup was removed'
        Assert-True (@(Get-ChildItem -LiteralPath $root -Filter '*.part').Count -eq 0) 'part file remains'
        $newBackups = @(Get-ChildItem -LiteralPath $root -Filter '*.bak' | Where-Object { $_.Name -ne 'node.exe.keep.bak' })
        Assert-True ($newBackups.Count -eq 0) 'new backup remains'
    }

    Run-Case 'post-commit-failure-restores-old-file' {
        $root = New-TestRoot 'post-commit-failure-restores-old-file'
        $old = [byte[]](1..64)
        $new = [byte[]](65..128)
        $target = Join-Path $root 'node.exe'
        $part = Join-Path $root 'node.part'
        [IO.File]::WriteAllBytes($target, $old)
        [IO.File]::WriteAllBytes($part, $new)
        $thrown = $false
        try { Complete-RuntimeFile -PartPath $part -Target $target -ExpectedHash (Get-TestHash $new) -ExpectedSize $new.Length -Force -PostCommitVerifier { param([string]$Path) $false } | Out-Null }
        catch { $thrown = $true }
        Assert-True $thrown 'post-commit verifier failure was accepted'
        Assert-True ((Get-FileSha256 $target) -eq (Get-TestHash $old)) 'old target was not restored'
    }

    Run-Case 'local-path-success' {
        $root = New-TestRoot 'local-path-success'
        $local = [byte[]](1..96)
        $nodeBody = [byte[]](97..160)
        $exe = [byte[]](5..64)
        $pins = [pscustomobject]@{ node = (New-NodePin $nodeBody); ffmpeg = (New-FfmpegPin (New-ZipBytes (New-Object System.Collections.Specialized.OrderedDictionary)) $exe) }
        $localPath = Join-Path $root 'local-node.exe'
        $nodeDst = Join-Path $root 'node.exe'
        $ffmpegDst = Join-Path $root 'ffmpeg.exe'
        [IO.File]::WriteAllBytes($localPath, $local)
        [IO.File]::WriteAllBytes($ffmpegDst, $exe)
        $handler = New-Object FakeHttpHandler
        Invoke-RuntimeInstall -Pins $pins -NodeDestination $nodeDst -FfmpegDestination $ffmpegDst -NodeSource $localPath -TemporaryRoot (Join-Path $root 'tmp') -HttpClientFactory (New-FakeFactory $handler) | Out-Null
        Assert-True ((Get-FileSha256 $nodeDst) -eq (Get-TestHash $local)) 'local node copy failed'
        Assert-True ($handler.Calls -eq 0) 'local path caused a request'
    }

    Run-Case 'local-path-missing-and-empty' {
        $root = New-TestRoot 'local-path-missing-and-empty'
        $nodeBody = [byte[]](1..32)
        $exe = [byte[]](2..33)
        $pins = [pscustomobject]@{ node = (New-NodePin $nodeBody); ffmpeg = (New-FfmpegPin (New-ZipBytes (New-Object System.Collections.Specialized.OrderedDictionary)) $exe) }
        $emptyPath = Join-Path $root 'empty.exe'
        [IO.File]::WriteAllBytes($emptyPath, [byte[]]@())
        foreach ($source in @((Join-Path $root 'missing.exe'), $emptyPath)) {
            $nodeDst = Join-Path $root ('node-' + [IO.Path]::GetFileName($source))
            $ffmpegDst = Join-Path $root 'ffmpeg.exe'
            if (-not (Test-Path -LiteralPath $ffmpegDst)) { [IO.File]::WriteAllBytes($ffmpegDst, $exe) }
            $thrown = $false
            try { Invoke-RuntimeInstall -Pins $pins -NodeDestination $nodeDst -FfmpegDestination $ffmpegDst -NodeSource $source -TemporaryRoot (Join-Path $root 'tmp') | Out-Null }
            catch { $thrown = $true }
            Assert-True $thrown 'invalid local source was accepted'
        }
    }

    Run-Case 'verify-only' {
        $root = New-TestRoot 'verify-only'
        $nodeBody = [byte[]](1..80)
        $exe = [byte[]](2..81)
        $pins = [pscustomobject]@{ node = (New-NodePin $nodeBody); ffmpeg = (New-FfmpegPin (New-ZipBytes (New-Object System.Collections.Specialized.OrderedDictionary)) $exe) }
        $nodeDst = Join-Path $root 'node.exe'
        $ffmpegDst = Join-Path $root 'ffmpeg.exe'
        [IO.File]::WriteAllBytes($nodeDst, $nodeBody)
        [IO.File]::WriteAllBytes($ffmpegDst, $exe)
        $handler = New-Object FakeHttpHandler
        Invoke-RuntimeInstall -Pins $pins -NodeDestination $nodeDst -FfmpegDestination $ffmpegDst -VerifyOnly -TemporaryRoot (Join-Path $root 'tmp') -HttpClientFactory (New-FakeFactory $handler) | Out-Null
        Assert-True ($handler.Calls -eq 0) 'verify-only caused a request'
    }

    Run-Case 'verify-only-rejects-tampered-target' {
        $root = New-TestRoot 'verify-only-rejects-tampered-target'
        $nodeBody = [byte[]](1..80)
        $exe = [byte[]](2..81)
        $pins = [pscustomobject]@{ node = (New-NodePin $nodeBody); ffmpeg = (New-FfmpegPin (New-ZipBytes (New-Object System.Collections.Specialized.OrderedDictionary)) $exe) }
        $nodeDst = Join-Path $root 'node.exe'
        $ffmpegDst = Join-Path $root 'ffmpeg.exe'
        [IO.File]::WriteAllBytes($nodeDst, [byte[]](9..90))
        [IO.File]::WriteAllBytes($ffmpegDst, $exe)
        $handler = New-Object FakeHttpHandler
        $thrown = $false
        try { Invoke-RuntimeInstall -Pins $pins -NodeDestination $nodeDst -FfmpegDestination $ffmpegDst -VerifyOnly -TemporaryRoot (Join-Path $root 'tmp') -HttpClientFactory (New-FakeFactory $handler) | Out-Null }
        catch { $thrown = $true }
        Assert-True $thrown 'tampered target passed verify-only'
        Assert-True ($handler.Calls -eq 0) 'verify-only rejection caused a request'
    }

    Run-Case 'node-size-mismatch' {
        $root = New-TestRoot 'node-size-mismatch'
        $pinBody = [byte[]](1..120)
        $servedBody = [byte[]](1..160)
        $exe = [byte[]](3..63)
        $pins = [pscustomobject]@{ node = (New-NodePin $pinBody); ffmpeg = (New-FfmpegPin (New-ZipBytes (New-Object System.Collections.Specialized.OrderedDictionary)) $exe) }
        $nodeDst = Join-Path $root 'node.exe'
        $ffmpegDst = Join-Path $root 'ffmpeg.exe'
        [IO.File]::WriteAllBytes($ffmpegDst, $exe)
        $handler = New-Object FakeHttpHandler
        $handler.Body = $servedBody
        $thrown = $false
        $message = ''
        try { Invoke-RuntimeInstall -Pins $pins -NodeDestination $nodeDst -FfmpegDestination $ffmpegDst -TemporaryRoot (Join-Path $root 'tmp') -HttpClientFactory (New-FakeFactory $handler) -RetryDelaysMilliseconds @(10, 10) | Out-Null }
        catch { $thrown = $true; $message = $_.Exception.Message }
        Assert-True $thrown 'size mismatch was accepted'
        Assert-True ($message -like '*大小不对*') "unexpected error: $message"
        Assert-True (-not (Test-Path -LiteralPath $nodeDst)) 'size mismatch left a target'
        Assert-True (@(Get-ChildItem -LiteralPath $root -Filter '*.part').Count -eq 0) 'part file remains'
    }

    Run-Case 'pins-invalid-fields' {
        $root = New-TestRoot 'pins-invalid-fields'
        $base = @'
{"node":{"version":"t","url":"https://node.test/win-x64/node.exe","fileName":"node.exe","sha256":"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA","size":10,"sourceUrl":"https://node.test/S","verifiedAt":"2026-11-12"},
 "ffmpeg":{"version":"t","url":"https://ffmpeg.test/f.zip","fileName":"f.zip","sha256":"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA","size":10,"zipSha256":"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA","zipSize":10,"entryName":"p/bin/ffmpeg.exe","ffmpegExeSha256":"BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB","ffmpegExeSize":10,"sourceUrl":"https://ffmpeg.test/f.zip.sha256","verifiedAt":"2026-11-12"}}
'@
        $cases = [ordered]@{
            'missing-node-sha256'  = @('"sha256":"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",', '"sha256":"" ,')
            'node-http-url'        = @('"url":"https://node.test/win-x64/node.exe"', '"url":"http://node.test/win-x64/node.exe"')
            'node-bad-hash'        = @('"sha256":"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"', '"sha256":"abc"')
            'node-zero-size'       = @('"size":10', '"size":0')
            'ffmpeg-bad-exe-hash'  = @('"ffmpegExeSha256":"BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"', '"ffmpegExeSha256":"abc"')
            'ffmpeg-bad-exe-size'  = @('"ffmpegExeSize":10', '"ffmpegExeSize":-1')
        }
        foreach ($key in $cases.Keys) {
            $pair = $cases[$key]
            $json = $base.Replace([string]$pair[0], [string]$pair[1])
            Assert-True ($json -ne $base) "case $key did not modify the fixture"
            $path = Join-Path $root ($key + '.json')
            [IO.File]::WriteAllText($path, $json, (New-Object Text.UTF8Encoding($false)))
            $thrown = $false
            try { Get-RuntimePins $path | Out-Null } catch { $thrown = $true }
            Assert-True $thrown "invalid pins accepted: $key"
        }
    }

    Run-Case 'production-pins-load' {
        $pins = Get-RuntimePins (Join-Path $repo 'tools\runtime-pins.json')
        foreach ($pin in @($pins.node, $pins.ffmpeg)) {
            Assert-True ($pin.url -like 'https://*') 'pin URL must be https'
            Assert-True ($pin.sha256 -match '^[0-9a-f]{64}$') 'pin sha256 must be 64 hex'
            Assert-True ([long]$pin.size -gt 0) 'pin size must be positive'
            Assert-True ([string]$pin.version -ne '') 'pin version missing'
        }
        Assert-True ($pins.ffmpeg.entryName -like '*/bin/ffmpeg.exe') 'unexpected ffmpeg entry'
        Assert-True ($pins.ffmpeg.ffmpegExeSha256 -match '^[0-9a-f]{64}$') 'ffmpeg exe sha256 must be 64 hex'
        Write-Host ('[runtime] pins: node ' + $pins.node.version + ' / ffmpeg ' + $pins.ffmpeg.version)
    }
} finally {
    if (Test-Path -LiteralPath $TestRoot) { Remove-Item -LiteralPath $TestRoot -Recurse -Force -ErrorAction SilentlyContinue }
}

Write-Host ("SUMMARY total={0} failed={1}" -f $script:CaseCount, $script:FailureCount)
if ($script:FailureCount -gt 0) { exit 1 }
exit 0
