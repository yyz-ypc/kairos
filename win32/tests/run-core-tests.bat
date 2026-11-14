@echo off
rem 编译并运行 core 组的新测试：E1 配置缓存失效、E4 结构化事件日志、E5 WebView2 版本探测。
rem 用法（在 win32 目录下）：tests\run-core-tests.bat
rem 只用临时目录 + 环境变量，不需要真实安装；KAIROS_WORKSPACE 之类的变量用完会自己撤掉。
setlocal
cd /d "%~dp0.."

if not "%MINGW_BIN%"=="" goto mingw_ready
if exist "build.local.bat" call "build.local.bat"
:mingw_ready
set "GXX=g++"
if not "%MINGW_BIN%"=="" set "GXX=%MINGW_BIN%\g++.exe"
if not exist "%GXX%" set "GXX=g++"

if not exist "build" mkdir "build"

rem 跟 build.bat 同一套开关；-static 是必须的：否则运行时会去 PATH 里抓别的 libstdc++
set "FLAGS=-std=c++17 -O2 -Wall -DUNICODE -D_UNICODE -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 -Isrc"
set "LIBS=-static -static-libgcc -static-libstdc++ -lgdiplus -lole32 -loleaut32 -luuid -lshell32 -lshlwapi -lcomdlg32 -lcomctl32 -lws2_32 -liphlpapi -lwinhttp -lgdi32 -luser32 -ladvapi32"
set "COMMON=src\her\her_config.cpp src\her\her_json.cpp src\util.cpp src\paths.cpp"

echo [1/6] her_config_cache（E1）
"%GXX%" %FLAGS% tests\her_config_cache.test.cpp %COMMON% -o build\her_config_cache.test.exe %LIBS%
if errorlevel 1 goto fail
build\her_config_cache.test.exe
if errorlevel 1 goto fail

echo [2/6] her_event（E4）
"%GXX%" %FLAGS% tests\her_event.test.cpp src\her\her_event.cpp %COMMON% -o build\her_event.test.exe %LIBS%
if errorlevel 1 goto fail
build\her_event.test.exe
if errorlevel 1 goto fail

echo [3/6] webview_runtime（E5）
"%GXX%" %FLAGS% tests\webview_runtime.test.cpp src\her\her_webview.cpp -o build\webview_runtime.test.exe %LIBS%
if errorlevel 1 goto fail
build\webview_runtime.test.exe
if errorlevel 1 goto fail

echo [4/6] url-policy（H-01 SSRF / H-02 Base 校验）
"%GXX%" %FLAGS% tests\url-policy.test.cpp src\url_policy.cpp -o build\url-policy.test.exe %LIBS%
if errorlevel 1 goto fail
build\url-policy.test.exe
if errorlevel 1 goto fail

echo [5/6] dsh-api-guard（H-02 非法 Base 不发请求）
"%GXX%" %FLAGS% tests\dsh-api-guard.test.cpp src\biliapi.cpp src\url_policy.cpp src\her\her_json.cpp src\util.cpp src\paths.cpp -o build\dsh-api-guard.test.exe %LIBS%
if errorlevel 1 goto fail
build\dsh-api-guard.test.exe
if errorlevel 1 goto fail

echo [6/6] onboarding QQ hard gate
"%GXX%" %FLAGS% tests\onboarding.test.cpp src\her\her_json.cpp -o build\onboarding.test.exe %LIBS%
if errorlevel 1 goto fail
build\onboarding.test.exe
if errorlevel 1 goto fail

echo.
"%GXX%" %FLAGS% tests\release-policy.test.cpp src\her\her_json.cpp src\util.cpp src\paths.cpp -o build\release-policy.test.exe %LIBS%
if errorlevel 1 goto fail
build\release-policy.test.exe
if errorlevel 1 goto fail

echo OK: core 测试全过（her_config_cache + her_event + webview_runtime + url-policy + dsh-api-guard）
exit /b 0

:fail
echo.
echo FAILED: 上面有编译错误或断言没过
exit /b 1
