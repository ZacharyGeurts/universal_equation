# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g -Iinclude -I/usr/include/glm
LDFLAGS = -lSDL3 -lSDL3_ttf -lvulkan

# Directories
SRC_DIR = src
INCLUDE_DIR = include
SHADER_DIR = shaders
FONT_DIR = .
BUILD_DIR = build
BIN_DIR = bin

# Files
SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SOURCES))
EXECUTABLE = $(BIN_DIR)/Navigator
FONT_FILE = $(FONT_DIR)/arial.ttf
FONT_DEST = $(BIN_DIR)/arial.ttf

# Shader files
SHADERS = $(wildcard $(SHADER_DIR)/*.spv)
SHADER_DEST = $(BIN_DIR)

# Default target
all: directories $(EXECUTABLE) copy-shaders copy-font

# Create build and bin directories
directories:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR)

# Link object files to create executable
$(EXECUTABLE): $(OBJECTS)
	@echo "Linking the executable: $@"
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)

# Compile source files to object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@echo "Compiling source file: $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Copy shader files to bin directory
copy-shaders: $(SHADERS)
	@echo "Copying shader files to $(SHADER_DEST)"
	@cp $(SHADERS) $(SHADER_DEST)

# Copy font file to bin directory
copy-font: $(FONT_FILE)
	@echo "Copying font file to $(BIN_DIR)"
	@cp $(FONT_FILE) $(BIN_DIR)

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR) $(BIN_DIR)

.PHONY: all directories copy-shaders copy-font clean