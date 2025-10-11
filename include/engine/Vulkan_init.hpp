// Vulkan_init.hpp
// AMOURANTH RTX Engine, October 2025 - Vulkan initialization and utility functions.
// Dependencies: Vulkan 1.3+, GLM, VulkanCore.hpp, VulkanBufferManager.hpp, ue_init.hpp, logging.hpp.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#pragma once
#ifndef VULKAN_INIT_HPP
#define VULKAN_INIT_HPP

#include "VulkanCore.hpp"
#include "VulkanBufferManager.hpp"
#include "ue_init.hpp"
#include "engine/logging.hpp" // Ensure logging.hpp is included for LOG_* macros
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <string>
#include <span>
#include <glm/glm.hpp>

namespace VulkanInitializer {
    // Function declarations for Vulkan initialization utilities
    VkPhysicalDevice findPhysicalDevice(VkInstance instance, bool preferNvidia = false);
    void initializeVulkan(VulkanContext& context, int width, int height);
    void createSwapchain(VulkanContext& context, int width, int height);
    void createImageViews(VulkanContext& context);
    VkShaderModule loadShader(VkDevice device, const std::string& filepath);
    void createRenderPass(VkDevice device, VkRenderPass& renderPass, VkFormat format);
    void createDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout& descriptorSetLayout);
    void createDescriptorPoolAndSet(VkDevice device, VkDescriptorSetLayout descriptorSetLayout,
                                   VkDescriptorPool& descriptorPool, VkDescriptorSet& descriptorSet,
                                   VkSampler& sampler, VkBuffer uniformBuffer, VkImageView storageImageView,
                                   VkAccelerationStructureKHR topLevelAS);
    void createStorageImage(VkDevice device, VkPhysicalDevice physicalDevice, VkImage& storageImage,
                           VkDeviceMemory& storageImageMemory, VkImageView& storageImageView,
                           uint32_t width, uint32_t height);
    void createRayTracingPipeline(VulkanContext& context, VkPipelineLayout pipelineLayout,
                                 VkShaderModule rayGenShader, VkShaderModule missShader,
                                 VkShaderModule closestHitShader, VkPipeline& pipeline);
    void createShaderBindingTable(VulkanContext& context);
    void createAccelerationStructures(VulkanContext& context, std::span<const glm::vec3> vertices, std::span<const uint32_t> indices);
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);
}

class VulkanSwapchainManager {
public:
    VulkanSwapchainManager(VulkanContext& context, VkSurfaceKHR surface);
    ~VulkanSwapchainManager();
    void initializeSwapchain(int width, int height);
    void cleanupSwapchain();
    void handleResize(int width, int height);
    VkSwapchainKHR getSwapchain() const { return swapchain_; }
    VkFormat getSwapchainImageFormat() const { return swapchainImageFormat_; }
    VkExtent2D getSwapchainExtent() const { return swapchainExtent_; }
    const std::vector<VkImage>& getSwapchainImages() const { return swapchainImages_; }
    const std::vector<VkImageView>& getSwapchainImageViews() const { return swapchainImageViews_; }
private:
    VulkanContext& context_;
    VkSwapchainKHR swapchain_;
    uint32_t imageCount_;
    VkFormat swapchainImageFormat_;
    VkExtent2D swapchainExtent_;
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
};

class VulkanPipelineManager {
public:
    VulkanPipelineManager(VulkanContext& context, int width, int height);
    ~VulkanPipelineManager();
    void createRenderPass();
    void createPipelineLayout();
    void createRayTracingPipeline();
    void createGraphicsPipeline(); // Added
    void cleanupPipeline();
    VkPipeline getRayTracingPipeline() const { return rayTracingPipeline_; }
    VkPipeline getGraphicsPipeline() const { return graphicsPipeline_; } // Added
    VkPipelineLayout getPipelineLayout() const { return pipelineLayout_; }
    VkRenderPass getRenderPass() const { return renderPass_; }
    VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout_; }
    VkShaderModule getRayGenShaderModule() const { return rayGenShaderModule_; }
    VkShaderModule getMissShaderModule() const { return missShaderModule_; }
    VkShaderModule getClosestHitShaderModule() const { return closestHitShaderModule_; }
private:
    VulkanContext& context_;
    int width_;
    int height_;
    VkRenderPass renderPass_;
    VkPipeline rayTracingPipeline_;
    VkPipeline graphicsPipeline_; // Added
    VkPipelineLayout pipelineLayout_;
    VkDescriptorSetLayout descriptorSetLayout_;
    VkShaderModule rayGenShaderModule_;
    VkShaderModule missShaderModule_;
    VkShaderModule closestHitShaderModule_;
};

class VulkanRenderer {
public:
    VulkanRenderer(VkInstance instance, VkSurfaceKHR surface,
                  std::span<const glm::vec3> vertices, std::span<const uint32_t> indices,
                  int width, int height);
    ~VulkanRenderer();
    void createFramebuffers();
    void createCommandBuffers();
    void createSyncObjects();
    void renderFrame(const AMOURANTH& camera);
    void handleResize(int width, int height);
    VkDevice getDevice() const { return context_.device; }
    VkPipeline getRayTracingPipeline() const { return pipelineManager_->getRayTracingPipeline(); }
    VkDeviceMemory getVertexBufferMemory() const;
    VkDeviceMemory getIndexBufferMemory() const;
private:
    VulkanContext context_;
    int width_;
    int height_;
    VkSemaphore imageAvailableSemaphore_ = VK_NULL_HANDLE;
    VkSemaphore renderFinishedSemaphore_ = VK_NULL_HANDLE;
    VkFence inFlightFence_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers_;
    std::vector<VkCommandBuffer> commandBuffers_;
    std::unique_ptr<VulkanSwapchainManager> swapchainManager_;
    std::unique_ptr<VulkanBufferManager> bufferManager_;
    std::unique_ptr<VulkanPipelineManager> pipelineManager_;
};

#endif // VULKAN_INIT_HPP