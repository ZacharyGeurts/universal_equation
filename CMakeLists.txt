# AMOURANTH RTX © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
# CMake Build System (October 2025)
# Builds the AMOURANTH RTX Engine for game developers (RTX-only: Linux & Windows).
# Compiles C++17 sources with SDL3, Vulkan 1.3+, SDL_ttf, SDL3_image, SDL3_mixer, fmt, spdlog, GLM, OpenMP, FreeType, and HarfBuzz dependencies.
# Builds SPIR-V shaders for Vulkan ray tracing and rasterization.
# Copies assets (fonts, textures, objects, materials, scenes, audio, scripts) and moves SPIR-V shaders to platform-specific output directories (bin/Linux/, bin/Windows/).
# Creates source directories if they don't exist.
# Supports native Linux builds (GCC/Clang) and cross-compilation for Windows (MinGW) on Linux.
# Dependencies: SDL3, Vulkan SDK, glslc, SDL3_ttf, SDL3_image, SDL3_mixer, fmt, spdlog, GLM, OpenMP, FreeType, HarfBuzz, X11, Xext, Xrandr, Xcursor, Xi, Xss.
# Usage: mkdir build && cd build && cmake .. && cmake --build .
# For Windows cross-compilation: cmake .. -DCMAKE_TOOLCHAIN_FILE=/path/to/mingw-toolchain.cmake
# Zachary Geurts 2025

cmake_minimum_required(VERSION 3.20)
project(AMOURANTH_RTX LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Global options
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    add_compile_options(/W4 /O2)
    add_link_options(/INCREMENTAL:NO)
else()
    add_compile_options(-Wall -Wextra -O3 -g)
endif()

# Platform check (RTX-only: Linux/Windows)
if(NOT WIN32)
    if(APPLE)
        message(FATAL_ERROR "macOS not supported (RTX-only: Linux/Windows).")
    endif()
    set(IS_LINUX TRUE)
endif()
set(IS_WINDOWS WIN32)

# Check for cross-compilation (Windows target on Linux)
if(CMAKE_CROSSCOMPILING AND IS_LINUX)
    set(BUILD_WINDOWS TRUE)
else()
    set(BUILD_WINDOWS FALSE)
endif()

# Create source directories if they don't exist
set(SOURCE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/textures"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/objects"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/materials"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/scenes"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/audio"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/scripts"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/raytracing"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/rasterization"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/engine/SDL3"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/engine/Vulkan"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/modes"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/engine"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/SDL3"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Vulkan"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/modes"
)
foreach(DIR ${SOURCE_DIRS})
    if(NOT EXISTS "${DIR}")
        file(MAKE_DIRECTORY "${DIR}")
        message(STATUS "Created source directory: ${DIR}")
    endif()
endforeach()

# Find Vulkan
if(IS_LINUX AND NOT BUILD_WINDOWS)
    find_library(VULKAN_LIBRARY NAMES vulkan vulkan-1 HINTS /usr/lib/x86_64-linux-gnu)
    find_path(VULKAN_INCLUDE_DIR NAMES vulkan/vulkan.h HINTS /usr/include)
else()
    find_library(VULKAN_LIBRARY NAMES vulkan-1 HINTS /usr/x86_64-w64-mingw32/lib)
    find_path(VULKAN_INCLUDE_DIR NAMES vulkan/vulkan.h HINTS /usr/x86_64-w64-mingw32/include)
endif()
if(NOT VULKAN_LIBRARY OR NOT VULKAN_INCLUDE_DIR)
    message(FATAL_ERROR "Vulkan not found. Install libvulkan-dev for Linux or MinGW-w64 Vulkan for Windows. Ensure Vulkan SDK is installed.")
endif()
find_program(GLSLC glslc HINTS /usr/bin REQUIRED)
if(NOT GLSLC)
    message(FATAL_ERROR "glslc not found. Install Vulkan SDK (vulkan-tools).")
endif()

# Find core dependencies
find_package(PkgConfig REQUIRED)
find_package(SDL3 REQUIRED)
find_package(OpenMP REQUIRED)
find_package(Freetype REQUIRED)

# Check for fmt, spdlog, glm, and harfbuzz using pkg-config for Linux
pkg_check_modules(FMT REQUIRED fmt)
pkg_check_modules(SPDLOG REQUIRED spdlog)
pkg_check_modules(GLM REQUIRED glm)
pkg_check_modules(HarfBuzz REQUIRED harfbuzz)

# Ensure spdlog uses external fmt to avoid conflicts
set(SPDLOG_FMT_EXTERNAL ON CACHE BOOL "Use external fmt library for spdlog")

# SDL extensions and X11-related (Linux only)
if(IS_LINUX AND NOT BUILD_WINDOWS)
    pkg_check_modules(SDL3_ttf REQUIRED sdl3-ttf)
    pkg_check_modules(SDL3_image REQUIRED sdl3-image)
    pkg_check_modules(SDL3_mixer REQUIRED sdl3-mixer)
    pkg_check_modules(X11 REQUIRED x11)
    pkg_check_modules(XEXT REQUIRED xext)
    pkg_check_modules(XRANDR REQUIRED xrandr)
    pkg_check_modules(XCURSOR REQUIRED xcursor)
    pkg_check_modules(XI REQUIRED xi)
    pkg_check_modules(XSS REQUIRED xscrnsaver)
    pkg_check_modules(XCB REQUIRED xcb)
endif()

# Windows-specific dependency checks
if(BUILD_WINDOWS)
    find_library(WIN_SDL3_LIBRARY NAMES SDL3 HINTS /usr/x86_64-w64-mingw32/lib)
    find_library(WIN_FMT_LIBRARY NAMES fmt HINTS /usr/x86_64-w64-mingw32/lib)
    find_library(WIN_SPDLOG_LIBRARY NAMES spdlog HINTS /usr/x86_64-w64-mingw32/lib)
    find_library(WIN_GLM_LIBRARY NAMES glm HINTS /usr/x86_64-w64-mingw32/lib)
    find_library(WIN_FREETYPE_LIBRARY NAMES freetype HINTS /usr/x86_64-w64-mingw32/lib)
    find_library(WIN_HARFBUZZ_LIBRARY NAMES harfbuzz HINTS /usr/x86_64-w64-mingw32/lib)
    find_library(WIN_SDL3_TTF_LIBRARY NAMES SDL3_ttf HINTS /usr/x86_64-w64-mingw32/lib)
    find_library(WIN_SDL3_IMAGE_LIBRARY NAMES SDL3_image HINTS /usr/x86_64-w64-mingw32/lib)
    find_library(WIN_SDL3_MIXER_LIBRARY NAMES SDL3_mixer HINTS /usr/x86_64-w64-mingw32/lib)
    find_library(WIN_OPENMP_LIBRARY NAMES gomp HINTS /usr/x86_64-w64-mingw32/lib)

    foreach(LIB WIN_SDL3_LIBRARY WIN_FMT_LIBRARY WIN_SPDLOG_LIBRARY WIN_GLM_LIBRARY WIN_FREETYPE_LIBRARY WIN_HARFBUZZ_LIBRARY WIN_SDL3_TTF_LIBRARY WIN_SDL3_IMAGE_LIBRARY WIN_SDL3_MIXER_LIBRARY WIN_OPENMP_LIBRARY)
        if(NOT ${LIB})
            message(FATAL_ERROR "Missing Windows dependency: ${LIB}. Install MinGW-w64 versions with: sudo apt install mingw-w64-x86-64-dev libvulkan-dev libsdl3-dev libfreetype-dev libharfbuzz-dev libfmt-dev libspdlog-dev libglm-dev, or build SDL3_ttf, SDL3_image, SDL3_mixer from source for MinGW-w64.")
        else()
            message(STATUS "Found Windows dependency: ${${LIB}}")
        endif()
    endforeach()
endif()

# Directories
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(ENGINE_SRC_DIR ${SRC_DIR}/engine)
set(SDL3_SRC_DIR ${ENGINE_SRC_DIR}/SDL3)
set(VULKAN_SRC_DIR ${ENGINE_SRC_DIR}/Vulkan)
set(MODES_SRC_DIR ${SRC_DIR}/modes)
set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(ASSET_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets)
set(FONT_DIR ${ASSET_DIR}/fonts)
set(TEXTURE_DIR ${ASSET_DIR}/textures)
set(OBJECT_DIR ${ASSET_DIR}/objects)
set(MATERIAL_DIR ${ASSET_DIR}/materials)
set(SCENE_DIR ${ASSET_DIR}/scenes)
set(AUDIO_DIR ${ASSET_DIR}/audio)
set(SCRIPT_DIR ${ASSET_DIR}/scripts)

# Collect sources
file(GLOB SOURCES RECURSIVE
    "${SRC_DIR}/*.cpp"
    "${ENGINE_SRC_DIR}/*.cpp"
    "${SDL3_SRC_DIR}/*.cpp"
    "${VULKAN_SRC_DIR}/*.cpp"
    "${MODES_SRC_DIR}/*.cpp"
)

# Fallback: Explicitly include known source files
if(NOT SOURCES)
    set(SOURCES
        "${VULKAN_SRC_DIR}/Vulkan_RTX.cpp"
        "${SRC_DIR}/main.cpp"
        "${SRC_DIR}/handleinput.cpp"
        "${SRC_DIR}/universal_equation.cpp"
        "${SDL3_SRC_DIR}/SDL3_Console.cpp"
        "${SDL3_SRC_DIR}/SDL3_audio.cpp"
        "${SDL3_SRC_DIR}/SDL3_image.cpp"
        "${SRC_DIR}/SDL3_input.cpp"
        "${SDL3_SRC_DIR}/SDL3_vulkan.cpp"
        "${SDL3_SRC_DIR}/SDL3_window.cpp"
        "${VULKAN_SRC_DIR}/Vulkan_func.cpp"
        "${VULKAN_SRC_DIR}/Vulkan_func_pipe.cpp"
        "${VULKAN_SRC_DIR}/Vulkan_func_swapchain.cpp"
        "${ENGINE_SRC_DIR}/Vulkan_init.cpp"
        "${ENGINE_SRC_DIR}/SDL3_init.cpp"
        "${MODES_SRC_DIR}/mode1.cpp"
        "${MODES_SRC_DIR}/mode2.cpp"
        "${MODES_SRC_DIR}/mode3.cpp"
        "${MODES_SRC_DIR}/mode4.cpp"
        "${MODES_SRC_DIR}/mode5.cpp"
        "${MODES_SRC_DIR}/mode6.cpp"
        "${MODES_SRC_DIR}/mode7.cpp"
        "${MODES_SRC_DIR}/mode8.cpp"
        "${MODES_SRC_DIR}/mode9.cpp"
    )
endif()

# Debugging: List source directories and files
message(STATUS "Source directories: ${SRC_DIR}, ${ENGINE_SRC_DIR}, ${SDL3_SRC_DIR}, ${VULKAN_SRC_DIR}, ${MODES_SRC_DIR}")
message(STATUS "Found source files: ${SOURCES}")

# Check for key source files
foreach(SOURCE_FILE ${SOURCES})
    if(NOT EXISTS "${SOURCE_FILE}")
        message(WARNING "Source file ${SOURCE_FILE} does not exist.")
    endif()
endforeach()

# Define platform-specific output directories
set(BIN_DIR ${CMAKE_BINARY_DIR}/bin)
set(LINUX_BIN_DIR ${BIN_DIR}/Linux)
set(WINDOWS_BIN_DIR ${BIN_DIR}/Windows)
set(LINUX_ASSET_OUT_DIR ${LINUX_BIN_DIR}/assets)
set(WINDOWS_ASSET_OUT_DIR ${WINDOWS_BIN_DIR}/assets)
set(LINUX_SHADER_OUT_DIR ${LINUX_ASSET_OUT_DIR}/shaders)
set(WINDOWS_SHADER_OUT_DIR ${WINDOWS_ASSET_OUT_DIR}/shaders)
set(LINUX_FONT_OUT_DIR ${LINUX_ASSET_OUT_DIR}/fonts)
set(WINDOWS_FONT_OUT_DIR ${WINDOWS_ASSET_OUT_DIR}/fonts)
set(LINUX_TEXTURE_OUT_DIR ${LINUX_ASSET_OUT_DIR}/textures)
set(WINDOWS_TEXTURE_OUT_DIR ${WINDOWS_ASSET_OUT_DIR}/textures)
set(LINUX_OBJECT_OUT_DIR ${LINUX_ASSET_OUT_DIR}/objects)
set(WINDOWS_OBJECT_OUT_DIR ${WINDOWS_ASSET_OUT_DIR}/objects)
set(LINUX_MATERIAL_OUT_DIR ${LINUX_ASSET_OUT_DIR}/materials)
set(WINDOWS_MATERIAL_OUT_DIR ${WINDOWS_ASSET_OUT_DIR}/materials)
set(LINUX_SCENE_OUT_DIR ${LINUX_ASSET_OUT_DIR}/scenes)
set(WINDOWS_SCENE_OUT_DIR ${WINDOWS_ASSET_OUT_DIR}/scenes)
set(LINUX_AUDIO_OUT_DIR ${LINUX_ASSET_OUT_DIR}/audio)
set(WINDOWS_AUDIO_OUT_DIR ${WINDOWS_ASSET_OUT_DIR}/audio)
set(LINUX_SCRIPT_OUT_DIR ${LINUX_ASSET_OUT_DIR}/scripts)
set(WINDOWS_SCRIPT_OUT_DIR ${WINDOWS_ASSET_OUT_DIR}/scripts)

# Create Linux executable
add_executable(amouranth_engine_linux ${SOURCES})
set_target_properties(amouranth_engine_linux PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${LINUX_BIN_DIR}
    OUTPUT_NAME "Navigator"
)
target_include_directories(amouranth_engine_linux PRIVATE
    ${INCLUDE_DIR}
    ${VULKAN_INCLUDE_DIR}
    ${FREETYPE_INCLUDE_DIRS}
    ${HarfBuzz_INCLUDE_DIRS}
    ${FMT_INCLUDE_DIRS}
    ${SPDLOG_INCLUDE_DIRS}
    ${GLM_INCLUDE_DIRS}
    ${SDL3_ttf_INCLUDE_DIRS}
    ${SDL3_image_INCLUDE_DIRS}
    ${SDL3_mixer_INCLUDE_DIRS}
    ${X11_INCLUDE_DIRS}
    ${XEXT_INCLUDE_DIRS}
    ${XRANDR_INCLUDE_DIRS}
    ${XCURSOR_INCLUDE_DIRS}
    ${XI_INCLUDE_DIRS}
    ${XSS_INCLUDE_DIRS}
    ${XCB_INCLUDE_DIRS}
)
target_link_libraries(amouranth_engine_linux PRIVATE
    ${VULKAN_LIBRARY}
    SDL3::SDL3
    ${FMT_LIBRARIES}
    ${SPDLOG_LIBRARIES}
    ${GLM_LIBRARIES}
    OpenMP::OpenMP_CXX
    Freetype::Freetype
    ${HarfBuzz_LIBRARIES}
    ${X11_LIBRARIES}
    ${XEXT_LIBRARIES}
    ${XRANDR_LIBRARIES}
    ${XCURSOR_LIBRARIES}
    ${XI_LIBRARIES}
    ${XSS_LIBRARIES}
    ${XCB_LIBRARIES}
    ${SDL3_ttf_LIBRARIES}
    ${SDL3_image_LIBRARIES}
    ${SDL3_mixer_LIBRARIES}
)
target_compile_options(amouranth_engine_linux PRIVATE -fPIC)

# Create Windows executable (cross-compilation)
if(BUILD_WINDOWS)
    add_executable(amouranth_engine_windows ${SOURCES})
    set_target_properties(amouranth_engine_windows PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${WINDOWS_BIN_DIR}
        OUTPUT_NAME "Navigator"
        SUFFIX ".exe"
    )
    target_include_directories(amouranth_engine_windows PRIVATE
        ${INCLUDE_DIR}
        ${VULKAN_INCLUDE_DIR}
        /usr/x86_64-w64-mingw32/include
        /usr/x86_64-w64-mingw32/include/SDL3
        /usr/x86_64-w64-mingw32/include/freetype2
        /usr/x86_64-w64-mingw32/include/harfbuzz
    )
    target_link_libraries(amouranth_engine_windows PRIVATE
        ${VULKAN_LIBRARY}
        ${WIN_SDL3_LIBRARY}
        ${WIN_FMT_LIBRARY}
        ${WIN_SPDLOG_LIBRARY}
        ${WIN_GLM_LIBRARY}
        ${WIN_OPENMP_LIBRARY}
        ${WIN_FREETYPE_LIBRARY}
        ${WIN_HARFBUZZ_LIBRARY}
        ${WIN_SDL3_TTF_LIBRARY}
        ${WIN_SDL3_IMAGE_LIBRARY}
        ${WIN_SDL3_MIXER_LIBRARY}
    )
    target_link_options(amouranth_engine_windows PRIVATE -static-libgcc -static-libstdc++)
endif()

# Shader compilation
set(SHADER_EXTS "*.vert" "*.frag" "*.rahit" "*.rchit" "*.rmiss" "*.rgen" "*.rint" "*.rcall")
set(SHADER_OUTPUTS "")
foreach(EXT ${SHADER_EXTS})
    file(GLOB SHADERS "${SHADER_DIR}/raytracing/${EXT}" "${SHADER_DIR}/rasterization/${EXT}")
    foreach(SHADER ${SHADERS})
        get_filename_component(SHADER_NAME ${SHADER} NAME_WE)
        set(SHADER_OUTPUT "${SHADER_DIR}/${SHADER_NAME}.spv")
        
        add_custom_command(
            OUTPUT ${SHADER_OUTPUT}
            COMMAND ${CMAKE_COMMAND} -E echo "Compiling ${SHADER} to ${SHADER_OUTPUT}"
            COMMAND ${GLSLC} ${SHADER} -o ${SHADER_OUTPUT} --target-env=vulkan1.3
            DEPENDS ${SHADER}
            COMMENT "Compiling shader ${SHADER_NAME}"
            VERBATIM
        )
        list(APPEND SHADER_OUTPUTS ${SHADER_OUTPUT})
    endforeach()
endforeach()

if(SHADER_OUTPUTS)
    add_custom_target(shaders DEPENDS ${SHADER_OUTPUTS})
    add_dependencies(amouranth_engine_linux shaders)
    if(BUILD_WINDOWS)
        add_dependencies(amouranth_engine_windows shaders)
    endif()
endif()

# Create output directories
set(OUTPUT_DIRS
    "${LINUX_BIN_DIR}"
    "${LINUX_ASSET_OUT_DIR}"
    "${LINUX_SHADER_OUT_DIR}"
    "${LINUX_FONT_OUT_DIR}"
    "${LINUX_TEXTURE_OUT_DIR}"
    "${LINUX_OBJECT_OUT_DIR}"
    "${LINUX_MATERIAL_OUT_DIR}"
    "${LINUX_SCENE_OUT_DIR}"
    "${LINUX_AUDIO_OUT_DIR}"
    "${LINUX_SCRIPT_OUT_DIR}"
)
if(BUILD_WINDOWS)
    list(APPEND OUTPUT_DIRS
        "${WINDOWS_BIN_DIR}"
        "${WINDOWS_ASSET_OUT_DIR}"
        "${WINDOWS_SHADER_OUT_DIR}"
        "${WINDOWS_FONT_OUT_DIR}"
        "${WINDOWS_TEXTURE_OUT_DIR}"
        "${WINDOWS_OBJECT_OUT_DIR}"
        "${WINDOWS_MATERIAL_OUT_DIR}"
        "${WINDOWS_SCENE_OUT_DIR}"
        "${WINDOWS_AUDIO_OUT_DIR}"
        "${WINDOWS_SCRIPT_OUT_DIR}"
    )
endif()

add_custom_target(directories
    COMMAND ${CMAKE_COMMAND} -E echo "Creating output directories"
    VERBATIM
)
foreach(DIR ${OUTPUT_DIRS})
    add_custom_command(TARGET directories
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DIR}"
        COMMAND ${CMAKE_COMMAND} -E echo "Created directory: ${DIR}"
        COMMENT "Creating ${DIR}"
        VERBATIM
    )
endforeach()
add_dependencies(amouranth_engine_linux directories)
if(BUILD_WINDOWS)
    add_dependencies(amouranth_engine_windows directories)
endif()

# Post-build moves for shaders
foreach(SHADER_OUTPUT ${SHADER_OUTPUTS})
    get_filename_component(SHADER_NAME ${SHADER_OUTPUT} NAME)
    add_custom_command(TARGET amouranth_engine_linux POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Moving ${SHADER_OUTPUT} to ${LINUX_SHADER_OUT_DIR}/${SHADER_NAME}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SHADER_OUTPUT} ${LINUX_SHADER_OUT_DIR}/${SHADER_NAME}
        COMMENT "Copying shader ${SHADER_NAME} to Linux"
        DEPENDS shaders
        VERBATIM
    )
    if(BUILD_WINDOWS)
        add_custom_command(TARGET amouranth_engine_windows POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo "Moving ${SHADER_OUTPUT} to ${WINDOWS_SHADER_OUT_DIR}/${SHADER_NAME}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SHADER_OUTPUT} ${WINDOWS_SHADER_OUT_DIR}/${SHADER_NAME}
            COMMENT "Copying shader ${SHADER_NAME} to Windows"
            DEPENDS shaders
            VERBATIM
        )
    endif()
endforeach()

# Post-build copy for assets
set(ASSET_TYPES
    "FONT:*.ttf,*.otf:${FONT_DIR}:${LINUX_FONT_OUT_DIR}:${WINDOWS_FONT_OUT_DIR}"
    "TEXTURE:*.png,*.jpg,*.ktx,*.dds:${TEXTURE_DIR}:${LINUX_TEXTURE_OUT_DIR}:${WINDOWS_TEXTURE_OUT_DIR}"
    "OBJECT:*.obj,*.fbx,*.gltf:${OBJECT_DIR}:${LINUX_OBJECT_OUT_DIR}:${WINDOWS_OBJECT_OUT_DIR}"
    "MATERIAL:*.json,*.mat:${MATERIAL_DIR}:${LINUX_MATERIAL_OUT_DIR}:${WINDOWS_MATERIAL_OUT_DIR}"
    "SCENE:*.json,*.scene:${SCENE_DIR}:${LINUX_SCENE_OUT_DIR}:${WINDOWS_SCENE_OUT_DIR}"
    "AUDIO:*.wav,*.ogg,*.mp3:${AUDIO_DIR}:${LINUX_AUDIO_OUT_DIR}:${WINDOWS_AUDIO_OUT_DIR}"
    "SCRIPT:*.lua,*.py:${SCRIPT_DIR}:${LINUX_SCRIPT_OUT_DIR}:${WINDOWS_SCRIPT_OUT_DIR}"
)

foreach(ASSET_TYPE ${ASSET_TYPES})
    string(REPLACE ":" ";" ASSET_INFO ${ASSET_TYPE})
    list(GET ASSET_INFO 0 TYPE_NAME)
    list(GET ASSET_INFO 1 EXTENSIONS)
    list(GET ASSET_INFO 2 SRC_DIR)
    list(GET ASSET_INFO 3 LINUX_OUT_DIR)
    list(GET ASSET_INFO 4 WINDOWS_OUT_DIR)
    string(REPLACE "," ";" EXTENSION_LIST ${EXTENSIONS})
    
    foreach(EXT ${EXTENSION_LIST})
        file(GLOB ASSET_FILES "${SRC_DIR}/${EXT}")
        if(NOT ASSET_FILES)
            message(STATUS "No ${TYPE_NAME} files found in ${SRC_DIR} for extension ${EXT}")
        else()
            add_custom_command(TARGET amouranth_engine_linux POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ASSET_FILES} ${LINUX_OUT_DIR}/
                COMMAND ${CMAKE_COMMAND} -E echo "Copied ${TYPE_NAME} files to ${LINUX_OUT_DIR}"
                COMMENT "Copying ${TYPE_NAME} files to ${LINUX_OUT_DIR}"
                VERBATIM
            )
            if(BUILD_WINDOWS)
                add_custom_command(TARGET amouranth_engine_windows POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ASSET_FILES} ${WINDOWS_OUT_DIR}/
                    COMMAND ${CMAKE_COMMAND} -E echo "Copied ${TYPE_NAME} files to ${WINDOWS_OUT_DIR}"
                    COMMENT "Copying ${TYPE_NAME} files to ${WINDOWS_OUT_DIR}"
                    VERBATIM
                )
            endif()
        endif()
    endforeach()
endforeach()

# Custom targets
add_custom_target(build-all DEPENDS amouranth_engine_linux)
if(BUILD_WINDOWS)
    add_dependencies(build-all amouranth_engine_windows)
endif()

add_custom_target(project-clean
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${BIN_DIR}
    COMMAND ${CMAKE_COMMAND} -E echo "Cleaned build directory ${BIN_DIR}"
    COMMENT "Cleaning build artifacts"
)