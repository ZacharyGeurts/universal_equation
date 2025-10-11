// Vulkan_init.hpp
// AMOURANTH RTX Engine, October 2025 - Vulkan initialization for swapchain, pipeline, and renderer.
// Dependencies: Vulkan 1.3+, GLM, logging.hpp, VulkanCore.hpp, VulkanBufferManager.hpp.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#pragma once
#ifndef VULKAN_INIT_HPP
#define VULKAN_INIT_HPP

#include "VulkanCore.hpp"
#include "VulkanBufferManager.hpp"
#include "engine/logging.hpp"
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <span>
#include <glm/glm.hpp>

// Forward declarations
class AMOURANTH;

class VulkanSwapchainManager {
public:
    VulkanSwapchainManager(VulkanContext& context, VkSurfaceKHR surface);
    ~VulkanSwapchainManager();
    void initializeSwapchain(int width, int height);
    void cleanupSwapchain();
    void handleResize(int width, int height);
    VkSwapchainKHR& getSwapchain() { return swapchain_; } // Changed to return reference
    VkFormat getSwapchainImageFormat() const { return swapchainImageFormat_; }
    VkExtent2D getSwapchainExtent() const { return swapchainExtent_; }
    const std::vector<VkImage>& getSwapchainImages() const { return swapchainImages_; } // Added getter
    const std::vector<VkImageView>& getSwapchainImageViews() const { return swapchainImageViews_; }

private:
    VulkanContext& context_;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    uint32_t imageCount_ = 0;
    VkFormat swapchainImageFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent_ = {0, 0};
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
};

class VulkanPipelineManager {
public:
    VulkanPipelineManager(VulkanContext& context, int width, int height);
    ~VulkanPipelineManager();
    void createRenderPass();
    void createPipelineLayout();
    void createGraphicsPipeline();
    void cleanupPipeline();
    VkRenderPass getRenderPass() const { return renderPass_; }
    VkPipeline getGraphicsPipeline() const { return graphicsPipeline_; }
    VkPipelineLayout getPipelineLayout() const { return pipelineLayout_; }
    VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout_; }

private:
    VulkanContext& context_;
    int width_;
    int height_;
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
    VkShaderModule vertexShaderModule_ = VK_NULL_HANDLE;
    VkShaderModule fragmentShaderModule_ = VK_NULL_HANDLE;
};

class VulkanRenderer {
public:
    VulkanRenderer(VkInstance instance, VkSurfaceKHR surface,
                  std::span<const glm::vec3> vertices, std::span<const uint32_t> indices,
                  int width, int height);
    ~VulkanRenderer();
    void renderFrame(const AMOURANTH& camera);
    void handleResize(int width, int height);
    VkDevice getDevice() const { return context_.device; }
    VkDeviceMemory getVertexBufferMemory() const;
    VkDeviceMemory getIndexBufferMemory() const;
    VkPipeline getGraphicsPipeline() const { return pipelineManager_->getGraphicsPipeline(); }
    uint32_t getWidth() const { return width_; }
    uint32_t getHeight() const { return height_; }

private:
    void createCommandBuffers();
    void createSyncObjects();
    void createFramebuffers();

    VulkanContext context_;
    std::unique_ptr<VulkanSwapchainManager> swapchainManager_;
    std::unique_ptr<VulkanPipelineManager> pipelineManager_;
    std::unique_ptr<VulkanBufferManager> bufferManager_;
    std::vector<VkCommandBuffer> commandBuffers_;
    std::vector<VkFramebuffer> framebuffers_;
    VkSemaphore imageAvailableSemaphore_ = VK_NULL_HANDLE;
    VkSemaphore renderFinishedSemaphore_ = VK_NULL_HANDLE;
    VkFence inFlightFence_ = VK_NULL_HANDLE;
    int width_;
    int height_;
};

#endif // VULKAN_INIT_HPP