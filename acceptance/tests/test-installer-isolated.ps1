#requires -Version 5.1
param([string]$Setup='')
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if(-not $Setup){$Setup=Join-Path $repo 'dist\Kairos-1.0.0-setup.exe'}
$work=[IO.Path]::GetFullPath((Join-Path $repo ('.tmp\installer-qa-'+[guid]::NewGuid().ToString('N'))))
$scope=[IO.Path]::GetFullPath((Join-Path $repo '.tmp')).TrimEnd('\')+'\'
if(-not $work.StartsWith($scope,[StringComparison]::OrdinalIgnoreCase)){throw 'Test path outside workspace'}
$program=Join-Path $work 'program';$workspace=Join-Path $work 'workspace';$state=Join-Path $work 'state';$backup=Join-Path $work 'registry-backup'
New-Item -ItemType Directory -Force $work,$workspace,$state,$backup|Out-Null
$keys=@(@{key='HKCU\Software\Kairos\Installer';ps='Registry::HKEY_CURRENT_USER\Software\Kairos\Installer';file=(Join-Path $backup 'installer.reg')},@{key='HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\Kairos_is1';ps='Registry::HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Uninstall\Kairos_is1';file=(Join-Path $backup 'uninstall.reg')})
foreach($entry in $keys){$entry.existed=Test-Path -LiteralPath $entry.ps;if($entry.existed){& reg.exe export $entry.key $entry.file /y|Out-Null;if($LASTEXITCODE){throw 'Registry backup failed'}}}
$oldState=$env:KAIROS_STATE_DIR;$oldProfile=$env:USERPROFILE
$env:KAIROS_STATE_DIR=$state;$env:USERPROFILE=(Join-Path $work 'profile');New-Item -ItemType Directory -Force $env:USERPROFILE|Out-Null
$report=[ordered]@{version='V1.0.0';isolated=$true;qq_accounts_used=$false;cases=@();registry_restored=$false;uninstalled=$false}
$failure=$null
function Check([bool]$condition,[string]$name){$script:report.cases+=@{name=$name;passed=$condition};if(-not $condition){throw ('FAIL '+$name)};Write-Output ('PASS '+$name)}
try {
 $args='/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /NOICONS /DIR="'+$program+'" /WORKSPACE="'+$workspace+'" /LOG="'+(Join-Path $work 'install.log')+'"'
 $proc=Start-Process -FilePath $Setup -ArgumentList $args -Wait -PassThru -WindowStyle Hidden
 Check ($proc.ExitCode -eq 0) 'actual installer completed into isolated directory'
 Check ((Get-Item (Join-Path $program 'Kairos.exe')).VersionInfo.ProductVersion.Trim() -eq 'V1.0.0') 'installed executable reports formal version'
 Check (Test-Path (Join-Path $state 'settings.json')) 'installer honors isolated state directory'
 $repair=Join-Path $program 'tools\repair-bundled.ps1'
 $output=& powershell -NoProfile -ExecutionPolicy Bypass -File $repair -Root $program -Check -Offline 2>&1|Out-String
 Check ($LASTEXITCODE -eq 0) 'all six installed runtime assets match per-file hashes'
 $file=[IO.Path]::GetFullPath((Join-Path $program 'sidecar\hermes\src\main.jsc'))
 if(-not $file.StartsWith($work+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Refusing file removal outside test directory'}
 $expected=(Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash;Remove-Item -LiteralPath $file -Force
 $output=& powershell -NoProfile -ExecutionPolicy Bypass -File $repair -Root $program -Check -Offline 2>&1|Out-String
 Check ($LASTEXITCODE -eq 2 -and $output -match 'MISSING hermes') 'installed health check detects deleted bundled file'
 $output=& powershell -NoProfile -ExecutionPolicy Bypass -File $repair -Root $program -Id hermes -Offline 2>&1|Out-String
 Check ($LASTEXITCODE -eq 0 -and (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -eq $expected) 'installed repair restores deleted file from local pack and verifies SHA256'
 $output=& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $program 'tools\bootstrap.ps1') -Base $program -Workspace $workspace -Bots (Join-Path $workspace 'bots') -Component hermes -Offline 2>&1|Out-String
 $output | Set-Content -LiteralPath (Join-Path $work 'hermes-bootstrap.log') -Encoding UTF8
 Check ($LASTEXITCODE -eq 0 -and (Test-Path (Join-Path $state 'hermes\config\onebot.json'))) 'installed compiled hermes initializes offline in a clean workspace'
 $output=& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $program 'tools\bootstrap.ps1') -Base $program -Workspace $workspace -Bots (Join-Path $workspace 'bots') -Component rift -Offline 2>&1|Out-String
 $output | Set-Content -LiteralPath (Join-Path $work 'rift-bootstrap.log') -Encoding UTF8
 Check ($LASTEXITCODE -eq 0 -and (Test-Path (Join-Path $workspace 'bots\Rift\rift.config.json'))) 'installed compiled rift initializes offline with pinned bundled npm dependencies'
 $entry=([uri](Join-Path $workspace 'bots\Rift\lib\index.js')).AbsoluteUri
 $output=& (Join-Path $program 'sidecar\node\node.exe') --input-type=module -e 'await import(process.argv[1]);' $entry 2>&1|Out-String
 Check ($LASTEXITCODE -eq 0) 'installed Rift bytecode loads in bundled Node without network'
 Check ((Test-Path (Join-Path $program 'sidecar\node\LICENSE')) -and (Test-Path (Join-Path $program 'sidecar\hermes\vendor\silk-wasm\LICENSE')) -and (Test-Path (Join-Path $program 'sidecar\rift\LICENSE'))) 'third-party notices remain installed'
 Check (-not (Test-Path (Join-Path $program 'sidecar\hermes\native\hook.cpp')) -and -not (Test-Path (Join-Path $program 'pack\login'))) 'private native source and login package are absent'
 Check ((Test-Path (Join-Path $program 'art\character\hero.webp')) -and (Test-Path (Join-Path $program 'art\character\avatar.webp'))) 'Mingling redrawn hero and original avatar remain installed'
} catch {$failure=$_.Exception.Message} finally {
 try {
  $uninstall=Join-Path $program 'unins000.exe'
  if(Test-Path -LiteralPath $uninstall){$p=Start-Process -FilePath $uninstall -ArgumentList '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART' -Wait -PassThru -WindowStyle Hidden;$report.uninstalled=$p.ExitCode -eq 0}
 } finally {
  foreach($entry in $keys){
   if(Test-Path -LiteralPath $entry.ps){Remove-Item -LiteralPath $entry.ps -Recurse -Force}
   if($entry.existed){& reg.exe import $entry.file|Out-Null;if($LASTEXITCODE){throw 'Registry restore failed'}}
  }
  $report.registry_restored=$true
  if($null -eq $oldState){Remove-Item Env:KAIROS_STATE_DIR -ErrorAction SilentlyContinue}else{$env:KAIROS_STATE_DIR=$oldState}
  if($null -eq $oldProfile){Remove-Item Env:USERPROFILE -ErrorAction SilentlyContinue}else{$env:USERPROFILE=$oldProfile}
 }
}
$report.failure=$failure;$out=Join-Path $repo 'evidence\release\installer-test.json';New-Item -ItemType Directory -Force (Split-Path $out)|Out-Null
[IO.File]::WriteAllText($out,($report|ConvertTo-Json -Depth 7),(New-Object Text.UTF8Encoding($false)))
if($failure){throw $failure}
if(-not $report.uninstalled){throw 'Isolated uninstall did not complete'}
Write-Output ('SUMMARY total='+$report.cases.Count+' failed=0; registry restored')
