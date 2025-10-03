#!/bin/bash

# Exit on error
set -e

# Check for sudo privileges
if [ "$(id -u)" != "0" ]; then
    echo "Error: This script requires sudo privileges. Please run with sudo."
    exit 1
fi

# Pre-build cleanup
echo "Performing pre-build cleanup for part 1..."
sudo rm -rf bzip2 libffi libpng pixman icu icu-host pcre2 glib freetype harfbuzz cairo icu/cairo icu/*/cairo
# Preserve toolchain and cross files during debugging
# rm -f mingw64-toolchain.cmake mingw64-cross.ini

# Update package lists (non-fatal)
echo "Updating package lists..."
apt update || echo "Warning: apt update failed, continuing with build..."

# Install required tools
echo "Installing required tools..."
sudo apt install -y ninja-build meson pkg-config mingw-w64 autoconf automake libtool python3

# Configure MinGW-w64 for 64-bit
echo "Configuring MinGW-w64 for 64-bit..."
sudo update-alternatives --set x86_64-w64-mingw32-gcc /usr/bin/x86_64-w64-mingw32-gcc-posix
sudo update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix

# Create MinGW toolchain file for CMake
echo "Creating MinGW CMake toolchain file..."
cat << EOF > mingw64-toolchain.cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(ENV{PKG_CONFIG_PATH} "/usr/x86_64-w64-mingw32/lib/pkgconfig")
EOF

# Create Meson cross-file
echo "Creating Meson cross-file..."
cat << EOF > mingw64-cross.ini
[binaries]
c = 'x86_64-w64-mingw32-gcc'
cpp = 'x86_64-w64-mingw32-g++'
ar = 'x86_64-w64-mingw32-ar'
strip = 'x86_64-w64-mingw32-strip'
windres = 'x86_64-w64-mingw32-windres'
pkg-config = 'pkg-config'
exe_wrapper = 'wine'

[host_machine]
system = 'windows'
cpu_family = 'x86_64'
cpu = 'x86_64'
endian = 'little'

[properties]
pkg_config_path = '/usr/x86_64-w64-mingw32/lib/pkgconfig'
needs_exe_wrapper = true
EOF

# Create a prefix directory for MinGW libraries
MINGW_PREFIX=/usr/x86_64-w64-mingw32
sudo mkdir -p $MINGW_PREFIX/lib $MINGW_PREFIX/include $MINGW_PREFIX/lib/pkgconfig

# Capture base dir (current working dir) as absolute
BASE_DIR=$(pwd)

# Verify pkg-config dependencies
echo "Verifying pkg-config dependencies..."
PKG_CONFIG_PATH=$MINGW_PREFIX/lib/pkgconfig pkg-config --modversion zlib || echo "Warning: zlib not found for MinGW"
PKG_CONFIG_PATH=$MINGW_PREFIX/lib/pkgconfig pkg-config --modversion libpng || echo "Warning: libpng not found for MinGW"

# Build BZip2 for MinGW (uses autotools)
echo "Building BZip2 for MinGW..."
git clone https://sourceware.org/git/bzip2.git
cd bzip2
make -j$(nproc) CC=x86_64-w64-mingw32-gcc AR=x86_64-w64-mingw32-ar RANLIB=x86_64-w64-mingw32-ranlib libbz2.a
sudo mkdir -p $MINGW_PREFIX/include
sudo cp bzlib.h $MINGW_PREFIX/include/
sudo mkdir -p $MINGW_PREFIX/lib
sudo cp libbz2.a $MINGW_PREFIX/lib/
cat << EOF | sudo tee $MINGW_PREFIX/lib/pkgconfig/libbz2.pc
prefix=$MINGW_PREFIX
exec_prefix=\${prefix}
libdir=\${prefix}/lib
includedir=\${prefix}/include

Name: bzip2
Description: bzip2 compression library
Version: 1.0.8
Libs: -L\${libdir} -lbz2
Cflags: -I\${includedir}
EOF
cd ..

# Build libffi for MinGW (uses autotools)
echo "Building libffi for MinGW..."
git clone https://github.com/libffi/libffi.git
cd libffi
./autogen.sh
./configure --host=x86_64-w64-mingw32 --prefix=$MINGW_PREFIX
cd x86_64-w64-mingw32
make -j$(nproc)
sudo mkdir -p $MINGW_PREFIX/include
sudo cp include/ffi.h include/ffitarget.h $MINGW_PREFIX/include/
sudo mkdir -p $MINGW_PREFIX/lib
sudo cp .libs/libffi.a $MINGW_PREFIX/lib/
sudo cp libffi.pc $MINGW_PREFIX/lib/pkgconfig/
cd ../..

# Build libpng for MinGW (use Ninja)
echo "Building libpng for MinGW..."
git clone https://github.com/glennrp/libpng.git
cd libpng
mkdir -p build && cd build
rm -rf CMakeCache.txt CMakeFiles
cmake .. -G Ninja -DCMAKE_TOOLCHAIN_FILE=$BASE_DIR/mingw64-toolchain.cmake -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=$MINGW_PREFIX \
    -DZLIB_LIBRARY=$MINGW_PREFIX/lib/libz.a \
    -DZLIB_INCLUDE_DIR=$MINGW_PREFIX/include
ninja -j$(nproc)
sudo ninja install
cd ../..

# Build pixman for MinGW (uses autotools)
echo "Building pixman for MinGW..."
git clone git://anongit.freedesktop.org/git/pixman.git
cd pixman
git checkout pixman-0.42.2
autoreconf -fiv
./configure --host=x86_64-w64-mingw32 --prefix=$MINGW_PREFIX \
    PKG_CONFIG_PATH=$MINGW_PREFIX/lib/pkgconfig
make -j$(nproc)
sudo make install
cd ..

# Build ICU for MinGW (uses autotools)
echo "Building ICU for host (Linux)..."
git clone https://github.com/unicode-org/icu
cd icu/icu4c/source
HOST_BUILD_DIR="$BASE_DIR/icu/icu4c/source/build-host"
mkdir -p build-host && cd build-host
../configure
make -j$(nproc)
cd ..
echo "Building ICU for MinGW..."
mkdir -p build-mingw && cd build-mingw
../configure --host=x86_64-w64-mingw32 --prefix=$MINGW_PREFIX \
    --with-cross-build="$HOST_BUILD_DIR" \
    PKG_CONFIG_PATH=$MINGW_PREFIX/lib/pkgconfig
make -j$(nproc)
sudo make install
cd ../../..

# Build Cairo for MinGW (use Ninja via Meson)
echo "Building Cairo for MinGW..."
git clone https://gitlab.freedesktop.org/cairo/cairo.git
cd cairo
mkdir -p build && cd build
rm -rf _build
meson setup --cross-file=$BASE_DIR/mingw64-cross.ini \
    --prefix=$MINGW_PREFIX --buildtype=release \
    -Dfreetype=enabled -Dpng=enabled -Dzlib=enabled \
    -Dxlib=disabled -Dxcb=disabled -Dxlib-xcb=disabled -Dwin32=enabled -Dcairo-win32=enabled \
    -Dpkg_config_path=$MINGW_PREFIX/lib/pkgconfig --reconfigure
ninja -j$(nproc)
sudo ninja install
cd ../..
# Verify Cairo installation
echo "Verifying Cairo installation..."
ls $MINGW_PREFIX/lib/libcairo.dll.a
ls $MINGW_PREFIX/include/cairo/cairo-ft.h
PKG_CONFIG_PATH=$MINGW_PREFIX/lib/pkgconfig pkg-config --modversion cairo || echo "Warning: Cairo not found"

# Build PCRE2 for MinGW (use Ninja)
echo "Building PCRE2 for MinGW..."
git clone https://github.com/PCRE2Project/pcre2.git
cd pcre2
mkdir -p build && cd build
rm -rf CMakeCache.txt CMakeFiles
cmake .. -G Ninja -DCMAKE_TOOLCHAIN_FILE=$BASE_DIR/mingw64-toolchain.cmake -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=$MINGW_PREFIX \
    -DPCRE2_BUILD_PCRE2_8=ON -DPCRE2_BUILD_PCRE2_16=OFF -DPCRE2_BUILD_PCRE2_32=OFF \
    -DPCRE2_BUILD_TESTS=OFF
ninja -j$(nproc)
sudo ninja install
cd ../..

# Build GLib for MinGW (uses Ninja via Meson)
echo "Building GLib for MinGW..."
git clone https://gitlab.gnome.org/GNOME/glib.git
cd glib
mkdir -p build && cd build
rm -rf _build
meson setup --cross-file=$BASE_DIR/mingw64-cross.ini \
    --prefix=$MINGW_PREFIX --buildtype=release \
    -Ddefault_library=shared -Dtests=false -Dsysprof=disabled \
    -Dpkg_config_path=$MINGW_PREFIX/lib/pkgconfig --reconfigure
ninja -j$(nproc)
sudo ninja install
cd ../..

# Build FreeType for MinGW (use Ninja)
echo "Building FreeType for MinGW..."
git clone https://github.com/freetype/freetype.git
cd freetype
mkdir -p build && cd build
rm -rf CMakeCache.txt CMakeFiles
cmake .. -G Ninja -DCMAKE_TOOLCHAIN_FILE=$BASE_DIR/mingw64-toolchain.cmake -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=$MINGW_PREFIX \
    -DBUILD_SHARED_LIBS=ON \
    -DWITH_BZip2=OFF
ninja -j$(nproc)
sudo ninja install
cd ../..
# Verify FreeType installation
echo "Verifying FreeType installation..."
ls $MINGW_PREFIX/lib/libfreetype.dll.a
PKG_CONFIG_PATH=$MINGW_PREFIX/lib/pkgconfig pkg-config --modversion freetype2 || echo "Warning: FreeType not found"

# Build HarfBuzz for MinGW (uses Ninja via Meson)
echo "Building HarfBuzz for MinGW..."
git clone https://github.com/harfbuzz/harfbuzz.git
cd harfbuzz
mkdir -p build && cd build
rm -rf _build
meson setup --cross-file=$BASE_DIR/mingw64-cross.ini \
    --prefix=$MINGW_PREFIX --buildtype=release \
    -Dfreetype=enabled -Dglib=enabled -Dicu=enabled -Dcairo=enabled \
    -Dpkg_config_path=$MINGW_PREFIX/lib/pkgconfig --reconfigure
ninja -j$(nproc)
sudo ninja install
cd ../..

# Verify installations
echo "Verifying installations..."
ls $MINGW_PREFIX/lib/libbz2.a $MINGW_PREFIX/lib/libffi.a $MINGW_PREFIX/lib/libpng*.dll.a \
   $MINGW_PREFIX/lib/libpixman-1*.dll.a $MINGW_PREFIX/lib/libicu*.dll.a \
   $MINGW_PREFIX/lib/libpcre2-8.dll.a $MINGW_PREFIX/lib/libglib-2.0.dll.a \
   $MINGW_PREFIX/lib/libfreetype.dll.a $MINGW_PREFIX/lib/libharfbuzz.dll.a \
   $MINGW_PREFIX/lib/libcairo.dll.a
ls $MINGW_PREFIX/include/cairo/cairo-ft.h

# Post-build cleanup (optional during debugging)
# echo "Performing post-build cleanup for part 1..."
# sudo rm -rf bzip2 libffi libpng pixman icu icu-host pcre2 glib freetype harfbuzz cairo
# rm -f mingw64-toolchain.cmake mingw64-cross.ini

echo "Part 1 build complete! Proceed to toolchain_part2.sh"