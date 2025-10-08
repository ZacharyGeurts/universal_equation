// AMOURANTH RTX Engine, October 2025 - Vulkan swapchain initialization.
// Manages swapchain creation and resizing.
// Dependencies: Vulkan 1.3+, C++20 standard library.
// Zachary Geurts 2025

#include "engine/Vulkan_init.hpp"
#include <stdexcept>
#include <algorithm>
#include <format>
#include <vector>
#include <cstdint>
#include <source_location>

VulkanSwapchainManager::VulkanSwapchainManager(VulkanContext& context, VkSurfaceKHR surface)
    : context_(context), surface_(surface), logger_() {
    logger_.log(Logging::LogLevel::Info, "Constructing VulkanSwapchainManager with surface={:p}",
                std::source_location::current(), static_cast<void*>(surface));
}

void VulkanSwapchainManager::initializeSwapchain(int width, int height) {
    logger_.log(Logging::LogLevel::Info, "Initializing swapchain with width={}, height={}",
                std::source_location::current(), width, height);

    VkSurfaceCapabilitiesKHR capabilities;
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context_.physicalDevice, surface_, &capabilities) != VK_SUCCESS) {
        logger_.log(Logging::LogLevel::Error, "Failed to get surface capabilities",
                    std::source_location::current());
        throw std::runtime_error("Failed to get surface capabilities");
    }

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(context_.physicalDevice, surface_, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(context_.physicalDevice, surface_, &formatCount, formats.data());
    VkSurfaceFormatKHR selectedFormat = formats[0];
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            selectedFormat = format;
            break;
        }
    }

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(context_.physicalDevice, surface_, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(context_.physicalDevice, surface_, &presentModeCount, presentModes.data());
    VkPresentModeKHR selectedPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (const auto& mode : presentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            selectedPresentMode = mode;
            break;
        }
    }

    VkExtent2D extent;
    if (capabilities.currentExtent.width != UINT32_MAX) {
        extent = capabilities.currentExtent;
    } else {
        extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
        extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }
    context_.swapchainExtent = extent;

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .surface = surface_,
        .minImageCount = imageCount,
        .imageFormat = selectedFormat.format,
        .imageColorSpace = selectedFormat.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = selectedPresentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE
    };

    if (vkCreateSwapchainKHR(context_.device, &createInfo, nullptr, &context_.swapchain) != VK_SUCCESS) {
        logger_.log(Logging::LogLevel::Error, "Failed to create swapchain",
                    std::source_location::current());
        throw std::runtime_error("Failed to create swapchain");
    }

    uint32_t swapchainImageCount = 0;
    vkGetSwapchainImagesKHR(context_.device, context_.swapchain, &swapchainImageCount, nullptr);
    context_.swapchainImages.resize(swapchainImageCount);
    vkGetSwapchainImagesKHR(context_.device, context_.swapchain, &swapchainImageCount, context_.swapchainImages.data());

    context_.swapchainImageViews.resize(context_.swapchainImages.size());
    for (size_t i = 0; i < context_.swapchainImages.size(); ++i) {
        VkImageViewCreateInfo viewInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = context_.swapchainImages[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = selectedFormat.format,
            .components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };
        if (vkCreateImageView(context_.device, &viewInfo, nullptr, &context_.swapchainImageViews[i]) != VK_SUCCESS) {
            logger_.log(Logging::LogLevel::Error, "Failed to create image view for swapchain image {}",
                        std::source_location::current(), i);
            throw std::runtime_error("Failed to create image view");
        }
    }

    logger_.log(Logging::LogLevel::Info, "Swapchain initialized with {} images, extent={{ {}, {} }}",
                std::source_location::current(), context_.swapchainImages.size(), extent.width, extent.height);
}

void VulkanSwapchainManager::handleResize(int width, int height) {
    logger_.log(Logging::LogLevel::Info, "Handling swapchain resize to width={}, height={}",
                std::source_location::current(), width, height);
    cleanupSwapchain();
    initializeSwapchain(width, height);
}

void VulkanSwapchainManager::cleanupSwapchain() {
    logger_.log(Logging::LogLevel::Info, "Cleaning up swapchain",
                std::source_location::current());

    for (auto imageView : context_.swapchainImageViews) {
        if (imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(context_.device, imageView, nullptr);
        }
    }
    context_.swapchainImageViews.clear();

    if (context_.swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(context_.device, context_.swapchain, nullptr);
        context_.swapchain = VK_NULL_HANDLE;
    }
    logger_.log(Logging::LogLevel::Debug, "Swapchain cleaned up",
                std::source_location::current());
}