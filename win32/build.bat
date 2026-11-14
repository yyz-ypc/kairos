@echo off
rem Build Mingling Workbench (Win32 + embedded HTML / WebView2)
setlocal
cd /d "%~dp0"
set "OUTDIR=%~dp0..\bin"
rem Optional output basename for side-by-side UI review (build.bat Kairos-html.exe).
for /f "delims=" %%N in ('node "%~dp0..\tools\brand.mjs" BRAND_EXE') do set "OUTPUT_NAME=%%N"
if not "%~1"=="" set "OUTPUT_NAME=%~nx1"

rem MinGW: prefer %MINGW_BIN%, then build.local.bat (git-ignored), then g++/windres from PATH
if not "%MINGW_BIN%"=="" goto mingw_ready
if exist "%~dp0build.local.bat" call "%~dp0build.local.bat"
:mingw_ready
set "GXX=g++"
set "WINDRES=windres"
if not "%MINGW_BIN%"=="" set "GXX=%MINGW_BIN%\g++.exe"
if not "%MINGW_BIN%"=="" set "WINDRES=%MINGW_BIN%\windres.exe"
if not exist "%GXX%" set "GXX=g++"
if not exist "%WINDRES%" set "WINDRES=windres"

if not exist "build" mkdir "build"
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

rem 版本自检：version.h 里的数字和字符串必须一致（没有 PowerShell 就跳过）
where powershell >nul 2>nul
if errorlevel 1 ( echo [版本] 没有 powershell，跳过自检 & goto version_ok )
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0..\tools\check-version.ps1"
if errorlevel 1 ( echo [ERROR] version.h 的数字和字符串对不上，先修好再编译。 & exit /b 1 )
:version_ok
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0..\tools\check-encoding.ps1"
if errorlevel 1 goto fail

if not exist "%~dp0..\sidecar\hermes\native\build.ps1" goto prebuilt_hermes
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0..\sidecar\hermes\native\build.ps1" -Compiler "%GXX%"
if errorlevel 1 goto fail
goto hermes_ready
:prebuilt_hermes
if not exist "%~dp0..\sidecar\hermes\native\build\hermes-qq-hook.dll" (echo Restore matching Release assets with tools\repair-bundled.ps1 first. & exit /b 1)
:hermes_ready

echo [1/3] resources
rem -c 65001：res\app.rc 是 UTF-8（带 BOM），windres 默认按系统 ANSI 代码页(936)读，
rem 中文会变成「鍐ラ浂」这种乱码并被截成问号——属性对话框的「文件说明 / 产品名称」就是那里来的。
"%WINDRES%" -c 65001 -I res -I src -O coff -o build\app.res.o res\app.rc
if errorlevel 1 goto fail

echo [2/3] compile
"%GXX%" -std=c++17 -O2 -Wall -DUNICODE -D_UNICODE -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 -fno-ident -ffunction-sections -fdata-sections src\main.cpp src\util.cpp src\paths.cpp src\whale.cpp src\biliapi.cpp src\webfetch.cpp src\url_policy.cpp src\ui.cpp src\webui.cpp src\her\her_json.cpp src\her\her_probe.cpp src\her\her_sqlite.cpp src\her\her_config.cpp src\her\her_features.cpp src\her\her_astr.cpp src\her\her_status.cpp src\her\her_actions.cpp src\her\her_setup.cpp src\her\her_selfupdate.cpp src\her\her_tide.cpp src\her\her_event.cpp src\her\her_webview.cpp build\app.res.o -o build\%OUTPUT_NAME% -Wl,--gc-sections -static -static-libgcc -static-libstdc++ -s -mwindows -lgdiplus -lwinhttp -lole32 -loleaut32 -luuid -lshell32 -lshlwapi -lcomdlg32 -lcomctl32 -lws2_32 -liphlpapi -lgdi32 -luser32 -ladvapi32
if errorlevel 1 goto fail

echo [3/3] copy
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0..\tools\check-encoding.ps1" -VerifyExe -Exe "%~dp0build\%OUTPUT_NAME%"
if errorlevel 1 goto fail
rem The loader is unchanged between app builds and may be locked by an open app.
if not exist "vendor\webview2\WebView2Loader.dll" goto restored_loader
if not exist "%OUTDIR%\WebView2Loader.dll" goto copy_loader
fc /b vendor\webview2\WebView2Loader.dll "%OUTDIR%\WebView2Loader.dll" >nul 2>nul
if not errorlevel 1 goto loader_ready
:restored_loader
if exist "%OUTDIR%\WebView2Loader.dll" goto loader_ready
echo Restore the WebView2 loader asset before building.
exit /b 1
:copy_loader
copy /y vendor\webview2\WebView2Loader.dll "%OUTDIR%\WebView2Loader.dll" >nul
if errorlevel 1 goto fail
:loader_ready
copy /y build\%OUTPUT_NAME% "%OUTDIR%\%OUTPUT_NAME%" >nul
if errorlevel 1 goto copylocked
echo.
echo OK: %OUTDIR%\%OUTPUT_NAME%
for %%F in ("%OUTDIR%\%OUTPUT_NAME%") do echo size: %%~zF bytes
exit /b 0

:copylocked
echo.
echo COPY FAILED: %OUTDIR%\%OUTPUT_NAME% is locked.
echo Close the running app (it is started from bin\) and run build.bat again.
echo What is in bin\ right now is still the OLD exe.
exit /b 1

:fail
echo.
echo BUILD FAILED - see errors above.
exit /b 1
