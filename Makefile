# Operating System Boot-Inspired Makefile (October 2025)
# Builds the AMOURANTH RTX Engine for game developers (e.g., Mario-like games).
# Supports cross-platform compilation for Linux, macOS, Windows (via MinGW), and Android (via NDK).
# Compiles C++17 sources with SDL3, Vulkan 1.3+, and SDL_ttf dependencies.
# Builds SPIR-V shaders for Vulkan ray tracing (raygen, miss, closesthit, anyhit, intersection, callable) and rasterization (vertex, fragment).
# Copies assets (shaders, fonts) to the output directory.
# Includes X11 libraries for Linux to resolve BadMatch errors.
# Adds Android NDK cross-compilation support for ARM64.
# Dependencies: SDL3, SDL_ttf, Vulkan 1.3+, libX11-dev (Linux), libomp (macOS), MinGW (Windows), Android NDK (Android).
# Usage: make [all|clean|directories|shaders|objects|link|copy-assets]
# Zachary Geurts 2025

# Detect OS
ifeq ($(OS),Windows_NT)
	# Windows (MinGW)
	UNAME_S := Windows
	CXX ?= x86_64-w64-mingw32-g++
	CXXFLAGS ?= -O3 -std=c++20 -Wall -Wextra -g -fopenmp -Iinclude
	LDFLAGS ?= -lSDL3 -lvulkan -lSDL3_ttf -fopenmp -static-libgcc -static-libstdc++
	EXE_SUFFIX ?= .exe
	MKDIR ?= mkdir
	CP ?= copy
	RM ?= del /Q
	RMDIR ?= rmdir /S /Q
	PATH_SEP ?= \\
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Linux)
	    # Linux
	    CXX ?= g++
	    CXXFLAGS ?= -fPIC -O3 -std=c++20 -Wall -Wextra -g -fopenmp -Iinclude -I/usr/include/fmt
	    LDFLAGS ?= -lSDL3 -lvulkan -lSDL3_ttf -lX11 -lxcb -fopenmp -lfmt
	    EXE_SUFFIX ?=
	    MKDIR ?= mkdir -p
	    CP ?= cp
	    RM ?= rm -f
	    RMDIR ?= rm -rf
	    PATH_SEP ?= /
	endif
	ifeq ($(UNAME_S),Darwin)
	    # macOS
	    CXX ?= clang++
	    CXXFLAGS ?= -O3 -std=c++20 -Wall -Wextra -g -Xclang -fopenmp -Iinclude -I/usr/local/include
	    LDFLAGS ?= -lSDL3 -lvulkan -lSDL3_ttf -lomp
	    EXE_SUFFIX ?=
	    MKDIR ?= mkdir -p
	    CP ?= cp
	    RM ?= rm -f
	    RMDIR ?= rm -rf
	    PATH_SEP ?= /
	endif
endif

# Override OS detection with TARGET_OS=Linux/macOS/Windows/Android
ifdef TARGET_OS
	ifeq ($(TARGET_OS),Windows)
	    UNAME_S := Windows
	    CXX ?= x86_64-w64-mingw32-g++
	    CXXFLAGS ?= -O3 -std=c++20 -Wall -Wextra -g -fopenmp -Iinclude
	    LDFLAGS ?= -lSDL3 -lvulkan -lSDL3_ttf -fopenmp -static-libgcc -static-libstdc++
	    EXE_SUFFIX ?= .exe
	    MKDIR ?= mkdir -p
	    CP ?= cp
	    RM ?= rm -f
	    RMDIR ?= rm -rf
	    PATH_SEP ?= /
	endif
	ifeq ($(TARGET_OS),macOS)
	    UNAME_S := Darwin
	    CXX ?= clang++
	    CXXFLAGS ?= -O3 -std=c++20 -Wall -Wextra -g -Xclang -fopenmp -Iinclude -I/usr/local/include
	    LDFLAGS ?= -lSDL3 -lvulkan -lSDL3_ttf -lomp
	    EXE_SUFFIX ?=
	    MKDIR ?= mkdir -p
	    CP ?= cp
	    RM ?= rm -f
	    RMDIR ?= rm -rf
	    PATH_SEP ?= /
	endif
	ifeq ($(TARGET_OS),Linux)
	    UNAME_S := Linux
	    CXX ?= g++
	    CXXFLAGS ?= -O3 -std=c++20 -Wall -Wextra -g -fopenmp -Iinclude
	    LDFLAGS ?= -lSDL3 -lvulkan -lSDL3_ttf -lX11 -lxcb -fopenmp
	    EXE_SUFFIX ?=
	    MKDIR ?= mkdir -p
	    CP ?= cp
	    RM ?= rm -f
	    RMDIR ?= rm -rf
	    PATH_SEP ?= /
	endif
	ifeq ($(TARGET_OS),Android)
	    UNAME_S := Android
	    NDK_HOME ?= $(shell echo $$NDK_HOME)
	    ifeq ($(NDK_HOME),)
	        $(error NDK_HOME is not set. Please set the Android NDK path.)
	    endif
	    ARCH ?= aarch64-linux-android
	    API_LEVEL ?= 33
	    TOOLCHAIN = $(NDK_HOME)/toolchains/llvm/prebuilt/$(shell uname -s | tr A-Z a-z)-x86_64
	    CXX = $(TOOLCHAIN)/bin/$(ARCH)-$(API_LEVEL)-clang++
	    CXXFLAGS ?= -O3 -std=c++20 -Wall -Wextra -g -Iinclude -I$(NDK_HOME)/sysroot/usr/include -I$(NDK_HOME)/sysroot/usr/include/aarch64-linux-android
	    LDFLAGS ?= -L$(NDK_HOME)/sysroot/usr/lib/aarch64-linux-android/$(API_LEVEL) -lSDL3 -lvulkan -lSDL3_ttf -llog
	    EXE_SUFFIX ?=
	    MKDIR ?= mkdir -p
	    CP ?= cp
	    RM ?= rm -f
	    RMDIR ?= rm -rf
	    PATH_SEP ?= /
	    EXECUTABLE = $(BIN_DIR)/libNavigator.so
	endif
endif

# Shader compiler (assuming glslc is in PATH; cross-platform)
GLSLC ?= glslc

# Directories
SRC_DIR = src
ENGINE_SRC_DIR = src/engine
SDL3_SRC_DIR = src/engine/SDL3
VULKAN_SRC_DIR = src/engine/Vulkan
MODES_SRC_DIR = src/modes
INCLUDE_DIR = include
ENGINE_INCLUDE_DIR = include/engine
SHADER_DIR = shaders
FONT_DIR = fonts
BUILD_DIR = build$(PATH_SEP)$(UNAME_S)
ENGINE_BUILD_DIR = $(BUILD_DIR)/engine
SDL3_BUILD_DIR = $(BUILD_DIR)/engine/SDL3
VULKAN_BUILD_DIR = $(BUILD_DIR)/engine/Vulkan
MODES_BUILD_DIR = $(BUILD_DIR)/modes
BIN_DIR = bin$(PATH_SEP)$(UNAME_S)
ASSET_DIR = $(BIN_DIR)$(PATH_SEP)assets
SHADER_OUT_DIR = $(ASSET_DIR)$(PATH_SEP)shaders
FONT_OUT_DIR = $(ASSET_DIR)$(PATH_SEP)fonts

# Files
SOURCES = $(wildcard $(SRC_DIR)/*.cpp) \
	      $(wildcard $(ENGINE_SRC_DIR)/*.cpp) \
	      $(wildcard $(SDL3_SRC_DIR)/*.cpp) \
	      $(wildcard $(VULKAN_SRC_DIR)/*.cpp) \
	      $(wildcard $(MODES_SRC_DIR)/*.cpp)
OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(wildcard $(SRC_DIR)/*.cpp)) \
	      $(patsubst $(ENGINE_SRC_DIR)/%.cpp,$(ENGINE_BUILD_DIR)/%.o,$(wildcard $(ENGINE_SRC_DIR)/*.cpp)) \
	      $(patsubst $(SDL3_SRC_DIR)/%.cpp,$(SDL3_BUILD_DIR)/%.o,$(wildcard $(SDL3_SRC_DIR)/*.cpp)) \
	      $(patsubst $(VULKAN_SRC_DIR)/%.cpp,$(VULKAN_BUILD_DIR)/%.o,$(wildcard $(VULKAN_SRC_DIR)/*.cpp)) \
	      $(patsubst $(MODES_SRC_DIR)/%.cpp,$(MODES_BUILD_DIR)/%.o,$(wildcard $(MODES_SRC_DIR)/*.cpp))
EXECUTABLE ?= $(BIN_DIR)/Navigator$(EXE_SUFFIX)

# Shader source files by type
VERT_SHADERS = $(wildcard $(SHADER_DIR)/*.vert)
FRAG_SHADERS = $(wildcard $(SHADER_DIR)/*.frag)
RAHIT_SHADERS = $(wildcard $(SHADER_DIR)/*.rahit)
RCHIT_SHADERS = $(wildcard $(SHADER_DIR)/*.rchit)
RMISS_SHADERS = $(wildcard $(SHADER_DIR)/*.rmiss)
RGEN_SHADERS = $(wildcard $(SHADER_DIR)/*.rgen)
RINT_SHADERS = $(wildcard $(SHADER_DIR)/*.rint)
RCALL_SHADERS = $(wildcard $(SHADER_DIR)/*.rcall)

# Shader objects (SPIR-V)
VERT_OBJECTS = $(patsubst $(SHADER_DIR)/%.vert,$(SHADER_DIR)/%.spv,$(VERT_SHADERS))
FRAG_OBJECTS = $(patsubst $(SHADER_DIR)/%.frag,$(SHADER_DIR)/%.spv,$(FRAG_SHADERS))
RAHIT_OBJECTS = $(patsubst $(SHADER_DIR)/%.rahit,$(SHADER_DIR)/%.spv,$(RAHIT_SHADERS))
RCHIT_OBJECTS = $(patsubst $(SHADER_DIR)/%.rchit,$(SHADER_DIR)/%.spv,$(RCHIT_SHADERS))
RMISS_OBJECTS = $(patsubst $(SHADER_DIR)/%.rmiss,$(SHADER_DIR)/%.spv,$(RMISS_SHADERS))
RGEN_OBJECTS = $(patsubst $(SHADER_DIR)/%.rgen,$(SHADER_DIR)/%.spv,$(RGEN_SHADERS))
RINT_OBJECTS = $(patsubst $(SHADER_DIR)/%.rint,$(SHADER_DIR)/%.spv,$(RINT_SHADERS))
RCALL_OBJECTS = $(patsubst $(SHADER_DIR)/%.rcall,$(SHADER_DIR)/%.spv,$(RCALL_SHADERS))

SHADER_OBJECTS = $(VERT_OBJECTS) $(FRAG_OBJECTS) $(RAHIT_OBJECTS) $(RCHIT_OBJECTS) $(RMISS_OBJECTS) $(RGEN_OBJECTS) $(RINT_OBJECTS) $(RCALL_OBJECTS)

# Default target: Boot the system!
all:
	@echo "=== SYSTEM BOOT SEQUENCE INITIATED (October 2025) ==="
	@echo "Power On Self Test (POST): Checking hardware compatibility for $(UNAME_S)..."
	@echo "BIOS initialized. Detecting peripherals..."
	$(MAKE) directories
	@echo "Bootloader loading: Preparing shader modules..."
	$(MAKE) shaders
	@echo "Kernel initialization: Compiling core components..."
	$(MAKE) objects
	@echo "Mounting filesystems: Linking executables..."
	$(MAKE) link
	@echo "Starting services: Copying assets and fonts..."
	$(MAKE) copy-assets
	@echo "=== BOOT COMPLETE: System ready at $(EXECUTABLE) ==="

# Create directories
directories:
	@echo "Creating directories: $(BUILD_DIR), $(ENGINE_BUILD_DIR), $(SDL3_BUILD_DIR), $(VULKAN_BUILD_DIR), $(MODES_BUILD_DIR), $(BIN_DIR), $(SHADER_OUT_DIR), $(FONT_OUT_DIR)"
	@$(MKDIR) $(BUILD_DIR)
	@$(MKDIR) $(ENGINE_BUILD_DIR)
	@$(MKDIR) $(SDL3_BUILD_DIR)
	@$(MKDIR) $(VULKAN_BUILD_DIR)
	@$(MKDIR) $(MODES_BUILD_DIR)
	@$(MKDIR) $(BIN_DIR)
	@$(MKDIR) $(SHADER_OUT_DIR)
	@$(MKDIR) $(FONT_OUT_DIR)

# Compile shaders
shaders: $(SHADER_OBJECTS)
	@echo "Shader drivers loaded successfully."

# Compile source objects
objects: $(OBJECTS)
	@echo "Core kernel modules compiled."

# Link executable
link: $(EXECUTABLE)
	@echo "Executable kernel linked."

# Copy assets
copy-assets: copy-shaders copy-fonts
	@echo "Userland assets deployed."

# Link object files to create executable or shared library
$(EXECUTABLE): $(OBJECTS) $(SHADER_OBJECTS)
	@echo "Linking system kernel: $@"
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS) $(if $(filter Android,$(UNAME_S)),-shared,)

# Compile source files to object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(if $(filter Android,$(UNAME_S)),-fPIC,)

$(ENGINE_BUILD_DIR)/%.o: $(ENGINE_SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(if $(filter Android,$(UNAME_S)),-fPIC,)

$(SDL3_BUILD_DIR)/%.o: $(SDL3_SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(if $(filter Android,$(UNAME_S)),-fPIC,)

$(VULKAN_BUILD_DIR)/%.o: $(VULKAN_SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(if $(filter Android,$(UNAME_S)),-fPIC,)

$(MODES_BUILD_DIR)/%.o: $(MODES_SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(if $(filter Android,$(UNAME_S)),-fPIC,)

# Compile shader files to SPIR-V (Vulkan 1.3 for ray tracing support)
$(SHADER_DIR)/%.spv: $(SHADER_DIR)/%.vert
	$(GLSLC) $< -o $@ --target-env=vulkan1.3

$(SHADER_DIR)/%.spv: $(SHADER_DIR)/%.frag
	$(GLSLC) $< -o $@ --target-env=vulkan1.3

$(SHADER_DIR)/%.spv: $(SHADER_DIR)/%.rahit
	$(GLSLC) $< -o $@ --target-env=vulkan1.3

$(SHADER_DIR)/%.spv: $(SHADER_DIR)/%.rchit
	$(GLSLC) $< -o $@ --target-env=vulkan1.3

$(SHADER_DIR)/%.spv: $(SHADER_DIR)/%.rmiss
	$(GLSLC) $< -o $@ --target-env=vulkan1.3

$(SHADER_DIR)/%.spv: $(SHADER_DIR)/%.rgen
	$(GLSLC) $< -o $@ --target-env=vulkan1.3

$(SHADER_DIR)/%.spv: $(SHADER_DIR)/%.rint
	$(GLSLC) $< -o $@ --target-env=vulkan1.3

$(SHADER_DIR)/%.spv: $(SHADER_DIR)/%.rcall
	$(GLSLC) $< -o $@ --target-env=vulkan1.3

# Copy shader files to bin/assets/shaders directory
copy-shaders: $(SHADER_OBJECTS)
	@echo "Mounting shader filesystem..."
	@if [ -n "$(SHADER_OBJECTS)" ]; then \
	    $(CP) $(SHADER_DIR)$(PATH_SEP)*.spv $(SHADER_OUT_DIR); \
	else \
	    echo "No shaders to copy."; \
	fi

# Copy font files to bin/assets/fonts directory
copy-fonts:
	@echo "Mounting font filesystem..."
	@if [ -d "$(FONT_DIR)" ] && [ -n "$(wildcard $(FONT_DIR)/*.ttf)" ]; then \
	    $(CP) $(FONT_DIR)$(PATH_SEP)*.ttf $(FONT_OUT_DIR); \
	else \
	    echo "Warning: No TTF fonts found in $(FONT_DIR) or directory does not exist."; \
	fi

# Clean build artifacts (System Shutdown)
clean:
	@echo "=== SYSTEM SHUTDOWN INITIATED ==="
	@echo "Unmounting filesystems..."
	@$(RMDIR) bin
	@$(RMDIR) build
	@$(RM) $(SHADER_DIR)$(PATH_SEP)*.spv
	@echo "=== SHUTDOWN COMPLETE ==="

.PHONY: all directories shaders objects link copy-assets copy-shaders copy-fonts clean