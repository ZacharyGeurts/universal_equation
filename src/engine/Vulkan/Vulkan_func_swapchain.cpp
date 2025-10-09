// Vulkan_func_swapchain.cpp
// AMOURANTH RTX Engine, October 2025 - Vulkan swapchain utilities implementation.
// Supports Windows/Linux; no mutexes; voxel geometry rendering.
// Dependencies: Vulkan 1.3+, C++20 standard library.
// Zachary Geurts 2025

#include "engine/Vulkan/Vulkan_func_swapchain.hpp"
#include "engine/logging.hpp"
#include <stdexcept>
#include <vector>

// Define VK_CHECK for consistent error handling
#ifndef VK_CHECK
#define VK_CHECK(result, msg) do { if ((result) != VK_SUCCESS) { LOG_ERROR_CAT("Vulkan", "{} (VkResult: {})", std::source_location::current(), (msg), static_cast<int>(result)); throw std::runtime_error((msg)); } } while (0)
#endif

namespace VulkanInitializer {

VkSurfaceFormatKHR selectSurfaceFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, const Logging::Logger& logger) {
    LOG_INFO_CAT("Vulkan", "Selecting surface format", std::source_location::current());

    uint32_t formatCount;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr), "Failed to get surface format count");
    if (formatCount == 0) {
        LOG_ERROR_CAT("Vulkan", "No surface formats available", std::source_location::current());
        throw std::runtime_error("No surface formats available");
    }

    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data()), "Failed to get surface formats");

    VkSurfaceFormatKHR selectedFormat = formats[0];
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            selectedFormat = format;
            break;
        }
    }

    LOG_INFO_CAT("Vulkan", "Selected surface format: {}", std::source_location::current(), static_cast<int>(selectedFormat.format));
    return selectedFormat;
}

void createSwapchain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, VkSwapchainKHR& swapchain,
                     std::vector<VkImage>& swapchainImages, std::vector<VkImageView>& swapchainImageViews,
                     VkFormat& swapchainFormat, uint32_t graphicsFamily, uint32_t presentFamily, int width, int height,
                     const Logging::Logger& logger) {
    LOG_INFO_CAT("Vulkan", "Creating swapchain with width={}, height={}", std::source_location::current(), width, height);

    VkSurfaceFormatKHR surfaceFormat = selectSurfaceFormat(physicalDevice, surface, logger);
    swapchainFormat = surfaceFormat.format;

    VkSurfaceCapabilitiesKHR capabilities;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities), "Failed to get surface capabilities");

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    std::vector<uint32_t> queueFamilyIndices;
    if (graphicsFamily != presentFamily) {
        sharingMode = VK_SHARING_MODE_CONCURRENT;
        queueFamilyIndices = {graphicsFamily, presentFamily};
    }

    VkSwapchainCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .surface = surface,
        .minImageCount = imageCount,
        .imageFormat = swapchainFormat,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)},
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = sharingMode,
        .queueFamilyIndexCount = static_cast<uint32_t>(queueFamilyIndices.size()),
        .pQueueFamilyIndices = queueFamilyIndices.empty() ? nullptr : queueFamilyIndices.data(),
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE
    };

    VK_CHECK(vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain), "Failed to create swapchain");

    uint32_t swapchainImageCount;
    VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr), "Failed to get swapchain image count");
    swapchainImages.resize(swapchainImageCount);
    VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data()), "Failed to get swapchain images");

    swapchainImageViews.resize(swapchainImageCount);
    for (size_t i = 0; i < swapchainImageCount; ++i) {
        VkImageViewCreateInfo viewInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = swapchainImages[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = swapchainFormat,
            .components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };

        VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &swapchainImageViews[i]), "Failed to create image view for swapchain image " + std::to_string(i));
    }

    LOG_INFO_CAT("Vulkan", "Swapchain created successfully with format: {}", std::source_location::current(), static_cast<int>(swapchainFormat));
}

void createFramebuffers(VkDevice device, VkRenderPass renderPass, std::vector<VkImageView>& swapchainImageViews,
                        std::vector<VkFramebuffer>& swapchainFramebuffers, int width, int height,
                        const Logging::Logger& logger) {
    LOG_INFO_CAT("Vulkan", "Creating framebuffers", std::source_location::current());

    swapchainFramebuffers.resize(swapchainImageViews.size());
    for (size_t i = 0; i < swapchainImageViews.size(); ++i) {
        VkFramebufferCreateInfo createInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderPass = renderPass,
            .attachmentCount = 1,
            .pAttachments = &swapchainImageViews[i],
            .width = static_cast<uint32_t>(width),
            .height = static_cast<uint32_t>(height),
            .layers = 1
        };

        VK_CHECK(vkCreateFramebuffer(device, &createInfo, nullptr, &swapchainFramebuffers[i]), "Failed to create framebuffer " + std::to_string(i));
    }

    LOG_INFO_CAT("Vulkan", "Framebuffers created successfully", std::source_location::current());
}

} // namespace VulkanInitializer