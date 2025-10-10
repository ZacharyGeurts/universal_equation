// include/engine/Vulkan/Vulkan_func_swapchain.hpp
#pragma once
#ifndef VULKAN_FUNC_SWAPCHAIN_HPP
#define VULKAN_FUNC_SWAPCHAIN_HPP

#include "engine/Vulkan_types.hpp"
#include <vulkan/vulkan.h>
#include <vector>

class VulkanSwapchainManager {
public:
    VulkanSwapchainManager(VulkanContext& context, VkSurfaceKHR surface);
    ~VulkanSwapchainManager();
    void initializeSwapchain(int width, int height);
    void cleanupSwapchain();
    void handleResize(int width, int height); // Added

private:
    VulkanContext& context_;
    VkSwapchainKHR swapchain_;
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    VkFormat swapchainImageFormat_;
    VkExtent2D swapchainExtent_;
    uint32_t imageCount_;
};

#endif // VULKAN_FUNC_SWAPCHAIN_HPP