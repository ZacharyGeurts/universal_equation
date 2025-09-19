# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g -Iinclude -Wno-unused-parameter
LDFLAGS = -lSDL3 -lvulkan

# Shader compiler
GLSLC = glslc

# Directories
SRC_DIR = src
INCLUDE_DIR = include
SHADER_DIR = shaders
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

# Default target
all: directories $(EXECUTABLE) copy-shaders copy-font

# Create build and bin directories
directories:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR) $(SHADER_DIR)

# Link object files to create executable
$(EXECUTABLE): $(OBJECTS)
	@echo "Linking the executable: $@"
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)

# Compile source files to object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@echo "Compiling source file: $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile shader files to SPIR-V
$(SHADER_DIR)/%.spv: $(SHADER_DIR)/%.vert
	@echo "Compiling vertex shader: $<"
	$(GLSLC) $< -o $@

$(SHADER_DIR)/%.spv: $(SHADER_DIR)/%.frag
	@echo "Compiling fragment shader: $<"
	$(GLSLC) $< -o $@

$(SHADER_DIR)/%.spv: $(SHADER_DIR)/%.rahit
	@echo "Compiling anyhit shader: $<"
	$(GLSLC) $< -o $@

$(SHADER_DIR)/%.spv: $(SHADER_DIR)/%.rchit
	@echo "Compiling closesthit shader: $<"
	$(GLSLC) $< -o $@

$(SHADER_DIR)/%.spv: $(SHADER_DIR)/%.rmiss
	@echo "Compiling miss shader: $<"
	$(GLSLC) $< -o $@

$(SHADER_DIR)/%.spv: $(SHADER_DIR)/%.rgen
	@echo "Compiling raygen shader: $<"
	$(GLSLC) $< -o $@

# Copy shader files to bin directory
copy-shaders: $(SHADER_OBJECTS)
	@echo "Copying shader files to $(BIN_DIR)"
	@cp $(SHADER_DIR)/*.spv $(BIN_DIR)/

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR) $(BIN_DIR) $(SHADER_DIR)/*.spv

.PHONY: all directories copy-shaders copy-font clean