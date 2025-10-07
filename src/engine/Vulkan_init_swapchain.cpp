// AMOURANTH RTX Engine, October 2025 - Vulkan swapchain management.
// Manages swapchain creation, image views, and resize handling.
// Dependencies: Vulkan 1.3+, GLM, C++20 standard library.
// Zachary Geurts 2025

#include "engine/Vulkan_init_swapchain.hpp"
#include "engine/logging.hpp"
#include <stdexcept>
#include <format>

VulkanSwapchainManager::VulkanSwapchainManager(VulkanContext& context, VkSurfaceKHR surface)
    : context_(context), surface_(surface) {}

void VulkanSwapchainManager::initializeSwapchain(int width, int height) {
    Logging::Logger logger;
    logger.log(Logging::LogLevel::Debug, "Creating swapchain", std::source_location::current());

    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context_.physicalDevice, surface_, &capabilities);
    std::vector<uint32_t> uniqueFamilies = {context_.graphicsFamily};
    if (context_.graphicsFamily != context_.presentFamily) {
        uniqueFamilies.push_back(context_.presentFamily);
    }
    VkSwapchainCreateInfoKHR swapchainCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .surface = surface_,
        .minImageCount = capabilities.minImageCount + 1,
        .imageFormat = VK_FORMAT_B8G8R8A8_SRGB,
        .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)},
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = context_.graphicsFamily == context_.presentFamily ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT,
        .queueFamilyIndexCount = static_cast<uint32_t>(context_.graphicsFamily == context_.presentFamily ? 0 : 2),
        .pQueueFamilyIndices = context_.graphicsFamily == context_.presentFamily ? nullptr : uniqueFamilies.data(),
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };
    if (vkCreateSwapchainKHR(context_.device, &swapchainCreateInfo, nullptr, &context_.swapchain) != VK_SUCCESS) {
        std::string error = "Failed to create swapchain";
        logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }
    context_.swapchainExtent = swapchainCreateInfo.imageExtent; // Store swapchain extent
    logger.log(Logging::LogLevel::Info, "Swapchain created", std::source_location::current());

    // Get swapchain images
    uint32_t imageCount = 0;
    vkGetSwapchainImagesKHR(context_.device, context_.swapchain, &imageCount, nullptr);
    context_.swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(context_.device, context_.swapchain, &imageCount, context_.swapchainImages.data());
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
            .format = VK_FORMAT_B8G8R8A8_SRGB,
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
        if (vkCreateImageView(context_.device, &viewInfo, nullptr, &context_.swapchainImageViews[i]) != VK_SUCCESS) {
            std::string error = "Failed to create image view";
            logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
            throw std::runtime_error(error);
        }
    }
    logger.log(Logging::LogLevel::Info, "Created {} image views", std::source_location::current(), imageCount);
}

void VulkanSwapchainManager::handleResize(int width, int height) {
    Logging::Logger logger;
    logger.log(Logging::LogLevel::Info, "Handling resize to {}x{}", std::source_location::current(), width, height);

    vkDeviceWaitIdle(context_.device);

    // Cleanup old resources
    for (auto framebuffer : context_.swapchainFramebuffers) {
        vkDestroyFramebuffer(context_.device, framebuffer, nullptr);
    }
    for (auto imageView : context_.swapchainImageViews) {
        vkDestroyImageView(context_.device, imageView, nullptr);
    }
    vkDestroySwapchainKHR(context_.device, context_.swapchain, nullptr);
    logger.log(Logging::LogLevel::Debug, "Old swapchain resources cleaned up", std::source_location::current());

    // Recreate swapchain
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context_.physicalDevice, surface_, &capabilities);
    std::vector<uint32_t> uniqueFamilies = {context_.graphicsFamily};
    if (context_.graphicsFamily != context_.presentFamily) {
        uniqueFamilies.push_back(context_.presentFamily);
    }
    VkSwapchainCreateInfoKHR swapchainCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .surface = surface_,
        .minImageCount = capabilities.minImageCount + 1,
        .imageFormat = VK_FORMAT_B8G8R8A8_SRGB,
        .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)},
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = context_.graphicsFamily == context_.presentFamily ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT,
        .queueFamilyIndexCount = static_cast<uint32_t>(context_.graphicsFamily == context_.presentFamily ? 0 : 2),
        .pQueueFamilyIndices = context_.graphicsFamily == context_.presentFamily ? nullptr : uniqueFamilies.data(),
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };
    if (vkCreateSwapchainKHR(context_.device, &swapchainCreateInfo, nullptr, &context_.swapchain) != VK_SUCCESS) {
        std::string error = "Failed to recreate swapchain";
        logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }
    context_.swapchainExtent = swapchainCreateInfo.imageExtent; // Update swapchain extent
    logger.log(Logging::LogLevel::Info, "Swapchain recreated", std::source_location::current());

    // Recreate image views
    uint32_t imageCount = 0;
    vkGetSwapchainImagesKHR(context_.device, context_.swapchain, &imageCount, nullptr);
    context_.swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(context_.device, context_.swapchain, &imageCount, context_.swapchainImages.data());
    context_.swapchainImageViews.resize(imageCount);
    for (size_t i = 0; i < imageCount; ++i) {
        VkImageViewCreateInfo viewInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = context_.swapchainImages[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = VK_FORMAT_B8G8R8A8_SRGB,
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
        if (vkCreateImageView(context_.device, &viewInfo, nullptr, &context_.swapchainImageViews[i]) != VK_SUCCESS) {
            std::string error = "Failed to recreate image view";
            logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
            throw std::runtime_error(error);
        }
    }
    logger.log(Logging::LogLevel::Info, "Recreated {} image views", std::source_location::current(), imageCount);

    // Recreate framebuffers
    context_.swapchainFramebuffers.resize(imageCount);
    for (size_t i = 0; i < imageCount; ++i) {
        VkImageView attachments[] = {context_.swapchainImageViews[i]};
        VkFramebufferCreateInfo framebufferInfo = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderPass = context_.renderPass,
            .attachmentCount = 1,
            .pAttachments = attachments,
            .width = static_cast<uint32_t>(width),
            .height = static_cast<uint32_t>(height),
            .layers = 1,
        };
        if (vkCreateFramebuffer(context_.device, &framebufferInfo, nullptr, &context_.swapchainFramebuffers[i]) != VK_SUCCESS) {
            std::string error = "Failed to recreate framebuffer";
            logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
            throw std::runtime_error(error);
        }
    }
    logger.log(Logging::LogLevel::Info, "Recreated {} framebuffers", std::source_location::current(), imageCount);
}