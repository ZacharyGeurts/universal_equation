// AMOURANTH RTX Engine, October 2025 - Vulkan core initialization.
// Initializes physical device, logical device, queues, command pool, and synchronization objects.
// Dependencies: Vulkan 1.3+, GLM, C++20 standard library.
// Zachary Geurts 2025

#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <span>
#include <chrono>
#include "engine/logging.hpp"
#include "engine/core.hpp" // For AMOURANTH definition

struct VulkanContext {
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    uint32_t graphicsFamily = 0;
    uint32_t presentFamily = 0;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    std::vector<VkFramebuffer> swapchainFramebuffers;
    VkExtent2D swapchainExtent;
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
    VkBuffer quadVertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory quadVertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer quadIndexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory quadIndexBufferMemory = VK_NULL_HANDLE;
    VkBuffer voxelVertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory voxelVertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer voxelIndexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory voxelIndexBufferMemory = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
};

struct PushConstants {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

class VulkanSwapchainManager {
public:
    VulkanSwapchainManager(VulkanContext& context, VkSurfaceKHR surface);
    void initializeSwapchain(int width, int height);
    void handleResize(int width, int height);
    void cleanupSwapchain();
private:
    VulkanContext& context_;
    VkSurfaceKHR surface_;
    Logging::Logger logger_;
};

class VulkanPipelineManager {
public:
    explicit VulkanPipelineManager(VulkanContext& context);
    ~VulkanPipelineManager();
    void initializePipeline(int width, int height);
    void cleanupPipeline();
private:
    VkShaderModule createShaderModule(const std::string& filename);
    VulkanContext& context_;
    VkShaderModule vertShaderModule_;
    VkShaderModule fragShaderModule_;
    Logging::Logger logger_;
};

class VulkanRenderer {
public:
    VulkanRenderer(VkInstance instance, VkSurfaceKHR surface, 
                  std::span<const glm::vec3> vertices, std::span<const uint32_t> indices,
                  int width, int height, const Logging::Logger& logger);
    ~VulkanRenderer();
    void renderFrame(AMOURANTH* amouranth);
    void handleResize(int width, int height);
    VkDevice getDevice() const { return context_.device; }
    VkDeviceMemory getVertexBufferMemory() const { return context_.vertexBufferMemory; }
    VkPipeline getGraphicsPipeline() const { return context_.pipeline; }
private:
    void initializeVulkan(std::span<const glm::vec3> vertices, std::span<const uint32_t> indices, int width, int height);
    void beginFrame();
    void endFrame();
    void cleanupVulkan();
    VulkanContext context_;
    VkInstance instance_;
    VkSurfaceKHR surface_;
    VulkanSwapchainManager swapchainManager_;
    VulkanPipelineManager pipelineManager_;
    const Logging::Logger& logger_;
    uint32_t currentFrame_ = 0;
    uint32_t currentImageIndex_ = 0;
    std::chrono::steady_clock::time_point lastFrameTime_;
};