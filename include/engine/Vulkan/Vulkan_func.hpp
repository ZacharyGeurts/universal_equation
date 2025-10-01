#ifndef VULKAN_FUNC_HPP
#define VULKAN_FUNC_HPP

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <functional>
#include <optional>
#include <glm/glm.hpp>
#include "engine/Vulkan/Vulkan_func_pipe.hpp"
#include "engine/Vulkan/Vulkan_func_swapchain.hpp"
// AMOURANTH RTX September 2025
// Zachary Geurts 2025

namespace VulkanInitializer {

    // Structure to hold queue family indices for graphics and presentation
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        bool isComplete() const { return graphicsFamily.has_value() && presentFamily.has_value(); }
    };

    // Structure defining Vulkan device requirements, including extensions and features
    struct DeviceRequirements {
        std::vector<const char*> extensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
            VK_KHR_MAINTENANCE_4_EXTENSION_NAME,
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
        };
        VkPhysicalDeviceMaintenance4Features maintenance4Features = {
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES,
            nullptr,
            VK_TRUE // Enable maintenance4 features
        };
        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingFeatures = {
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
            nullptr,
            VK_TRUE,  // Enable ray tracing pipeline
            VK_FALSE, // Disable shader group handle capture replay
            VK_FALSE, // Disable mixed capture replay
            VK_FALSE, // Disable indirect trace rays
            VK_FALSE  // Disable primitive culling
        };
        VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures = {
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
            nullptr,
            VK_TRUE,  // Enable acceleration structures
            VK_FALSE, // Disable capture replay
            VK_FALSE, // Disable indirect build
            VK_FALSE, // Disable host commands
            VK_FALSE  // Disable descriptor binding update after bind
        };
        VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures = {
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
            nullptr,
            VK_TRUE,  // Enable buffer device address
            VK_FALSE, // Disable capture replay
            VK_FALSE  // Disable multi-device
        };
    };

    // Initialize all Vulkan resources for rendering
    void initializeVulkan(
        VkInstance instance, VkPhysicalDevice& physicalDevice, VkDevice& device, VkSurfaceKHR surface,
        VkQueue& graphicsQueue, VkQueue& presentQueue, uint32_t& graphicsFamily, uint32_t& presentFamily,
        VkSwapchainKHR& swapchain, std::vector<VkImage>& swapchainImages, std::vector<VkImageView>& swapchainImageViews,
        VkRenderPass& renderPass, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout,
        VkDescriptorSetLayout& descriptorSetLayout, std::vector<VkFramebuffer>& swapchainFramebuffers,
        VkCommandPool& commandPool, std::vector<VkCommandBuffer>& commandBuffers,
        std::vector<VkSemaphore>& imageAvailableSemaphores, std::vector<VkSemaphore>& renderFinishedSemaphores,
        std::vector<VkFence>& inFlightFences, VkBuffer& vertexBuffer, VkDeviceMemory& vertexBufferMemory,
        VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory,
        VkBuffer& sphereStagingBuffer, VkDeviceMemory& sphereStagingBufferMemory,
        VkBuffer& indexStagingBuffer, VkDeviceMemory& indexStagingBufferMemory,
        VkDescriptorSetLayout& descriptorSetLayout2, VkDescriptorPool& descriptorPool, VkDescriptorSet& descriptorSet,
        VkSampler& sampler, VkShaderModule& vertShaderModule, VkShaderModule& fragShaderModule,
        const std::vector<glm::vec3>& vertices, const std::vector<uint32_t>& indices, int width, int height);

    // Initialize quad vertex and index buffers
    void initializeQuadBuffers(
        VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
        VkBuffer& quadVertexBuffer, VkDeviceMemory& quadVertexBufferMemory,
        VkBuffer& quadIndexBuffer, VkDeviceMemory& quadIndexBufferMemory,
        VkBuffer& quadStagingBuffer, VkDeviceMemory& quadStagingBufferMemory,
        VkBuffer& quadIndexStagingBuffer, VkDeviceMemory& quadIndexStagingBufferMemory,
        const std::vector<glm::vec3>& quadVertices, const std::vector<uint32_t>& quadIndices);

    // Clean up Vulkan resources
    void cleanupVulkan(
        VkDevice device, VkSwapchainKHR& swapchain, std::vector<VkImageView>& swapchainImageViews,
        std::vector<VkFramebuffer>& swapchainFramebuffers, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout,
        VkRenderPass& renderPass, VkCommandPool& commandPool, std::vector<VkCommandBuffer>& commandBuffers,
        std::vector<VkSemaphore>& imageAvailableSemaphores, std::vector<VkSemaphore>& renderFinishedSemaphores,
        std::vector<VkFence>& inFlightFences, VkBuffer& vertexBuffer, VkDeviceMemory& vertexBufferMemory,
        VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory,
        VkDescriptorSetLayout& descriptorSetLayout, VkDescriptorPool& descriptorPool, VkDescriptorSet& descriptorSet,
        VkSampler& sampler, VkBuffer& sphereStagingBuffer, VkDeviceMemory& sphereStagingBufferMemory,
        VkBuffer& indexStagingBuffer, VkDeviceMemory& indexStagingBufferMemory,
        VkShaderModule& vertShaderModule, VkShaderModule& fragShaderModule);

    // Create a physical device with graphics and presentation support
    void createPhysicalDevice(VkInstance instance, VkPhysicalDevice& physicalDevice, uint32_t& graphicsFamily,
                             uint32_t& presentFamily, VkSurfaceKHR surface, bool preferNvidia,
                             std::function<void(const std::string&)> logMessage);

    // Create a logical device and retrieve graphics/present queues
    void createLogicalDevice(VkPhysicalDevice physicalDevice, VkDevice& device, VkQueue& graphicsQueue,
                             VkQueue& presentQueue, uint32_t graphicsFamily, uint32_t presentFamily);

    // Create a command pool for allocating command buffers
    void createCommandPool(VkDevice device, VkCommandPool& commandPool, uint32_t graphicsFamily);

    // Allocate command buffers for rendering
    void createCommandBuffers(VkDevice device, VkCommandPool commandPool, std::vector<VkCommandBuffer>& commandBuffers,
                              std::vector<VkFramebuffer>& swapchainFramebuffers);

    // Create synchronization objects (semaphores and fences)
    void createSyncObjects(VkDevice device, std::vector<VkSemaphore>& imageAvailableSemaphores,
                           std::vector<VkSemaphore>& renderFinishedSemaphores, std::vector<VkFence>& inFlightFences,
                           uint32_t maxFramesInFlight);

    // Create a buffer with specified usage and memory properties
    void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags props, VkBuffer& buffer, VkDeviceMemory& memory);

    // Copy data between buffers
    void copyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkBuffer src, VkBuffer dst,
                    VkDeviceSize size);

    // Create a vertex buffer with staging
    void createVertexBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                            VkBuffer& vertexBuffer, VkDeviceMemory& vertexBufferMemory, VkBuffer& stagingBuffer,
                            VkDeviceMemory& stagingBufferMemory, const std::vector<glm::vec3>& vertices);

    // Create an index buffer with staging
    void createIndexBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                           VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory, VkBuffer& stagingBuffer,
                           VkDeviceMemory& stagingBufferMemory, const std::vector<uint32_t>& indices);

}

#endif // VULKAN_FUNC_HPP