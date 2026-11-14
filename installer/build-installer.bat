@echo off
setlocal
cd /d "%~dp0.."
if defined KAIROS_ISCC (set "ISCC=%KAIROS_ISCC%") else (set "ISCC=tools\innosetup6\app\ISCC.exe")
if not exist "%ISCC%" set ISCC=ISCC.exe
"%ISCC%" /? >nul 2>&1
rem Inno Setup 6 displays /? help with exit code 1; 2+ means this probe failed.
if errorlevel 2 (echo [ERROR] ISCC cannot start. Installer is NOT verified. & exit /b 1)

rem Optional switch: -BundleLogin bundles the official QQ login-client zip into setup.exe (off by default).
rem Read the upstream license (section 5.4) before shipping a bundled build: packing it into a third-party installer needs prior written permission.
set "BUNDLE=0"
set "OFFLINE=0"
set "APP_BINARY=bin\Kairos.exe"
for %%a in (%*) do (
  if /i "%%~a"=="-BundleLogin" set "BUNDLE=1"
  if /i "%%~a"=="-Offline" set "OFFLINE=1"
  if /i "%%~a"=="-FromBuild" set "APP_BINARY=win32\build\Kairos.exe"
)
if not exist "%APP_BINARY%" ( echo [ERROR] %APP_BINARY% missing; run win32\build.bat first. & exit /b 1 )
set "OFFLINE_ARG="
if "%OFFLINE%"=="1" set "OFFLINE_ARG= -Offline"
if not exist "bin\WebView2Loader.dll" ( echo [ERROR] bin\WebView2Loader.dll missing; run win32\build.bat first. & exit /b 1 )
if not exist "sidecar\rift\tools\bili.mjs" ( echo [ERROR] sidecar\rift missing. & exit /b 1 )
if not exist "sidecar\node\node.exe" ( echo [ERROR] sidecar\node\node.exe missing; run tools\fetch-runtime.ps1. & exit /b 1 )
if not exist "sidecar\rift\bin\ffmpeg.exe" ( echo [ERROR] sidecar ffmpeg missing; run tools\fetch-runtime.ps1. & exit /b 1 )
rem 潮信 TideLetter 随包内置：源码、ws 运行时、vendor 与原生 dll/exe 缺一个就别出包（装完要能离线启动）。
if not exist "sidecar\hermes\src\main.mjs" ( echo [ERROR] sidecar\hermes missing; the bundled QQ bridge is required. & exit /b 1 )
if not exist "sidecar\hermes\vendor\ws\wrapper.mjs" ( echo [ERROR] sidecar\hermes\vendor\ws missing; the runtime loads ws from vendor. & exit /b 1 )
if not exist "sidecar\hermes\vendor\silk-wasm\silk.wasm" ( echo [ERROR] sidecar\hermes\vendor\silk-wasm missing. & exit /b 1 )
if not exist "sidecar\hermes\native\build\hermes-qq-hook.dll" ( echo [ERROR] Hermes native hook dll missing; run sidecar\hermes\native\build.ps1. & exit /b 1 )
if not exist "sidecar\hermes\native\build\hermes-qq-launcher.exe" ( echo [ERROR] Hermes native launcher missing; run sidecar\hermes\native\build.ps1. & exit /b 1 )
if not exist "tools\source-manifests\hermes.json" ( echo [ERROR] tools\source-manifests\hermes.json missing. & exit /b 1 )
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0..\tools\check-source-manifests.ps1"
if errorlevel 1 ( echo [ERROR] bundled source trees do not match components.json sha256; re-run tools\source-manifest.ps1 -Create for the changed component. & exit /b 1 )
if not exist "installer\secrets-README.txt" ( echo [ERROR] installer\secrets-README.txt missing; it goes into ^<Base^>\secrets. & exit /b 1 )
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0..\tools\check-version.ps1"
if errorlevel 1 ( echo [ERROR] version.h numbers and strings disagree; fix before packaging. & exit /b 1 )

rem Version numbers come from win32\src\version.h only. This script just copies three strings to Inno.
rem   AppVer      = file version (4 parts, 3.0.0.2) -> VersionInfoVersion / resource version
rem   AppVerTag   = human readable tag (V3-snapshot-2) -> display version in Apps & features
rem   AppVerShort = short string for the setup file name (3-snapshot-2)
rem   AppSnapshot = channel switch (1 = snapshot; separate AppId / install dir / uninstall entry)
set "VSTR=" & set "VTAG=" & set "VSHORT=" & set "VSNAP=0"
for /f "tokens=3" %%a in ('findstr /b /c:"#define APP_VERSION_STR " "%~dp0..\win32\src\version.h"') do set "VSTR=%%a"
for /f "tokens=3" %%a in ('findstr /b /c:"#define APP_VERSION_TAG " "%~dp0..\win32\src\version.h"') do set "VTAG=%%a"
for /f "tokens=3" %%a in ('findstr /b /c:"#define APP_VERSION_SHORT " "%~dp0..\win32\src\version.h"') do set "VSHORT=%%a"
for /f "tokens=3" %%a in ('findstr /b /c:"#define APP_SNAPSHOT " "%~dp0..\win32\src\version.h"') do set "VSNAP=%%a"
if defined VSTR set "VSTR=%VSTR:"=%"
if defined VTAG set "VTAG=%VTAG:"=%"
if defined VSHORT set "VSHORT=%VSHORT:"=%"
if defined VSNAP set "VSNAP=%VSNAP:"=%"
if "%VSTR%"=="" ( echo [ERROR] cannot read APP_VERSION_STR from version.h. & exit /b 1 )
if "%VTAG%"=="" ( echo [ERROR] cannot read APP_VERSION_TAG from version.h. & exit /b 1 )
if "%VSHORT%"=="" ( echo [ERROR] cannot read APP_VERSION_SHORT from version.h. & exit /b 1 )
rem A running old exe can prevent build.bat from copying the new build. Never package it as a new version.
powershell -NoProfile -Command "if ((Get-Item -LiteralPath '%APP_BINARY%').VersionInfo.FileVersion -ne '%VSTR%') { Write-Host '[ERROR] exe version differs from version.h. Rebuild before packaging.'; exit 1 }"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0..\tools\check-encoding.ps1" -VerifyExe -Exe "%APP_BINARY%"
if errorlevel 1 exit /b 1
if "%VSNAP%"=="" set "VSNAP=0"
> "installer\version_auto.iss" echo ; generated by installer\build-installer.bat from win32\src\version.h - do not edit
>> "installer\version_auto.iss" echo #define AppVer "%VSTR%"
>> "installer\version_auto.iss" echo #define AppVerTag "%VTAG%"
>> "installer\version_auto.iss" echo #define AppVerShort "%VSHORT%"
>> "installer\version_auto.iss" echo #define AppSnapshot %VSNAP%
echo [VERSION] %VTAG% (file %VSTR%, setup %VSHORT%, snapshot=%VSNAP%)

rem login_auto.iss tells Inno whether the login client rides along. Without -BundleLogin
rem this step only writes BundleLogin=0, so packaging behaves exactly as before.
rem It carries no version numbers, so it is regenerated every run to drop stale flags.
if "%BUNDLE%"=="1" (
  echo [BUNDLE] preparing the bundled login-client package ...
  powershell -NoProfile -ExecutionPolicy Bypass -File "installer\prepare-login.ps1" -Enable%OFFLINE_ARG%
  if errorlevel 1 ( echo [ERROR] login-client bundle is not ready; packaging aborted. & exit /b 1 )
) else (
  powershell -NoProfile -ExecutionPolicy Bypass -File "installer\prepare-login.ps1"
  if errorlevel 1 ( echo [ERROR] cannot reset login_auto.iss. & exit /b 1 )
)

powershell -NoProfile -ExecutionPolicy Bypass -File tools\generate-brand.ps1
if errorlevel 1 exit /b 1
if not exist "dist" mkdir dist
"%ISCC%" /DAppExeSource="..\%APP_BINARY%" installer\kairos.iss
if errorlevel 1 ( echo [ERROR] Inno Setup build failed. & exit /b 1 )
rem Bundled builds get their own file name so they never overwrite the release artifact.
set "OUTSUFFIX="
if "%BUNDLE%"=="1" set "OUTSUFFIX=-login"
echo [OK] dist\Kairos-%VSHORT%-setup%OUTSUFFIX%.exe
