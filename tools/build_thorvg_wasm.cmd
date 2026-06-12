@echo off
rem Builds ThorVG (software engine, no threads) for wasm32 into
rem ..\thorvg\build_wasm — consumed by the figmalib web build.
setlocal
if "%EMSDK_HOME%"=="" set EMSDK_HOME=D:\devlib\emsdk
call %EMSDK_HOME%\emsdk_env.bat >nul 2>&1
cd /d D:\work_open\thorvg
if not exist build_wasm\build.ninja (
    rem No lottie on the web: its jerryscript engine needs JS-sjlj, which
    rem conflicts with -fwasm-exceptions at link time.
    meson setup build_wasm --cross-file %~dp0wasm32_emscripten.txt --buildtype=release --default-library=static -Dstatic=true -Dthreads=false -Dsimd=false -Dengines=cpu -Dloaders=svg,ttf,png,jpg,webp -Dextra= -Dtools= -Dtests=false
    if errorlevel 1 exit /b 1
)
ninja -C build_wasm
