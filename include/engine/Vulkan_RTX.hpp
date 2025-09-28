#ifndef VULKAN_RTX_HPP
#define VULKAN_RTX_HPP

// AMOURANTH RTX Engine - September 2025
// Header for ray tracing (RTX) pipeline management.
// Manages acceleration structures (BLAS/TLAS), SBT, pipeline, and descriptors for hybrid rendering.
// Requires Vulkan 1.2+ with VK_KHR_ray_tracing_pipeline and VK_KHR_acceleration_structure extensions.
// Author: Zachary Geurts, 2025

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>  // For core types if not in vulkan.h
#include <vector>
#include <string>
#include <array>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>  // If transforms used
#include <mutex>
#include <future>
#include <optional>
#include <cstdint>  // For uint32_t etc.

struct PushConstants {
    alignas(16) glm::mat4 model;       // 64 bytes: Per-object world transform
    alignas(16) glm::mat4 view_proj;   // 64 bytes: Pre-computed view * projection (per-frame)
    static constexpr VkDeviceSize size = sizeof(glm::mat4) * 2;  // Explicit 128 bytes for validation
};

struct SBT {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkStridedDeviceAddressRegionKHR raygen = {};
    VkStridedDeviceAddressRegionKHR miss = {};
    VkStridedDeviceAddressRegionKHR hit = {};
    VkStridedDeviceAddressRegionKHR callable = {};
};

class VulkanRTX {
private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout dsLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool dsPool_ = VK_NULL_HANDLE;
    VkDescriptorSet ds_ = VK_NULL_HANDLE;
    VkPipelineLayout rtPipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline rtPipeline_ = VK_NULL_HANDLE;
    VkAccelerationStructureKHR blas_ = VK_NULL_HANDLE;
    VkBuffer blasBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory blasMemory_ = VK_NULL_HANDLE;
    VkAccelerationStructureKHR tlas_ = VK_NULL_HANDLE;
    VkBuffer tlasBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory tlasMemory_ = VK_NULL_HANDLE;
    SBT sbt_;

    // Shader feature flags (bitfield for future expansion)
    enum class ShaderFeatures : uint32_t {
        None = 0,
        AnyHit = 1 << 0,
        Intersection = 1 << 1,
        Callable = 1 << 2
    };
    ShaderFeatures shaderFeatures_ = ShaderFeatures::None;

    static std::mutex functionPtrMutex_;
    static std::mutex shaderModuleMutex_;
    static PFN_vkGetBufferDeviceAddress vkGetBufferDeviceAddress;
    static PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;
    static PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
    static PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
    static PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
    static PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
    static PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
    static PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;
    static PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;

public:
    /**
     * Constructs the VulkanRTX manager with the given device.
     * Loads ray tracing extension functions thread-safely.
     * @param device Valid VkDevice handle.
     * @throws std::runtime_error If extension loading fails.
     */
    explicit VulkanRTX(VkDevice device);
    ~VulkanRTX();

    // Deleted copy/move for resource safety
    VulkanRTX(const VulkanRTX&) = delete;
    VulkanRTX& operator=(const VulkanRTX&) = delete;
    VulkanRTX(VulkanRTX&&) = delete;
    VulkanRTX& operator=(VulkanRTX&&) = delete;

    /**
     * Initializes the ray tracing pipeline and resources.
     * Builds BLAS from provided vertex/index buffers (assumes positions only; stride=12 bytes).
     * Creates TLAS with single BLAS instance (identity transform).
     * Loads shaders asynchronously from 'assets/shaders/'.
     * @param physicalDevice For memory queries and properties.
     * @param commandPool For one-time build command buffers.
     * @param graphicsQueue For submitting builds.
     * @param vertexBuffer Device-local buffer with glm::vec3 positions.
     * @param indexBuffer Device-local uint32 indices (triangles).
     * @param vertexCount Number of vertices.
     * @param indexCount Number of indices (must be multiple of 3).
     * @param maxRayRecursionDepth Optional max recursion (defaults to 2; query hardware limit recommended).
     * @param vertexStride Optional vertex stride (defaults to sizeof(glm::vec3); for interleaved attributes).
     * @throws std::runtime_error On invalid inputs or creation failures.
     */
    void initializeRTX(
        VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
        VkBuffer vertexBuffer, VkBuffer indexBuffer, uint32_t vertexCount, uint32_t indexCount,
        uint32_t maxRayRecursionDepth = 2,
        uint64_t vertexStride = sizeof(glm::vec3)
    );

    /**
     * Cleans up all RTX resources (AS, pipeline, descriptors, buffers).
     * Safe to call multiple times.
     */
    void cleanupRTX();

    /**
     * Creates a 2D storage image suitable for ray tracing output.
     * Usage: STORAGE | COLOR_ATTACHMENT | TRANSFER_SRC/DST.
     * @param physicalDevice For memory allocation.
     * @param extent Image dimensions.
     * @param format Preferred format (e.g., VK_FORMAT_R8G8B8A8_UNORM).
     * @param[out] image Created VkImage.
     * @param[out] imageView Created VkImageView (2D, full subresource).
     * @param[out] memory Allocated VkDeviceMemory (device-local).
     * @throws std::runtime_error On creation failures.
     */
    void createStorageImage(
        VkPhysicalDevice physicalDevice, VkExtent2D extent, VkFormat format,
        VkImage& image, VkImageView& imageView, VkDeviceMemory& memory
    );

    /**
     * Updates descriptor set bindings for camera UBO (binding 2) and material SSBO (binding 3).
     * Call per-frame if buffers change.
     * @param cameraBuffer Uniform buffer with view/proj data.
     * @param materialBuffer Storage buffer with material properties.
     */
    void updateCameraAndMaterialDescriptor(VkBuffer cameraBuffer, VkBuffer materialBuffer);

    /**
     * Records ray tracing commands into the command buffer.
     * Binds pipeline/descriptors, pushes constants, traces rays, transitions output image.
     * Updates descriptors for output image and TLAS (if provided; reverts if temporary).
     * Assumes cmdBuffer is in recording state.
     * @param cmdBuffer Target command buffer.
     * @param extent Ray dispatch dimensions (width/height).
     * @param outputImage Storage image for results.
     * @param outputImageView View for the image.
     * @param pc Push constants (model/view_proj).
     * @param tlas Optional TLAS (defaults to internal; updates binding 0 temporarily).
     */
    void recordRayTracingCommands(
        VkCommandBuffer cmdBuffer, VkExtent2D extent, VkImage outputImage, VkImageView outputImageView,
        const PushConstants& pc, VkAccelerationStructureKHR tlas = VK_NULL_HANDLE
    );

    // Getters for integration with external render loops
    VkPipeline getPipeline() const { return rtPipeline_; }
    VkPipelineLayout getPipelineLayout() const { return rtPipelineLayout_; }
    const SBT& getSBT() const { return sbt_; }
    VkAccelerationStructureKHR getTLAS() const { return tlas_; }
    VkAccelerationStructureKHR getBLAS() const { return blas_; }
    VkDescriptorSet getDescriptorSet() const { return ds_; }
    bool hasShaderFeature(ShaderFeatures feature) const { return (static_cast<uint32_t>(shaderFeatures_) & static_cast<uint32_t>(feature)) != 0; }

private:
    void createDescriptorSetLayout();
    void createDescriptorPoolAndSet();
    void createRayTracingPipeline(uint32_t maxRayRecursionDepth);
    void createShaderBindingTable(VkPhysicalDevice physicalDevice);
    void createBottomLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                             VkBuffer vertexBuffer, VkBuffer indexBuffer, uint32_t vertexCount, uint32_t indexCount,
                             uint64_t vertexStride);
    void createTopLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue);
    void updateDescriptorSetForTLAS(VkAccelerationStructureKHR tlas);
    void createBuffer(VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer& buffer, VkDeviceMemory& memory);
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags props);
    VkShaderModule createShaderModule(const std::string& filename);
    bool shaderFileExists(const std::string& filename) const;
};

#endif // VULKAN_RTX_HPP