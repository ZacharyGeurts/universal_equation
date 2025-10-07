// AMOURANTH RTX Engine, October 2025 - Vulkan swapchain management.
// Initializes and manages swapchain, image views, and framebuffers.
// Dependencies: Vulkan 1.3+, C++20 standard library.
// Zachary Geurts 2025

#ifndef VULKAN_INIT_SWAPCHAIN_HPP
#define VULKAN_INIT_SWAPCHAIN_HPP

#include <vulkan/vulkan.h>
#include <vector>

// Forward declaration of VulkanContext
struct VulkanContext;

class VulkanSwapchainManager {
public:
    VulkanSwapchainManager(VulkanContext& context, VkSurfaceKHR surface);
    ~VulkanSwapchainManager();

    void initializeSwapchain(int width, int height);
    void handleResize(int width, int height);
    void cleanupSwapchain();

private:
    VulkanContext& context_;
    VkSurfaceKHR surface_;
};

#endif // VULKAN_INIT_SWAPCHAIN_HPP