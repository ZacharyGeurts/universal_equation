#ifndef VULKAN_INIT_HPP
#define VULKAN_INIT_HPP
// AMOURANTH RTX Engine, October 2025 - VulkanRenderer for Vulkan initialization and rendering.
// Initializes Vulkan resources, including swapchain, pipeline, and geometry buffers (vertex, index, quad, voxel).
// Thread-safe with no mutexes; designed for Windows/Linux (X11/Wayland).
// Dependencies: Vulkan 1.3+, GLM, C++20 standard library.
// Usage: Initialize with instance, surface, shaders, and geometry; call initializeQuadBuffers or initializeVoxelBuffers for additional geometries.
// Potential Issues: Ensure Vulkan extensions (VK_KHR_surface, platform-specific) and NVIDIA GPU selection for hybrid systems.
// Zachary Geurts 2025

#include <vulkan/vulkan.h>
#include <vector>
#include <glm/glm.hpp>
#include <stdexcept>
#include <span>
#include "engine/logging.hpp"
#include "engine/Vulkan/Vulkan_func.hpp"
#include "engine/Vulkan/Vulkan_func_swapchain.hpp"
#include "engine/Vulkan/Vulkan_func_pipe.hpp"

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
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VkShaderModule vertShaderModule = VK_NULL_HANDLE;
    VkShaderModule fragShaderModule = VK_NULL_HANDLE;
    // Quad buffers
    VkBuffer quadVertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory quadVertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer quadIndexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory quadIndexBufferMemory = VK_NULL_HANDLE;
    // Voxel buffers
    VkBuffer voxelVertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory voxelVertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer voxelIndexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory voxelIndexBufferMemory = VK_NULL_HANDLE;
};

// RAII class for Vulkan resources: init in ctor, cleanup in dtor
class VulkanRenderer {
public:
    VulkanRenderer(
        VkInstance instance, VkSurfaceKHR surface,
        std::span<const glm::vec3> vertices, std::span<const uint32_t> indices,
        VkShaderModule vertShaderModule, VkShaderModule fragShaderModule,
        int width, int height,
        const Logging::Logger& logger);

    ~VulkanRenderer();

    const VulkanContext& getContext() const { return context_; }

    void initializeQuadBuffers(std::span<const glm::vec3> quadVertices, std::span<const uint32_t> quadIndices);
    void initializeVoxelBuffers(std::span<const glm::vec3> voxelVertices, std::span<const uint32_t> voxelIndices);

private:
    VulkanContext context_;
    VkInstance instance_ = VK_NULL_HANDLE;  // Not owned; caller manages
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;  // Not owned; caller manages
    const Logging::Logger& logger_;

    void initializeVulkan(
        std::span<const glm::vec3> vertices, std::span<const uint32_t> indices,
        VkShaderModule vertShaderModule, VkShaderModule fragShaderModule,
        int width, int height);

    void cleanupVulkan();
};

#endif // VULKAN_INIT_HPP