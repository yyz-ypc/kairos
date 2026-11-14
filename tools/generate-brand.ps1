$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$header=[IO.File]::ReadAllText((Join-Path $root 'win32\src\brand.h'),[Text.Encoding]::UTF8)
function Brand([string]$key) { $m=[regex]::Match($header,'(?m)^#define '+$key+' L?"([^"]*)"'); if(!$m.Success){throw "Missing brand $key"}; return $m.Groups[1].Value.Replace('\\','\') }
$values=[ordered]@{BrandName=(Brand 'BRAND_NAME');BrandId=(Brand 'BRAND_ID');BrandExe=(Brand 'BRAND_EXE');BrandShortcut=(Brand 'BRAND_SHORTCUT');BrandInstallDir=(Brand 'BRAND_INSTALL_DIR');BrandWorkspace=(Brand 'BRAND_WORKSPACE');BrandState=(Brand 'BRAND_STATE');BrandInstallerKey=(Brand 'BRAND_INSTALLER_KEY');BrandUninstallKey=(Brand 'BRAND_UNINSTALL_KEY');BrandAppId=('{{'+(Brand 'BRAND_APP_ID')+'}')}
$lines=@();foreach($key in $values.Keys){$lines+='#define '+$key+' "'+$values[$key]+'"'}
[IO.File]::WriteAllLines((Join-Path $root 'installer\brand_auto.iss'),$lines,(New-Object Text.UTF8Encoding($true)))
