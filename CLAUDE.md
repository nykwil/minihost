# minihost — Claude Code Guide

## Build

### Requirements
- CMake 3.22+, Ninja, a C++20 compiler
- A local [JUCE](https://github.com/juce-framework/JUCE) checkout (`JUCE_DIR`)

### Build commands

**Windows:**
```bat
set JUCE_DIR=C:\path\to\JUCE
build.bat
```
Requires `cl.exe` on PATH — use a Developer Command Prompt, or call `vcvars64.bat` first.

**macOS / Linux:**
```sh
export JUCE_DIR=/path/to/JUCE
./build.sh
```

### Running builds from a bash shell (e.g. Claude Code on Windows)
`cl.exe` is not on PATH in a plain bash shell. The reliable pattern:
1. Write a `.bat` that calls `vcvars64.bat` then runs the cmake commands
2. Execute that `.bat` directly from bash, redirecting output to a temp file
3. Read the temp file for results — do NOT chain commands with `&&` from bash into cmd.exe, as output gets truncated

Example wrapper pattern:
```bat
@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat"
cmake --build D:\path\to\minihost\build --config Debug
echo BUILD_EXIT=%ERRORLEVEL%
```

### Build output paths
| Platform | Path |
|----------|------|
| Windows  | `build\minihost_artefacts\Debug\minihost.exe` |
| macOS    | `build/minihost_artefacts/Debug/minihost.app/Contents/MacOS/minihost` |
| Linux    | `build/minihost_artefacts/Debug/minihost` |

### Clean build
Delete `build/` and re-run the build script.

## Testing
No automated test suite. Manual test: run with `--test` and a VST3 plugin path.
```sh
./minihost --test /path/to/plugin.vst3
```
Exits 0 on success, 1 on failure. In `--test` mode the audio device is skipped, so it works even when a DAW holds the device.
