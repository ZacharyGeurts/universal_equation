#include "main.hpp"
#include <iostream>

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    try {
        DimensionalNavigator navigator;
        navigator.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}