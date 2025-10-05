# toolchain-mingw64.cmake
# MinGW64 toolchain for cross-compiling Windows binaries on Linux

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Specify the MinGW64 toolchain
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

# Set the path to MinGW64 (adjust as needed)
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)

# Search for programs, libraries, and headers in the target system
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Adjust pkg-config for cross-compilation
set(ENV{PKG_CONFIG_PATH} "/usr/x86_64-w64-mingw32/lib/pkgconfig:/usr/x86_64-w64-mingw32/share/pkgconfig")
set(ENV{PKG_CONFIG_LIBDIR} "/usr/x86_64-w64-mingw32/lib/pkgconfig:/usr/x86_64-w64-mingw32/share/pkgconfig")
set(CMAKE_PREFIX_PATH /usr/x86_64-w64-mingw32)

# Ensure static linking for MinGW libraries
set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")