#requires -Version 5.1
param([string]$Exe = '', [switch]$VerifyExe)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$utf8 = New-Object Text.UTF8Encoding($false, $true)
$files = @((Join-Path $root 'win32\res\app.rc'), (Join-Path $root 'installer\kairos.iss'))
$files += @(Get-ChildItem -LiteralPath (Join-Path $root 'tools'), (Join-Path $root 'installer'), (Join-Path $root 'acceptance') -Filter '*.ps1' -File -Recurse | Where-Object { $_.FullName -notmatch '[\\/]evidence[\\/]' } | ForEach-Object FullName)
foreach ($file in $files) {
    $bytes = [IO.File]::ReadAllBytes($file)
    try { $text = $utf8.GetString($bytes) } catch { throw "Invalid UTF-8: $file" }
    # Windows PowerShell 5.1 decodes BOM-less scripts as the ANSI code page.
    if ([IO.Path]::GetExtension($file) -eq '.ps1' -and $text -match '[^\x00-\x7F]' -and
        -not ($bytes.Length -ge 3 -and $bytes[0] -eq 239 -and $bytes[1] -eq 187 -and $bytes[2] -eq 191)) {
        throw "Non-ASCII PowerShell requires UTF-8 BOM for Windows PowerShell 5.1: $file"
    }
    if ($text.Contains([string][char]0xfffd)) { throw "Replacement character in source: $file" }
}
$build = [IO.File]::ReadAllText((Join-Path $root 'win32\build.bat'))
if ($build -notmatch '"%WINDRES%" -c 65001') { throw 'windres must use -c 65001' }
if ($VerifyExe) {
    if (-not $Exe) { $Exe = Join-Path $root 'bin\Kairos.exe' }
    $v = (Get-Item -LiteralPath $Exe).VersionInfo
    $brand = [IO.File]::ReadAllText((Join-Path $root 'win32\src\brand.h'), [Text.Encoding]::UTF8)
    $name = [regex]::Match($brand, '(?m)^#define BRAND_NAME L"([^"]+)"').Groups[1].Value
    if ($v.ProductName -ne $name -or -not $v.FileDescription.StartsWith($name)) {
        throw 'Executable Chinese product name or file description is incorrect'
    }
    Write-Output ('Executable resources OK: ' + $v.ProductName + ' / ' + $v.FileDescription)
}
Write-Output ('Encoding OK: ' + $files.Count + ' sources; windres UTF-8')
