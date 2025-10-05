# Toolchain file for cross-compiling to Windows 64-bit using MinGW-w64
# Assumes MinGW-w64 is installed in /usr or /usr/local
# Target: Windows 64-bit (x86_64)
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-mingw64.cmake ..

# Set the target system
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Specify the MinGW-w64 toolchain prefix
set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)

# Set compiler paths
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
set(CMAKE_RC_COMPILER ${TOOLCHAIN_PREFIX}-windres)

# Set the root path for finding dependencies
set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX} /usr/local/${TOOLCHAIN_PREFIX})

# Adjust the default behavior of the FIND_XXX() commands
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Specify the target environment
set(CMAKE_SYSTEM_PREFIX_PATH /usr/${TOOLCHAIN_PREFIX};/usr/local/${TOOLCHAIN_PREFIX})

# Find Vulkan for MinGW-w64
set(Vulkan_INCLUDE_DIR ${CMAKE_SYSTEM_PREFIX_PATH}/include)
set(Vulkan_LIBRARY ${CMAKE_SYSTEM_PREFIX_PATH}/lib/libvulkan.dll.a)

# Find SDL3 for MinGW-w64
set(SDL3_DIR ${CMAKE_SYSTEM_PREFIX_PATH}/cmake)
find_path(SDL3_INCLUDE_DIR SDL3/SDL.h HINTS ${CMAKE_SYSTEM_PREFIX_PATH}/include)
find_library(SDL3_LIBRARY NAMES SDL3 HINTS ${CMAKE_SYSTEM_PREFIX_PATH}/lib)

# Find GLM (header-only, should be in standard include path)
set(GLM_INCLUDE_DIR ${CMAKE_SYSTEM_PREFIX_PATH}/include)

# Ensure executables have .exe extension
set(CMAKE_EXECUTABLE_SUFFIX ".exe")

# Compiler flags for MinGW-w64
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static-libgcc")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")

# Linker flags for Windows
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static")

# Define output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/Windows)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)