#!/usr/bin/env bash
# build.sh — Configure and build OpenYourBox.
#
# Usage:
#   ./build.sh            Build in Release mode
#   ./build.sh -d         Build in Debug mode
#   ./build.sh -a         Build then launch Ableton Live
#   ./build.sh -d -a      Debug build then launch Ableton Live

set -euo pipefail

BUILD_TYPE="Release"
LAUNCH_ABLETON=false

while getopts "da" opt; do
    case "$opt" in
        d) BUILD_TYPE="Debug" ;;
        a) LAUNCH_ABLETON=true ;;
        *) echo "Usage: $0 [-d] [-a]" >&2; exit 1 ;;
    esac
done

BUILD_DIR="OpenYourBox/Builds/${BUILD_TYPE}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
LIBTORCH_DIR="${SCRIPT_DIR}/libtorch"
TORCH_PREFIX=""

# Prefer a local LibTorch bundle only when it looks like a macOS package.
if [[ -f "${LIBTORCH_DIR}/share/cmake/Torch/TorchConfig.cmake" && -f "${LIBTORCH_DIR}/lib/libtorch.dylib" ]]; then
    TORCH_PREFIX="${LIBTORCH_DIR}"
else
    # Fall back to Python Torch's CMake package path (works with macOS wheels).
    if [[ -x "${SCRIPT_DIR}/.venv/bin/python" ]]; then
        TORCH_PREFIX="$("${SCRIPT_DIR}/.venv/bin/python" -c 'import torch; print(torch.utils.cmake_prefix_path)')"
    else
        TORCH_PREFIX="$(python3 -c 'import torch; print(torch.utils.cmake_prefix_path)')"
    fi
fi

if [[ -z "${TORCH_PREFIX}" ]]; then
    echo "Failed to locate a valid Torch CMake prefix path." >&2
    exit 1
fi

cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DOPENYOURBOX_JUCE_PATH="/Users/hugo/JUCE" \
    -DCMAKE_PREFIX_PATH="${TORCH_PREFIX}"
cmake --build "$BUILD_DIR" --parallel "$(sysctl -n hw.logicalcpu 2>/dev/null || nproc)"

if [[ "${LAUNCH_ABLETON}" == true ]]; then
    open "/Applications/Ableton Live 12 Suite.app"
fi
