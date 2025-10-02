set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)

# Allow finding Vulkan in the host system for glslc and headers
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Add Vulkan paths for MinGW-w64
list(APPEND CMAKE_PREFIX_PATH /usr/x86_64-w64-mingw32)
list(APPEND CMAKE_LIBRARY_PATH /usr/x86_64-w64-mingw32/lib)
list(APPEND CMAKE_INCLUDE_PATH /usr/x86_64-w64-mingw32/include)