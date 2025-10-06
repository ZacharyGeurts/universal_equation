#ifndef VULKAN_FUNC_HPP
#define VULKAN_FUNC_HPP
// AMOURANTH RTX Engine, October 2025 - Vulkan core utilities for initialization and buffer management.
// Supports Windows/Linux (X11/Wayland); no mutexes; compatible with voxel geometry rendering.
// Dependencies: Vulkan 1.3+, GLM, C++20 standard library.
// Usage: Core Vulkan initialization for VulkanRenderer; integrates with Vulkan_func_pipe.hpp and Vulkan_func_swapchain.hpp.
// Zachary Geurts 2025

#include <vulkan/vulkan.h>
#include <vector>
#include <span>
#include <string_view>
#include <optional>
#include <array>
#include <glm/glm.hpp>
#include "engine/logging.hpp" // For Logging::Logger
#include "engine/core.hpp"   // For PushConstants

namespace VulkanInitializer {

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    
    [[nodiscard]] constexpr bool isComplete() const noexcept {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct DeviceRequirements {
    static constexpr std::array<const char*, 6> extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_MAINTENANCE_4_EXTENSION_NAME
    };

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
        .pNext = nullptr,
        .rayTracingPipeline = VK_TRUE,
        .rayTracingPipelineShaderGroupHandleCaptureReplay = VK_FALSE,
        .rayTracingPipelineShaderGroupHandleCaptureReplayMixed = VK_FALSE,
        .rayTracingPipelineTraceRaysIndirect = VK_FALSE,
        .rayTraversalPrimitiveCulling = VK_FALSE
    };

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
        .pNext = nullptr,
        .accelerationStructure = VK_TRUE,
        .accelerationStructureCaptureReplay = VK_FALSE,
        .accelerationStructureIndirectBuild = VK_FALSE,
        .accelerationStructureHostCommands = VK_FALSE,
        .descriptorBindingAccelerationStructureUpdateAfterBind = VK_FALSE
    };

    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
        .pNext = nullptr,
        .bufferDeviceAddress = VK_TRUE,
        .bufferDeviceAddressCaptureReplay = VK_FALSE,
        .bufferDeviceAddressMultiDevice = VK_FALSE
    };

    VkPhysicalDeviceMaintenance4Features maintenance4Features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES,
        .pNext = nullptr,
        .maintenance4 = VK_TRUE
    };
};

void initializeVulkan(
    VkInstance instance, 
    VkPhysicalDevice& physicalDevice, 
    VkDevice& device, 
    VkSurfaceKHR surface,
    VkQueue& graphicsQueue, 
    VkQueue& presentQueue, 
    uint32_t& graphicsFamily, 
    uint32_t& presentFamily,
    VkSwapchainKHR& swapchain, 
    std::vector<VkImage>& swapchainImages, 
    std::vector<VkImageView>& swapchainImageViews,
    VkRenderPass& renderPass, 
    VkPipeline& pipeline, 
    VkPipelineLayout& pipelineLayout,
    VkDescriptorSetLayout& descriptorSetLayout, 
    std::vector<VkFramebuffer>& swapchainFramebuffers,
    VkCommandPool& commandPool, 
    std::vector<VkCommandBuffer>& commandBuffers,
    std::vector<VkSemaphore>& imageAvailableSemaphores, 
    std::vector<VkSemaphore>& renderFinishedSemaphores,
    std::vector<VkFence>& inFlightFences, 
    VkBuffer& vertexBuffer, 
    VkDeviceMemory& vertexBufferMemory,
    VkBuffer& indexBuffer, 
    VkDeviceMemory& indexBufferMemory,
    VkBuffer& sphereStagingBuffer, 
    VkDeviceMemory& sphereStagingBufferMemory,
    VkBuffer& indexStagingBuffer, 
    VkDeviceMemory& indexStagingBufferMemory,
    VkDescriptorSetLayout& descriptorSetLayout2, 
    VkDescriptorPool& descriptorPool, 
    VkDescriptorSet& descriptorSet,
    VkSampler& sampler, 
    VkShaderModule& vertShaderModule, 
    VkShaderModule& fragShaderModule,
    std::span<const glm::vec3> vertices, 
    std::span<const uint32_t> indices, 
    int width, 
    int height,
    const Logging::Logger& logger);

void initializeQuadBuffers(
    VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
    VkBuffer& quadVertexBuffer, VkDeviceMemory& quadVertexBufferMemory,
    VkBuffer& quadIndexBuffer, VkDeviceMemory& quadIndexBufferMemory,
    VkBuffer& quadStagingBuffer, VkDeviceMemory& quadStagingBufferMemory,
    VkBuffer& quadIndexStagingBuffer, VkDeviceMemory& quadIndexStagingBufferMemory,
    std::span<const glm::vec3> quadVertices, std::span<const uint32_t> quadIndices,
    const Logging::Logger& logger);

void initializeVoxelBuffers(
    VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
    VkBuffer& voxelVertexBuffer, VkDeviceMemory& voxelVertexBufferMemory,
    VkBuffer& voxelIndexBuffer, VkDeviceMemory& voxelIndexBufferMemory,
    VkBuffer& voxelStagingBuffer, VkDeviceMemory& voxelStagingBufferMemory,
    VkBuffer& voxelIndexStagingBuffer, VkDeviceMemory& voxelIndexStagingBufferMemory,
    std::span<const glm::vec3> voxelVertices, std::span<const uint32_t> voxelIndices,
    const Logging::Logger& logger);

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
    VkShaderModule& vertShaderModule, VkShaderModule& fragShaderModule,
    const Logging::Logger& logger);

void createPhysicalDevice(
    VkInstance instance, VkPhysicalDevice& physicalDevice, uint32_t& graphicsFamily,
    uint32_t& presentFamily, VkSurfaceKHR surface, bool preferNvidia, const Logging::Logger& logger);

void createLogicalDevice(
    VkPhysicalDevice physicalDevice, VkDevice& device, VkQueue& graphicsQueue,
    VkQueue& presentQueue, uint32_t graphicsFamily, uint32_t presentFamily, const Logging::Logger& logger);

VkSurfaceFormatKHR selectSurfaceFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, const Logging::Logger& logger);

void createCommandPool(VkDevice device, VkCommandPool& commandPool, uint32_t graphicsFamily, const Logging::Logger& logger);

void createCommandBuffers(
    VkDevice device, VkCommandPool commandPool, std::vector<VkCommandBuffer>& commandBuffers,
    std::vector<VkFramebuffer>& swapchainFramebuffers, const Logging::Logger& logger);

void createSyncObjects(
    VkDevice device, std::vector<VkSemaphore>& imageAvailableSemaphores,
    std::vector<VkSemaphore>& renderFinishedSemaphores, std::vector<VkFence>& inFlightFences,
    uint32_t maxFramesInFlight, const Logging::Logger& logger);

void createBuffer(
    VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
    VkMemoryPropertyFlags props, VkBuffer& buffer, VkDeviceMemory& memory, const Logging::Logger& logger);

void copyBuffer(
    VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkBuffer src, VkBuffer dst,
    VkDeviceSize size, const Logging::Logger& logger);

void createVertexBuffer(
    VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
    VkBuffer& vertexBuffer, VkDeviceMemory& vertexBufferMemory, VkBuffer& stagingBuffer,
    VkDeviceMemory& stagingBufferMemory, std::span<const glm::vec3> vertices, const Logging::Logger& logger);

void createIndexBuffer(
    VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
    VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory, VkBuffer& stagingBuffer,
    VkDeviceMemory& stagingBufferMemory, std::span<const uint32_t> indices, const Logging::Logger& logger);

} // namespace VulkanInitializer

#endif // VULKAN_FUNC_HPP