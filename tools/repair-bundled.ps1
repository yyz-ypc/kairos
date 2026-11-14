#requires -Version 5.1
param([string]$Root=(Split-Path -Parent $PSScriptRoot),[string[]]$Id=@(),[switch]$Offline,[switch]$Check,[string]$PackDir='')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'bundled-assets.ps1')
$catalog=Get-BundledCatalog $Root
if(-not $catalog){throw 'Bundled asset catalog missing; use the matching Release pack'}
$missing=0
foreach($asset in $catalog.assets){
 if($Id.Count -and $Id -notcontains $asset.id){continue}
 if(Test-BundledAsset $Root $asset){Write-Output ('OK '+$asset.id);continue}
 if($Check){$missing++;Write-Output ('MISSING '+$asset.id);continue}
 Restore-BundledAsset -Root $Root -Asset $asset -Offline:$Offline -PackDir $PackDir
}
if($missing){exit 2}
