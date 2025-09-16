#include "main.hpp"
#include <iostream>
#include <ctime>

// Screen resolution 1500x1000, keep above 1280x720
int main() {
    try {
        DimensionalNavigator navigator("Dimensional Navigator", 1500, 1000, "arial.ttf", 24);
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