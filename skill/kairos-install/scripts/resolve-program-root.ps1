#requires -Version 5.1
param([switch]$Quiet)
$ErrorActionPreference = 'Stop'
function ValidRoot([string]$p) {
    if (-not $p) { return $false }
    $p = $p.Trim().Trim('"')
    if (-not $p) { return $false }
    return (Test-Path -LiteralPath (Join-Path $p 'tools\bootstrap.ps1') -PathType Leaf)
}
$candidates = New-Object System.Collections.ArrayList
foreach ($key in @(
    'HKCU:\Software\Kairos\Installer',
    'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\Kairos_is1'
)) {
    try {
        $v = (Get-ItemProperty -LiteralPath $key -ErrorAction Stop).ProgramRoot
        if (-not $v) { $v = (Get-ItemProperty -LiteralPath $key -ErrorAction Stop).InstallLocation }
        if ($v) { [void]$candidates.Add([string]$v) }
    } catch {}
}
[void]$candidates.Add((Join-Path $env:ProgramFiles 'Kairos-snapshot'))
foreach ($p in $candidates) {
    if (ValidRoot $p) {
        if (-not $Quiet) { Write-Output ([IO.Path]::GetFullPath($p)) }
        exit 0
    }
}
[Console]::Error.WriteLine('找不到时序之子程序目录（需要其中的 tools\bootstrap.ps1）。请先运行安装包，或修复 HKCU\Software\Kairos\ProgramRoot。')
exit 2
