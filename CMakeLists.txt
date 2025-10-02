# AMOURANTH RTX © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
# CMake Build System (October 2025)
# Builds the AMOURANTH RTX Engine for game developers (RTX-only: Linux & Windows).
# Compiles C++17 sources with SDL3, Vulkan 1.3+, SDL_ttf, SDL3_image, SDL3_mixer, fmt, spdlog, GLM, OpenMP, FreeType, and HarfBuzz dependencies.
# Builds SPIR-V shaders for Vulkan ray tracing and rasterization.
# Moves SPIR-V shaders and copies fonts to the output directory.
# Supports native builds on Linux (GCC/Clang) and Windows (MSVC/MinGW).
# Dependencies: SDL3, Vulkan SDK, glslc, SDL3_ttf, SDL3_image, SDL3_mixer, fmt, spdlog, GLM, OpenMP, FreeType, HarfBuzz, X11, Xext, Xrandr, Xcursor, Xi, Xss.
# Usage: mkdir build && cd build && cmake .. && cmake --build .
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

# Find core dependencies
find_package(Vulkan REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(SDL3 REQUIRED)
find_package(OpenMP REQUIRED)
find_package(Freetype REQUIRED)

# Check for fmt, spdlog, glm, and harfbuzz using pkg-config
pkg_check_modules(FMT REQUIRED fmt)
pkg_check_modules(SPDLOG REQUIRED spdlog)
pkg_check_modules(GLM REQUIRED glm)
pkg_check_modules(HarfBuzz REQUIRED harfbuzz)

# Ensure spdlog uses external fmt to avoid conflicts
set(SPDLOG_FMT_EXTERNAL ON CACHE BOOL "Use external fmt library for spdlog")

# SDL extensions and X11-related
if(IS_LINUX)
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

find_program(GLSLC glslc HINTS ${Vulkan_INCLUDE_DIRS}/../bin REQUIRED)
if(NOT GLSLC)
    message(FATAL_ERROR "glslc not found. Install Vulkan SDK.")
endif()

# Directories
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(ENGINE_SRC_DIR ${SRC_DIR}/engine)
set(SDL3_SRC_DIR ${ENGINE_SRC_DIR}/SDL3)
set(VULKAN_SRC_DIR ${ENGINE_SRC_DIR}/Vulkan)
set(MODES_SRC_DIR ${SRC_DIR}/modes)
set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(FONT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/fonts)

# Collect sources
file(GLOB SOURCES
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
        "${SDL3_SRC_DIR}/SDL3_input.cpp"
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

# Executable
add_executable(amouranth_engine ${SOURCES})
target_include_directories(amouranth_engine PRIVATE
    ${INCLUDE_DIR}
    ${FREETYPE_INCLUDE_DIRS}
    ${HarfBuzz_INCLUDE_DIRS}
    ${FMT_INCLUDE_DIRS}
    ${SPDLOG_INCLUDE_DIRS}
    ${GLM_INCLUDE_DIRS}
)

# Core links
target_link_libraries(amouranth_engine PRIVATE
    Vulkan::Vulkan
    SDL3::SDL3
    ${FMT_LIBRARIES}
    ${SPDLOG_LIBRARIES}
    ${GLM_LIBRARIES}
    OpenMP::OpenMP_CXX
    Freetype::Freetype
    ${HarfBuzz_LIBRARIES}
)

# Platform-specific
if(IS_LINUX)
    target_include_directories(amouranth_engine PRIVATE
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
    target_link_libraries(amouranth_engine PRIVATE
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
    target_compile_options(amouranth_engine PRIVATE -fPIC)
elseif(IS_WINDOWS)
    target_link_libraries(amouranth_engine PRIVATE SDL3_ttf SDL3_image SDL3_mixer)
    if(MINGW)
        target_link_options(amouranth_engine PRIVATE -static-libgcc -static-libstdc++)
    endif()
endif()

# Shader compilation
set(SHADER_EXTS "*.vert" "*.frag" "*.rahit" "*.rchit" "*.rmiss" "*.rgen" "*.rint" "*.rcall")
set(SHADER_OUTPUTS "")
foreach(EXT ${SHADER_EXTS})
    file(GLOB SHADERS "${SHADER_DIR}/${EXT}")
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
    add_dependencies(amouranth_engine shaders)
endif()

# Output setup
set(BIN_DIR ${CMAKE_BINARY_DIR}/bin)
set(ASSET_DIR ${BIN_DIR}/assets)
set(SHADER_OUT_DIR ${ASSET_DIR}/shaders)
set(FONT_OUT_DIR ${ASSET_DIR}/fonts)

add_custom_target(directories
    COMMAND ${CMAKE_COMMAND} -E make_directory ${BIN_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${ASSET_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUT_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${FONT_OUT_DIR}
    COMMAND ${CMAKE_COMMAND} -E echo "Created directories: ${BIN_DIR}, ${ASSET_DIR}, ${SHADER_OUT_DIR}, ${FONT_OUT_DIR}"
    COMMENT "Creating directories"
)

add_dependencies(amouranth_engine directories)

# Post-build moves for shaders
foreach(SHADER_OUTPUT ${SHADER_OUTPUTS})
    get_filename_component(SHADER_NAME ${SHADER_OUTPUT} NAME)
    add_custom_command(TARGET amouranth_engine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Moving ${SHADER_OUTPUT} to ${SHADER_OUT_DIR}/${SHADER_NAME}"
        COMMAND ${CMAKE_COMMAND} -E rename ${SHADER_OUTPUT} ${SHADER_OUT_DIR}/${SHADER_NAME}
        COMMENT "Moving shader ${SHADER_NAME}"
        DEPENDS shaders
        VERBATIM
    )
endforeach()

# Post-build copy for fonts
file(GLOB FONT_FILES "${FONT_DIR}/*.ttf")
if(NOT FONT_FILES)
    message(WARNING "No .ttf files found in ${FONT_DIR}")
endif()
add_custom_command(TARGET amouranth_engine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${FONT_FILES} ${FONT_OUT_DIR}/
    COMMENT "Copying fonts"
    VERBATIM
)

set_target_properties(amouranth_engine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${BIN_DIR}
    OUTPUT_NAME "Navigator"
)

# Custom targets
add_custom_target(build-all DEPENDS amouranth_engine)
add_custom_target(project-clean
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${BIN_DIR}
    COMMAND ${CMAKE_COMMAND} -E echo "Cleaned build directory ${BIN_DIR}"
    COMMENT "Cleaning build artifacts"
)

# Install (optional)
install(TARGETS amouranth_engine DESTINATION bin)
install(DIRECTORY ${SHADER_OUT_DIR}/ DESTINATION bin/assets/shaders OPTIONAL)
install(DIRECTORY ${FONT_OUT_DIR}/ DESTINATION bin/assets/fonts OPTIONAL)