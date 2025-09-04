# Makefile for compiling the Dimension Dance project
# This is like a recipe book to build our playground game!

# Compiler to use (like picking our cooking tool)
CC = g++

# Flags for the compiler (extra instructions for safety and fun)
CFLAGS = -std=c++17 -Wall -Wextra -g

# Libraries to link (our playground tools)
# Adjust paths if SDL3/SDL3_ttf are in a custom location
LIBS = -lSDL3 -lSDL3_ttf

# Target executable (the finished game!)
TARGET = dimension_dance

# Source files (the ingredients)
SOURCES = main.cpp

# Object files (the cooked ingredients)
OBJECTS = $(SOURCES:.cpp=.o)

# Default target (what to make when we say "make")
all: $(TARGET)

# Link object files to create the executable
$(TARGET): $(OBJECTS)
	@echo "Mixing all the ingredients to make the game!"
	$(CC) $(OBJECTS) -o $(TARGET) $(LIBS)

# Compile source files to object files
%.o: %.cpp
	@echo "Cooking the source file: $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up (tidy the kitchen!)
clean:
	@echo "Cleaning up the playground!"
	rm -f $(OBJECTS) $(TARGET)

# Phony targets (not real files, just commands)
.PHONY: all clean