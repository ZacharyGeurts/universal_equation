#pragma once
#ifndef VULKAN_INIT_HPP
#define VULKAN_INIT_HPP

#include "logging.hpp"
#include "ue_init.hpp"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <span>

class AMOURANTH; // Forward declaration

struct VulkanContext {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    uint32_t graphicsQueueFamilyIndex = 0;
    uint32_t presentQueueFamilyIndex = 0;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkPhysicalDeviceMemoryProperties memoryProperties;
    VkImage storageImage = VK_NULL_HANDLE;
    VkDeviceMemory storageImageMemory = VK_NULL_HANDLE;
    VkImageView storageImageView = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkAccelerationStructureKHR topLevelAS = VK_NULL_HANDLE;
    VkBuffer topLevelASBuffer = VK_NULL_HANDLE;
    VkDeviceMemory topLevelASBufferMemory = VK_NULL_HANDLE;
    VkAccelerationStructureKHR bottomLevelAS = VK_NULL_HANDLE;
    VkBuffer bottomLevelASBuffer = VK_NULL_HANDLE;
    VkDeviceMemory bottomLevelASBufferMemory = VK_NULL_HANDLE;
    VkPipeline rayTracingPipeline = VK_NULL_HANDLE;
    VkPipelineLayout rayTracingPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout rayTracingDescriptorSetLayout = VK_NULL_HANDLE;
    VkBuffer shaderBindingTable = VK_NULL_HANDLE;
    VkDeviceMemory shaderBindingTableMemory = VK_NULL_HANDLE;
};

class VulkanInitializer {
public:
    static void initializeVulkan(VulkanContext& context, int width, int height);
    static void createSwapchain(VulkanContext& context);
    static void createImageViews(VulkanContext& context);
    static void createAccelerationStructures(VulkanContext& context, std::span<const glm::vec3> vertices, std::span<const uint32_t> indices);
    static void createRayTracingPipeline(VulkanContext& context);
    static void createShaderBindingTable(VulkanContext& context);
    static VkDeviceAddress getBufferDeviceAddress(VkDevice device, VkBuffer buffer);
    static uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);
    static void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size,
                            VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                            VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    static VkShaderModule loadShader(VkDevice device, const std::string& filepath);
    static void createRenderPass(VkDevice device, VkRenderPass& renderPass, VkFormat format);
    static void createDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout& descriptorSetLayout);
    static void createGraphicsPipeline(VkDevice device, VkRenderPass renderPass, VkPipeline& pipeline,
                                      VkPipelineLayout& pipelineLayout, VkDescriptorSetLayout& descriptorSetLayout,
                                      int width, int height, VkShaderModule& vertexShaderModule,
                                      VkShaderModule& fragmentShaderModule);
    static void createDescriptorPoolAndSet(VkDevice device, VkDescriptorSetLayout descriptorSetLayout,
                                          VkDescriptorPool& descriptorPool, VkDescriptorSet& descriptorSet,
                                          VkSampler& sampler, VkBuffer uniformBuffer, VkImageView storageImageView,
                                          VkAccelerationStructureKHR topLevelAS);
    static void createStorageImage(VkDevice device, VkPhysicalDevice physicalDevice, VkImage& storageImage,
                                  VkDeviceMemory& storageImageMemory, VkImageView& storageImageView,
                                  uint32_t width, uint32_t height);
};

class VulkanSwapchainManager {
public:
    VulkanSwapchainManager(VulkanContext& context, VkSurfaceKHR surface);
    ~VulkanSwapchainManager();
    void initializeSwapchain(int width, int height);
    void cleanupSwapchain();
    void handleResize(int width, int height);
    VkSwapchainKHR getSwapchain() const { return swapchain_; }
    const std::vector<VkImageView>& getSwapchainImageViews() const { return swapchainImageViews_; }
    VkFormat getSwapchainImageFormat() const { return swapchainImageFormat_; }
    VkExtent2D getSwapchainExtent() const { return swapchainExtent_; }

private:
    VulkanContext& context_;
    VkSwapchainKHR swapchain_;
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    uint32_t imageCount_;
    VkFormat swapchainImageFormat_;
    VkExtent2D swapchainExtent_;
};

class VulkanBufferManager {
public:
    VulkanBufferManager(VulkanContext& context);
    ~VulkanBufferManager();
    void initializeBuffers(std::span<const glm::vec3> vertices, std::span<const uint32_t> indices);
    void cleanupBuffers();
    VkBuffer getVertexBuffer() const { return vertexBuffer_; }
    VkDeviceMemory getVertexBufferMemory() const { return vertexBufferMemory_; }
    VkBuffer getIndexBuffer() const { return indexBuffer_; }
    VkDeviceMemory getIndexBufferMemory() const { return indexBufferMemory_; } // Added
    VkBuffer getUniformBuffer() const { return uniformBuffer_; }
    VkDeviceMemory getUniformBufferMemory() const { return uniformBufferMemory_; }
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

private:
    VulkanContext& context_;
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory_ = VK_NULL_HANDLE;
    VkBuffer indexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory_ = VK_NULL_HANDLE; // Added
    VkBuffer uniformBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory uniformBufferMemory_ = VK_NULL_HANDLE;
};

class VulkanPipelineManager {
public:
    VulkanPipelineManager(VulkanContext& context, int width, int height);
    ~VulkanPipelineManager();
    void createRenderPass();
    void createPipelineLayout();
    void createGraphicsPipeline();
    void cleanupPipeline();
    VkPipeline getGraphicsPipeline() const { return graphicsPipeline_; }
    VkPipelineLayout getPipelineLayout() const { return pipelineLayout_; }
    VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout_; }
    VkRenderPass getRenderPass() const { return renderPass_; }

private:
    VulkanContext& context_;
    int width_;
    int height_;
    VkPipeline graphicsPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
    VkShaderModule vertexShaderModule_ = VK_NULL_HANDLE;
    VkShaderModule fragmentShaderModule_ = VK_NULL_HANDLE;
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
};

class VulkanRenderer {
public:
    VulkanRenderer(VkInstance instance, VkSurfaceKHR surface,
                  std::span<const glm::vec3> vertices, std::span<const uint32_t> indices,
                  int width, int height);
    ~VulkanRenderer();
    void renderFrame(const AMOURANTH& amouranth);
    void handleResize(int width, int height);
    VkDeviceMemory getIndexBufferMemory() const { return bufferManager_->getIndexBufferMemory(); } // Added

private:
    VulkanContext context_;
    std::unique_ptr<VulkanSwapchainManager> swapchainManager_;
    std::unique_ptr<VulkanBufferManager> bufferManager_;
    std::unique_ptr<VulkanPipelineManager> pipelineManager_;
    std::vector<VkFramebuffer> framebuffers_;
    std::vector<VkCommandBuffer> commandBuffers_;
    VkSemaphore imageAvailableSemaphore_ = VK_NULL_HANDLE;
    VkSemaphore renderFinishedSemaphore_ = VK_NULL_HANDLE;
    VkFence inFlightFence_ = VK_NULL_HANDLE;
    int width_;
    int height_;
};

#endif // VULKAN_INIT_HPP