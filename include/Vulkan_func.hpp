#ifndef VULKAN_FUNC_HPP
#define VULKAN_FUNC_HPP

// Vulkan AMOURANTH RTX engine September, 2025 - Header for Vulkan utility functions.
// Provides static methods for creating and managing specific Vulkan resources.
// Used by Vulkan initialization routines.
// Zachary Geurts 2025, updated for robust NVIDIA GPU selection, surface handling, and device fallback

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <optional>
#include <functional>
#include <glm/glm.hpp>

class VulkanInitializer {
public:
    // Structure to hold queue family indices for clarity and safety.
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;  // Index of queue family supporting graphics operations.
        std::optional<uint32_t> presentFamily;   // Index of queue family supporting presentation to surface.

        // Checks if both required queue families have been found.
        bool isComplete() const {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    // Structure to encapsulate device extension and feature requirements.
    struct DeviceRequirements {
        // Required device extensions for swapchain and ray tracing support.
        std::vector<const char*> extensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
            VK_KHR_MAINTENANCE_4_EXTENSION_NAME,
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
        };

        // Feature structures chained via pNext for comprehensive enablement.
        VkPhysicalDeviceMaintenance4Features maintenance4Features = {
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES,
            nullptr,
            VK_TRUE
        };
        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingFeatures = {
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
            nullptr,
            VK_TRUE, VK_FALSE, VK_FALSE, VK_FALSE, VK_FALSE
        };
        VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures = {
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
            nullptr,
            VK_TRUE, VK_FALSE, VK_FALSE, VK_FALSE, VK_FALSE
        };
        VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures = {
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
            nullptr,
            VK_TRUE, VK_FALSE, VK_FALSE
        };
    };

    // Selects and configures a suitable physical device, preferring NVIDIA GPUs with fallback to other devices.
    static void createPhysicalDevice(VkInstance instance, VkPhysicalDevice& physicalDevice, uint32_t& graphicsFamily, uint32_t& presentFamily, VkSurfaceKHR surface, bool preferNvidia = true, std::function<void(const std::string&)> logMessage = [](const std::string&) {});

    // Creates the logical device with required queues and extensions (including ray tracing support).
    static void createLogicalDevice(VkPhysicalDevice physicalDevice, VkDevice& device, VkQueue& graphicsQueue, VkQueue& presentQueue, uint32_t graphicsFamily, uint32_t presentFamily);

    // Sets up the swapchain, images, and image views with optimal settings.
    static void createSwapchain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, VkSwapchainKHR& swapchain,
                                std::vector<VkImage>& swapchainImages, std::vector<VkImageView>& swapchainImageViews, VkFormat& swapchainFormat,
                                uint32_t graphicsFamily, uint32_t presentFamily, int width, int height);

    // Creates a basic render pass for color rendering.
    static void createRenderPass(VkDevice device, VkRenderPass& renderPass, VkFormat format);

    // Loads and creates a shader module from SPIR-V binary.
    static VkShaderModule createShaderModule(VkDevice device, const std::string& filename);

    // Creates the descriptor set layout for uniform and texture bindings.
    static void createDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout& descriptorSetLayout);

    // Creates descriptor pool, allocates a set, and prepares for updates.
    static void createDescriptorPoolAndSet(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, VkDescriptorPool& descriptorPool, VkDescriptorSet& descriptorSet, [[maybe_unused]] VkSampler sampler);

    // Creates a sampler for texture sampling.
    static void createSampler(VkDevice device, VkPhysicalDevice physicalDevice, VkSampler& sampler);

    // Configures the graphics pipeline with shaders, input assembly, etc.
    static void createGraphicsPipeline(VkDevice device, VkRenderPass renderPass, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout, VkDescriptorSetLayout& descriptorSetLayout, int width, int height);

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

#endif // VULKAN_FUNC_HPP