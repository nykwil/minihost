@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 (
    echo ERROR: Could not find vcvars64.bat. Adjust path to your VS install.
    pause
    exit /b 1
)

echo.
echo === Configuring with CMake ===
cmake -B build -G Ninja
if errorlevel 1 (
    echo ERROR: CMake configuration failed.
    pause
    exit /b 1
)

echo.
echo === Building with Ninja ===
:: Force recompile of user source files to avoid stale cache issues
del /q build\CMakeFiles\minihost.dir\Source\*.obj 2>nul
ninja -C build
if errorlevel 1 (
    echo ERROR: Build failed.
    pause
    exit /b 1
)

echo.
echo === Build succeeded! ===
echo Executable: build\minihost_artefacts\Debug\minihost.exe
echo.
echo Usage:
echo   minihost.exe [--test] "C:\path\to\plugin.vst3"
echo   --test  : Process one block and exit immediately (for CI/testing)
echo   (no flag): Open plugin GUI and run until closed
echo.
