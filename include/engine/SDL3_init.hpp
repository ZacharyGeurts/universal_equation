// AMOURANTH RTX Engine, October 2025 - SDL3 and Vulkan initialization.
// Creates Vulkan instance and surface via SDL3; handles window creation.
// Thread-safe, no mutexes; designed for Windows/Linux (X11/Wayland).
// Dependencies: SDL3, Vulkan 1.3+, C++20 standard library.
// Usage: Create SDL3Initializer, call getInstance() and getSurface() for VulkanRenderer.
// Zachary Geurts 2025

#ifndef SDL3_INIT_HPP
#define SDL3_INIT_HPP

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <string>

namespace SDL3Initializer {

class SDL3Initializer {
public:
    SDL3Initializer(const std::string& title, int width, int height);
    ~SDL3Initializer();

    VkInstance getInstance() const;
    void setInstance(VkInstance instance);
    VkSurfaceKHR getSurface() const;
    void setSurface(VkSurfaceKHR surface);
    bool shouldQuit() const;
    void pollEvents();
    SDL_Window* getWindow() const;
    void setWindow(SDL_Window* window);

private:
    SDL_Window* window_ = nullptr;
    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
};

} // namespace SDL3Initializer

#endif // SDL3_INIT_HPP