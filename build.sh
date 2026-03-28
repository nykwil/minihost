#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"

if [ -z "${JUCE_DIR:-}" ]; then
    echo "ERROR: JUCE_DIR is not set."
    echo "Set JUCE_DIR to your local JUCE checkout or pass -DJUCE_DIR when running CMake manually."
    exit 1
fi

CMAKE_GENERATOR="${CMAKE_GENERATOR:-Ninja}"

echo ""
echo "=== Configuring with CMake ==="
cmake -S "$ROOT" -B "$ROOT/build" -G "$CMAKE_GENERATOR" -DJUCE_DIR="$JUCE_DIR"

echo ""
echo "=== Building ==="
cmake --build "$ROOT/build" --config Debug

echo ""
echo "=== Build succeeded! ==="
if [ "$(uname)" = "Darwin" ]; then
    echo "Executable: build/minihost_artefacts/Debug/minihost.app/Contents/MacOS/minihost"
else
    echo "Executable: build/minihost_artefacts/Debug/minihost"
fi
