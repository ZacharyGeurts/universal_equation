#ifndef SDL3_VULKAN_HPP
#define SDL3_VULKAN_HPP

#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <functional>
#include <algorithm>
#include <cstring>
#include "engine/SDL3/SDL3_window.hpp"
#include "engine/Vulkan/Vulkan_func.hpp"
// AMOURANTH RTX Engine September 2025
// Zachary Geurts 2025
namespace SDL3Initializer {

    // Custom deleter for VkInstance
    struct VulkanInstanceDeleter {
        void operator()(VkInstance i) const {
            std::cout << "Destroying Vulkan instance\n";
            if (i) vkDestroyInstance(i, nullptr);
        }
    };

    // Custom deleter for VkSurfaceKHR
    struct VulkanSurfaceDeleter {
        void operator()(VkSurfaceKHR s) const {
            std::cout << "Destroying Vulkan surface\n";
            if (s && m_instance) {
                vkDestroySurfaceKHR(m_instance, s, nullptr);
            } else if (s) {
                std::cout << "Warning: Cannot destroy VkSurfaceKHR because VkInstance is invalid\n";
            }
        }
        VkInstance m_instance;
    };

    // Initializes Vulkan instance and surface
    void initVulkan(SDL_Window* window, std::unique_ptr<std::remove_pointer_t<VkInstance>, VulkanInstanceDeleter>& instance,
                    std::unique_ptr<std::remove_pointer_t<VkSurfaceKHR>, VulkanSurfaceDeleter>& surface,
                    bool enableValidation, bool preferNvidia, bool rt, const char* title,
                    std::function<void(const std::string&)> logMessage);

    // Gets Vulkan instance
    VkInstance getVkInstance(const std::unique_ptr<std::remove_pointer_t<VkInstance>, VulkanInstanceDeleter>& instance);

    // Gets Vulkan surface
    VkSurfaceKHR getVkSurface(const std::unique_ptr<std::remove_pointer_t<VkSurfaceKHR>, VulkanSurfaceDeleter>& surface);

    // Gets Vulkan instance extensions
    std::vector<std::string> getVulkanExtensions(std::function<void(const std::string&)> logMessage);

}

#endif // SDL3_VULKAN_HPP
