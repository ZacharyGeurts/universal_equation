// AMOURANTH RTX Engine, October 2025 - SDL3 window creation and management.
// Dependencies: SDL3, Vulkan 1.3+, C++20 standard library, logging.hpp.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#include "engine/SDL3/SDL3_window.hpp"
#include "engine/logging.hpp"
#include <SDL3/SDL_vulkan.h>
#include <stdexcept>
#include <source_location>

namespace SDL3Initializer {

void SDLWindowDeleter::operator()(SDL_Window* w) const {
    if (w) {
        LOG_INFO_CAT("General", "Destroying SDL window", std::source_location::current());
        SDL_DestroyWindow(w);
    }
}

SDLWindowPtr createWindow(
    const char* title, 
    int w, 
    int h, 
    Uint32 flags) 
{
    LOG_INFO_CAT("General", "Creating SDL window with title={}, width={}, height={}, flags=0x{:x}", 
                 std::source_location::current(), title, w, h, flags);

    // Ensure Vulkan flag is set
    flags |= SDL_WINDOW_VULKAN;
    
    // Set hints to prefer Wayland on Linux and ensure Vulkan context
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland,x11");
    
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0) {
        LOG_ERROR_CAT("General", "SDL_Init failed: {}", std::source_location::current(), SDL_GetError());
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
    }

    SDLWindowPtr window(SDL_CreateWindow(title, w, h, flags));
    if (!window) {
        LOG_ERROR_CAT("General", "SDL_CreateWindow failed: {}", std::source_location::current(), SDL_GetError());
        throw std::runtime_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
    }

    // Log video driver and window flags
    const char* videoDriver = SDL_GetCurrentVideoDriver();
    LOG_INFO_CAT("General", "SDL window created with video driver: {}, flags=0x{:x}", 
                 std::source_location::current(), videoDriver ? videoDriver : "none", SDL_GetWindowFlags(window.get()));

    return window;
}

SDL_Window* getWindow(const SDLWindowPtr& window) {
    LOG_DEBUG_CAT("General", "Retrieving SDL window", std::source_location::current());
    return window.get();
}

} // namespace SDL3Initializer