// src/engine/Vulkan/Vulkan_func_swapchain.cpp
#include "engine/Vulkan/Vulkan_func_swapchain.hpp"
#include "engine/Vulkan_init_pipeline.hpp" // Added for vkFormatToString
#include <stdexcept>
#include "engine/logging.hpp"
#include <source_location>

VulkanSwapchainManager::VulkanSwapchainManager(VulkanContext& context, VkSurfaceKHR surface)
    : context_(context), surface_(surface) {
    LOG_INFO_CAT("VulkanSwapchainManager", "Constructing VulkanSwapchainManager with context.device={:p}, surface={:p}",
                 std::source_location::current(), context_.device, surface_);
}

VulkanSwapchainManager::~VulkanSwapchainManager() {
    LOG_DEBUG_CAT("VulkanSwapchainManager", "Destroying VulkanSwapchainManager", std::source_location::current());
    cleanupSwapchain();
}

void VulkanSwapchainManager::initializeSwapchain(int width, int height) {
    LOG_DEBUG_CAT("VulkanSwapchainManager", "Initializing swapchain with width={}, height={}",
                  std::source_location::current(), width, height);

    VkSurfaceCapabilitiesKHR surfaceCaps;
    VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context_.physicalDevice, surface_, &surfaceCaps);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanSwapchainManager", "Failed to get surface capabilities: result={}",
                      std::source_location::current(), result);
        throw std::runtime_error("Failed to get surface capabilities");
    }
    LOG_DEBUG_CAT("VulkanSwapchainManager", "Surface capabilities: minImageCount={}, maxImageCount={}",
                  std::source_location::current(), surfaceCaps.minImageCount, surfaceCaps.maxImageCount);

    uint32_t imageCount = surfaceCaps.minImageCount;
    if (surfaceCaps.maxImageCount > 0 && imageCount > surfaceCaps.maxImageCount) {
        imageCount = surfaceCaps.maxImageCount;
    }
    LOG_DEBUG_CAT("VulkanSwapchainManager", "Selected imageCount={}", std::source_location::current(), imageCount);

    VkSwapchainCreateInfoKHR createInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .surface = surface_,
        .minImageCount = imageCount,
        .imageFormat = VK_FORMAT_B8G8R8A8_SRGB,
        .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)},
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .preTransform = surfaceCaps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE
    };
    LOG_DEBUG_CAT("VulkanSwapchainManager", "Creating swapchain with format={}, extent={}x{}",
                  std::source_location::current(), vkFormatToString(VK_FORMAT_B8G8R8A8_SRGB), width, height);

    result = vkCreateSwapchainKHR(context_.device, &createInfo, nullptr, &context_.swapchain);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanSwapchainManager", "Failed to create swapchain: result={}",
                      std::source_location::current(), result);
        throw std::runtime_error("Failed to create swapchain");
    }
    LOG_DEBUG_CAT("VulkanSwapchainManager", "Swapchain created: swapchain={:p}",
                  std::source_location::current(), context_.swapchain);

    uint32_t imageCountRetrieved;
    result = vkGetSwapchainImagesKHR(context_.device, context_.swapchain, &imageCountRetrieved, nullptr);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanSwapchainManager", "Failed to get swapchain image count: result={}",
                      std::source_location::current(), result);
        throw std::runtime_error("Failed to get swapchain image count");
    }
    context_.swapchainImages.resize(imageCountRetrieved);
    result = vkGetSwapchainImagesKHR(context_.device, context_.swapchain, &imageCountRetrieved, context_.swapchainImages.data());
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanSwapchainManager", "Failed to get swapchain images: result={}",
                      std::source_location::current(), result);
        throw std::runtime_error("Failed to get swapchain images");
    }
    LOG_DEBUG_CAT("VulkanSwapchainManager", "Retrieved {} swapchain images",
                  std::source_location::current(), imageCountRetrieved);

    context_.swapchainImageViews.resize(imageCountRetrieved);
    for (size_t i = 0; i < imageCountRetrieved; ++i) {
        VkImageViewCreateInfo viewInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = context_.swapchainImages[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = VK_FORMAT_B8G8R8A8_SRGB,
            .components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                          VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };
        result = vkCreateImageView(context_.device, &viewInfo, nullptr, &context_.swapchainImageViews[i]);
        if (result != VK_SUCCESS) {
            LOG_ERROR_CAT("VulkanSwapchainManager", "Failed to create swapchain image view {}: result={}",
                          std::source_location::current(), i, result);
            throw std::runtime_error("Failed to create swapchain image view");
        }
        LOG_DEBUG_CAT("VulkanSwapchainManager", "Created image view {}: imageView={:p}",
                      std::source_location::current(), i, context_.swapchainImageViews[i]);
    }

    LOG_INFO_CAT("VulkanSwapchainManager", "Swapchain initialized with width={}, height={}",
                 std::source_location::current(), width, height);
}

void VulkanSwapchainManager::cleanupSwapchain() {
    LOG_DEBUG_CAT("VulkanSwapchainManager", "Cleaning up swapchain", std::source_location::current());
    if (context_.device == VK_NULL_HANDLE) {
        LOG_WARNING_CAT("VulkanSwapchainManager", "Device is null, skipping cleanup", std::source_location::current());
        return;
    }

    vkDeviceWaitIdle(context_.device);
    LOG_DEBUG_CAT("VulkanSwapchainManager", "Device idle, proceeding with cleanup", std::source_location::current());

    for (auto& imageView : context_.swapchainImageViews) {
        if (imageView != VK_NULL_HANDLE) {
            LOG_DEBUG_CAT("VulkanSwapchainManager", "Destroying swapchain imageView={:p}",
                          std::source_location::current(), imageView);
            vkDestroyImageView(context_.device, imageView, nullptr);
            imageView = VK_NULL_HANDLE;
        }
    }
    for (auto& framebuffer : context_.swapchainFramebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            LOG_DEBUG_CAT("VulkanSwapchainManager", "Destroying framebuffer={:p}",
                          std::source_location::current(), framebuffer);
            vkDestroyFramebuffer(context_.device, framebuffer, nullptr);
            framebuffer = VK_NULL_HANDLE;
        }
    }
    if (context_.swapchain != VK_NULL_HANDLE) {
        LOG_DEBUG_CAT("VulkanSwapchainManager", "Destroying swapchain={:p}",
                      std::source_location::current(), context_.swapchain);
        vkDestroySwapchainKHR(context_.device, context_.swapchain, nullptr);
        context_.swapchain = VK_NULL_HANDLE;
    }
    context_.swapchainImages.clear();
    context_.swapchainImageViews.clear();
    context_.swapchainFramebuffers.clear();
    LOG_DEBUG_CAT("VulkanSwapchainManager", "Swapchain cleaned up", std::source_location::current());
}

void VulkanSwapchainManager::handleResize(int width, int height) {
    LOG_DEBUG_CAT("VulkanSwapchainManager", "Handling resize to width={}, height={}",
                  std::source_location::current(), width, height);
    cleanupSwapchain();
    initializeSwapchain(width, height);
    LOG_INFO_CAT("VulkanSwapchainManager", "Swapchain resized to width={}, height={}",
                 std::source_location::current(), width, height);
}