@echo off
rem Heart P0 smoke tests: build a standalone console test and run it.
rem Does not touch build.bat or the product exe.
setlocal
cd /d "%~dp0"
cd ..

rem MinGW: prefer %MINGW_BIN%, then build.local.bat, then g++ from PATH
if not "%MINGW_BIN%"=="" goto mingw_ready
if exist "%~dp0build.local.bat" call "%~dp0build.local.bat"
:mingw_ready
set "GXX=g++"
if not "%MINGW_BIN%"=="" set "GXX=%MINGW_BIN%\g++.exe"
if not exist "%GXX%" set "GXX=g++"

if not exist "build" mkdir "build"

set "SRC=src\heart\store.cpp src\heart\http.cpp src\heart\kernel.cpp src\heart\eventbus.cpp src\her\her_json.cpp tests\heart_p0.test.cpp"

"%GXX%" -std=c++17 -O2 -Wall -Wextra -Wno-cast-function-type -DUNICODE -D_UNICODE -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 -municode -Isrc %SRC% -o build\heart_p0.test.exe -static -static-libgcc -static-libstdc++ -lws2_32 -ladvapi32
if errorlevel 1 (
  echo.
  echo HEART TEST BUILD FAILED - see errors above.
  exit /b 1
)

rem 独立可执行也编一遍，确认 main.cpp 可用（不进产品构建）
set "EXE_SRC=src\heart\store.cpp src\heart\http.cpp src\heart\kernel.cpp src\heart\eventbus.cpp src\her\her_json.cpp src\heart\main.cpp"
"%GXX%" -std=c++17 -O2 -Wall -Wno-cast-function-type -DUNICODE -D_UNICODE -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 -municode -Isrc %EXE_SRC% -o build\heart.exe -static -static-libgcc -static-libstdc++ -lws2_32 -ladvapi32
if errorlevel 1 (
  echo.
  echo HEART EXE BUILD FAILED - see errors above.
  exit /b 1
)
echo [run] build\heart.exe --port 0 --check
build\heart.exe --port 0 --check --home "%~dp0..\build\heart-home"
if errorlevel 1 exit /b 1

echo.
echo [run] build\heart_p0.test.exe
build\heart_p0.test.exe
set "RC=%ERRORLEVEL%"
echo exit=%RC%
exit /b %RC%
