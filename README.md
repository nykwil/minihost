# minihost

A minimal VST3 plugin host built with C++ and [JUCE](https://juce.com/). It loads a VST3 plugin, feeds it audio and MIDI, and either opens the plugin's GUI or runs a quick headless test block — making it useful for both interactive use and automated CI pipelines.

## Features

- Loads and runs any VST3 plugin
- Opens the plugin's native GUI window (if the plugin has one)
- **Test mode** (`--test`): processes a fixed number of audio blocks and exits immediately — ideal for CI/automated testing
- Feeds the plugin audio from a WAV/AIFF file, or falls back to a generated 440 Hz sine wave
- Sends MIDI from a `.mid` file, or falls back to a simple repeating note pattern (middle C at 120 BPM)
- Optional `minihost_config.json` for configuring audio and MIDI file paths
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
minihost.exe [--test] <path\to\plugin.vst3>
```

| Argument | Description |
|---|---|
| `<path\to\plugin.vst3>` | Required. Absolute or relative path to the VST3 plugin file or bundle |
| `--test` | Optional. Process 10 audio blocks and exit immediately (no GUI). Useful for CI |

### Examples

Open a plugin's GUI and process audio in real time:

```bat
minihost.exe "C:\VST3\MyPlugin.vst3"
```

Verify a plugin loads and processes audio without opening a window:

```bat
minihost.exe --test "C:\VST3\MyPlugin.vst3"
```

## Configuration

Place a `minihost_config.json` file in the **working directory** when launching `minihost.exe` to override the default audio and MIDI sources.

```json
{
    "audio_file": "C:\\audio\\my_input.wav",
    "midi_file":  "C:\\midi\\my_sequence.mid"
}
```

| Key | Description |
|---|---|
| `audio_file` | Path to a WAV or AIFF file to use as audio input to the plugin. If omitted, a 440 Hz sine wave is generated. |
| `midi_file` | Path to a Standard MIDI File (`.mid`) to send to the plugin. If omitted, a simple repeating note pattern is generated (middle C, 120 BPM). |

Both keys are optional. If the file is omitted or cannot be read, the built-in fallback generator is used instead.

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
