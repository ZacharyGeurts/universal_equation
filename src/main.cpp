// src/main.cpp
#include "main.hpp"
#include <iostream>
#include <ctime>
#include <stdexcept>

// Change resolution here.
int main() {
    try {
        // Validate resolution
        const int width = 1280;
        const int height = 720;
        if (width < 320 || height < 200) {
            throw std::runtime_error("Resolution must be at least 320x200");
        }

        // Updated constructor call to match main.hpp (removed fontPath and fontSize)
        DimensionalNavigator navigator("Dimensional Navigator", width, height);
        navigator.run();
    } catch (const std::exception& e) {
        std::time_t now = std::time(nullptr);
        char timeStr[64];
        std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
        std::cerr << "[" << timeStr << "] Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}