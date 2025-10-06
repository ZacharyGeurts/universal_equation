#ifndef VULKAN_FUNC_SWAPCHAIN_HPP
#define VULKAN_FUNC_SWAPCHAIN_HPP

// AMOURANTH RTX Engine, October 2025 - Vulkan swapchain utilities for surface format, swapchain, and framebuffer creation.
// Supports Windows/Linux (X11/Wayland); no mutexes; compatible with voxel geometry rendering.
// Dependencies: Vulkan 1.3+, C++20 standard library.
// Usage: Used by VulkanRenderer for swapchain setup; integrates with Vulkan_func.hpp.
// Zachary Geurts 2025

#include <vulkan/vulkan.h>
#include <vector>

namespace VulkanInitializer {

VkSurfaceFormatKHR selectSurfaceFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);

void createSwapchain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, VkSwapchainKHR& swapchain,
                     std::vector<VkImage>& swapchainImages, std::vector<VkImageView>& swapchainImageViews,
                     VkFormat& swapchainFormat, uint32_t graphicsFamily, uint32_t presentFamily, int width, int height);

void createFramebuffers(VkDevice device, VkRenderPass renderPass, std::vector<VkImageView>& swapchainImageViews,
                        std::vector<VkFramebuffer>& swapchainFramebuffers, int width, int height);

class SwapchainManager {
public:
    SwapchainManager(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface,
                     uint32_t graphicsFamily, uint32_t presentFamily, int width, int height)
        : physicalDevice_(physicalDevice), device_(device), surface_(surface),
          graphicsFamily_(graphicsFamily), presentFamily_(presentFamily), width_(width), height_(height) {}

    void setSwapchain(VkSwapchainKHR swapchain) { swapchain_ = swapchain; }
    VkSwapchainKHR getSwapchain() const { return swapchain_; }

    void setSwapchainImages(const std::vector<VkImage>& images) { swapchainImages_ = images; }
    const std::vector<VkImage>& getSwapchainImages() const { return swapchainImages_; }

    void setSwapchainImageViews(const std::vector<VkImageView>& views) { swapchainImageViews_ = views; }
    const std::vector<VkImageView>& getSwapchainImageViews() const { return swapchainImageViews_; }

    void setSwapchainFormat(VkFormat format) { swapchainFormat_ = format; }
    VkFormat getSwapchainFormat() const { return swapchainFormat_; }

    void setFramebuffers(const std::vector<VkFramebuffer>& framebuffers) { swapchainFramebuffers_ = framebuffers; }
    const std::vector<VkFramebuffer>& getFramebuffers() const { return swapchainFramebuffers_; }

    void initializeSwapchain() {
        VkSurfaceFormatKHR format = selectSurfaceFormat(physicalDevice_, surface_);
        swapchainFormat_ = format.format;
        createSwapchain(physicalDevice_, device_, surface_, swapchain_, swapchainImages_, swapchainImageViews_,
                        swapchainFormat_, graphicsFamily_, presentFamily_, width_, height_);
    }

    void initializeFramebuffers(VkRenderPass renderPass) {
        createFramebuffers(device_, renderPass, swapchainImageViews_, swapchainFramebuffers_, width_, height_);
    }

    void cleanup() {
        for (auto& framebuffer : swapchainFramebuffers_) {
            if (framebuffer != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(device_, framebuffer, nullptr);
                framebuffer = VK_NULL_HANDLE;
            }
        }
        swapchainFramebuffers_.clear();
        for (auto& imageView : swapchainImageViews_) {
            if (imageView != VK_NULL_HANDLE) {
                vkDestroyImageView(device_, imageView, nullptr);
                imageView = VK_NULL_HANDLE;
            }
        }
        swapchainImageViews_.clear();
        if (swapchain_ != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device_, swapchain_, nullptr);
            swapchain_ = VK_NULL_HANDLE;
        }
    }

private:
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    uint32_t graphicsFamily_ = 0;
    uint32_t presentFamily_ = 0;
    int width_ = 0;
    int height_ = 0;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    VkFormat swapchainFormat_ = VK_FORMAT_UNDEFINED;
    std::vector<VkFramebuffer> swapchainFramebuffers_;
};

} // namespace VulkanInitializer

#endif // VULKAN_FUNC_SWAPCHAIN_HPP