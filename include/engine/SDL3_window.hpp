#ifndef SDL3_WINDOW_HPP
#define SDL3_WINDOW_HPP

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>
#include <memory>
#include <string>
#include <iostream>
#include <functional> // Added for std::function

// Platform-specific includes for window-related Vulkan surface creation
#ifdef _WIN32
#include <vulkan/vulkan_win32.h>
#elif defined(__ANDROID__)
#include <vulkan/vulkan_android.h>
#elif defined(__linux__)
#include <X11/Xlib.h>
#include <vulkan/vulkan_xlib.h>
#include <vulkan/vulkan_wayland.h>
#elif defined(__APPLE__)
#include <vulkan/vulkan_macos_moltenvk.h>
#endif
// Handles SDL window creation, management, and cleanup.
// AMOURANTH RTX Engine September 2025
// Zachary Geurts 2025
namespace SDL3Initializer {

    // Custom deleter for SDL_Window
    struct SDLWindowDeleter {
        void operator()(SDL_Window* w) const {
            std::cout << "Destroying SDL window\n";
            if (w) SDL_DestroyWindow(w);
        }
    };

    // Creates an SDL window
    std::unique_ptr<SDL_Window, SDLWindowDeleter> createWindow(const char* title, int w, int h, Uint32 flags,
                                                              std::function<void(const std::string&)> logMessage);

    // Gets the SDL window
    SDL_Window* getWindow(const std::unique_ptr<SDL_Window, SDLWindowDeleter>& window);

}

#endif // SDL3_WINDOW_HPP