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

- CMake 3.22 or newer
- A local [JUCE](https://github.com/juce-framework/JUCE) checkout
- A C++20-capable compiler
- Ninja (recommended) or another CMake generator

**Windows:** Visual Studio 2022 or newer (MSVC). Run from a Developer Command Prompt when using Ninja.

**macOS:** Xcode command line tools (`xcode-select --install`).

**Linux:** GCC or Clang, plus the following development packages:
```sh
# Debian / Ubuntu
sudo apt install libasound2-dev libx11-dev libxrandr-dev libxinerama-dev \
     libxcursor-dev libfreetype6-dev libfontconfig1-dev libgl1-mesa-dev
```

## Building

Set `JUCE_DIR` to your local JUCE checkout, then run the build script.

**Windows:**
```bat
set JUCE_DIR=C:\path\to\JUCE
build.bat
```

**macOS / Linux:**
```sh
export JUCE_DIR=/path/to/JUCE
chmod +x build.sh
./build.sh
```

`build.bat` / `build.sh` default to the `Ninja` generator. To use a different generator, set `CMAKE_GENERATOR` first:

```bat
set CMAKE_GENERATOR=Visual Studio 17 2022
build.bat
```

You can also configure and build manually:

```sh
cmake -S . -B build -G Ninja -DJUCE_DIR="/path/to/JUCE"
cmake --build build --config Debug
```

### Build output

| Platform | Executable path |
|----------|----------------|
| Windows  | `build\minihost_artefacts\Debug\minihost.exe` |
| macOS    | `build/minihost_artefacts/Debug/minihost.app/Contents/MacOS/minihost` |
| Linux    | `build/minihost_artefacts/Debug/minihost` |

## Usage

```text
minihost [--test] [--config <path/to/minihost_config.json>] [--bpm <value>] <path/to/plugin.vst3>
```

- `<path/to/plugin.vst3>`: required plugin path
- `--test`: process 10 blocks and exit without opening a GUI; exits 0 on success, 1 on failure
- `--config <path>`: load config from a specific JSON file
- `--bpm <value>`: override BPM from config; must be greater than `0`

**Windows:**
```bat
minihost.exe "C:\VST3\MyPlugin.vst3"
minihost.exe --test "C:\VST3\MyPlugin.vst3"
minihost.exe --config ".\minihost_config.json" "C:\VST3\MyPlugin.vst3"
minihost.exe --bpm 100 "C:\VST3\MyPlugin.vst3"
```

**macOS / Linux:**
```sh
./minihost /path/to/MyPlugin.vst3
./minihost --test /path/to/MyPlugin.vst3
./minihost --config ./minihost_config.json /path/to/MyPlugin.vst3
./minihost --bpm 100 /path/to/MyPlugin.vst3
```

On macOS, VST3 plugins are typically found in `~/Library/Audio/Plug-Ins/VST3/` or `/Library/Audio/Plug-Ins/VST3/`.

## Configuration

The repository includes [`minihost_config.example.json`](./minihost_config.example.json) as a reference. The host looks for `minihost_config.json` in the current working directory by default, and that local file is ignored by Git so machine-specific paths do not get checked in.

Example:

```json
{
  "audio_1": "Samples/input.wav",
  "audio_2": 1,
  "midi_1": "Samples/sequence.mid",
  "midi_2": 1,
  "log_path": "logs/minihost.log",
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
- In GUI mode, the host falls back to `Desktop/minihost.log` when `log_path` is not set

## Project Structure

```text
minihost/
├── CMakeLists.txt
├── build.bat
├── build.sh
├── minihost_config.example.json
└── Source/
    ├── HostApp.cpp
    ├── HostApp.h
    └── Main.cpp
```
