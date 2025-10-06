// AMOURANTH RTX Engine, September 2025 - SDL3 window creation and management.
// Dependencies: SDL3, C++20 standard library.
// Zachary Geurts 2025

#include "engine/SDL3/SDL3_window.hpp"
#include <stdexcept>

namespace SDL3Initializer {

std::unique_ptr<SDL_Window, SDLWindowDeleter> createWindow(const char* title, int w, int h, Uint32 flags,
                                                          std::function<void(const std::string&)> logMessage) {
    logMessage("Creating SDL window with flags: " + std::to_string(flags));
    auto window = std::unique_ptr<SDL_Window, SDLWindowDeleter>(SDL_CreateWindow(title, w, h, flags));
    if (!window) {
        std::string error = "SDL_CreateWindow failed: " + std::string(SDL_GetError());
        logMessage(error);
        throw std::runtime_error(error);
    }
    return window;
}

SDL_Window* getWindow(const std::unique_ptr<SDL_Window, SDLWindowDeleter>& window) {
    return window.get();
}

} // namespace SDL3Initializer