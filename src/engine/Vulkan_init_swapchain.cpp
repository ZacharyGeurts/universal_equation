// AMOURANTH RTX Engine, October 2025 - Vulkan swapchain management.
// Manages swapchain creation, image views, and resize handling.
// Dependencies: Vulkan 1.3+, GLM, C++20 standard library.
// Zachary Geurts 2025

#include "engine/Vulkan_init_swapchain.hpp"
#include "engine/Vulkan_init.hpp" // Added for VulkanContext definition
#include "engine/logging.hpp"
#include <stdexcept>
#include <format>
#include <algorithm>

VulkanSwapchainManager::VulkanSwapchainManager(VulkanContext& context, VkSurfaceKHR surface)
    : context_(context), surface_(surface) {
    Logging::Logger logger;
    logger.log(Logging::LogLevel::Debug, "VulkanSwapchainManager constructed with context={:p}, surface={:p}", 
               std::source_location::current(), static_cast<void*>(&context_), static_cast<void*>(surface));
}

VulkanSwapchainManager::~VulkanSwapchainManager() {
    Logging::Logger logger;
    logger.log(Logging::LogLevel::Debug, "VulkanSwapchainManager destructor called", std::source_location::current());
    cleanupSwapchain();
    logger.log(Logging::LogLevel::Debug, "VulkanSwapchainManager destroyed", std::source_location::current());
}

void VulkanSwapchainManager::initializeSwapchain(int width, int height) {
    Logging::Logger logger;
    logger.log(Logging::LogLevel::Debug, "Creating swapchain with width={}, height={}", std::source_location::current(), width, height);

    // Query surface capabilities
    VkSurfaceCapabilitiesKHR capabilities;
    VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context_.physicalDevice, surface_, &capabilities);
    if (result != VK_SUCCESS) {
        logger.log(Logging::LogLevel::Error, "Failed to get surface capabilities: VkResult={}", std::source_location::current(), static_cast<int>(result));
        throw std::runtime_error(std::format("Failed to get surface capabilities: VkResult={}", static_cast<int>(result)));
    }

    // Validate image count
    uint32_t minImageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && minImageCount > capabilities.maxImageCount) {
        minImageCount = capabilities.maxImageCount;
    }
    logger.log(Logging::LogLevel::Debug, "Surface capabilities: minImageCount={}, maxImageCount={}", 
               std::source_location::current(), capabilities.minImageCount, capabilities.maxImageCount);

    // Validate surface extent
    VkExtent2D extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    if (capabilities.currentExtent.width != UINT32_MAX) {
        extent = capabilities.currentExtent;
    } else {
        extent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, extent.width));
        extent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, extent.height));
    }
    logger.log(Logging::LogLevel::Debug, "Swapchain extent: width={}, height={}", std::source_location::current(), extent.width, extent.height);

    // Query supported formats
    uint32_t formatCount;
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(context_.physicalDevice, surface_, &formatCount, nullptr);
    if (result != VK_SUCCESS || formatCount == 0) {
        logger.log(Logging::LogLevel::Error, "No surface formats available: VkResult={}", std::source_location::current(), static_cast<int>(result));
        throw std::runtime_error("No surface formats available");
    }
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(context_.physicalDevice, surface_, &formatCount, formats.data());
    VkSurfaceFormatKHR selectedFormat = formats[0];
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            selectedFormat = format;
            break;
        }
    }
    logger.log(Logging::LogLevel::Debug, "Selected surface format: format={}, colorSpace={}", 
               std::source_location::current(), static_cast<int>(selectedFormat.format), static_cast<int>(selectedFormat.colorSpace));

    // Query supported present modes
    uint32_t presentModeCount;
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(context_.physicalDevice, surface_, &presentModeCount, nullptr);
    if (result != VK_SUCCESS || presentModeCount == 0) {
        logger.log(Logging::LogLevel::Error, "No present modes available: VkResult={}", std::source_location::current(), static_cast<int>(result));
        throw std::runtime_error("No present modes available");
    }
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(context_.physicalDevice, surface_, &presentModeCount, presentModes.data());
    VkPresentModeKHR selectedPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (const auto& mode : presentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            selectedPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
    }
    logger.log(Logging::LogLevel::Debug, "Selected present mode: {}", std::source_location::current(), static_cast<int>(selectedPresentMode));

    // Create swapchain
    std::vector<uint32_t> uniqueFamilies = {context_.graphicsFamily};
    if (context_.graphicsFamily != context_.presentFamily) {
        uniqueFamilies.push_back(context_.presentFamily);
    }
    VkSwapchainCreateInfoKHR swapchainCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .surface = surface_,
        .minImageCount = minImageCount,
        .imageFormat = selectedFormat.format,
        .imageColorSpace = selectedFormat.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = context_.graphicsFamily == context_.presentFamily ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT,
        .queueFamilyIndexCount = static_cast<uint32_t>(context_.graphicsFamily == context_.presentFamily ? 0 : uniqueFamilies.size()),
        .pQueueFamilyIndices = context_.graphicsFamily == context_.presentFamily ? nullptr : uniqueFamilies.data(),
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = selectedPresentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };
    result = vkCreateSwapchainKHR(context_.device, &swapchainCreateInfo, nullptr, &context_.swapchain);
    if (result != VK_SUCCESS) {
        logger.log(Logging::LogLevel::Error, "Failed to create swapchain: VkResult={}", std::source_location::current(), static_cast<int>(result));
        throw std::runtime_error(std::format("Failed to create swapchain: VkResult={}", static_cast<int>(result)));
    }
    context_.swapchainExtent = extent;
    logger.log(Logging::LogLevel::Info, "Swapchain created with {} images", std::source_location::current(), minImageCount);

    // Get swapchain images
    uint32_t imageCount = 0;
    result = vkGetSwapchainImagesKHR(context_.device, context_.swapchain, &imageCount, nullptr);
    if (result != VK_SUCCESS) {
        logger.log(Logging::LogLevel::Error, "Failed to get swapchain image count: VkResult={}", std::source_location::current(), static_cast<int>(result));
        throw std::runtime_error("Failed to get swapchain image count");
    }
    context_.swapchainImages.resize(imageCount);
    result = vkGetSwapchainImagesKHR(context_.device, context_.swapchain, &imageCount, context_.swapchainImages.data());
    if (result != VK_SUCCESS) {
        logger.log(Logging::LogLevel::Error, "Failed to retrieve swapchain images: VkResult={}", std::source_location::current(), static_cast<int>(result));
        throw std::runtime_error("Failed to retrieve swapchain images");
    }
    logger.log(Logging::LogLevel::Info, "Retrieved {} swapchain images", std::source_location::current(), imageCount);

    // Create image views
    context_.swapchainImageViews.resize(imageCount);
    for (size_t i = 0; i < imageCount; ++i) {
        VkImageViewCreateInfo viewInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = context_.swapchainImages[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = selectedFormat.format,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
        result = vkCreateImageView(context_.device, &viewInfo, nullptr, &context_.swapchainImageViews[i]);
        if (result != VK_SUCCESS) {
            logger.log(Logging::LogLevel::Error, "Failed to create image view {}: VkResult={}", std::source_location::current(), i, static_cast<int>(result));
            throw std::runtime_error(std::format("Failed to create image view {}", i));
        }
    }
    logger.log(Logging::LogLevel::Info, "Created {} image views", std::source_location::current(), imageCount);
}

void VulkanSwapchainManager::handleResize(int width, int height) {
    Logging::Logger logger;
    logger.log(Logging::LogLevel::Info, "Handling resize to {}x{}", std::source_location::current(), width, height);

    vkDeviceWaitIdle(context_.device);
    cleanupSwapchain();
    initializeSwapchain(width, height);
}

void VulkanSwapchainManager::cleanupSwapchain() {
    Logging::Logger logger;
    logger.log(Logging::LogLevel::Debug, "Cleaning up swapchain resources", std::source_location::current());

    // Destroy framebuffers
    for (auto& framebuffer : context_.swapchainFramebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(context_.device, framebuffer, nullptr);
            framebuffer = VK_NULL_HANDLE;
        }
    }
    context_.swapchainFramebuffers.clear();
    logger.log(Logging::LogLevel::Debug, "Framebuffers destroyed", std::source_location::current());

    // Destroy image views
    for (auto& imageView : context_.swapchainImageViews) {
        if (imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(context_.device, imageView, nullptr);
            imageView = VK_NULL_HANDLE;
        }
    }
    context_.swapchainImageViews.clear();
    logger.log(Logging::LogLevel::Debug, "Image views destroyed", std::source_location::current());

    // Destroy swapchain
    if (context_.swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(context_.device, context_.swapchain, nullptr);
        context_.swapchain = VK_NULL_HANDLE;
    }
    context_.swapchainImages.clear();
    logger.log(Logging::LogLevel::Info, "Swapchain resources cleaned up", std::source_location::current());
}