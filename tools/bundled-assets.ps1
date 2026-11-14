#requires -Version 5.1
# Shared repair primitive. Only catalogued package files may be replaced.
function Get-BundledCatalog([string]$Root,[string]$Catalog='') {
    if(-not $Catalog){$Catalog=Join-Path $Root 'tools\bundled-assets.json'}
    if(-not (Test-Path -LiteralPath $Catalog)){return $null}
    return ([IO.File]::ReadAllText($Catalog,[Text.Encoding]::UTF8)|ConvertFrom-Json)
}
function Resolve-BundledPath([string]$Root,[string]$Relative) {
    if($Relative -notmatch '^(sidecar/|bin/WebView2Loader\.dll$|WebView2Loader\.dll$|pack/mingling-vendor/)' -or $Relative -match '(^|/)\.\.(/|$)|[:\\]'){throw 'Invalid bundled path'}
    if($Relative -eq 'bin/WebView2Loader.dll' -and (Test-Path -LiteralPath (Join-Path $Root 'Kairos.exe')) -and -not (Test-Path -LiteralPath (Join-Path $Root 'win32\src\main.cpp'))){$Relative='WebView2Loader.dll'}
    $base=[IO.Path]::GetFullPath($Root).TrimEnd('\')+'\'
    $target=[IO.Path]::GetFullPath((Join-Path $base $Relative))
    if(-not $target.StartsWith($base,[StringComparison]::OrdinalIgnoreCase)){throw 'Bundled path escapes root'}
    $walk=Split-Path $target
    while($walk.Length -ge $base.TrimEnd('\').Length){
        if((Test-Path -LiteralPath $walk) -and ((Get-Item -LiteralPath $walk -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Bundled destination is a reparse point'}
        $next=Split-Path $walk;if($next -eq $walk){break};$walk=$next
    }
    if((Test-Path -LiteralPath $target) -and ((Get-Item -LiteralPath $target -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Bundled file is a reparse point'}
    return $target
}
function Test-BundledAsset([string]$Root,$Asset) {
    foreach($f in $Asset.files){
        $p=Resolve-BundledPath $Root $f.path
        if(-not (Test-Path -LiteralPath $p -PathType Leaf)){return $false}
        if((Get-Item -LiteralPath $p).Length -ne $f.size -or (Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant() -ne $f.sha256){return $false}
    }
    return $true
}
function Test-BundledNetwork {
    if($env:KAIROS_OFFLINE -eq '1'){return $false}
    try { $nlm=New-Object -ComObject 'NetworkListManager';return [bool]$nlm.IsConnectedToInternet } catch { return [Net.NetworkInformation.NetworkInterface]::GetIsNetworkAvailable() }
}
function Restore-BundledAsset([string]$Root,$Asset,[switch]$Offline,[string]$PackDir='') {
    if(Test-BundledAsset $Root $Asset){return}
    if($Asset.sha256 -notmatch '^[a-f0-9]{64}$' -or $Asset.name -notmatch '^[A-Za-z0-9._-]+\.zip$'){throw 'Invalid bundled archive identity'}
    if(-not $PackDir){$PackDir=Join-Path $Root 'pack\components'}
    $archive=Join-Path $PackDir $Asset.name
    $guide=('Release: https://github.com/YYZ-YPC/kairos/releases/tag/V1.0.0 ; offline pack: '+$archive+' ; verify: Get-FileHash -Algorithm SHA256 -LiteralPath "'+$archive+'" ; expected '+$Asset.sha256)
    $temp=Join-Path ([IO.Path]::GetTempPath()) ('kairos-bundle-'+[guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $temp -Force|Out-Null
    try {
        if(Test-Path -LiteralPath $archive){
            if((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $Asset.sha256){throw ('Local pack SHA256 mismatch; '+$guide)}
        }else{
            if($Offline -or -not (Test-BundledNetwork)){throw ('Offline; '+$guide)}
            if($Asset.url -notmatch '^https://github\.com/YYZ-YPC/kairos/releases/download/[^\s]+$'){throw 'HTTPS release URL required'}
            $archive=Join-Path $temp $Asset.name
            Invoke-WebRequest -UseBasicParsing -Uri $Asset.url -OutFile $archive -TimeoutSec 90
            if((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $Asset.sha256){throw 'Downloaded pack SHA256 mismatch'}
        }
        if((Get-Item -LiteralPath $archive).Length -ne $Asset.size){throw 'Archive size mismatch'}
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        $zip=[IO.Compression.ZipFile]::OpenRead($archive)
        $expected=@{};foreach($f in $Asset.files){$null=Resolve-BundledPath $Root $f.path;if($expected.ContainsKey([string]$f.path)){throw 'Duplicate catalog file'};$expected[[string]$f.path]=$f}
        $staged=@{}
        try {
            foreach($entry in $zip.Entries){
                if($entry.FullName.EndsWith('/')){continue}
                $name=$entry.FullName;if(-not $expected.ContainsKey($name) -or $staged.ContainsKey($name)){throw 'Unexpected or duplicate archive entry'}
                $f=$expected[$name];$p=Join-Path $temp ('files\'+$name);New-Item -ItemType Directory -Path (Split-Path $p) -Force|Out-Null
                [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$p,$true)
                if((Get-Item -LiteralPath $p).Length -ne $f.size -or (Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant() -ne $f.sha256){throw ('Per-file SHA256 mismatch: '+$name)}
                $staged[$name]=$p
            }
        }finally{$zip.Dispose()}
        if($staged.Count -ne $expected.Count){throw 'Archive is incomplete'}
        $committed=New-Object Collections.ArrayList
        try {
            foreach($name in @($staged.Keys|Sort-Object)){
                $dest=Resolve-BundledPath $Root $name;New-Item -ItemType Directory -Path (Split-Path $dest) -Force|Out-Null
                $backup='';if(Test-Path -LiteralPath $dest){$backup=Join-Path $temp ('backup\'+$name);New-Item -ItemType Directory -Path (Split-Path $backup) -Force|Out-Null;Copy-Item -LiteralPath $dest -Destination $backup -Force}
                [void]$committed.Add(@{dest=$dest;backup=$backup});Copy-Item -LiteralPath $staged[$name] -Destination $dest -Force
            }
            if(-not (Test-BundledAsset $Root $Asset)){throw 'Post-repair verification failed'}
        }catch{
            foreach($v in $committed){if($v.backup){Copy-Item -LiteralPath $v.backup -Destination $v.dest -Force}else{Remove-Item -LiteralPath $v.dest -Force -ErrorAction SilentlyContinue}}
            throw
        }
        Write-Output ('Repaired bundled component '+$Asset.id+'; archive and every file SHA256 verified')
    } finally {
        $resolved=[IO.Path]::GetFullPath($temp);$allowed=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\'
        if($resolved.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase) -and (Split-Path $resolved -Leaf).StartsWith('kairos-bundle-')){Remove-Item -LiteralPath $resolved -Recurse -Force -ErrorAction SilentlyContinue}
    }
}
