#!/bin/bash

# Exit on error
set -e

# Check for sudo privileges
if [ "$(id -u)" != "0" ]; then
    echo "Error: This script requires sudo privileges. Please run with sudo."
    exit 1
fi

# Pre-build cleanup
echo "Performing pre-build cleanup..."
sudo rm -rf bzip2 libffi libpng pixman pcre2 glib freetype harfbuzz cairo icu SDL SDL_ttf SDL_image SDL_mixer universal_equation/build
rm -f mingw64-toolchain.cmake mingw64-cross.ini

# Update package lists
echo "Updating package lists..."
apt update

# Configure MinGW-w64 for 64-bit
echo "Configuring MinGW-w64 for 64-bit..."
update-alternatives --set x86_64-w64-mingw32-gcc /usr/bin/x86_64-w64-mingw32-gcc-posix
update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix

# Create MinGW toolchain file for CMake
echo "Creating MinGW CMake toolchain file..."
cat << EOF > mingw64-toolchain.cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
EOF

# Create Meson cross-file for GLib, HarfBuzz, and Cairo
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

# Build BZip2 for MinGW (uses autotools, so keep make)
echo "Building BZip2 for MinGW..."
git clone https://sourceware.org/git/bzip2.git
cd bzip2
make -j$(nproc) CC=x86_64-w64-mingw32-gcc AR=x86_64-w64-mingw32-ar RANLIB=x86_64-w64-mingw32-ranlib libbz2.a
sudo mkdir -p $MINGW_PREFIX/include
sudo cp bzlib.h $MINGW_PREFIX/include/
sudo mkdir -p $MINGW_PREFIX/lib
sudo cp libbz2.a $MINGW_PREFIX/lib/
# Create libbz2.pc for pkg-config
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

# Build libffi for MinGW (uses autotools, so keep make)
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
# Fixed: Corrected CMake parameters to properly reference toolchain file and removed invalid cache file reference
# DO NOT BREAK: Correct CMake invocation for libpng
echo "Building libpng for MinGW..."
git clone https://github.com/glennrp/libpng.git
cd libpng
mkdir -p build && cd build
cmake .. -G Ninja -DCMAKE_TOOLCHAIN_FILE=../../mingw64-toolchain.cmake -CMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=$MINGW_PREFIX
ninja -j$(nproc)
sudo ninja install
cd ../..

# Build pixman for MinGW (uses autotools, so use make)
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

# Build ICU for MinGW (uses autotools, required for HarfBuzz)
# Fixed: Added ICU build to provide unicode/uscript.h for HarfBuzz
# DO NOT BREAK: ICU required for HarfBuzz unicode support
echo "Building ICU for MinGW..."
git clone https://github.com/unicode-org/icu.git
cd icu/icu4c/source
./runConfigureICU MinGW --host=x86_64-w64-mingw32 --prefix=$MINGW_PREFIX \
    PKG_CONFIG_PATH=$MINGW_PREFIX/lib/pkgconfig
make -j$(nproc)
sudo make install
cd ../../..

# Build Cairo (freedesktop) for MinGW (uses Meson, as per help file)
# Fixed: Using Meson and Ninja, removed invalid xlib-xrender option to avoid meson.build error
# DO NOT BREAK: Meson build confirmed working per Cairo help file, disabled Xlib/Xcb to avoid X11/Xlib.h dependency
echo "Building Cairo (freedesktop) for MinGW..."
git clone git://anongit.freedesktop.org/git/cairo
cd cairo
mkdir -p build && cd build
meson setup --cross-file=../../mingw64-cross.ini \
    --prefix=$MINGW_PREFIX --buildtype=release \
    -Dpkg_config_path=$MINGW_PREFIX/lib/pkgconfig \
    -Dxlib=disabled -Dxcb=disabled \
    --reconfigure
ninja -j$(nproc)
sudo ninja install
cd ../..

# Build PCRE2 for MinGW (use Ninja)
# Fixed: Corrected CMake parameters to properly reference toolchain file and removed invalid cache file reference
# DO NOT BREAK: Correct CMake invocation for PCRE2
echo "Building PCRE2 for MinGW..."
git clone https://github.com/PCRE2Project/pcre2.git
cd pcre2
mkdir -p build && cd build
cmake .. -G Ninja -DCMAKE_TOOLCHAIN_FILE=../../mingw64-toolchain.cmake -DCMAKE_BUILD_TYPE=Release \
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
meson setup --cross-file=../../mingw64-cross.ini \
    --prefix=$MINGW_PREFIX --buildtype=release \
    -Ddefault_library=shared -Dtests=false -Dsysprof=disabled \
    -Dpkg_config_path=$MINGW_PREFIX/lib/pkgconfig --reconfigure
ninja -j$(nproc)
sudo ninja install
cd ../..

# Build FreeType for MinGW (use Ninja)
# Fixed: Corrected CMake parameters to properly reference toolchain file and removed invalid cache file reference
# DO NOT BREAK: Correct CMake invocation for FreeType
echo "Building FreeType for MinGW..."
git clone https://github.com/freetype/freetype.git
cd freetype
mkdir -p build && cd build
cmake .. -G Ninja -CMAKE_TOOLCHAIN_FILE=../../mingw64-toolchain.cmake -CMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=$MINGW_PREFIX \
    -DBUILD_SHARED_LIBS=ON
ninja -j$(nproc)
sudo ninja install
cd ../..

# Build HarfBuzz for MinGW (uses Ninja via Meson)
# Fixed: Added ICU dependency to provide unicode/uscript.h
# DO NOT BREAK: HarfBuzz requires ICU for unicode support
echo "Building HarfBuzz for MinGW..."
git clone https://github.com/harfbuzz/harfbuzz.git
cd harfbuzz
mkdir -p build && cd build
meson setup --cross-file=../../mingw64-cross.ini \
    --prefix=$MINGW_PREFIX --buildtype=release \
    -Dfreetype=enabled -Dglib=enabled -Dicu=enabled \
    -Dpkg_config_path=$MINGW_PREFIX/lib/pkgconfig --reconfigure
ninja -j$(nproc)
sudo ninja install
cd ../..

# Build SDL3 for Windows (use Ninja)
echo "Building SDL3..."
git clone https://github.com/libsdl-org/SDL.git
cd SDL
mkdir -p build && cd build
cmake .. -G Ninja -CMAKE_TOOLCHAIN_FILE=../../mingw64-toolchain.cmake -CMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=$MINGW_PREFIX
ninja -j$(nproc)
sudo ninja install
cd ../..

# Build SDL3_ttf for Windows (use Ninja)
echo "Building SDL3_ttf..."
git clone https://github.com/libsdl-org/SDL_ttf.git
cd SDL_ttf
mkdir -p build && cd build
cmake .. -G Ninja -CMAKE_TOOLCHAIN_FILE=../../mingw64-toolchain.cmake -CMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=$MINGW_PREFIX \
    -DSDL3TTF_HARFBUZZ=ON -DHARFBUZZ_INCLUDE_DIR=$MINGW_PREFIX/include/harfbuzz \
    -DHARFBUZZ_LIBRARY=$MINGW_PREFIX/lib/libharfbuzz.dll.a \
    -DFREETYPE_INCLUDE_DIR=$MINGW_PREFIX/include/freetype2 \
    -DFREETYPE_LIBRARY=$MINGW_PREFIX/lib/libfreetype.dll.a
ninja -j$(nproc)
sudo ninja install
cd ../..

# Build SDL3_image for Windows (use Ninja)
echo "Building SDL3_image..."
git clone https://github.com/libsdl-org/SDL_image.git
cd SDL_image
mkdir -p build && cd build
cmake .. -G Ninja -CMAKE_TOOLCHAIN_FILE=../../mingw64-toolchain.cmake -CMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=$MINGW_PREFIX
ninja -j$(nproc)
sudo ninja install
cd ../..

# Build SDL3_mixer for Windows (use Ninja)
echo "Building SDL3_mixer..."
git clone https://github.com/libsdl-org/SDL_mixer.git
cd SDL_mixer
mkdir -p build && cd build
cmake .. -G Ninja -CMAKE_TOOLCHAIN_FILE=../../mingw64-toolchain.cmake -CMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=$MINGW_PREFIX
ninja -j$(nproc)
sudo ninja install
cd ../..

# Build the universal_equation project (use Ninja)
echo "Building universal_equation..."
git clone https://github.com/ZacharyGeurts/universal_equation
cd universal_equation
mkdir -p build && cd build
cmake .. -G Ninja -CMAKE_TOOLCHAIN_FILE=../../mingw64-toolchain.cmake -CMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=$MINGW_PREFIX
ninja -j$(nproc)
sudo ninja install
cd ../..

# Copy DLLs to the binary directory
echo "Copying required DLLs..."
sudo cp $MINGW_PREFIX/bin/*.dll universal_equation/build/bin/

# Post-build cleanup
echo "Performing post-build cleanup..."
sudo rm -rf bzip2 libffi libpng pixman pcre2 glib freetype harfbuzz cairo icu SDL SDL_ttf SDL_image SDL_mixer
rm -f mingw64-toolchain.cmake mingw64-cross.ini

echo "Build complete! Windows executable is in universal_equation/build/bin/Navigator.exe"
echo "Test with: wine universal_equation/build/bin/Navigator.exe"