// AMOURANTH RTX Engine, October 2025 - Vulkan swapchain utilities implementation.
// Supports Windows/Linux; no mutexes; voxel geometry rendering.
// Dependencies: Vulkan 1.3+, C++20 standard library.
// Zachary Geurts 2025

#include "engine/Vulkan/Vulkan_func_swapchain.hpp"
#include "engine/logging.hpp"
#include <stdexcept>
#include <format>
#include <vector>

namespace VulkanInitializer {

std::string vkResultToString(VkResult result) {
    switch (result) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
        default: return std::to_string(static_cast<int>(result));
    }
}

std::string vkFormatToString(VkFormat format) {
    switch (format) {
        case VK_FORMAT_UNDEFINED: return "VK_FORMAT_UNDEFINED";
        case VK_FORMAT_R8G8B8A8_SRGB: return "VK_FORMAT_R8G8B8A8_SRGB";
        case VK_FORMAT_R8G8B8A8_UNORM: return "VK_FORMAT_R8G8B8A8_UNORM";
        case VK_FORMAT_B8G8R8A8_SRGB: return "VK_FORMAT_B8G8R8A8_SRGB";
        case VK_FORMAT_B8G8R8A8_UNORM: return "VK_FORMAT_B8G8R8A8_UNORM";
        case VK_FORMAT_R32G32B32_SFLOAT: return "VK_FORMAT_R32G32B32_SFLOAT";
        case VK_FORMAT_R32G32B32A32_SFLOAT: return "VK_FORMAT_R32G32B32A32_SFLOAT";
        case VK_FORMAT_D32_SFLOAT: return "VK_FORMAT_D32_SFLOAT";
        case VK_FORMAT_S8_UINT: return "VK_FORMAT_S8_UINT";
        case VK_FORMAT_D24_UNORM_S8_UINT: return "VK_FORMAT_D24_UNORM_S8_UINT";
        default: return std::to_string(static_cast<int>(format));
    }
}

VkSurfaceFormatKHR selectSurfaceFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, const Logging::Logger& logger) {
    logger.log(Logging::LogLevel::Info, "Selecting surface format", std::source_location::current());

    uint32_t formatCount;
    VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
    if (result != VK_SUCCESS) {
        logger.log(Logging::LogLevel::Error, "Failed to get surface format count: {}", std::source_location::current(), vkResultToString(result));
        throw std::runtime_error(std::format("Failed to get surface format count: {}", vkResultToString(result)));
    }
    if (formatCount == 0) {
        logger.log(Logging::LogLevel::Error, "No surface formats available", std::source_location::current());
        throw std::runtime_error("No surface formats available");
    }

    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());
    if (result != VK_SUCCESS) {
        logger.log(Logging::LogLevel::Error, "Failed to get surface formats: {}", std::source_location::current(), vkResultToString(result));
        throw std::runtime_error(std::format("Failed to get surface formats: {}", vkResultToString(result)));
    }

    VkSurfaceFormatKHR selectedFormat = formats[0];
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            selectedFormat = format;
            break;
        }
    }

    logger.log(Logging::LogLevel::Info, "Selected surface format: {}", std::source_location::current(), vkFormatToString(selectedFormat.format));
    return selectedFormat;
}

void createSwapchain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, VkSwapchainKHR& swapchain,
                     std::vector<VkImage>& swapchainImages, std::vector<VkImageView>& swapchainImageViews,
                     VkFormat& swapchainFormat, uint32_t graphicsFamily, uint32_t presentFamily, int width, int height,
                     const Logging::Logger& logger) {
    logger.log(Logging::LogLevel::Info, "Creating swapchain with width={}, height={}", std::source_location::current(), width, height);

    VkSurfaceFormatKHR surfaceFormat = selectSurfaceFormat(physicalDevice, surface, logger);
    swapchainFormat = surfaceFormat.format;

    VkSurfaceCapabilitiesKHR capabilities;
    VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);
    if (result != VK_SUCCESS) {
        logger.log(Logging::LogLevel::Error, "Failed to get surface capabilities: {}", std::source_location::current(), vkResultToString(result));
        throw std::runtime_error(std::format("Failed to get surface capabilities: {}", vkResultToString(result)));
    }

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

    result = vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain);
    if (result != VK_SUCCESS) {
        logger.log(Logging::LogLevel::Error, "Failed to create swapchain: {}", std::source_location::current(), vkResultToString(result));
        throw std::runtime_error(std::format("Failed to create swapchain: {}", vkResultToString(result)));
    }

    uint32_t swapchainImageCount;
    result = vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr);
    if (result != VK_SUCCESS) {
        logger.log(Logging::LogLevel::Error, "Failed to get swapchain image count: {}", std::source_location::current(), vkResultToString(result));
        throw std::runtime_error(std::format("Failed to get swapchain image count: {}", vkResultToString(result)));
    }
    swapchainImages.resize(swapchainImageCount);
    result = vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data());
    if (result != VK_SUCCESS) {
        logger.log(Logging::LogLevel::Error, "Failed to get swapchain images: {}", std::source_location::current(), vkResultToString(result));
        throw std::runtime_error(std::format("Failed to get swapchain images: {}", vkResultToString(result)));
    }

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

        result = vkCreateImageView(device, &viewInfo, nullptr, &swapchainImageViews[i]);
        if (result != VK_SUCCESS) {
            logger.log(Logging::LogLevel::Error, "Failed to create image view for swapchain image {}: {}", std::source_location::current(), i, vkResultToString(result));
            throw std::runtime_error(std::format("Failed to create image view for swapchain image {}: {}", i, vkResultToString(result)));
        }
    }

    logger.log(Logging::LogLevel::Info, "Swapchain created successfully with format: {}", std::source_location::current(), vkFormatToString(swapchainFormat));
}

void createFramebuffers(VkDevice device, VkRenderPass renderPass, std::vector<VkImageView>& swapchainImageViews,
                        std::vector<VkFramebuffer>& swapchainFramebuffers, int width, int height,
                        const Logging::Logger& logger) {
    logger.log(Logging::LogLevel::Info, "Creating framebuffers", std::source_location::current());

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

        VkResult result = vkCreateFramebuffer(device, &createInfo, nullptr, &swapchainFramebuffers[i]);
        if (result != VK_SUCCESS) {
            logger.log(Logging::LogLevel::Error, "Failed to create framebuffer {}: {}", std::source_location::current(), i, vkResultToString(result));
            throw std::runtime_error(std::format("Failed to create framebuffer {}: {}", i, vkResultToString(result)));
        }
    }

    logger.log(Logging::LogLevel::Info, "Framebuffers created successfully", std::source_location::current());
}

} // namespace VulkanInitializer