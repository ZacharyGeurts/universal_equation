# Operating System Boot-Inspired Makefile (September 2025)
# This Makefile simulates an OS boot process while building the AMOURANTH RTX Engine.
# Supports cross-platform compilation for Linux, macOS, and Windows (via MinGW).
# Detects the host OS and adjusts compilers, flags, and library links accordingly.
# Key features:
# - Compiles C++17 sources with SDL3, Vulkan 1.3+, and SDL_ttf dependencies.
# - Builds SPIR-V shaders for Vulkan ray tracing (vertex, fragment, raygen, etc.).
# - Copies assets (shaders, fonts) to the output directory.
# - Includes X11 libraries for Linux to resolve BadMatch errors.
# - Detailed logging for build debugging.
# Dependencies: SDL3, SDL_ttf, Vulkan 1.3+, libX11-dev (Linux), libomp (macOS), MinGW (Windows).
# Usage: make [all|clean|directories|shaders|objects|link|copy-assets]
# Zachary Geurts, September 2025

# Detect OS
ifeq ($(OS),Windows_NT)
	# Windows (MinGW)
	UNAME_S := Windows
	CXX ?= g++
	CXXFLAGS ?= -O3 -std=c++17 -Wall -Wextra -g -fopenmp -Iinclude
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
		CXXFLAGS ?= -O3 -std=c++17 -Wall -Wextra -g -fopenmp -Iinclude
		LDFLAGS ?= -lSDL3 -lvulkan -lSDL3_ttf -lX11 -lxcb -fopenmp
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
		CXXFLAGS ?= -O3 -std=c++17 -Wall -Wextra -g -Xclang -fopenmp -Iinclude -I/usr/local/include
		LDFLAGS ?= -lSDL3 -lvulkan -lSDL3_ttf -lomp
		EXE_SUFFIX ?=
		MKDIR ?= mkdir -p
		CP ?= cp
		RM ?= rm -f
		RMDIR ?= rm -rf
		PATH_SEP ?= /
	endif
endif

# Override OS detection with TARGET_OS=Linux/macOS/Windows
ifdef TARGET_OS
	ifeq ($(TARGET_OS),Windows)
		UNAME_S := Windows
		CXX ?= x86_64-w64-mingw32-g++
		CXXFLAGS ?= -O3 -std=c++17 -Wall -Wextra -g -fopenmp -Iinclude
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
		CXXFLAGS ?= -O3 -std=c++17 -Wall -Wextra -g -Xclang -fopenmp -Iinclude -I/usr/local/include
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
		CXXFLAGS ?= -O3 -std=c++17 -Wall -Wextra -g -fopenmp -Iinclude
		LDFLAGS ?= -lSDL3 -lvulkan -lSDL3_ttf -lX11 -lxcb -fopenmp
		EXE_SUFFIX ?=
		MKDIR ?= mkdir -p
		CP ?= cp
		RM ?= rm -f
		RMDIR ?= rm -rf
		PATH_SEP ?= /
	endif
endif

# Shader compiler (assuming glslc is in PATH; cross-platform)
GLSLC ?= glslc

# Directories
SRC_DIR = src
INCLUDE_DIR = include
SHADER_DIR = shaders
FONT_DIR = fonts
BUILD_DIR = build$(PATH_SEP)$(UNAME_S)
BIN_DIR = bin$(PATH_SEP)$(UNAME_S)
ASSET_DIR = $(BIN_DIR)$(PATH_SEP)assets
SHADER_OUT_DIR = $(ASSET_DIR)$(PATH_SEP)shaders
FONT_OUT_DIR = $(ASSET_DIR)$(PATH_SEP)fonts

# Files
SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SOURCES))
EXECUTABLE = $(BIN_DIR)/Navigator$(EXE_SUFFIX)

# Shader source files by type
VERT_SHADERS = $(wildcard $(SHADER_DIR)/*.vert)
FRAG_SHADERS = $(wildcard $(SHADER_DIR)/*.frag)
RAHIT_SHADERS = $(wildcard $(SHADER_DIR)/*.rahit)
RCHIT_SHADERS = $(wildcard $(SHADER_DIR)/*.rchit)
RMISS_SHADERS = $(wildcard $(SHADER_DIR)/*.rmiss)
RGEN_SHADERS = $(wildcard $(SHADER_DIR)/*.rgen)

# Shader objects (SPIR-V)
VERT_OBJECTS = $(patsubst $(SHADER_DIR)/%.vert,$(SHADER_DIR)/%.spv,$(VERT_SHADERS))
FRAG_OBJECTS = $(patsubst $(SHADER_DIR)/%.frag,$(SHADER_DIR)/%.spv,$(FRAG_SHADERS))
RAHIT_OBJECTS = $(patsubst $(SHADER_DIR)/%.rahit,$(SHADER_DIR)/%.spv,$(RAHIT_SHADERS))
RCHIT_OBJECTS = $(patsubst $(SHADER_DIR)/%.rchit,$(SHADER_DIR)/%.spv,$(RCHIT_SHADERS))
RMISS_OBJECTS = $(patsubst $(SHADER_DIR)/%.rmiss,$(SHADER_DIR)/%.spv,$(RMISS_SHADERS))
RGEN_OBJECTS = $(patsubst $(SHADER_DIR)/%.rgen,$(SHADER_DIR)/%.spv,$(RGEN_SHADERS))

SHADER_OBJECTS = $(VERT_OBJECTS) $(FRAG_OBJECTS) $(RAHIT_OBJECTS) $(RCHIT_OBJECTS) $(RMISS_OBJECTS) $(RGEN_OBJECTS)

# Common GLSL file
COMMON_GLSL = $(SHADER_DIR)/common.glsl

# Default target: Boot the system!
all:
	@echo "=== SYSTEM BOOT SEQUENCE INITIATED (September 2025) ==="
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
	@echo "Creating directories: $(BUILD_DIR), $(BIN_DIR), $(SHADER_OUT_DIR), $(FONT_OUT_DIR)"
	@$(MKDIR) $(BUILD_DIR)
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

# Link object files to create executable
$(EXECUTABLE): $(OBJECTS) $(SHADER_OBJECTS)
	@echo "Linking system kernel: $@"
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)

# Compile source files to object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@echo "Loading module: $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile shader files to SPIR-V (Vulkan 1.3 for ray tracing support)
$(SHADER_DIR)/%.spv: $(SHADER_DIR)/%.vert
	@echo "Initializing vertex driver: $<"
	$(GLSLC) $< -o $@ --target-env=vulkan1.3

$(SHADER_DIR)/%.spv: $(SHADER_DIR)/%.frag
	@echo "Initializing fragment driver: $<"
	$(GLSLC) $< -o $@ --target-env=vulkan1.3

$(SHADER_DIR)/%.spv: $(SHADER_DIR)/%.rahit $(COMMON_GLSL)
	@echo "Initializing anyhit driver: $<"
	$(GLSLC) $< -o $@ --target-env=vulkan1.3

$(SHADER_DIR)/%.spv: $(SHADER_DIR)/%.rchit $(COMMON_GLSL)
	@echo "Initializing closesthit driver: $<"
	$(GLSLC) $< -o $@ --target-env=vulkan1.3

$(SHADER_DIR)/%.spv: $(SHADER_DIR)/%.rmiss $(COMMON_GLSL)
	@echo "Initializing miss driver: $<"
	$(GLSLC) $< -o $@ --target-env=vulkan1.3

$(SHADER_DIR)/%.spv: $(SHADER_DIR)/%.rgen $(COMMON_GLSL)
	@echo "Initializing raygen driver: $<"
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
	@$(RMDIR) $(BUILD_DIR)
	@$(RMDIR) $(BIN_DIR)
	@$(RM) $(SHADER_DIR)$(PATH_SEP)*.spv
	@echo "=== SHUTDOWN COMPLETE ==="

.PHONY: all directories shaders objects link copy-assets copy-shaders copy-fonts clean