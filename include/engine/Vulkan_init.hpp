// Vulkan_init.hpp
// Vulkan core initialization for AMOURANTH RTX Engine
// AMOURANTH RTX Engine, October 2025
// Zachary Geurts 2025

#ifndef VULKAN_INIT_HPP
#define VULKAN_INIT_HPP

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <span>
#include <optional>
#include <array>
#include <string>
#include <source_location>
#include "engine/logging.hpp"

// Forward declaration of AMOURANTH
class AMOURANTH;

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

    VkPhysicalDeviceMaintenance4Features maintenance4Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES,
        .pNext = nullptr,
        .maintenance4 = VK_TRUE
    };

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
        .pNext = nullptr,
        .rayTracingPipeline = VK_TRUE,
        .rayTracingPipelineShaderGroupHandleCaptureReplay = VK_FALSE,
        .rayTracingPipelineShaderGroupHandleCaptureReplayMixed = VK_FALSE,
        .rayTracingPipelineTraceRaysIndirect = VK_FALSE,
        .rayTraversalPrimitiveCulling = VK_FALSE
    };

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
        .pNext = nullptr,
        .accelerationStructure = VK_TRUE,
        .accelerationStructureCaptureReplay = VK_FALSE,
        .accelerationStructureIndirectBuild = VK_FALSE,
        .accelerationStructureHostCommands = VK_FALSE,
        .descriptorBindingAccelerationStructureUpdateAfterBind = VK_FALSE
    };

    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
        .pNext = nullptr,
        .bufferDeviceAddress = VK_TRUE,
        .bufferDeviceAddressCaptureReplay = VK_FALSE,
        .bufferDeviceAddressMultiDevice = VK_FALSE
    };
};

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
    VkSampler sampler = VK_NULL_HANDLE;

    // KHR extension function pointers
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = nullptr;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR = nullptr;
    PFN_vkCmdWriteAccelerationStructuresPropertiesKHR vkCmdWriteAccelerationStructuresPropertiesKHR = nullptr;
    PFN_vkCmdCopyAccelerationStructureKHR vkCmdCopyAccelerationStructureKHR = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
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
};

class VulkanPipelineManager {
public:
    VulkanPipelineManager(VulkanContext& context);
    ~VulkanPipelineManager();
    void initializePipeline(int width, int height);
    void cleanupPipeline();

    VkShaderModule getVertShaderModule() const { return vertShaderModule_; }
    VkShaderModule getFragShaderModule() const { return fragShaderModule_; }

private:
    VkShaderModule createShaderModule(const std::string& filename);
    VulkanContext& context_;
    VkShaderModule vertShaderModule_;
    VkShaderModule fragShaderModule_;
};

class VulkanRenderer {
public:
    VulkanRenderer(VkInstance instance, VkSurfaceKHR surface,
                  std::span<const glm::vec3> vertices, std::span<const uint32_t> indices,
                  int width, int height);
    ~VulkanRenderer() noexcept;
    void renderFrame(AMOURANTH*);
    void handleResize(int width, int height);
    VkDevice getDevice() const;
    VkDeviceMemory getVertexBufferMemory() const;
    VkPipeline getGraphicsPipeline() const;

private:
    void initializePhysicalDevice(VkInstance instance);
    void initializeLogicalDevice();
    void initializeBuffers(std::span<const glm::vec3> vertices, std::span<const uint32_t> indices);
    void loadExtensionFunctions();
    void cleanupBuffers();
    void cleanupLogicalDevice();

    VulkanContext context_;
    VkSurfaceKHR surface_;
    VulkanSwapchainManager swapchainManager_;
    std::unique_ptr<VulkanPipelineManager> pipelineManager_;
};

namespace VulkanInitializer {
// Declarations moved to Vulkan_func.hpp to avoid duplicates
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
    std::span<const glm::vec3> vertices, std::span<const uint32_t> indices, int width, int height);

void initializeQuadBuffers(
    VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
    VkBuffer& quadVertexBuffer, VkDeviceMemory& quadVertexBufferMemory,
    VkBuffer& quadIndexBuffer, VkDeviceMemory& quadIndexBufferMemory,
    VkBuffer& quadStagingBuffer, VkDeviceMemory& quadStagingBufferMemory,
    VkBuffer& quadIndexStagingBuffer, VkDeviceMemory& quadIndexStagingBufferMemory,
    std::span<const glm::vec3> quadVertices, std::span<const uint32_t> quadIndices);

void initializeVoxelBuffers(
    VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
    VkBuffer& voxelVertexBuffer, VkDeviceMemory& voxelVertexBufferMemory,
    VkBuffer& voxelIndexBuffer, VkDeviceMemory& voxelIndexBufferMemory,
    VkBuffer& voxelStagingBuffer, VkDeviceMemory& voxelStagingBufferMemory,
    VkBuffer& voxelIndexStagingBuffer, VkDeviceMemory& voxelIndexStagingBufferMemory,
    std::span<const glm::vec3> voxelVertices, std::span<const uint32_t> voxelIndices);

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

void createPhysicalDevice(
    VkInstance instance, VkPhysicalDevice& physicalDevice, uint32_t& graphicsFamily,
    uint32_t& presentFamily, VkSurfaceKHR surface, bool preferNvidia);

void createLogicalDevice(
    VkPhysicalDevice physicalDevice, VkDevice& device, VkQueue& graphicsQueue,
    VkQueue& presentQueue, uint32_t graphicsFamily, uint32_t presentFamily);

void createCommandPool(
    VkDevice device, VkCommandPool& commandPool, uint32_t graphicsFamily);

void createCommandBuffers(
    VkDevice device, VkCommandPool commandPool, std::vector<VkCommandBuffer>& commandBuffers,
    std::vector<VkFramebuffer>& swapchainFramebuffers);

void createSyncObjects(
    VkDevice device, std::vector<VkSemaphore>& imageAvailableSemaphores,
    std::vector<VkSemaphore>& renderFinishedSemaphores, std::vector<VkFence>& inFlightFences,
    uint32_t maxFramesInFlight);

void createBuffer(
    VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
    VkMemoryPropertyFlags props, VkBuffer& buffer, VkDeviceMemory& memory);

void createVertexBuffer(
    VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
    VkBuffer& vertexBuffer, VkDeviceMemory& vertexBufferMemory, VkBuffer& stagingBuffer,
    VkDeviceMemory& stagingBufferMemory, std::span<const glm::vec3> vertices);

void createIndexBuffer(
    VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
    VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory, VkBuffer& stagingBuffer,
    VkDeviceMemory& stagingBufferMemory, std::span<const uint32_t> indices);

} // namespace VulkanInitializer

#endif // VULKAN_INIT_HPP