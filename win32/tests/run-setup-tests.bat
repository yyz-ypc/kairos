@echo off
setlocal
cd /d "%~dp0.."
if exist build.local.bat call build.local.bat
set "GXX=g++"
if not "%MINGW_BIN%"=="" set "GXX=%MINGW_BIN%\g++.exe"
"%GXX%" -std=c++17 -O2 -Wall -DUNICODE -D_UNICODE -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 -ffunction-sections -fdata-sections -Isrc tests\setup_manager.test.cpp src\her\her_setup.cpp src\her\her_config.cpp src\her\her_json.cpp src\util.cpp src\paths.cpp -Wl,--gc-sections -o build\setup_manager.test.exe -static -static-libgcc -static-libstdc++ -lgdiplus -lole32 -loleaut32 -luuid -lshell32 -lshlwapi -lcomdlg32 -lcomctl32 -lws2_32 -liphlpapi -lwinhttp -lgdi32 -luser32 -ladvapi32
exit /b %errorlevel%
