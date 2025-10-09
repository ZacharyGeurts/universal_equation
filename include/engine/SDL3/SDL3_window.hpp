// AMOURANTH RTX Engine, October 2025 - SDL3 window creation and management.
// Dependencies: SDL3, Vulkan 1.3+, C++20 standard library.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#ifndef SDL3_WINDOW_HPP
#define SDL3_WINDOW_HPP

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>
#include <memory>

// Platform-specific includes for window-related Vulkan surface creation
#ifdef _WIN32
#include <vulkan/vulkan_win32.h>
#elif defined(__linux__)
#include <X11/Xlib.h> // Added before vulkan_xlib.h for Display, Window, VisualID
#include <vulkan/vulkan_xlib.h>
#include <vulkan/vulkan_wayland.h>
#endif

namespace SDL3Initializer {

struct SDLWindowDeleter {
    void operator()(SDL_Window* w) const;
};

using SDLWindowPtr = std::unique_ptr<SDL_Window, SDLWindowDeleter>;

SDLWindowPtr createWindow(
    const char* title, 
    int w, 
    int h, 
    Uint32 flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
);

SDL_Window* getWindow(const SDLWindowPtr& window);

} // namespace SDL3Initializer

#endif // SDL3_WINDOW_HPP