# minihost

`minihost` is a small VST3 plugin host built with C++ and [JUCE](https://juce.com/). It can open a plugin editor for interactive use or run a short headless processing pass with `--test` for automation and smoke tests.

## Features

- Loads a VST3 plugin from a file or bundle path
- Opens the plugin GUI when an editor is available
- Supports headless `--test` mode for quick validation runs
- Routes multiple audio and MIDI inputs with `audio_<n>` / `midi_<n>` config keys
- Accepts audio file paths or live input channel IDs for audio slots
- Accepts MIDI file paths or MIDI input device IDs for MIDI slots
- Falls back to generated sine audio and a simple MIDI note pattern when no inputs are configured
- Supports configurable BPM through config or `--bpm`
- Logs to `stdout`/`stderr` in `--test` mode and optionally to a file

## Requirements

- Windows
- CMake 3.22 or newer
- A local JUCE checkout
- A C++20-capable compiler
- A CMake generator such as Ninja or Visual Studio 2022

## Building

Set `JUCE_DIR` to your local JUCE checkout, then run the build script:

```bat
set JUCE_DIR=D:\path\to\JUCE
build.bat
```

`build.bat` defaults to `Ninja`. When using `Ninja` with MSVC, run it from a Developer Command Prompt or another shell where `cl.exe` is already available. To use a different generator, set `CMAKE_GENERATOR` first:

```bat
set CMAKE_GENERATOR=Visual Studio 17 2022
build.bat
```

If you run `Ninja` manually instead of using `build.bat`, do it from a Developer Command Prompt or another shell where the compiler environment is already configured.

You can also configure and build manually:

```bat
cmake -S . -B build -G Ninja -DJUCE_DIR="D:/path/to/JUCE"
cmake --build build --config Debug
```

The executable is written to:

```text
build\minihost_artefacts\Debug\minihost.exe
```

## Usage

```text
minihost.exe [--test] [--config <path\to\minihost_config.json>] [--bpm <value>] <path\to\plugin.vst3>
```

- `<path\to\plugin.vst3>`: required plugin path
- `--test`: process 10 blocks and exit without opening a GUI
- `--config <path>`: load config from a specific JSON file
- `--bpm <value>`: override BPM from config; must be greater than `0`

Examples:

```bat
minihost.exe "C:\VST3\MyPlugin.vst3"
minihost.exe --test "C:\VST3\MyPlugin.vst3"
minihost.exe --config ".\minihost_config.json" "C:\VST3\MyPlugin.vst3"
minihost.exe --bpm 100 "C:\VST3\MyPlugin.vst3"
```

## Configuration

The repository includes [`minihost_config.example.json`](./minihost_config.example.json) as a reference. The host looks for `minihost_config.json` in the current working directory by default, and that local file is ignored by Git so machine-specific paths do not get checked in.

Example:

```json
{
  "audio_1": "Samples\\input.wav",
  "audio_2": 1,
  "midi_1": "Samples\\sequence.mid",
  "midi_2": 1,
  "log_path": "logs\\minihost.log",
  "bpm": 120
}
```

- `audio_<n>` routes source slot `n` to plugin input channel `n`
- `midi_<n>` adds MIDI source slot `n`
- Audio values can be a WAV/AIFF path or an input channel ID
- MIDI values can be a `.mid` path or a MIDI input device ID
- `log_path` is optional; relative paths are resolved from the config file folder
- `bpm` is optional and defaults to `120`

Input and device IDs are one-based. `0` is also accepted as the first channel or device. Relative file paths are resolved from the config file folder.

Legacy `audio_file` and `midi_file` keys are still accepted and map to slot `1` when indexed keys are not present.

## Logging

- In `--test` mode, logs are written to `stdout` and `stderr`
- If `log_path` is set, logs are also written to that file
- In GUI mode, the host falls back to `Desktop\minihost.log` when `log_path` is not set

## Project Structure

```text
minihost/
├── CMakeLists.txt
├── build.bat
├── minihost_config.example.json
└── Source/
    ├── HostApp.cpp
    ├── HostApp.h
    └── Main.cpp
```
