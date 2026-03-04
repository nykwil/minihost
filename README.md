# minihost

A minimal VST3 plugin host built with C++ and [JUCE](https://juce.com/). It loads a VST3 plugin, feeds it audio and MIDI, and either opens the plugin's GUI or runs a quick headless test block — making it useful for both interactive use and automated CI pipelines.

## Features

- Loads and runs any VST3 plugin
- Opens the plugin's native GUI window (if the plugin has one)
- **Test mode** (`--test`): processes a fixed number of audio blocks and exits immediately — ideal for CI/automated testing
- Feeds the plugin audio from a WAV/AIFF file, or falls back to a generated 440 Hz sine wave
- Sends MIDI from a `.mid` file, or falls back to a simple repeating note pattern (middle C)
- Supports configurable BPM (`bpm` in config or `--bpm` on command line)
- Optional `minihost_config.json` for configuring audio/MIDI/BPM
- Optional `--config <path>` to load config from any location (default remains working directory)
- MIDI playback is aligned to start with the audio sample start
- Writes a log file (`minihost.log`) to the Desktop on every run

## Requirements

| Dependency | Notes |
|---|---|
| [JUCE](https://juce.com/) | Tested with JUCE 7+. The path to your local JUCE checkout must be set in `CMakeLists.txt` |
| CMake ≥ 3.22 | Build system |
| Ninja | Build tool (used by `build.bat`) |
| Visual Studio (MSVC) | `build.bat` targets Visual Studio 2022. Adjust the `vcvars64.bat` path for other versions |
| Windows | The current build scripts target Windows. The JUCE-based code itself is cross-platform |

## Building

1. **Clone** this repository.

2. **Edit `CMakeLists.txt`** and update the JUCE path to point to your local JUCE checkout:

   ```cmake
   add_subdirectory("C:/path/to/your/JUCE" juce_build)
   ```

3. **Run the build script** from the repository root:

   ```bat
   build.bat
   ```

   This configures CMake with Ninja and produces the executable at:

   ```
   build\minihost_artefacts\Debug\minihost.exe
   ```

   Alternatively, configure and build manually:

   ```bat
   cmake -B build -G Ninja
   ninja -C build
   ```

## Usage

```
minihost.exe [--test] [--config <path\to\minihost_config.json>] [--bpm <value>] <path\to\plugin.vst3>
```

| Argument | Description |
|---|---|
| `<path\to\plugin.vst3>` | Required. Absolute or relative path to the VST3 plugin file or bundle |
| `--test` | Optional. Process 10 audio blocks and exit immediately (no GUI). Useful for CI |
| `--config <path>` | Optional. Load configuration from the given JSON file path. If omitted, the host looks for `minihost_config.json` in the current working directory. |
| `--bpm <value>` | Optional. Set BPM for fallback MIDI and MIDI-file timing scale. Must be greater than 0. Overrides `bpm` from config. |

### Examples

Open a plugin's GUI and process audio in real time:

```bat
minihost.exe "C:\VST3\MyPlugin.vst3"
```

Verify a plugin loads and processes audio without opening a window:

```bat
minihost.exe --test "C:\VST3\MyPlugin.vst3"
```

Launch from another folder while explicitly pointing to a config file:

```bat
minihost.exe --config "D:\Nykwil\Audio\minihost\minihost_config.json" "C:\VST3\MyPlugin.vst3"
```

Override BPM from the command line:

```bat
minihost.exe --bpm 100 "C:\VST3\MyPlugin.vst3"
```

## Configuration

By default, place a `minihost_config.json` file in the **working directory** when launching `minihost.exe` to override the default audio, MIDI, and BPM settings.

If you launch from another folder, pass `--config <path>` to point to the config explicitly.

```json
{
    "audio_file": "C:\\audio\\my_input.wav",
    "midi_file":  "C:\\midi\\my_sequence.mid",
    "bpm": 120
}
```

| Key | Description |
|---|---|
| `audio_file` | Path to a WAV or AIFF file to use as audio input to the plugin. If omitted, a 440 Hz sine wave is generated. |
| `midi_file` | Path to a Standard MIDI File (`.mid`) to send to the plugin. If omitted, a simple repeating note pattern is generated (middle C). |
| `bpm` | Optional BPM value (> 0). Used for fallback MIDI timing and MIDI-file playback timing scale. Defaults to `120`. |

`audio_file` and `midi_file` may be absolute or relative paths. Relative paths are resolved from the config file's folder.

All keys are optional. If a file is omitted or cannot be read, the built-in fallback generator is used instead.

## Logging

On every run, `minihost.exe` creates (or overwrites) `minihost.log` on the current user's **Desktop**. The log captures initialization steps, plugin loading results, and any errors.

## Project Structure

```
minihost/
├── CMakeLists.txt       # CMake build definition
├── build.bat            # Windows convenience build script
└── Source/
    ├── Main.cpp         # JUCE application entry point, command-line parsing, PluginWindow
    ├── HostApp.h        # HostApp class declaration
    └── HostApp.cpp      # Audio device management, plugin loading, audio/MIDI routing
```
