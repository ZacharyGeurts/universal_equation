#!/bin/bash
# windows.sh
# Cross-compiles AMOURANTH RTX for Windows using MinGW64
# Usage: ./windows.sh [clean]

set -e

# Define directories
BUILD_DIR="build-win"
BIN_DIR="bin/Windows"

# Clean build directory if 'clean' is passed
if [ "$1" == "clean" ]; then
    echo "Cleaning Windows build directory..."
    rm -rf "$BUILD_DIR" "$BIN_DIR"
fi

# Create and enter build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Run CMake with MinGW64 toolchain and build
echo "Configuring CMake for Windows (MinGW64 cross-compilation)..."
cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain-mingw64.cmake
echo "Building AMOURANTH RTX for Windows..."
cmake --build . -j$(nproc)

# Verify output
if [ -f "$BIN_DIR/Navigator.exe" ]; then
    echo "Windows build successful! Binary located at $BIN_DIR/Navigator.exe"
else
    echo "Windows build failed! Check errors above."
    exit 1
fi

echo "Windows build completed."