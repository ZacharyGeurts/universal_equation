#include "Vulkan_func_swapchain.hpp"
#include <stdexcept>
#include <vector>
#include <algorithm>
#include <limits>
#include <array>    // Added for std::array
#include <cstdint>  // Added for uint32_t
// This file handles swapchain creation, surface format selection, and framebuffer setup.
// AMOURANTH RTX September 2025
// Zachary Geurts 2025
namespace VulkanInitializer {

    // Select a suitable surface format for the swapchain
    VkSurfaceFormatKHR selectSurfaceFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
        uint32_t formatCount = 0;
        VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
        if (result != VK_SUCCESS || formatCount == 0) {
            throw std::runtime_error("Failed to get surface formats: " + std::to_string(result));
        }
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to retrieve surface formats: " + std::to_string(result));
        }
        for (const auto& format : formats) {
            if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return format;
            }
        }
        return formats[0]; // Fallback to first available format
    }

    // Create a swapchain and its associated images/views
    void createSwapchain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, VkSwapchainKHR& swapchain,
                         std::vector<VkImage>& swapchainImages, std::vector<VkImageView>& swapchainImageViews,
                         VkFormat& swapchainFormat, uint32_t graphicsFamily, uint32_t presentFamily, int width, int height) {
        VkSurfaceCapabilitiesKHR capabilities;
        VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to get surface capabilities: " + std::to_string(result));
        }

        uint32_t formatCount = 0;
        result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
        if (result != VK_SUCCESS || formatCount == 0) {
            throw std::runtime_error("Failed to get surface formats: " + std::to_string(result));
        }

        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to retrieve surface formats: " + std::to_string(result));
        }

        VkSurfaceFormatKHR selectedFormat = formats[0];
        for (const auto& availableFormat : formats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
                availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                selectedFormat = availableFormat;
                break;
            }
        }
        swapchainFormat = selectedFormat.format;

        VkImageFormatProperties formatProps;
        result = vkGetPhysicalDeviceImageFormatProperties(
            physicalDevice, selectedFormat.format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 0, &formatProps);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Selected swapchain format not supported: " + std::to_string(result));
        }

        uint32_t presentModeCount = 0;
        result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
        if (result != VK_SUCCESS || presentModeCount == 0) {
            throw std::runtime_error("Failed to get present modes: " + std::to_string(result));
        }

        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data());
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to retrieve present modes: " + std::to_string(result));
        }

        VkPresentModeKHR selectedPresentMode = VK_PRESENT_MODE_FIFO_KHR;
        for (const auto& availablePresentMode : presentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                selectedPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
                break;
            }
        }

        VkExtent2D extent;
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            extent = capabilities.currentExtent;
        } else {
            extent = {
                std::clamp(static_cast<uint32_t>(width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
                std::clamp(static_cast<uint32_t>(height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
            };
        }

        uint32_t imageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
            imageCount = capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo = {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .surface = surface,
            .minImageCount = imageCount,
            .imageFormat = selectedFormat.format,
            .imageColorSpace = selectedFormat.colorSpace,
            .imageExtent = extent,
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .imageSharingMode = (graphicsFamily != presentFamily) ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = (graphicsFamily != presentFamily) ? 2u : 0u,
            .pQueueFamilyIndices = (graphicsFamily != presentFamily) ? std::array<uint32_t, 2>{graphicsFamily, presentFamily}.data() : nullptr,
            .preTransform = capabilities.currentTransform,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode = selectedPresentMode,
            .clipped = VK_TRUE,
            .oldSwapchain = VK_NULL_HANDLE
        };

        result = vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create swapchain: " + std::to_string(result));
        }

        result = vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to get swapchain image count: " + std::to_string(result));
        }
        swapchainImages.resize(imageCount);
        result = vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to retrieve swapchain images: " + std::to_string(result));
        }

        swapchainImageViews.resize(imageCount);
        for (size_t i = 0; i < swapchainImages.size(); ++i) {
            VkImageViewCreateInfo viewInfo = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
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
                throw std::runtime_error("Failed to create swapchain image view " + std::to_string(i) + ": " + std::to_string(result));
            }
        }
    }

    // Create framebuffers for the swapchain images
    void createFramebuffers(VkDevice device, VkRenderPass renderPass, std::vector<VkImageView>& swapchainImageViews,
                            std::vector<VkFramebuffer>& swapchainFramebuffers, int width, int height) {
        swapchainFramebuffers.resize(swapchainImageViews.size());
        for (size_t i = 0; i < swapchainImageViews.size(); ++i) {
            VkImageView attachments[] = {swapchainImageViews[i]};
            VkFramebufferCreateInfo framebufferInfo = {
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .renderPass = renderPass,
                .attachmentCount = 1,
                .pAttachments = attachments,
                .width = static_cast<uint32_t>(width),
                .height = static_cast<uint32_t>(height),
                .layers = 1
            };

            VkResult result = vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapchainFramebuffers[i]);
            if (result != VK_SUCCESS) {
                throw std::runtime_error("Failed to create framebuffer " + std::to_string(i) + ": " + std::to_string(result));
            }
        }
    }

}