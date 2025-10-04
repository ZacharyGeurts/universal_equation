#!/bin/bash

# Exit on error
set -e

# Check for sudo privileges
if [ "$(id -u)" != "0" ]; then
    echo "Error: This script requires sudo privileges. Please run with sudo."
    exit 1
fi

# Update package lists
echo "Updating package lists..."
apt update

# List of required dependencies
DEPENDENCIES=(
    git
    g++
    mingw-w64
    libglm-dev
    libfreetype-dev
    libasound2-dev
    libpulse-dev
    libx11-dev
    libxext-dev
    libxrandr-dev
    libxcursor-dev
    libxi-dev
    libxss-dev
    libvulkan-dev
    vulkan-tools
    libtbb-dev
    libomp-dev
    libfreetype6
    libspdlog-dev
    libfmt-dev
    libglib2.0-dev
    libpcre2-dev
    libbz2-dev
    libffi-dev
    libpng-dev
    libcairo2-dev
    libpixman-1-dev
    libharfbuzz-dev
    pkg-config
    cmake
    ninja-build
    python3
    python3-setuptools
    coreutils
    autoconf
    automake
    libtool
    libgmp-dev
    libmpfr-dev
    libmpc-dev
    libsdl3-dev
    libsdl3-ttf-dev
    libsdl3-image-dev
    libsdl3-mixer-dev
)

# Install dependencies
echo "Installing dependencies..."
apt install -y "${DEPENDENCIES[@]}"

# Verify installation of dependencies
echo "Verifying installed dependencies..."
for pkg in "${DEPENDENCIES[@]}"; do
    if dpkg -l | grep -qw "$pkg"; then
        echo "$pkg is installed."
    else
        echo "Error: $pkg failed to install."
        exit 1
    fi
done

# Verify MinGW-w64 toolchain for 64-bit
echo "Verifying MinGW-w64 toolchain..."
if command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1; then
    echo "x86_64-w64-mingw32-gcc is installed."
else
    echo "Error: MinGW-w64 64-bit toolchain (x86_64-w64-mingw32-gcc) is not installed."
    exit 1
fi
if command -v x86_64-w64-mingw32-g++ >/dev/null 2>&1; then
    echo "x86_64-w64-mingw32-g++ is installed."
else
    echo "Error: MinGW-w64 64-bit toolchain (x86_64-w64-mingw32-g++) is not installed."
    exit 1
fi
if command -v x86_64-w64-mingw32-windres >/dev/null 2>&1; then
    echo "x86_64-w64-mingw32-windres is installed."
else
    echo "Error: MinGW-w64 resource compiler (x86_64-w64-mingw32-windres) is not installed."
    exit 1
fi

# Verify Vulkan SDK (glslc)
echo "Verifying Vulkan SDK..."
if command -v glslc >/dev/null 2>&1; then
    echo "glslc is installed."
else
    echo "Error: glslc is not installed. Please install vulkan-tools."
    exit 1
fi

# Verify nproc
echo "Verifying nproc..."
if command -v nproc >/dev/null 2>&1; then
    echo "nproc is installed."
else
    echo "Error: nproc is not installed."
    exit 1
fi

# Install fmt 11.0.2 from source
echo "Installing fmt 11.0.2 from source..."
git clone https://github.com/fmtlib/fmt.git || true
cd fmt
git checkout 11.0.2 || echo "Error: Failed to checkout fmt 11.0.2 tag."
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON ..
make -j$(nproc)
sudo make install
cd ../..

# Install spdlog with external fmt
echo "Installing spdlog 1.14.1 with external fmt..."
git clone https://github.com/gabime/spdlog.git || true
cd spdlog
git checkout v1.14.1 || echo "Error: Failed to checkout spdlog v1.14.1 tag."
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DSPDLOG_FMT_EXTERNAL=ON ..
make -j$(nproc)
sudo make install
cd ../..

# Verify fmt and spdlog versions
echo "Verifying fmt and spdlog versions..."
FMT_VERSION=$(pkg-config --modversion fmt)
if [[ "$(printf '%s\n11.0.2' "$FMT_VERSION" | sort -V | head -n1)" == "11.0.2" ]]; then
    echo "fmt version $FMT_VERSION is installed."
else
    echo "Error: fmt version $FMT_VERSION is installed, but >= 11.0.2 is required."
    exit 1
fi
SPDLOG_VERSION=$(pkg-config --modversion spdlog)
if [[ "$(printf '%s\n1.14.1' "$SPDLOG_VERSION" | sort -V | head -n1)" == "1.14.1" ]]; then
    echo "spdlog version $SPDLOG_VERSION is installed."
else
    echo "Error: spdlog version $SPDLOG_VERSION is installed, but >= 1.14.1 is required."
    exit 1
fi

echo "All dependencies are installed and verified successfully!"