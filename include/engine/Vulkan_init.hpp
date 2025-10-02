#ifndef VULKAN_INIT_HPP
#define VULKAN_INIT_HPP

#include <vulkan/vulkan.h>
#include <vector>
#include <glm/glm.hpp>
#include <stdexcept>
#include <iostream>
#include <functional>  // For logging callback
#include "engine/Vulkan/Vulkan_func.hpp"
// AMOURANTH RTX September 2025
// Zachary Geurts 2025
// Struct to group Vulkan resources for better manageability
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
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> swapchainFramebuffers;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;  // Assume used (e.g., bound in command buffers)
    VkSampler sampler = VK_NULL_HANDLE;
    VkShaderModule vertShaderModule = VK_NULL_HANDLE;  // Caller provides; not created here
    VkShaderModule fragShaderModule = VK_NULL_HANDLE;  // Caller provides; not created here

    // Optional quad buffers (init separately if needed)
    VkBuffer quadVertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory quadVertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer quadIndexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory quadIndexBufferMemory = VK_NULL_HANDLE;
};

// RAII class for Vulkan resources: init in ctor, cleanup in dtor
class VulkanRenderer {
public:
    // Constructor: Initializes Vulkan with provided instance/surface/shaders/vertices
    VulkanRenderer(
        VkInstance instance, VkSurfaceKHR surface,
        const std::vector<glm::vec3>& vertices, const std::vector<uint32_t>& indices,
        VkShaderModule vertShaderModule, VkShaderModule fragShaderModule,
        int width, int height,
        std::function<void(const std::string&)> logger = [](const std::string& msg) { std::cerr << "Vulkan: " << msg << "\n"; });

    // Destructor: Automatically cleans up
    ~VulkanRenderer();

    // Access the context (const to prevent modification)
    const VulkanContext& getContext() const { return context_; }

    // Optional: Initialize quad buffers (call after ctor if needed)
    void initializeQuadBuffers(
        const std::vector<glm::vec3>& quadVertices, const std::vector<uint32_t>& quadIndices);

private:
    VulkanContext context_;
    VkInstance instance_ = VK_NULL_HANDLE;  // Not owned; caller manages
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;  // Not owned; caller manages
    std::function<void(const std::string&)> logger_;

    // Internal init (called by ctor)
    void initializeVulkan(
        const std::vector<glm::vec3>& vertices, const std::vector<uint32_t>& indices,
        VkShaderModule vertShaderModule, VkShaderModule fragShaderModule,
        int width, int height);

    // Internal cleanup (called by dtor)
    void cleanupVulkan();
};

#endif // VULKAN_INIT_HPP