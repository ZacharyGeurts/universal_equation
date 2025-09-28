#ifndef VULKAN_FUNC_HPP
#define VULKAN_FUNC_HPP

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <functional>
#include <optional>
#include <glm/glm.hpp>

namespace VulkanInitializer {
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        bool isComplete() const { return graphicsFamily.has_value() && presentFamily.has_value(); }
    };

    struct DeviceRequirements {
        std::vector<const char*> extensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
            VK_KHR_MAINTENANCE_4_EXTENSION_NAME,
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
        };
        VkPhysicalDeviceMaintenance4Features maintenance4Features{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES,
            nullptr,
            VK_TRUE // maintenance4
        };
        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingFeatures{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
            nullptr,
            VK_TRUE, // rayTracingPipeline
            VK_FALSE, // rayTracingPipelineShaderGroupHandleCaptureReplay
            VK_FALSE, // rayTracingPipelineShaderGroupHandleCaptureReplayMixed
            VK_FALSE, // rayTracingPipelineTraceRaysIndirect
            VK_FALSE  // rayTraversalPrimitiveCulling
        };
        VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
            nullptr,
            VK_TRUE, // accelerationStructure
            VK_FALSE, // accelerationStructureCaptureReplay
            VK_FALSE, // accelerationStructureIndirectBuild
            VK_FALSE, // accelerationStructureHostCommands
            VK_FALSE  // descriptorBindingAccelerationStructureUpdateAfterBind
        };
        VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
            nullptr,
            VK_TRUE, // bufferDeviceAddress
            VK_FALSE, // bufferDeviceAddressCaptureReplay
            VK_FALSE  // bufferDeviceAddressMultiDevice
        };
    };

    VkSurfaceFormatKHR selectSurfaceFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
    void createPhysicalDevice(VkInstance instance, VkPhysicalDevice& physicalDevice, uint32_t& graphicsFamily, uint32_t& presentFamily, VkSurfaceKHR surface, bool preferNvidia, std::function<void(const std::string&)> logMessage);
    void createLogicalDevice(VkPhysicalDevice physicalDevice, VkDevice& device, VkQueue& graphicsQueue, VkQueue& presentQueue, uint32_t graphicsFamily, uint32_t presentFamily);
    void createSwapchain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, VkSwapchainKHR& swapchain,
                         std::vector<VkImage>& swapchainImages, std::vector<VkImageView>& swapchainImageViews, VkFormat& swapchainFormat,
                         uint32_t graphicsFamily, uint32_t presentFamily, int width, int height);
    void createRenderPass(VkDevice device, VkRenderPass& renderPass, VkFormat format);
    VkShaderModule createShaderModule(VkDevice device, const std::string& filename);
    void createDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout& descriptorSetLayout);
    void createDescriptorPoolAndSet(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, VkDescriptorPool& descriptorPool, VkDescriptorSet& descriptorSet, VkSampler sampler);
    void createSampler(VkDevice device, VkPhysicalDevice physicalDevice, VkSampler& sampler);
    void createGraphicsPipeline(VkDevice device, VkRenderPass renderPass, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout, VkDescriptorSetLayout& descriptorSetLayout, int width, int height, VkShaderModule& vertShaderModule, VkShaderModule& fragShaderModule);
    void createFramebuffers(VkDevice device, VkRenderPass renderPass, std::vector<VkImageView>& swapchainImageViews, std::vector<VkFramebuffer>& swapchainFramebuffers, int width, int height);
    void createCommandPool(VkDevice device, VkCommandPool& commandPool, uint32_t graphicsFamily);
    void createCommandBuffers(VkDevice device, VkCommandPool commandPool, std::vector<VkCommandBuffer>& commandBuffers, std::vector<VkFramebuffer>& swapchainFramebuffers);
    void createSyncObjects(VkDevice device, std::vector<VkSemaphore>& imageAvailableSemaphores, std::vector<VkSemaphore>& renderFinishedSemaphores, std::vector<VkFence>& inFlightFences, uint32_t maxFramesInFlight);
    void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer& buffer, VkDeviceMemory& memory);
    void copyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkBuffer src, VkBuffer dst, VkDeviceSize size);
    void createVertexBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                            VkBuffer& vertexBuffer, VkDeviceMemory& vertexBufferMemory, VkBuffer& stagingBuffer, VkDeviceMemory& stagingBufferMemory,
                            const std::vector<glm::vec3>& vertices);
    void createIndexBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                           VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory, VkBuffer& stagingBuffer, VkDeviceMemory& stagingBufferMemory,
                           const std::vector<uint32_t>& indices);
}

#endif