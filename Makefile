# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g -Iinclude -Wno-unused-parameter
LDFLAGS = -lSDL3 -lSDL3_ttf -lvulkan

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
FONT_FILE = arial.ttf
FONT_DEST = $(BIN_DIR)/arial.ttf
SHADER_SOURCES = $(wildcard $(SHADER_DIR)/*.vert) $(wildcard $(SHADER_DIR)/*.frag)
SHADER_OBJECTS = $(patsubst $(SHADER_DIR)/%.vert,$(SHADER_DIR)/%.spv,$(SHADER_SOURCES:.frag=.spv))

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

# Copy shader files to bin directory
copy-shaders: $(SHADER_OBJECTS)
	@echo "Copying shader files to $(BIN_DIR)"
	@cp $(SHADER_DIR)/*.spv $(BIN_DIR)/

# Copy font file to bin directory
copy-font: $(FONT_FILE)
	@echo "Copying font file to $(BIN_DIR)"
	@cp $(FONT_FILE) $(BIN_DIR)

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR) $(BIN_DIR) $(SHADER_DIR)/*.spv

.PHONY: all directories copy-shaders copy-font clean