#requires -Version 5.1
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $repo 'tools\bundled-assets.ps1')
Add-Type -AssemblyName System.IO.Compression.FileSystem
$root=Join-Path $repo ('.tmp\bundle-test-'+[guid]::NewGuid().ToString('N'));New-Item -ItemType Directory -Path $root -Force|Out-Null
$cases=0
function Assert([bool]$condition,[string]$name){if(-not $condition){throw ('FAIL '+$name)};$script:cases++;Write-Output ('PASS '+$name)}
function Hash([string]$p){return (Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$payload=Join-Path $root 'source\sidecar\hermes\native\build\fixture.dll';New-Item -ItemType Directory -Path (Split-Path $payload) -Force|Out-Null;[IO.File]::WriteAllBytes($payload,[byte[]](1,2,3,4,5))
$pack=Join-Path $root 'target\pack\components';New-Item -ItemType Directory -Path $pack -Force|Out-Null;$zip=Join-Path $pack 'fixture.zip';$z=[IO.Compression.ZipFile]::Open($zip,'Create');try{[void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($z,$payload,'sidecar/hermes/native/build/fixture.dll')}finally{$z.Dispose()}
$a=[pscustomobject]@{id='hermes';name='fixture.zip';url='https://github.com/YYZ-YPC/kairos/releases/download/V1.0.0/fixture.zip';sha256=(Hash $zip);size=(Get-Item $zip).Length;files=@([pscustomobject]@{path='sidecar/hermes/native/build/fixture.dll';size=5;sha256=(Hash $payload)})}
$target=Join-Path $root 'target';$network=0
function Test-BundledNetwork{$script:network++;throw 'Network must not be consulted when local pack is valid or Offline is set'}
Assert (-not (Test-BundledAsset $target $a)) 'missing bundled file detected'
Restore-BundledAsset -Root $target -Asset $a -Offline
Assert (Test-BundledAsset $target $a) 'local pack repaired missing file; per-file SHA256 passed'
Assert ($network -eq 0) 'offline repair made no network attempt'
$dest=Join-Path $target 'sidecar\hermes\native\build\fixture.dll';[IO.File]::WriteAllBytes($dest,[byte[]](9,9,9));$before=Hash $dest
$originalHash=$a.sha256;$a.sha256=('0'*64);$rejected=$false
try{Restore-BundledAsset -Root $target -Asset $a -Offline}catch{$rejected=$_.Exception.Message -like '*SHA256 mismatch*'}
Assert $rejected 'tampered archive digest rejected';Assert ((Hash $dest) -eq $before) 'rejection preserved existing file'
$a.sha256=$originalHash;$fileHash=$a.files[0].sha256;$a.files[0].sha256=('0'*64);$rejected=$false
try{Restore-BundledAsset -Root $target -Asset $a -Offline}catch{$rejected=$_.Exception.Message -like '*Per-file SHA256 mismatch*'}
Assert $rejected 'tampered per-file digest rejected before writes';Assert ((Hash $dest) -eq $before) 'per-file rejection preserved original'
$a.files[0].sha256=$fileHash;Move-Item -LiteralPath $zip -Destination ($zip+'.held');$message=''
try{Restore-BundledAsset -Root $target -Asset $a -Offline}catch{$message=$_.Exception.Message}
Assert ($message -like '*https://github.com/YYZ-YPC/kairos/releases*' -and $message -like '*offline pack*' -and $message -like '*Get-FileHash*') 'offline guidance includes Release, location and verification command'
Assert ($network -eq 0) 'offline without pack made zero network calls'
$rejected=$false;try{Resolve-BundledPath $target 'sidecar/../../escape.dll'}catch{$rejected=$true};Assert $rejected 'path traversal rejected'
Move-Item -LiteralPath ($zip+'.held') -Destination $zip
Restore-BundledAsset -Root $target -Asset $a -Offline
Assert (Test-BundledAsset $target $a) 'repair can recover corrupted file'
Write-Output ('SUMMARY total='+$cases+' failed=0')
