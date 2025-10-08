#ifndef SDL3_INIT_HPP
#define SDL3_INIT_HPP

// AMOURANTH RTX Engine, October 2025 - SDL3 and Vulkan initialization.
// Creates Vulkan instance and surface via SDL3; handles window creation.
// Thread-safe, no mutexes; designed for Windows/Linux (X11/Wayland).
// Dependencies: SDL3, Vulkan 1.3+, C++20 standard library.
// Usage: Create SDL3Initializer, call getInstance() and getSurface() for VulkanRenderer.
// Zachary Geurts 2025

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <string>
#include <vector>

// Forward declaration to avoid including full logging.hpp
namespace Logging {
    class Logger;
}

class SDL3Initializer {
public:
    SDL3Initializer(const std::string& title, int width, int height, const Logging::Logger& logger);
    ~SDL3Initializer();

    VkInstance getInstance() const { return instance_; }
    VkSurfaceKHR getSurface() const { return surface_; }
    bool shouldQuit() const;
    void pollEvents();

private:
    SDL_Window* window_;
    VkInstance instance_;
    VkSurfaceKHR surface_;
    const Logging::Logger& logger_;
};

#endif // SDL3_INIT_HPP