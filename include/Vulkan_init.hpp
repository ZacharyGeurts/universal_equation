#ifndef VULKAN_INIT_HPP
#define VULKAN_INIT_HPP

// Vulkan AMOURANTH RTX engine - Header for base Vulkan initialization.
// Provides static methods for setting up and cleaning up Vulkan resources.
// Designed for developer-friendliness with clear parameters, exceptions on errors, and comprehensive comments.
// Note: This is for rasterization; RTX (ray tracing) is in a separate module.

#include <vulkan/vulkan.h>
#include <SDL3/SDL_vulkan.h>
#include <vector>
#include <string>  // Added for std::string
#include <glm/glm.hpp>

// Forward declarations for Vulkan types to reduce header dependencies.
struct VkInstance_T;              // VkInstance
struct VkPhysicalDevice_T;        // VkPhysicalDevice
struct VkDevice_T;                // VkDevice
struct VkSurfaceKHR_T;            // VkSurfaceKHR
struct VkQueue_T;                 // VkQueue
struct VkSwapchainKHR_T;          // VkSwapchainKHR
struct VkImage_T;                 // VkImage
struct VkImageView_T;             // VkImageView
struct VkRenderPass_T;            // VkRenderPass
struct VkPipeline_T;              // VkPipeline
struct VkPipelineLayout_T;        // VkPipelineLayout
struct VkFramebuffer_T;           // VkFramebuffer
struct VkCommandPool_T;           // VkCommandPool
struct VkCommandBuffer_T;         // VkCommandBuffer
struct VkSemaphore_T;             // VkSemaphore
struct VkFence_T;                 // VkFence
struct VkBuffer_T;                // VkBuffer
struct VkDeviceMemory_T;          // VkDeviceMemory
struct VkShaderModule_T;          // VkShaderModule

class VulkanInitializer {
public:
    // Initializes all core Vulkan resources: device, queues, swapchain, render pass, pipeline, framebuffers,
    // command pool/buffers, sync objects, and geometry buffers for a sphere.
    // Inputs: instance, surface, geometry data (vertices/indices), window dimensions.
    // Outputs: All other parameters are filled with created resources.
    static void initializeVulkan(
        VkInstance& instance, VkPhysicalDevice& physicalDevice, VkDevice& device, VkSurfaceKHR& surface,
        VkQueue& graphicsQueue, VkQueue& presentQueue, uint32_t& graphicsFamily, uint32_t& presentFamily,
        VkSwapchainKHR& swapchain, std::vector<VkImage>& swapchainImages, std::vector<VkImageView>& swapchainImageViews,
        VkRenderPass& renderPass, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout,
        std::vector<VkFramebuffer>& swapchainFramebuffers, VkCommandPool& commandPool,
        std::vector<VkCommandBuffer>& commandBuffers, VkSemaphore& imageAvailableSemaphore,
        VkSemaphore& renderFinishedSemaphore, VkFence& inFlightFence, VkBuffer& vertexBuffer,
        VkDeviceMemory& vertexBufferMemory, VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory,
        const std::vector<glm::vec3>& sphereVertices, const std::vector<uint32_t>& sphereIndices, int width, int height
    );

    // Initializes vertex and index buffers for quad geometry.
    // Reuses existing device, physical device, command pool, and queue.
    static void initializeQuadBuffers(
        VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
        VkBuffer& vertexBuffer, VkDeviceMemory& vertexBufferMemory, VkBuffer& indexBuffer,
        VkDeviceMemory& indexBufferMemory, const std::vector<glm::vec3>& vertices,
        const std::vector<uint32_t>& indices
    );

    // Cleans up all resources created by initializeVulkan and initializeQuadBuffers.
    // Safe to call even if some resources are null.
    // Instance and surface are not destroyed here (handled externally).
    static void cleanupVulkan(
        [[maybe_unused]] VkInstance instance, VkDevice& device, [[maybe_unused]] VkSurfaceKHR surface, VkSwapchainKHR& swapchain,
        std::vector<VkImageView>& swapchainImageViews, std::vector<VkFramebuffer>& swapchainFramebuffers,
        VkPipeline& pipeline, VkPipelineLayout& pipelineLayout, VkRenderPass& renderPass, VkCommandPool& commandPool,
        std::vector<VkCommandBuffer>& commandBuffers, VkSemaphore& imageAvailableSemaphore,
        VkSemaphore& renderFinishedSemaphore, VkFence& inFlightFence, VkBuffer& vertexBuffer,
        VkDeviceMemory& vertexBufferMemory, VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory,
        VkBuffer& quadVertexBuffer, VkDeviceMemory& quadVertexBufferMemory, VkBuffer& quadIndexBuffer,
        VkDeviceMemory& quadIndexBufferMemory
    );

private:
    // Selects and configures a suitable physical device, preferring discrete GPUs.
    static void createPhysicalDevice(VkInstance instance, VkPhysicalDevice& physicalDevice, uint32_t& graphicsFamily, uint32_t& presentFamily, VkSurfaceKHR surface);

    // Creates the logical device with required queues and extensions (including ray tracing support).
    static void createLogicalDevice(VkPhysicalDevice physicalDevice, VkDevice& device, VkQueue& graphicsQueue, VkQueue& presentQueue, uint32_t graphicsFamily, uint32_t presentFamily);

    // Sets up the swapchain, images, and image views with optimal settings.
    static void createSwapchain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, VkSwapchainKHR& swapchain,
                                std::vector<VkImage>& swapchainImages, std::vector<VkImageView>& swapchainImageViews, VkFormat& swapchainFormat, int width, int height);

    // Creates a basic render pass for color rendering.
    static void createRenderPass(VkDevice device, VkRenderPass& renderPass, VkFormat format);

    // Loads and creates a shader module from SPIR-V binary.
    static VkShaderModule createShaderModule(VkDevice device, const std::string& filename);

    // Configures the graphics pipeline with shaders, input assembly, etc.
    static void createGraphicsPipeline(VkDevice device, VkRenderPass renderPass, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout, int width, int height);

    // Creates framebuffers bound to swapchain image views.
    static void createFramebuffers(VkDevice device, VkRenderPass renderPass, std::vector<VkImageView>& swapchainImageViews, std::vector<VkFramebuffer>& swapchainFramebuffers, int width, int height);

    // Creates a command pool for the graphics queue family.
    static void createCommandPool(VkDevice device, VkCommandPool& commandPool, uint32_t graphicsFamily);

    // Allocates command buffers for rendering.
    static void createCommandBuffers(VkDevice device, VkCommandPool commandPool, std::vector<VkCommandBuffer>& commandBuffers, std::vector<VkFramebuffer>& swapchainFramebuffers);

    // Creates semaphores and fences for frame synchronization.
    static void createSyncObjects(VkDevice device, VkSemaphore& imageAvailableSemaphore, VkSemaphore& renderFinishedSemaphore, VkFence& inFlightFence);

    // Generic buffer creation utility.
    static void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer& buffer, VkDeviceMemory& memory);

    // Copies data between buffers using a command buffer.
    static void copyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkBuffer src, VkBuffer dst, VkDeviceSize size);

    // Creates vertex buffer with data upload via staging.
    static void createVertexBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                                   VkBuffer& vertexBuffer, VkDeviceMemory& vertexBufferMemory, const std::vector<glm::vec3>& vertices);

    // Creates index buffer with data upload via staging.
    static void createIndexBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                                  VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory, const std::vector<uint32_t>& indices);
};

#endif // VULKAN_INIT_HPP