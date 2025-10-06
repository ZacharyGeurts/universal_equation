// AMOURANTH RTX Engine, October 2025 - Vulkan swapchain utilities implementation.
// Supports Windows/Linux; no mutexes; compatible with voxel geometry rendering.
// Dependencies: Vulkan 1.3+, C++20 standard library.
// Integrates with VulkanRTX for presenting ray-traced voxel scenes.
// Zachary Geurts 2025

#include "engine/Vulkan/Vulkan_func_swapchain.hpp"
#include <stdexcept>
#include <vector>
#include <algorithm>
#include <limits>
#include <array>
#include <format>
#include <syncstream>
#include <iostream>
#include <ranges> // Added for std::views::iota

// ANSI color codes
#define RESET "\033[0m"
#define MAGENTA "\033[1;35m" // Bold magenta for errors
#define CYAN "\033[1;36m"    // Bold cyan for debug
#define YELLOW "\033[1;33m"  // Bold yellow for warnings
#define GREEN "\033[1;32m"   // Bold green for info
#define BOLD "\033[1m"

namespace VulkanInitializer {

// Helper function to convert VkResult to string
std::string vkResultToString(VkResult result) {
    switch (result) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_NOT_READY: return "VK_NOT_READY";
        case VK_TIMEOUT: return "VK_TIMEOUT";
        case VK_EVENT_SET: return "VK_EVENT_SET";
        case VK_EVENT_RESET: return "VK_EVENT_RESET";
        case VK_INCOMPLETE: return "VK_INCOMPLETE";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        default: return std::format("Unknown VkResult ({})", static_cast<int>(result));
    }
}

// Formatter specialization for VkResult
template <>
struct std::formatter<VkResult, char> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(VkResult result, FormatContext& ctx) const {
        return std::format_to(ctx.out(), "{}", vkResultToString(result));
    }
};

VkSurfaceFormatKHR selectSurfaceFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Selecting surface format" << RESET << std::endl;

    uint32_t formatCount = 0;
    VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
    if (result != VK_SUCCESS || formatCount == 0) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to get surface formats: " << result << RESET << std::endl;
        throw std::runtime_error(std::format("Failed to get surface formats: {}", result));
    }

    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());
    if (result != VK_SUCCESS) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to retrieve surface formats: " << result << RESET << std::endl;
        throw std::runtime_error(std::format("Failed to retrieve surface formats: {}", result));
    }

    std::osyncstream(std::cout) << CYAN << "[DEBUG] Found " << formatCount << " surface formats" << RESET << std::endl;
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            std::osyncstream(std::cout) << CYAN << "[DEBUG] Selected format: VK_FORMAT_B8G8R8A8_UNORM, colorSpace: VK_COLOR_SPACE_SRGB_NONLINEAR_KHR" << RESET << std::endl;
            return format;
        }
    }

    std::osyncstream(std::cout) << YELLOW << "[WARNING] Preferred format VK_FORMAT_B8G8R8A8_UNORM not found, falling back to first available format" << RESET << std::endl;
    return formats[0];
}

void validateQueueFamilies(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t graphicsFamily, uint32_t presentFamily) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Validating queue families: graphicsFamily=" << graphicsFamily << ", presentFamily=" << presentFamily << RESET << std::endl;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    std::osyncstream(std::cout) << CYAN << "[DEBUG] Found " << queueFamilyCount << " queue families" << RESET << std::endl;

    bool graphicsValid = false;
    bool presentValid = false;
    for (uint32_t i : std::ranges::views::iota(0u, queueFamilyCount)) { // Fixed: Use std::ranges::views
        if (i == graphicsFamily && (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            graphicsValid = true;
            std::osyncstream(std::cout) << CYAN << "[DEBUG] Graphics queue family " << i << " is valid" << RESET << std::endl;
        }
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
        if (i == presentFamily && presentSupport) {
            presentValid = true;
            std::osyncstream(std::cout) << CYAN << "[DEBUG] Present queue family " << i << " is valid" << RESET << std::endl;
        }
    }
    if (!graphicsValid || !presentValid) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Invalid queue family indices: graphicsFamily=" << graphicsFamily << ", presentFamily=" << presentFamily << RESET << std::endl;
        throw std::runtime_error(std::format("Invalid queue family indices: graphicsFamily={}, presentFamily={}", graphicsFamily, presentFamily));
    }
    std::osyncstream(std::cout) << GREEN << "[INFO] Queue families validated successfully" << RESET << std::endl;
}

void createSwapchain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, VkSwapchainKHR& swapchain,
                     std::vector<VkImage>& swapchainImages, std::vector<VkImageView>& swapchainImageViews,
                     VkFormat& swapchainFormat, uint32_t graphicsFamily, uint32_t presentFamily, int width, int height) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Creating swapchain with width=" << width << ", height=" << height << RESET << std::endl;

    validateQueueFamilies(physicalDevice, surface, graphicsFamily, presentFamily);

    VkSurfaceCapabilitiesKHR capabilities;
    VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);
    if (result != VK_SUCCESS) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to get surface capabilities: " << result << RESET << std::endl;
        throw std::runtime_error(std::format("Failed to get surface capabilities: {}", result));
    }
    std::osyncstream(std::cout) << CYAN << "[DEBUG] Surface capabilities: minImageCount=" << capabilities.minImageCount << ", maxImageCount=" << capabilities.maxImageCount << RESET << std::endl;

    VkSurfaceFormatKHR selectedFormat = selectSurfaceFormat(physicalDevice, surface);
    swapchainFormat = selectedFormat.format;
    std::osyncstream(std::cout) << CYAN << "[DEBUG] Selected swapchain format: " << selectedFormat.format << ", colorSpace: " << selectedFormat.colorSpace << RESET << std::endl;

    VkImageFormatProperties formatProps;
    result = vkGetPhysicalDeviceImageFormatProperties(
        physicalDevice, selectedFormat.format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 0, &formatProps);
    if (result != VK_SUCCESS) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Selected swapchain format not supported: " << result << RESET << std::endl;
        throw std::runtime_error(std::format("Selected swapchain format not supported: {}", result));
    }

    uint32_t presentModeCount = 0;
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
    if (result != VK_SUCCESS || presentModeCount == 0) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to get present modes: " << result << RESET << std::endl;
        throw std::runtime_error(std::format("Failed to get present modes: {}", result));
    }

    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data());
    if (result != VK_SUCCESS) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to retrieve present modes: " << result << RESET << std::endl;
        throw std::runtime_error(std::format("Failed to retrieve present modes: {}", result));
    }

    VkPresentModeKHR selectedPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (const auto& availablePresentMode : presentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            selectedPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
            std::osyncstream(std::cout) << CYAN << "[DEBUG] Selected present mode: VK_PRESENT_MODE_MAILBOX_KHR" << RESET << std::endl;
            break;
        }
    }
    if (selectedPresentMode == VK_PRESENT_MODE_FIFO_KHR) {
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Fallback to present mode: VK_PRESENT_MODE_FIFO_KHR" << RESET << std::endl;
    }

    VkExtent2D extent;
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        extent = capabilities.currentExtent;
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Using current extent: width=" << extent.width << ", height=" << extent.height << RESET << std::endl;
    } else {
        extent = {
            std::clamp(static_cast<uint32_t>(width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
            std::clamp(static_cast<uint32_t>(height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
        };
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Clamped extent: width=" << extent.width << ", height=" << extent.height << RESET << std::endl;
    }

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
        std::osyncstream(std::cout) << YELLOW << "[WARNING] Image count clamped to maxImageCount=" << imageCount << RESET << std::endl;
    }
    std::osyncstream(std::cout) << CYAN << "[DEBUG] Swapchain image count: " << imageCount << RESET << std::endl;

    std::array<uint32_t, 2> queueFamilyIndices = {graphicsFamily, presentFamily};
    VkSwapchainCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr, // Explicitly initialize
        .flags = 0,       // Explicitly initialize
        .surface = surface,
        .minImageCount = imageCount,
        .imageFormat = selectedFormat.format,
        .imageColorSpace = selectedFormat.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = (graphicsFamily != presentFamily) ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = (graphicsFamily != presentFamily) ? 2u : 0u,
        .pQueueFamilyIndices = (graphicsFamily != presentFamily) ? queueFamilyIndices.data() : nullptr,
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = selectedPresentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE
    };

    result = vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain);
    if (result != VK_SUCCESS) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to create swapchain: " << result << RESET << std::endl;
        throw std::runtime_error(std::format("Failed to create swapchain: {}", result));
    }

    result = vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
    if (result != VK_SUCCESS) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to get swapchain image count: " << result << RESET << std::endl;
        throw std::runtime_error(std::format("Failed to get swapchain image count: {}", result));
    }
    swapchainImages.resize(imageCount);
    result = vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());
    if (result != VK_SUCCESS) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to retrieve swapchain images: " << result << RESET << std::endl;
        throw std::runtime_error(std::format("Failed to retrieve swapchain images: {}", result));
    }
    std::osyncstream(std::cout) << CYAN << "[DEBUG] Retrieved " << imageCount << " swapchain images" << RESET << std::endl;

    swapchainImageViews.resize(imageCount);
    for (size_t i : std::ranges::views::iota(0u, swapchainImages.size())) { // Fixed: Use std::ranges::views
        VkImageViewCreateInfo viewInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr, // Explicitly initialize
            .flags = 0,       // Explicitly initialize
            .image = swapchainImages[i],
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

        result = vkCreateImageView(device, &viewInfo, nullptr, &swapchainImageViews[i]);
        if (result != VK_SUCCESS) {
            std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to create swapchain image view {}: {}" << i << result << RESET << std::endl;
            throw std::runtime_error(std::format("Failed to create swapchain image view {}: {}", i, result));
        }
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Created image view for swapchain image " << i << RESET << std::endl;
    }
    std::osyncstream(std::cout) << GREEN << "[INFO] Swapchain created successfully with " << imageCount << " images" << RESET << std::endl;
}

void createFramebuffers(VkDevice device, VkRenderPass renderPass, std::vector<VkImageView>& swapchainImageViews,
                        std::vector<VkFramebuffer>& swapchainFramebuffers, int width, int height) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Creating framebuffers for " << swapchainImageViews.size() << " image views, width=" << width << ", height=" << height << RESET << std::endl;

    swapchainFramebuffers.resize(swapchainImageViews.size());
    for (size_t i : std::ranges::views::iota(0u, swapchainImageViews.size())) { // Fixed: Use std::ranges::views
        VkImageView attachments[] = {swapchainImageViews[i]};
        VkFramebufferCreateInfo framebufferInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = nullptr, // Explicitly initialize
            .flags = 0,       // Explicitly initialize
            .renderPass = renderPass,
            .attachmentCount = 1,
            .pAttachments = attachments,
            .width = static_cast<uint32_t>(width),
            .height = static_cast<uint32_t>(height),
            .layers = 1
        };

        VkResult result = vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapchainFramebuffers[i]);
        if (result != VK_SUCCESS) {
            std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to create framebuffer {}: {}" << i << result << RESET << std::endl;
            throw std::runtime_error(std::format("Failed to create framebuffer {}: {}", i, result));
        }
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Created framebuffer " << i << RESET << std::endl;
    }
    std::osyncstream(std::cout) << GREEN << "[INFO] Framebuffers created successfully" << RESET << std::endl;
}

} // namespace VulkanInitializer