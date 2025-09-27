#ifndef VULKAN_FUNC_HPP
#define VULKAN_FUNC_HPP

// AMOURANTH RTX Engine - September 2025
// Utility functions header for Vulkan resource creation and management.
// Static methods for device selection, swapchain, pipelines, buffers, etc.
// Zachary Geurts, 2025

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>  // For core types if needed
#include <vector>
#include <string>
#include <optional>
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>  // For transforms if used in shaders

class VulkanInitializer {
public:
    // Queue family indices container.
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() const {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    // Device feature and extension requirements for ray tracing support.
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

    /**
     * Selects the best physical device based on score (discrete GPU preferred, NVIDIA bonus).
     * Verifies queue families, extensions, and ray tracing features.
     * @param instance Vulkan instance.
     * @param[out] physicalDevice Selected device.
     * @param[out] graphicsFamily Graphics queue family index.
     * @param[out] presentFamily Present queue family index.
     * @param surface Presentation surface.
     * @param preferNvidia Prioritize NVIDIA if true (default).
     * @param logMessage Optional logger callback.
     * @throws std::runtime_error On no suitable device.
     */
    static void createPhysicalDevice(VkInstance instance, VkPhysicalDevice& physicalDevice, uint32_t& graphicsFamily, uint32_t& presentFamily, VkSurfaceKHR surface, bool preferNvidia = true, std::function<void(const std::string&)> logMessage = [](const std::string&) {});

    /**
     * Creates logical device with required queues and features.
     * @param physicalDevice Selected physical device.
     * @param[out] device Created VkDevice.
     * @param[out] graphicsQueue Graphics queue handle.
     * @param[out] presentQueue Present queue handle.
     * @param graphicsFamily Graphics family index.
     * @param presentFamily Present family index.
     * @throws std::runtime_error On creation failure.
     */
    static void createLogicalDevice(VkPhysicalDevice physicalDevice, VkDevice& device, VkQueue& graphicsQueue, VkQueue& presentQueue, uint32_t graphicsFamily, uint32_t presentFamily);

    /**
     * Creates swapchain with optimal format (B8G8R8A8_UNORM preferred) and present mode (MAILBOX if available).
     * @param physicalDevice Physical device.
     * @param device Logical device.
     * @param surface Surface.
     * @param[out] swapchain Created swapchain.
     * @param[out] swapchainImages Retrieved images.
     * @param[out] swapchainImageViews Created views.
     * @param[out] swapchainFormat Selected format.
     * @param graphicsFamily Graphics family.
     * @param presentFamily Present family.
     * @param width Window width.
     * @param height Window height.
     * @throws std::runtime_error On creation failure.
     */
    static void createSwapchain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, VkSwapchainKHR& swapchain,
                                std::vector<VkImage>& swapchainImages, std::vector<VkImageView>& swapchainImageViews, VkFormat& swapchainFormat,
                                uint32_t graphicsFamily, uint32_t presentFamily, int width, int height);

    /**
     * Creates a simple forward render pass with color clear and present layout.
     * @param device Logical device.
     * @param[out] renderPass Created render pass.
     * @param format Swapchain image format.
     * @throws std::runtime_error On creation failure.
     */
    static void createRenderPass(VkDevice device, VkRenderPass& renderPass, VkFormat format);

    /**
     * Loads and creates a shader module from SPIR-V file.
     * @param device Logical device.
     * @param filename Path to .spv file.
     * @return Created VkShaderModule (destroy with vkDestroyShaderModule).
     * @throws std::runtime_error If file not found or invalid SPIR-V.
     */
    static VkShaderModule createShaderModule(VkDevice device, const std::string& filename);

    /**
     * Creates descriptor set layout for a single combined image sampler (binding 0, fragment stage).
     * @param device Logical device.
     * @param[out] descriptorSetLayout Created layout.
     * @throws std::runtime_error On creation failure.
     */
    static void createDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout& descriptorSetLayout);

    /**
     * Creates descriptor pool and allocates a single set for image sampler.
     * @param device Logical device.
     * @param descriptorSetLayout Layout for the set.
     * @param[out] descriptorPool Created pool.
     * @param[out] descriptorSet Allocated set.
     * @param sampler Sampler (unused; kept for future expansion).
     * @throws std::runtime_error On creation failure.
     */
    static void createDescriptorPoolAndSet(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, VkDescriptorPool& descriptorPool, VkDescriptorSet& descriptorSet, VkSampler sampler);

    /**
     * Creates a linear anisotropic sampler with repeat addressing.
     * @param device Logical device.
     * @param physicalDevice For limits (maxAnisotropy).
     * @param[out] sampler Created sampler.
     * @throws std::runtime_error On creation failure.
     */
    static void createSampler(VkDevice device, VkPhysicalDevice physicalDevice, VkSampler& sampler);

    /**
     * Creates a graphics pipeline for textured triangle rendering with push constants.
     * Supports vertex attributes: pos/normal/uv/tangent/bitangent/color.
     * Blending: Alpha src/dst. No depth.
     * @param device Logical device.
     * @param renderPass Render pass.
     * @param[out] pipeline Created pipeline.
     * @param[out] pipelineLayout Created layout (with push constants and descriptor set).
     * @param descriptorSetLayout Descriptor layout.
     * @param width Viewport width.
     * @param height Viewport height.
     * @throws std::runtime_error On creation failure (shaders must exist).
     */
    static void createGraphicsPipeline(VkDevice device, VkRenderPass renderPass, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout, VkDescriptorSetLayout& descriptorSetLayout, int width, int height);

    /**
     * Creates framebuffers for each swapchain image view.
     * @param device Logical device.
     * @param renderPass Render pass.
     * @param swapchainImageViews Image views.
     * @param[out] swapchainFramebuffers Created framebuffers.
     * @param width Framebuffer width.
     * @param height Framebuffer height.
     * @throws std::runtime_error On creation failure.
     */
    static void createFramebuffers(VkDevice device, VkRenderPass renderPass, std::vector<VkImageView>& swapchainImageViews, std::vector<VkFramebuffer>& swapchainFramebuffers, int width, int height);

    /**
     * Creates a resettable command pool for the graphics family.
     * @param device Logical device.
     * @param[out] commandPool Created pool.
     * @param graphicsFamily Queue family index.
     * @throws std::runtime_error On creation failure.
     */
    static void createCommandPool(VkDevice device, VkCommandPool& commandPool, uint32_t graphicsFamily);

    /**
     * Allocates primary command buffers matching framebuffer count.
     * @param device Logical device.
     * @param commandPool Command pool.
     * @param[out] commandBuffers Allocated buffers (resized to match framebuffers).
     * @param swapchainFramebuffers Framebuffers (for sizing).
     * @throws std::runtime_error On allocation failure.
     */
    static void createCommandBuffers(VkDevice device, VkCommandPool commandPool, std::vector<VkCommandBuffer>& commandBuffers, std::vector<VkFramebuffer>& swapchainFramebuffers);

    /**
     * Creates synchronization objects for multi-frame rendering.
     * Semaphores for image available/render finished; fences for in-flight.
     * @param device Logical device.
     * @param[out] imageAvailableSemaphores Acquire semaphores.
     * @param[out] renderFinishedSemaphores Render complete semaphores.
     * @param[out] inFlightFences Frame fences (signaled initially).
     * @param maxFramesInFlight Max concurrent frames (default 2; match swapchain images recommended).
     * @throws std::runtime_error On creation failure.
     */
    static void createSyncObjects(VkDevice device, std::vector<VkSemaphore>& imageAvailableSemaphores, std::vector<VkSemaphore>& renderFinishedSemaphores, std::vector<VkFence>& inFlightFences, uint32_t maxFramesInFlight = 2);

    /**
     * Creates a buffer with specified usage and properties.
     * Supports device address allocation if flagged.
     * @param device Logical device.
     * @param physicalDevice For memory properties.
     * @param size Buffer size in bytes.
     * @param usage Buffer usage flags.
     * @param props Memory property flags.
     * @param[out] buffer Created buffer.
     * @param[out] memory Allocated memory.
     * @throws std::runtime_error On creation/allocation failure.
     */
    static void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer& buffer, VkDeviceMemory& memory);

    /**
     * Copies data from src buffer to dst buffer using a one-time command buffer.
     * @param device Logical device.
     * @param commandPool Transient pool.
     * @param graphicsQueue Queue for submission.
     * @param src Source buffer.
     * @param dst Destination buffer.
     * @param size Bytes to copy.
     */
    static void copyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkBuffer src, VkBuffer dst, VkDeviceSize size);

    /**
     * Creates vertex buffer from positions, filling defaults for other attributes.
     * Uses staging buffer for host-visible upload.
     * @param device Logical device.
     * @param physicalDevice For memory.
     * @param commandPool For copy commands.
     * @param graphicsQueue For submission.
     * @param[out] vertexBuffer Created buffer.
     * @param[out] vertexBufferMemory Allocated memory.
     * @param vertices Position data (expanded to full Vertex struct).
     * @throws std::runtime_error On empty data or failure.
     */
    static void createVertexBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                                   VkBuffer& vertexBuffer, VkDeviceMemory& vertexBufferMemory, const std::vector<glm::vec3>& vertices);

    /**
     * Creates index buffer from uint32 indices using staging upload.
     * @param device Logical device.
     * @param physicalDevice For memory.
     * @param commandPool For copy commands.
     * @param graphicsQueue For submission.
     * @param[out] indexBuffer Created buffer.
     * @param[out] indexBufferMemory Allocated memory.
     * @param indices Index data.
     * @throws std::runtime_error On empty data or failure.
     */
    static void createIndexBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                                  VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory, const std::vector<uint32_t>& indices);
};

#endif // VULKAN_FUNC_HPP