// src/engine/Vulkan_init_swapchain.cpp
#include "engine/Vulkan/Vulkan_func_swapchain.hpp"
#include "engine/logging.hpp"
#include <stdexcept>
#include <source_location>

std::string vkResultToString(VkResult result) {
    switch (result) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_NOT_READY: return "VK_NOT_READY";
        case VK_TIMEOUT: return "VK_TIMEOUT";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        default: return "Unknown VkResult (" + std::to_string(result) + ")";
    }
}

VulkanSwapchainManager::VulkanSwapchainManager(VulkanContext& context, VkSurfaceKHR surface)
    : context_(context), swapchain_(VK_NULL_HANDLE), imageCount_(0) {
    LOG_INFO_CAT("VulkanSwapchain", "Constructing VulkanSwapchainManager with context.device={:p}, surface={:p}",
                 std::source_location::current(), static_cast<void*>(context_.device), static_cast<void*>(surface));
    context_.surface = surface;
}

VulkanSwapchainManager::~VulkanSwapchainManager() {
    LOG_INFO_CAT("VulkanSwapchain", "Destroying VulkanSwapchainManager", std::source_location::current());
    cleanupSwapchain();
}

void VulkanSwapchainManager::initializeSwapchain(int width, int height) {
    LOG_DEBUG_CAT("VulkanSwapchain", "Initializing swapchain with width={}, height={}",
                  std::source_location::current(), width, height);

    VkSurfaceCapabilitiesKHR surfaceCaps;
    VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context_.physicalDevice, context_.surface, &surfaceCaps);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanSwapchain", "Failed to get surface capabilities: result={}",
                      std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to get surface capabilities");
    }

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(context_.physicalDevice, context_.surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(context_.physicalDevice, context_.surface, &formatCount, formats.data());

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(context_.physicalDevice, context_.surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(context_.physicalDevice, context_.surface, &presentModeCount, presentModes.data());

    VkSurfaceFormatKHR surfaceFormat = formats[0];
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = format;
            break;
        }
    }

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (const auto& mode : presentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            presentMode = mode;
            break;
        }
    }

    VkExtent2D extent;
    if (surfaceCaps.currentExtent.width != UINT32_MAX) {
        extent = surfaceCaps.currentExtent;
    } else {
        extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
        extent.width = std::max(surfaceCaps.minImageExtent.width, std::min(surfaceCaps.maxImageExtent.width, extent.width));
        extent.height = std::max(surfaceCaps.minImageExtent.height, std::min(surfaceCaps.maxImageExtent.height, extent.height));
    }

    imageCount_ = surfaceCaps.minImageCount + 1;
    if (surfaceCaps.maxImageCount > 0 && imageCount_ > surfaceCaps.maxImageCount) {
        imageCount_ = surfaceCaps.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .surface = context_.surface,
        .minImageCount = imageCount_,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .preTransform = surfaceCaps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = swapchain_
    };

    result = vkCreateSwapchainKHR(context_.device, &createInfo, nullptr, &swapchain_);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanSwapchain", "Failed to create swapchain: result={}",
                      std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to create swapchain");
    }
    LOG_DEBUG_CAT("VulkanSwapchain", "Created swapchain: swapchain={:p}", std::source_location::current(), static_cast<void*>(swapchain_));

    vkGetSwapchainImagesKHR(context_.device, swapchain_, &imageCount_, nullptr);
    swapchainImages_.resize(imageCount_);
    vkGetSwapchainImagesKHR(context_.device, swapchain_, &imageCount_, swapchainImages_.data());

    swapchainImageViews_.resize(imageCount_);
    for (uint32_t i = 0; i < imageCount_; ++i) {
        VkImageViewCreateInfo viewInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = swapchainImages_[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = surfaceFormat.format,
            .components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };
        result = vkCreateImageView(context_.device, &viewInfo, nullptr, &swapchainImageViews_[i]);
        if (result != VK_SUCCESS) {
            LOG_ERROR_CAT("VulkanSwapchain", "Failed to create image view {}: result={}",
                          std::source_location::current(), i, vkResultToString(result));
            throw std::runtime_error("Failed to create image view");
        }
        LOG_DEBUG_CAT("VulkanSwapchain", "Created image view {}: imageView={:p}",
                      std::source_location::current(), i, static_cast<void*>(swapchainImageViews_[i]));
    }

    swapchainExtent_ = extent;
    swapchainImageFormat_ = surfaceFormat.format;
    LOG_DEBUG_CAT("VulkanSwapchain", "Swapchain initialized with format={}, extent=[{}, {}]",
                  std::source_location::current(), swapchainImageFormat_, extent.width, extent.height);
}

void VulkanSwapchainManager::handleResize(int width, int height) {
    LOG_DEBUG_CAT("VulkanSwapchain", "Handling resize to width={}, height={}",
                  std::source_location::current(), width, height);
    cleanupSwapchain();
    initializeSwapchain(width, height);
}