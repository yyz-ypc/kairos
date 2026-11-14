# Source-only live transfer probe. Never installs/extracts the downloaded package.
param([string]$Destination,[string]$Id,[string]$Url)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $repo 'tools\common.ps1')
. (Join-Path $repo 'tools\progress.ps1')
$manifest=[IO.File]::ReadAllText((Join-Path $repo 'tools\components.json')) | ConvertFrom-Json
$c=$manifest.components | Where-Object { $_.id -eq $Id } | Select-Object -First 1
Progress-Begin -Directory $Destination -Components @($c) -Selection @($Id)
try {
    Get-File -Urls @($Url) -OutFile (Join-Path $Destination 'package.partial') -ExpectedBytes $c.size_bytes -Sha256 $c.source.sha256 -ProgressId $Id | Out-Null
    Progress-Total -State done -Phase '下载完成（未安装）' -Force
} catch {
    if ($_.Exception.Message -eq 'KAIROS_SETUP_PAUSED') { exit 75 }
    Write-Host $_.Exception.Message
    exit 1
}
