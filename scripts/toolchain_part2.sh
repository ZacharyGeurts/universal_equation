#!/bin/bash

# Exit on error
set -e

# Check for sudo privileges
if [ "$(id -u)" != "0" ]; then
    echo "Error: This script requires sudo privileges. Please run with sudo."
    exit 1
fi

# Pre-build cleanup
echo "Performing pre-build cleanup for part 2..."
sudo rm -rf SDL SDL_ttf SDL_image SDL_mixer universal_equation/build
# Preserve toolchain and cross files during debugging
# rm -f mingw64-toolchain.cmake mingw64-cross.ini

# Ensure required tools
sudo apt install -y ninja-build meson pkg-config mingw-w64

# Set MinGW prefix
MINGW_PREFIX=/usr/x86_64-w64-mingw32
BASE_DIR=$(pwd)

# Verify pkg-config dependencies
echo "Verifying pkg-config dependencies..."
PKG_CONFIG_PATH=$MINGW_PREFIX/lib/pkgconfig pkg-config --modversion freetype2 harfbuzz glib-2.0 icu-uc libpng || echo "Warning: Some dependencies not found"

# Build SDL3 for Windows (use Ninja)
echo "Building SDL3..."
git clone https://github.com/libsdl-org/SDL.git
cd SDL
mkdir -p build && cd build
rm -rf CMakeCache.txt CMakeFiles
cmake .. -G Ninja -DCMAKE_TOOLCHAIN_FILE=$BASE_DIR/mingw64-toolchain.cmake -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=$MINGW_PREFIX
ninja -j$(nproc)
sudo ninja install
cd ../..

# Build SDL3_ttf for Windows (use Ninja)
echo "Building SDL3_ttf..."
git clone https://github.com/libsdl-org/SDL_ttf.git
cd SDL_ttf
mkdir -p build && cd build
rm -rf CMakeCache.txt CMakeFiles
cmake .. -G Ninja -DCMAKE_TOOLCHAIN_FILE=$BASE_DIR/mingw64-toolchain.cmake -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=$MINGW_PREFIX \
    -DSDL3TTF_HARFBUZZ=ON \
    -DFREETYPE_INCLUDE_DIR=$MINGW_PREFIX/include/freetype2 \
    -DFREETYPE_LIBRARY=$MINGW_PREFIX/lib/libfreetype.dll.a \
    -DHARFBUZZ_INCLUDE_DIR=$MINGW_PREFIX/include/harfbuzz \
    -DHARFBUZZ_LIBRARY=$MINGW_PREFIX/lib/libharfbuzz.dll.a
ninja -j$(nproc)
sudo ninja install
cd ../..

# Build SDL3_image for Windows (use Ninja)
echo "Building SDL3_image..."
git clone https://github.com/libsdl-org/SDL_image.git
cd SDL_image
mkdir -p build && cd build
rm -rf CMakeCache.txt CMakeFiles
cmake .. -G Ninja -DCMAKE_TOOLCHAIN_FILE=$BASE_DIR/mingw64-toolchain.cmake -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=$MINGW_PREFIX \
    -DSDL3IMAGE_PNG=ON -DSDL3IMAGE_JPG=ON
ninja -j$(nproc)
sudo ninja install
cd ../..

# Build SDL3_mixer for Windows (use Ninja)
echo "Building SDL3_mixer..."
git clone https://github.com/libsdl-org/SDL_mixer.git
cd SDL_mixer
mkdir -p build && cd build
rm -rf CMakeCache.txt CMakeFiles
cmake .. -G Ninja -DCMAKE_TOOLCHAIN_FILE=$BASE_DIR/mingw64-toolchain.cmake -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=$MINGW_PREFIX \
    -DSDL3MIXER_MP3=ON
ninja -j$(nproc)
sudo ninja install
cd ../..

# Build the universal_equation project (use Ninja)
echo "Building universal_equation..."
git clone https://github.com/ZacharyGeurts/universal_equation
cd universal_equation
mkdir -p build && cd build
rm -rf CMakeCache.txt CMakeFiles
cmake .. -G Ninja -DCMAKE_TOOLCHAIN_FILE=$BASE_DIR/mingw64-toolchain.cmake -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=$MINGW_PREFIX
ninja -j$(nproc)
sudo ninja install
cd ../..

# Copy DLLs to the binary directory
echo "Copying required DLLs..."
sudo mkdir -p universal_equation/build/bin
sudo cp $MINGW_PREFIX/bin/*.dll universal_equation/build/bin/

# Verify installations
echo "Verifying installations..."
ls $MINGW_PREFIX/lib/libSDL3*.dll.a $MINGW_PREFIX/lib/libSDL3_ttf*.dll.a \
   $MINGW_PREFIX/lib/libSDL3_image*.dll.a $MINGW_PREFIX/lib/libSDL3_mixer*.dll.a
ls universal_equation/build/bin/Navigator.exe

# Post-build cleanup (optional during debugging)
# echo "Performing post-build cleanup for part 2..."
# sudo rm -rf SDL SDL_ttf SDL_image SDL_mixer
# rm -f mingw64-toolchain.cmake mingw64-cross.ini

echo "Part 2 build complete! Windows executable is in universal_equation/build/bin/Navigator.exe"
echo "Test with: wine universal_equation/build/bin/Navigator.exe"