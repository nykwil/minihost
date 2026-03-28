# minihost — Claude Code Guide

## Build

### Requirements
- Windows, CMake 3.22+, Ninja, Visual Studio (MSVC)
- A local [JUCE](https://github.com/juce-framework/JUCE) checkout

### Environment
Set `JUCE_DIR` to your JUCE checkout before building:
```
set JUCE_DIR=C:\path\to\JUCE
```

`cl.exe` must be on PATH. The easiest way is to run from a Visual Studio Developer Command Prompt, or call `vcvars64.bat` before building.

### Build command
```
build.bat
```

This configures CMake with Ninja into `build/` and compiles a Debug build. Output: `build\minihost_artefacts\Debug\minihost.exe`

### Running from Claude Code (bash shell)
Since `cl.exe` is not on PATH in a plain bash shell, invoke the build by calling `vcvars64.bat` first:

```bash
"/path/to/vcvars64.bat" > /tmp/build_out.txt 2>&1
# or write a wrapper .bat that calls vcvars then build.bat, and run that
```

The pattern that works reliably from bash:
1. Write a `.bat` that calls `vcvars64.bat` then runs cmake configure + build
2. Execute that `.bat` directly from bash and redirect output to a temp file
3. Read the temp file for results

### Clean build
Delete `build/` and re-run `build.bat`.

## Testing
No automated test suite. Manual test: run `minihost.exe` with a config file pointing to a VST3 plugin, audio file, and MIDI file. See `minihost_config.example.json`.
