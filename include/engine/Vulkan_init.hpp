#ifndef VULKAN_INIT_HPP
#define VULKAN_INIT_HPP

// AMOURANTH RTX Engine, October 2025 - Vulkan core initialization.
// Initializes physical device, logical device, queues, command pool, and synchronization objects.
// Dependencies: Vulkan 1.3+, GLM, C++20 standard library.
// Zachary Geurts 2025

#include "Vulkan_init_swapchain.hpp"
#include "Vulkan_init_pipeline.hpp"
#include "Vulkan_init_buffers.hpp"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <span>
#include <vector>
#include <string>
#include <chrono>
#include "engine/logging.hpp"

// Forward declaration of AMOURANTH
class AMOURANTH;

struct VulkanContext {
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    uint32_t graphicsFamily = UINT32_MAX;
    uint32_t presentFamily = UINT32_MAX;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    std::vector<VkFramebuffer> swapchainFramebuffers;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;
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
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkExtent2D swapchainExtent = {0, 0};
};

class VulkanRenderer {
public:
    VulkanRenderer(VkInstance instance, VkSurfaceKHR surface,
                  std::span<const glm::vec3> vertices, std::span<const uint32_t> indices,
                  int width, int height, const Logging::Logger& logger);
    ~VulkanRenderer();

    void beginFrame();
    void endFrame();
    void renderFrame(AMOURANTH* amouranth);
    void initializeVulkan(std::span<const glm::vec3> vertices, std::span<const uint32_t> indices,
                         int width, int height);
    void handleResize(int width, int height);
    void cleanupVulkan();

    uint32_t getCurrentImageIndex() const { return currentImageIndex_; }
    VkBuffer getVertexBuffer() const { return context_.vertexBuffer; }
    VkBuffer getIndexBuffer() const { return context_.indexBuffer; }
    VkCommandBuffer getCommandBuffer() const { return context_.commandBuffers[currentImageIndex_]; }
    VkPipelineLayout getPipelineLayout() const { return context_.pipelineLayout; }
    VkDescriptorSet getDescriptorSet() const { return context_.descriptorSet; }
    const VulkanContext& getContext() const { return context_; }

    // Additional getters for AMOURANTH integration
    VkDevice getDevice() const { return context_.device; }
    VkDeviceMemory getVertexBufferMemory() const { return context_.vertexBufferMemory; }
    VkPipeline getGraphicsPipeline() const { return context_.pipeline; }
    VkRenderPass getRenderPass() const { return context_.renderPass; }
    VkFramebuffer getFramebuffer() const { return context_.swapchainFramebuffers[currentImageIndex_]; }

private:
    VulkanContext context_;
    VkInstance instance_;
    VkSurfaceKHR surface_;
    VulkanSwapchainManager swapchainManager_;
    VulkanPipelineManager pipelineManager_;
    const Logging::Logger& logger_; // Added logger reference
    uint32_t currentFrame_ = 0;
    uint32_t currentImageIndex_ = 0;
    std::chrono::steady_clock::time_point lastFrameTime_; // Added for deltaTime calculation
};

#endif // VULKAN_INIT_HPP