// include/engine/Vulkan_init.hpp
// Vulkan initialization and management for AMOURANTH RTX Engine
// Zachary Geurts 2025
#pragma once
#ifndef VULKAN_INIT_HPP
#define VULKAN_INIT_HPP

#include "logging.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <span>
#include <memory>
#include <glm/glm.hpp>
#include "universal_equation.hpp"

std::string vkResultToString(VkResult result); // Moved here for global access

struct VulkanContext {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkPhysicalDeviceMemoryProperties memoryProperties;
    uint32_t graphicsQueueFamilyIndex = UINT32_MAX;
    uint32_t presentQueueFamilyIndex = UINT32_MAX;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VkBuffer uniformBuffer = VK_NULL_HANDLE;
    VkDeviceMemory uniformBufferMemory = VK_NULL_HANDLE;
    VkImage storageImage = VK_NULL_HANDLE;
    VkDeviceMemory storageImageMemory = VK_NULL_HANDLE;
    VkImageView storageImageView = VK_NULL_HANDLE;
    VkBuffer shaderBindingTable = VK_NULL_HANDLE;
    VkDeviceMemory shaderBindingTableMemory = VK_NULL_HANDLE;
    VkPipeline rayTracingPipeline = VK_NULL_HANDLE;
    VkPipelineLayout rayTracingPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout rayTracingDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet rayTracingDescriptorSet = VK_NULL_HANDLE;
    VkAccelerationStructureKHR topLevelAS = VK_NULL_HANDLE;
    VkBuffer topLevelASBuffer = VK_NULL_HANDLE;
    VkDeviceMemory topLevelASBufferMemory = VK_NULL_HANDLE;
    VkAccelerationStructureKHR bottomLevelAS = VK_NULL_HANDLE;
    VkBuffer bottomLevelASBuffer = VK_NULL_HANDLE;
    VkDeviceMemory bottomLevelASBufferMemory = VK_NULL_HANDLE;
};

class VulkanSwapchainManager {
public:
    VulkanSwapchainManager(VulkanContext& context, VkSurfaceKHR surface);
    ~VulkanSwapchainManager();

    void initializeSwapchain(int width, int height);
    void handleResize(int width, int height);
    void cleanupSwapchain();

    VkSwapchainKHR getSwapchain() const { return swapchain_; }
    VkFormat getSwapchainImageFormat() const { return swapchainImageFormat_; }
    VkExtent2D getSwapchainExtent() const { return swapchainExtent_; }
    std::vector<VkImageView>& getSwapchainImageViews() { return swapchainImageViews_; }
    std::vector<VkImage>& getSwapchainImages() { return swapchainImages_; }
    uint32_t getImageCount() const { return imageCount_; }

private:
    VulkanContext& context_;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchainImageFormat_;
    VkExtent2D swapchainExtent_;
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    uint32_t imageCount_;
};

class VulkanPipelineManager {
public:
    VulkanPipelineManager(VulkanContext& context, int width, int height);
    ~VulkanPipelineManager();

    void createRenderPass();
    void createPipelineLayout();
    void createGraphicsPipeline();

    VkRenderPass getRenderPass() const { return renderPass_; }
    VkPipeline getGraphicsPipeline() const { return graphicsPipeline_; }
    VkPipelineLayout getPipelineLayout() const { return pipelineLayout_; }
    VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout_; }

private:
    VulkanContext& context_;
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
    VkShaderModule vertexShaderModule_ = VK_NULL_HANDLE;
    VkShaderModule fragmentShaderModule_ = VK_NULL_HANDLE;
    int width_;
    int height_;
};

class VulkanBufferManager {
public:
    VulkanBufferManager(VulkanContext& context, std::span<const glm::vec3> vertices, std::span<const uint32_t> indices);
    ~VulkanBufferManager();

    void initializeBuffers(std::span<const glm::vec3> vertices, std::span<const uint32_t> indices); // Added
    VkBuffer getVertexBuffer() const { return vertexBuffer_; }
    VkDeviceMemory getVertexBufferMemory() const { return vertexBufferMemory_; }
    VkBuffer getIndexBuffer() const { return indexBuffer_; }
    VkDeviceMemory getIndexBufferMemory() const { return indexBufferMemory_; }
    VkBuffer getUniformBuffer() const { return uniformBuffer_; }
    VkDeviceMemory getUniformBufferMemory() const { return uniformBufferMemory_; }

private:
    VulkanContext& context_;
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory_ = VK_NULL_HANDLE;
    VkBuffer indexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory_ = VK_NULL_HANDLE;
    VkBuffer uniformBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory uniformBufferMemory_ = VK_NULL_HANDLE;
};

class VulkanInitializer {
public:
    static VkDeviceAddress getBufferDeviceAddress(VkDevice device, VkBuffer buffer); // Added
    static uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);
    static void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size,
                            VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                            VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    static void createDescriptorPoolAndSet(VkDevice device, VkDescriptorSetLayout descriptorSetLayout,
                                          VkDescriptorPool& descriptorPool, VkDescriptorSet& descriptorSet,
                                          VkSampler& sampler, VkBuffer uniformBuffer);
    static VkShaderModule loadShader(VkDevice device, const std::string& filepath);
    static void createRayTracingPipeline(VulkanContext& context);
    static void createAccelerationStructures(VulkanContext& context, std::span<const glm::vec3> vertices, std::span<const uint32_t> indices);
    static void createShaderBindingTable(VulkanContext& context);
    static void createStorageImage(VulkanContext& context, uint32_t width, uint32_t height);
    static void createRenderPass(VkDevice device, VkRenderPass& renderPass, VkFormat format);
    static void createDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout& descriptorSetLayout);
    static void createGraphicsPipeline(VkDevice device, VkRenderPass renderPass,
                                      VkPipeline& pipeline, VkPipelineLayout& pipelineLayout,
                                      VkDescriptorSetLayout& descriptorSetLayout,
                                      int width, int height,
                                      VkShaderModule& vertexShaderModule,
                                      VkShaderModule& fragmentShaderModule);
};

class VulkanRenderer {
public:
    VulkanRenderer(VkInstance instance, VkSurfaceKHR surface,
                  std::span<const glm::vec3> vertices, std::span<const uint32_t> indices,
                  int width, int height);
    ~VulkanRenderer();
    void renderFrame(const AMOURANTH& amouranth);
    void handleResize(int width, int height);

    VkDevice getDevice() const { return context_.device; }
    VkDeviceMemory getVertexBufferMemory() const { return bufferManager_->getVertexBufferMemory(); }
    VkPipeline getGraphicsPipeline() const { return pipelineManager_->getGraphicsPipeline(); }
    VkRenderPass getRenderPass() const { return pipelineManager_->getRenderPass(); }
    VkPipelineLayout getPipelineLayout() const { return pipelineManager_->getPipelineLayout(); }
    VkDescriptorSetLayout getDescriptorSetLayout() const { return pipelineManager_->getDescriptorSetLayout(); }
    std::vector<VkImageView>& getSwapchainImageViews() { return swapchainManager_->getSwapchainImageViews(); }
    VkBuffer getVertexBuffer() const { return bufferManager_->getVertexBuffer(); }
    VkBuffer getIndexBuffer() const { return bufferManager_->getIndexBuffer(); }
    VkCommandBuffer getCommandBuffer() const { return commandBuffers_[0]; }

private:
    VulkanContext context_;
    std::unique_ptr<VulkanSwapchainManager> swapchainManager_;
    std::unique_ptr<VulkanPipelineManager> pipelineManager_;
    std::unique_ptr<VulkanBufferManager> bufferManager_;
    std::vector<VkCommandBuffer> commandBuffers_;
    int width_;
    int height_;
};

#endif // VULKAN_INIT_HPP