// Vulkan_func_swapchain.hpp
// AMOURANTH RTX Engine, October 2025 - Vulkan swapchain utilities for surface format, swapchain, and framebuffer creation.
// Supports Windows/Linux (X11/Wayland); no mutexes; compatible with voxel geometry rendering.
// Dependencies: Vulkan 1.3+, GLM, C++20 standard library.
// Usage: Used by VulkanRenderer for swapchain setup; integrates with Vulkan_func.hpp.
// Thread-safety: Uses Logging::Logger for thread-safe logging; no mutexes required.
// Zachary Geurts 2025

#ifndef VULKAN_FUNC_SWAPCHAIN_HPP
#define VULKAN_FUNC_SWAPCHAIN_HPP

#include <vulkan/vulkan.h>
#include <vector>
#include "engine/logging.hpp" // For Logging::Logger
#include "engine/Vulkan/Vulkan_func.hpp" // For PushConstants

namespace VulkanInitializer {

// Declaration only; implementation in Vulkan_func_swapchain.cpp
VkSurfaceFormatKHR selectSurfaceFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, const Logging::Logger& logger);

// Creates the Vulkan swapchain and associated images/image views
void createSwapchain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, VkSwapchainKHR& swapchain,
                     std::vector<VkImage>& swapchainImages, std::vector<VkImageView>& swapchainImageViews,
                     VkFormat& swapchainFormat, uint32_t graphicsFamily, uint32_t presentFamily, int width, int height,
                     const Logging::Logger& logger);

// Creates framebuffers for the swapchain image views
void createFramebuffers(VkDevice device, VkRenderPass renderPass, std::vector<VkImageView>& swapchainImageViews,
                        std::vector<VkFramebuffer>& swapchainFramebuffers, int width, int height,
                        const Logging::Logger& logger);

class SwapchainManager {
public:
    SwapchainManager(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface,
                     uint32_t graphicsFamily, uint32_t presentFamily, int width, int height,
                     const Logging::Logger& logger)
        : physicalDevice_(physicalDevice), device_(device), surface_(surface),
          graphicsFamily_(graphicsFamily), presentFamily_(presentFamily), width_(width), height_(height),
          logger_(logger) {
        if (width <= 0 || height <= 0) {
            LOG_ERROR_CAT("Vulkan", "Invalid swapchain dimensions: width={}, height={}", std::source_location::current(), width, height);
            throw std::invalid_argument("Swapchain dimensions must be positive");
        }
        if (physicalDevice == VK_NULL_HANDLE || device == VK_NULL_HANDLE || surface == VK_NULL_HANDLE) {
            LOG_ERROR_CAT("Vulkan", "Invalid Vulkan handles: physicalDevice={}, device={}, surface={}",
                          std::source_location::current(),
                          physicalDevice == VK_NULL_HANDLE ? "null" : "non-null",
                          device == VK_NULL_HANDLE ? "null" : "non-null",
                          surface == VK_NULL_HANDLE ? "null" : "non-null");
            throw std::invalid_argument("Vulkan handles cannot be null");
        }
        LOG_DEBUG_CAT("Vulkan", "SwapchainManager initialized with width={}, height={}", std::source_location::current(), width_, height_);
    }

    // Setters
    void setSwapchain(VkSwapchainKHR swapchain) {
        swapchain_ = swapchain;
        LOG_DEBUG_CAT("Vulkan", "Set swapchain: {}", std::source_location::current(),
                      swapchain == VK_NULL_HANDLE ? "null" : "non-null");
    }

    void setSwapchainImages(const std::vector<VkImage>& images) {
        swapchainImages_ = images;
        LOG_DEBUG_CAT("Vulkan", "Set {} swapchain images", std::source_location::current(), images.size());
    }

    void setSwapchainImageViews(const std::vector<VkImageView>& views) {
        swapchainImageViews_ = views;
        LOG_DEBUG_CAT("Vulkan", "Set {} swapchain image views", std::source_location::current(), views.size());
    }

    void setSwapchainFormat(VkFormat format) {
        swapchainFormat_ = format;
        LOG_DEBUG_CAT("Vulkan", "Set swapchain format: {}", std::source_location::current(), static_cast<int>(format));
    }

    void setFramebuffers(const std::vector<VkFramebuffer>& framebuffers) {
        swapchainFramebuffers_ = framebuffers;
        LOG_DEBUG_CAT("Vulkan", "Set {} framebuffers", std::source_location::current(), framebuffers.size());
    }

    // Getters
    VkSwapchainKHR getSwapchain() const { return swapchain_; }
    const std::vector<VkImage>& getSwapchainImages() const { return swapchainImages_; }
    const std::vector<VkImageView>& getSwapchainImageViews() const { return swapchainImageViews_; }
    VkFormat getSwapchainFormat() const { return swapchainFormat_; }
    const std::vector<VkFramebuffer>& getFramebuffers() const { return swapchainFramebuffers_; }

    // Initializes the swapchain and associated resources
    void initializeSwapchain() {
        LOG_INFO_CAT("Vulkan", "Initializing swapchain with width={}, height={}", std::source_location::current(), width_, height_);
        VkSurfaceFormatKHR format = selectSurfaceFormat(physicalDevice_, surface_, logger_);
        swapchainFormat_ = format.format;
        createSwapchain(physicalDevice_, device_, surface_, swapchain_, swapchainImages_, swapchainImageViews_,
                        swapchainFormat_, graphicsFamily_, presentFamily_, width_, height_, logger_);
        LOG_INFO_CAT("Vulkan", "Swapchain initialized with format: {}", std::source_location::current(), static_cast<int>(swapchainFormat_));
    }

    // Initializes framebuffers for the swapchain
    void initializeFramebuffers(VkRenderPass renderPass) {
        if (swapchainImageViews_.empty()) {
            LOG_ERROR_CAT("Vulkan", "Cannot initialize framebuffers: no swapchain image views", std::source_location::current());
            throw std::runtime_error("No swapchain image views available");
        }
        LOG_INFO_CAT("Vulkan", "Initializing {} framebuffers", std::source_location::current(), swapchainImageViews_.size());
        createFramebuffers(device_, renderPass, swapchainImageViews_, swapchainFramebuffers_, width_, height_, logger_);
        LOG_INFO_CAT("Vulkan", "Framebuffers initialized successfully", std::source_location::current());
    }

    // Cleans up swapchain resources
    void cleanup() {
        LOG_INFO_CAT("Vulkan", "Cleaning up swapchain resources", std::source_location::current());
        if (device_ == VK_NULL_HANDLE) {
            LOG_WARNING_CAT("Vulkan", "Device is null, skipping swapchain cleanup", std::source_location::current());
            return;
        }

        // Destroy framebuffers
        for (size_t i = 0; i < swapchainFramebuffers_.size(); ++i) {
            if (swapchainFramebuffers_[i] != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(device_, swapchainFramebuffers_[i], nullptr);
                LOG_DEBUG_CAT("Vulkan", "Destroyed framebuffer {}", std::source_location::current(), i);
                swapchainFramebuffers_[i] = VK_NULL_HANDLE;
            }
        }
        swapchainFramebuffers_.clear();

        // Destroy image views
        for (size_t i = 0; i < swapchainImageViews_.size(); ++i) {
            if (swapchainImageViews_[i] != VK_NULL_HANDLE) {
                vkDestroyImageView(device_, swapchainImageViews_[i], nullptr);
                LOG_DEBUG_CAT("Vulkan", "Destroyed image view {}", std::source_location::current(), i);
                swapchainImageViews_[i] = VK_NULL_HANDLE;
            }
        }
        swapchainImageViews_.clear();

        // Destroy swapchain
        if (swapchain_ != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device_, swapchain_, nullptr);
            LOG_DEBUG_CAT("Vulkan", "Destroyed swapchain", std::source_location::current());
            swapchain_ = VK_NULL_HANDLE;
        }

        LOG_INFO_CAT("Vulkan", "Swapchain cleanup completed", std::source_location::current());
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
    const Logging::Logger& logger_;
};

} // namespace VulkanInitializer

#endif // VULKAN_FUNC_SWAPCHAIN_HPP