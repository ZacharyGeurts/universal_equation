#include "main.hpp"
#include <iostream>
#include <ctime>

int main() {
    try {
        DimensionalNavigator navigator("Dimensional Navigator", 1280, 720, "assets/fonts/arial.ttf", 24);
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