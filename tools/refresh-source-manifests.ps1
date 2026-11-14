#requires -Version 5.1
# Explicit release preparation after rebuilding bundled components. Third-party download pins are not changed.
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$file=Join-Path $repo 'tools\components.json'
$doc=[IO.File]::ReadAllText($file,[Text.Encoding]::UTF8) | ConvertFrom-Json
foreach($component in $doc.components) {
    if($component.source.kind -ne 'bundled') { continue }
    $exclude=@($component.install.steps | Where-Object kind -eq 'copy_tree' | ForEach-Object exclude) -join ';'
    $args_= @('-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $PSScriptRoot 'source-manifest.ps1'),
        '-Root',(Join-Path $repo $component.source.url),'-Manifest',(Join-Path $repo $component.source.manifest),'-Create')
    if($exclude) { $args_ += @('-Exclude',$exclude) }
    $digest=(& powershell.exe @args_ | Select-Object -Last 1).Trim()
    if($LASTEXITCODE -ne 0 -or $digest -notmatch '^[a-f0-9]{64}$') { throw ('Cannot refresh '+$component.id) }
    $component.source.sha256=$digest
    foreach($step in $component.install.steps) { if($step.kind -in @('copy_tree','verify_tree') -and $step.manifest) { $step.sha256=$digest } }
    Write-Output ('Refreshed '+$component.id)
}
[IO.File]::WriteAllText($file,($doc | ConvertTo-Json -Depth 40)+"`n",(New-Object Text.UTF8Encoding($false)))
& (Join-Path $PSScriptRoot 'check-source-manifests.ps1')
if($LASTEXITCODE -ne 0) { throw 'Bundled sources failed verification' }
