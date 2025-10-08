// AMOURANTH RTX Engine, October 2025 - SDL3 window creation and management.
// Dependencies: SDL3, Vulkan 1.3+, C++20 standard library.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#include "engine/SDL3/SDL3_window.hpp"
#include <SDL3/SDL_vulkan.h>
#include <stdexcept>
#include <string>

namespace SDL3Initializer {

std::unique_ptr<SDL_Window, SDLWindowDeleter> createWindow(
    const char* title, 
    int w, 
    int h, 
    Uint32 flags,
    std::function<void(const std::string&)> logMessage) 
{
    Logging::Logger logger;
    std::string logMsg = "Creating SDL window with title=" + std::string(title) + 
                         ", width=" + std::to_string(w) + 
                         ", height=" + std::to_string(h) + 
                         ", flags=0x" + std::to_string(flags);
    logMessage(logMsg);
    
    // Ensure Vulkan flag is set
    flags |= SDL_WINDOW_VULKAN;
    
    // Set hints to prefer Wayland on Linux and ensure Vulkan context
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland,x11");
    
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0) {
        std::string error = "SDL_Init failed: " + std::string(SDL_GetError());
        logMessage(error);
        logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }

    auto window = std::unique_ptr<SDL_Window, SDLWindowDeleter>(
        SDL_CreateWindow(title, w, h, flags)
    );
    if (!window) {
        std::string error = "SDL_CreateWindow failed: " + std::string(SDL_GetError());
        logMessage(error);
        logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }

    // Log video driver and window flags
    const char* videoDriver = SDL_GetCurrentVideoDriver();
    std::string driverLog = "SDL window created with video driver: " + 
                            std::string(videoDriver ? videoDriver : "none") + 
                            ", flags=0x" + std::to_string(SDL_GetWindowFlags(window.get()));
    logMessage(driverLog);
    logger.log(Logging::LogLevel::Info, "{}", std::source_location::current(), driverLog);

    return window;
}

SDL_Window* getWindow(const std::unique_ptr<SDL_Window, SDLWindowDeleter>& window) {
    return window.get();
}

} // namespace SDL3Initializer