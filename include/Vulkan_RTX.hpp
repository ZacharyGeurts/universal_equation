#ifndef VULKAN_RTX_HPP
#define VULKAN_RTX_HPP

// AMOURANTH RTX engine - Header for ray tracing (RTX) pipeline management.
// Provides a class for initializing, building, and managing Vulkan ray tracing resources,
// including acceleration structures (BLAS/TLAS), shader binding table (SBT), and ray tracing pipeline.
// Designed for developer-friendliness: Thread-safe function pointer loading, exception-based error handling,
// comprehensive cleanup, and optional shader support (e.g., any-hit, intersection, callable).
// Optimizations: Lazy loading of extensions, one-time command buffers for AS builds, aligned SBT handles.
// Memory safety: All resources are RAII-managed via references; cleanup nulls handles and destroys in reverse order.
// Note: Requires Vulkan 1.2+ with ray tracing extensions enabled in the device.

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <mutex>
#include <future>
#include <optional>

// Forward declarations for Vulkan types to reduce header dependencies.
struct VkDevice_T;                // VkDevice
struct VkPhysicalDevice_T;        // VkPhysicalDevice
struct VkCommandPool_T;           // VkCommandPool
struct VkQueue_T;                 // VkQueue
struct VkBuffer_T;                // VkBuffer
struct VkPipeline_T;              // VkPipeline
struct VkPipelineLayout_T;        // VkPipelineLayout
struct VkAccelerationStructureKHR_T;  // VkAccelerationStructureKHR
struct VkDeviceMemory_T;          // VkDeviceMemory
struct VkShaderModule_T;          // VkShaderModule

class VulkanRTX {
public:
    // Structure for shader binding table (SBT) regions.
    struct ShaderBindingTable {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkStridedDeviceAddressRegionKHR raygen = {};
        VkStridedDeviceAddressRegionKHR miss = {};
        VkStridedDeviceAddressRegionKHR hit = {};
        VkStridedDeviceAddressRegionKHR callable = {};
    };

    // Push constants for ray tracing shaders.
    struct PushConstants {
        alignas(16) glm::mat4 viewProj;
        alignas(16) glm::vec3 camPos;
        float wavePhase;
        float cycleProgress;
        float zoomFactor;
        float interactionStrength;
        float darkMatter;
        float darkEnergy;
    };

    // Constructor: Initializes with device and loads extension function pointers.
    explicit VulkanRTX(VkDevice device);

    // Destructor: No-op; cleanup is explicit via cleanupRTX.
    ~VulkanRTX() = default;

    // Non-copyable and non-movable.
    VulkanRTX(const VulkanRTX&) = delete;
    VulkanRTX& operator=(const VulkanRTX&) = delete;
    VulkanRTX(VulkanRTX&&) = delete;
    VulkanRTX& operator=(VulkanRTX&&) = delete;

    // Initializes RTX resources: pipeline, BLAS, TLAS, SBT.
    void initializeRTX(
        VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
        VkBuffer vertexBuffer, VkBuffer indexBuffer, uint32_t vertexCount, uint32_t indexCount,
        VkPipeline& rtPipeline, VkPipelineLayout& rtPipelineLayout, ShaderBindingTable& sbt,
        VkAccelerationStructureKHR& tlas, VkBuffer& tlasBuffer, VkDeviceMemory& tlasMemory,
        VkAccelerationStructureKHR& blas, VkBuffer& blasBuffer, VkDeviceMemory& blasMemory
    );

    // Cleans up RTX resources.
    void cleanupRTX(
        VkPipeline& rtPipeline, VkPipelineLayout& rtPipelineLayout,
        VkBuffer& sbtBuffer, VkDeviceMemory& sbtMemory, VkAccelerationStructureKHR& tlas,
        VkBuffer& tlasBuffer, VkDeviceMemory& tlasMemory, VkAccelerationStructureKHR& blas,
        VkBuffer& blasBuffer, VkDeviceMemory& blasMemory
    );

private:
    VkDevice device_;  // Stored device handle.

    // Static mutexes for thread safety.
    static std::mutex functionPtrMutex_;
    static std::mutex shaderModuleMutex_;

    // Function pointers for ray tracing extensions.
    static PFN_vkGetBufferDeviceAddress vkGetBufferDeviceAddress;
    static PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;
    static PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
    static PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
    static PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
    static PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
    static PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
    static PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;
    static PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;

    // Creates ray tracing pipeline with optional shaders.
    void createRayTracingPipeline(VkPipeline& pipeline, VkPipelineLayout& rtPipelineLayout);

    // Creates shader binding table with aligned handles.
    void createShaderBindingTable(VkPhysicalDevice physicalDevice, VkPipeline pipeline, ShaderBindingTable& sbt);

    // Builds bottom-level acceleration structure (BLAS).
    void createBottomLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                             VkBuffer vertexBuffer, VkBuffer indexBuffer, uint32_t vertexCount, uint32_t indexCount,
                             VkAccelerationStructureKHR& as, VkBuffer& asBuffer, VkDeviceMemory& asMemory);

    // Builds top-level acceleration structure (TLAS).
    void createTopLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                          VkAccelerationStructureKHR blas, VkAccelerationStructureKHR& as, VkBuffer& asBuffer, VkDeviceMemory& asMemory);

    // Creates buffer with specified properties.
    void createBuffer(VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer& buffer, VkDeviceMemory& memory);

    // Creates shader module from SPIR-V file.
    VkShaderModule createShaderModule(const std::string& filename);

    // Checks if shader file exists.
    bool shaderFileExists(const std::string& filename) const;
};

#endif // VULKAN_RTX_HPP