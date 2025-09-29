#ifndef VULKAN_FUNC_SWAPCHAIN_HPP
#define VULKAN_FUNC_SWAPCHAIN_HPP
// AMOURANTH RTX September 2025
// Zachary Geurts 2025
#include <vulkan/vulkan.h>
#include <vector>

namespace VulkanInitializer {

    // Select a suitable surface format for the swapchain
    VkSurfaceFormatKHR selectSurfaceFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);

    // Create a swapchain and its associated images/views
    void createSwapchain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, VkSwapchainKHR& swapchain,
                         std::vector<VkImage>& swapchainImages, std::vector<VkImageView>& swapchainImageViews,
                         VkFormat& swapchainFormat, uint32_t graphicsFamily, uint32_t presentFamily, int width, int height);

    // Create framebuffers for the swapchain images
    void createFramebuffers(VkDevice device, VkRenderPass renderPass, std::vector<VkImageView>& swapchainImageViews,
                            std::vector<VkFramebuffer>& swapchainFramebuffers, int width, int height);

}

#endif // VULKAN_FUNC_SWAPCHAIN_HPP
