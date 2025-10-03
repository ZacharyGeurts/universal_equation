#!/bin/bash
# linux.sh
# Builds AMOURANTH RTX for Linux natively
# Usage: ./linux.sh [clean]

set -e

# Define directories
BUILD_DIR="build"
BIN_DIR="bin/Linux"

# Clean build directory if 'clean' is passed
if [ "$1" == "clean" ]; then
    echo "Cleaning Linux build directory..."
    rm -rf "$BUILD_DIR" "$BIN_DIR"
fi

# Create and enter build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Run CMake and build
echo "Configuring CMake for Linux..."
cmake ..
echo "Building AMOURANTH RTX for Linux..."
cmake --build . -j$(nproc)

# Verify output
if [ -f "$BIN_DIR/Navigator" ]; then
    echo "Linux build successful! Binary located at $BIN_DIR/Navigator"
else
    echo "Linux build failed! Check errors above."
    exit 1
fi

echo "Linux build completed."