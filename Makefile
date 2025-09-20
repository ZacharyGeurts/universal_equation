# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g -Iinclude -Wno-unused-parameter
LDFLAGS = -lSDL3 -lvulkan -lSDL3_ttf

# Shader compiler
GLSLC = glslc

# Directories
SRC_DIR = src
INCLUDE_DIR = include
SHADER_DIR = shaders
FONT_DIR = fonts
BUILD_DIR = build
BIN_DIR = bin

# Files
SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SOURCES))
EXECUTABLE = $(BIN_DIR)/Navigator

# Shader source files (vertex, fragment, and ray-tracing shaders)
SHADER_SOURCES = $(wildcard $(SHADER_DIR)/*.vert) \
				$(wildcard $(SHADER_DIR)/*.frag) \
				$(wildcard $(SHADER_DIR)/*.rahit) \
				$(wildcard $(SHADER_DIR)/*.rchit) \
				$(wildcard $(SHADER_DIR)/*.rmiss) \
				$(wildcard $(SHADER_DIR)/*.rgen)
SHADER_OBJECTS = $(patsubst $(SHADER_DIR)/%.vert,$(SHADER_DIR)/%.spv,$(SHADER_SOURCES)) \
				$(patsubst $(SHADER_DIR)/%.frag,$(SHADER_DIR)/%.spv,$(SHADER_SOURCES)) \
				$(patsubst $(SHADER_DIR)/%.rahit,$(SHADER_DIR)/%.spv,$(SHADER_SOURCES)) \
				$(patsubst $(SHADER_DIR)/%.rchit,$(SHADER_DIR)/%.spv,$(SHADER_SOURCES)) \
				$(patsubst $(SHADER_DIR)/%.rmiss,$(SHADER_DIR)/%.spv,$(SHADER_SOURCES)) \
				$(patsubst $(SHADER_DIR)/%.rgen,$(SHADER_DIR)/%.spv,$(SHADER_SOURCES))

# Common GLSL file
COMMON_GLSL = $(SHADER_DIR)/common.glsl

# Default target
all: directories $(EXECUTABLE) copy-shaders copy-font

# Create build and bin directories
directories:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR) $(SHADER_DIR)

# Link object files to create executable
$(EXECUTABLE): $(OBJECTS) $(SHADER_OBJECTS)
	@echo "Linking the executable: $@"
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)

# Compile source files to object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@echo "Compiling source file: $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile shader files to SPIR-V
$(SHADER_DIR)/%.spv: $(SHADER_DIR)/%.vert
	@echo "Compiling vertex shader: $<"
	$(GLSLC) $< -o $@ --target-env=vulkan1.4

$(SHADER_DIR)/%.spv: $(SHADER_DIR)/%.frag
	@echo "Compiling fragment shader: $<"
	$(GLSLC) $< -o $@ --target-env=vulkan1.4

$(SHADER_DIR)/%.spv: $(SHADER_DIR)/%.rahit $(COMMON_GLSL)
	@echo "Compiling anyhit shader: $<"
	$(GLSLC) $< -o $@ --target-env=vulkan1.4

$(SHADER_DIR)/%.spv: $(SHADER_DIR)/%.rchit $(COMMON_GLSL)
	@echo "Compiling closesthit shader: $<"
	$(GLSLC) $< -o $@ --target-env=vulkan1.4

$(SHADER_DIR)/%.spv: $(SHADER_DIR)/%.rmiss $(COMMON_GLSL)
	@echo "Compiling miss shader: $<"
	$(GLSLC) $< -o $@ --target-env=vulkan1.4

$(SHADER_DIR)/%.spv: $(SHADER_DIR)/%.rgen $(COMMON_GLSL)
	@echo "Compiling raygen shader: $<"
	$(GLSLC) $< -o $@ --target-env=vulkan1.4

# Copy shader files to bin directory
copy-shaders: $(SHADER_OBJECTS)
	@echo "Creating paths"
	@mkdir $(BIN_DIR)/assets
	@echo "Copying shader files to $(BIN_DIR)/assets/shaders"
	@mkdir $(BIN_DIR)/assets/shaders
	@cp $(SHADER_DIR)/*.spv $(BIN_DIR)/assets/shaders

	@echo "Copying font files to $(BIN_DIR)/assets/fonts"
	@mkdir $(BIN_DIR)/assets/fonts
	@cp $(FONT_DIR)/*.ttf $(BIN_DIR)/assets/fonts

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR) $(BIN_DIR) $(SHADER_DIR)/*.spv

.PHONY: all directories copy-shaders copy-font clean