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
    texinfo
    wine
    pkg-config
    ninja-build
    python3
    python3-setuptools
    coreutils
    autoconf
    automake
    libtool
)

# Install dependencies
echo "Installing dependencies..."
apt install -y "${DEPENDENCIES[@]}"

# Remove any existing Meson installation to avoid conflicts
echo "Removing any existing Meson installation..."
apt remove -y meson || true
rm -f /usr/local/bin/meson || true

# Clean up existing meson directory
echo "Cleaning up existing meson directory..."
sudo rm -rf meson

# Build Meson from source
echo "Building Meson 1.4.0 from source..."
git clone https://github.com/mesonbuild/meson.git
cd meson
git checkout 1.4.0 || echo "Error: Failed to checkout Meson 1.4.0 tag."
python3 setup.py build
sudo python3 setup.py install
cd ..

# Verify Meson version
echo "Verifying Meson version..."
if ! command -v meson >/dev/null 2>&1; then
    echo "Error: Meson is not installed or not in PATH."
    exit 1
fi
MESON_VERSION=$(meson --version)
if [[ "$(printf '%s\n1.4.0' "$MESON_VERSION" | sort -V | head -n1)" == "1.4.0" ]]; then
    echo "Meson version $MESON_VERSION is installed."
else
    echo "Error: Meson version $MESON_VERSION is installed, but >= 1.4.0 is required."
    exit 1
fi

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

# Verify nproc
echo "Verifying nproc..."
if command -v nproc >/dev/null 2>&1; then
    echo "nproc is installed."
else
    echo "Error: nproc is not installed."
    exit 1
fi

echo "All dependencies are installed and verified successfully!"