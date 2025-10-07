// AMOURANTH RTX Engine, October 2025 - Vulkan swapchain management.
// Manages swapchain creation, image views, and resize handling.
// Dependencies: Vulkan 1.3+, GLM, C++20 standard library.
// Zachary Geurts 2025

#ifndef VULKAN_INIT_SWAPCHAIN_HPP
#define VULKAN_INIT_SWAPCHAIN_HPP

#include <vulkan/vulkan.h>
#include "Vulkan_init.hpp"
#include <vector>

class VulkanSwapchainManager {
public:
    VulkanSwapchainManager(VulkanContext& context, VkSurfaceKHR surface);
    void initializeSwapchain(int width, int height);
    void handleResize(int width, int height);

private:
    VulkanContext& context_;
    VkSurfaceKHR surface_;
};

#endif // VULKAN_INIT_SWAPCHAIN_HPP