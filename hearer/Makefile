# Makefile for DUST v2 Shower Hearer (SDL2 + OpenGL + UniversalEquation)
# Compile with: make
# Run with: ./shower_hearer [max_dims]
# Clean with: make clean

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -fopenmp
LDFLAGS = -fopenmp
INCLUDES = -I/usr/include/SDL2  # Adjust if SDL2 is in non-standard path
LIBS = -lSDL2 -lSDL2main -lSDL2_mixer -lGL -lGLU

TARGET = shower_hearer
SOURCES = shower_hearer.cpp universal_equation.cpp
OBJECTS = $(SOURCES:.cpp=.o)
HEADERS = universal_equation.hpp

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET) $(LDFLAGS) $(LIBS)

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)