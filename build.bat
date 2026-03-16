@echo off
setlocal

set "ROOT=%~dp0"

if "%JUCE_DIR%"=="" (
    echo ERROR: JUCE_DIR is not set.
    echo Set JUCE_DIR to your local JUCE checkout or pass -DJUCE_DIR when running CMake manually.
    exit /b 1
)

if "%CMAKE_GENERATOR%"=="" (
    set "CMAKE_GENERATOR=Ninja"
)

if /I "%CMAKE_GENERATOR%"=="Ninja" (
    where cl >nul 2>nul
    if errorlevel 1 (
        echo ERROR: Ninja builds with MSVC require a Developer Command Prompt or another shell with cl.exe on PATH.
        echo Set CMAKE_GENERATOR to another generator if you do not want to use Ninja.
        exit /b 1
    )
)

echo.
echo === Configuring with CMake ===
cmake -S "%ROOT%" -B "%ROOT%build" -G "%CMAKE_GENERATOR%" -DJUCE_DIR="%JUCE_DIR%"
if errorlevel 1 (
    echo ERROR: CMake configuration failed.
    exit /b 1
)

echo.
echo === Building ===
cmake --build "%ROOT%build" --config Debug
if errorlevel 1 (
    echo ERROR: Build failed.
    exit /b 1
)

echo.
echo === Build succeeded! ===
echo Executable: build\minihost_artefacts\Debug\minihost.exe
echo.
