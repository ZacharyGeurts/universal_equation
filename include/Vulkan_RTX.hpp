#ifndef VULKAN_RTX_HPP
#define VULKAN_RTX_HPP

// AMOURANTH RTX engine - Header for ray tracing (RTX) pipeline management.
// Provides a class for initializing, building, and managing Vulkan ray tracing resources,
// including acceleration structures (BLAS/TLAS), shader binding table (SBT), and ray tracing pipeline.
// Extended for hybrid rendering: Supports recording ray tracing commands into command buffers for integration with rasterization.
// Designed for developer-friendliness: Thread-safe function pointer loading, exception-based error handling,
// comprehensive cleanup, optional shader support (e.g., any-hit, intersection, callable), and extensive comments.
// Optimizations: Lazy loading of extensions, one-time command buffers for AS builds, aligned SBT handles, and efficient image transitions.
// Memory safety: All resources are RAII-managed via references; cleanup nulls handles and destroys in reverse order.
// Note: Requires Vulkan 1.2+ with ray tracing extensions enabled in the device (VK_KHR_ray_tracing_pipeline, VK_KHR_acceleration_structure).
// Best Practices Incorporated:
// - Minimize active ray queries for performance (from Khronos Vulkan Ray Tracing Best Practices).
// - Use device-local memory for AS and scratch buffers.
// - Prefer fast-trace flags for AS builds.
// - Aligned SBT handles to avoid misalignment issues.
// - Hybrid integration: Record ray tracing into existing raster command buffers; use storage images for output.
// - Extensibility: Add descriptor sets for input textures/buffers if needed for advanced shaders.
// Usage Example:
//   VulkanRTX rtx(device);
//   rtx.initializeRTX(physicalDevice, commandPool, queue, vertexBuffer, indexBuffer, vertexCount, indexCount, rtPipeline, rtLayout, sbt, tlas, tlasBuffer, tlasMemory, blas, blasBuffer, blasMemory);
//   // In render loop:
//   VkCommandBuffer cmd = ...; // Begin command buffer.
//   VkImage rtOutputImage = ...; // Create storage image beforehand.
//   rtx.recordRayTracingCommands(cmd, {width, height}, rtOutputImage, tlas);
//   // End and submit cmd; composite rtOutputImage with raster result if hybrid.

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <mutex>
#include <future>
#include <optional>

// Forward declarations for Vulkan types to reduce header dependencies and improve compile times.
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
    // Stores buffer, memory, and strided address regions for raygen, miss, hit, and callable shaders.
    // Developer Note: SBT is used to map shader groups to ray tracing stages; ensure alignment for performance.
    struct ShaderBindingTable {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkStridedDeviceAddressRegionKHR raygen = {};
        VkStridedDeviceAddressRegionKHR miss = {};
        VkStridedDeviceAddressRegionKHR hit = {};
        VkStridedDeviceAddressRegionKHR callable = {};
    };

    // Push constants for ray tracing shaders (raygen, closest-hit, any-hit, callable).
    // Includes view-proj matrix, camera position, and custom parameters for dynamic effects.
    // Developer Note: Alignas(16) ensures proper GPU alignment; extend for additional uniforms if needed.
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

    // Constructor: Initializes with device handle and loads ray tracing extension function pointers thread-safely.
    // Throws std::runtime_error if any required function fails to load.
    // Developer Note: Function pointers are static for sharing across instances; mutex prevents race conditions.
    explicit VulkanRTX(VkDevice device);

    // Destructor: No-op; cleanup is explicit via cleanupRTX for control over resource lifetime.
    ~VulkanRTX() = default;

    // Non-copyable and non-movable for safety (Vulkan handles are not thread-safe to copy).
    VulkanRTX(const VulkanRTX&) = delete;
    VulkanRTX& operator=(const VulkanRTX&) = delete;
    VulkanRTX(VulkanRTX&&) = delete;
    VulkanRTX& operator=(VulkanRTX&&) = delete;

    // Initializes all RTX resources: pipeline, BLAS, TLAS, SBT.
    // Inputs: Physical device, command pool/queue for AS builds, vertex/index buffers and counts for geometry.
    // Outputs: Pipeline, layout, SBT, TLAS/BLAS handles and buffers/memories (null-initialized on entry).
    // Throws std::runtime_error on failure; calls cleanupRTX on partial failure for safety.
    // Developer Note: Use after rasterization setup for hybrid rendering; extend for dynamic geometry updates.
    void initializeRTX(
        VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
        VkBuffer vertexBuffer, VkBuffer indexBuffer, uint32_t vertexCount, uint32_t indexCount,
        VkPipeline& rtPipeline, VkPipelineLayout& rtPipelineLayout, ShaderBindingTable& sbt,
        VkAccelerationStructureKHR& tlas, VkBuffer& tlasBuffer, VkDeviceMemory& tlasMemory,
        VkAccelerationStructureKHR& blas, VkBuffer& blasBuffer, VkDeviceMemory& blasMemory
    );

    // Cleans up all RTX resources: Destroys pipeline/layout, SBT buffer/memory, TLAS/BLAS and their buffers/memories.
    // Safe to call on null handles; nulls all output parameters after destruction.
    // Waits for device idle implicitly via Vulkan destruction order.
    // Developer Note: Call in app shutdown or on resize if AS needs rebuilding; extend for dynamic updates.
    void cleanupRTX(
        VkPipeline& rtPipeline, VkPipelineLayout& rtPipelineLayout,
        VkBuffer& sbtBuffer, VkDeviceMemory& sbtMemory, VkAccelerationStructureKHR& tlas,
        VkBuffer& tlasBuffer, VkDeviceMemory& tlasMemory, VkAccelerationStructureKHR& blas,
        VkBuffer& blasBuffer, VkDeviceMemory& blasMemory
    );

    // Extension: Creates a storage image for ray tracing output (e.g., for hybrid rendering).
    // Inputs: Physical device for memory, device for creation, image extent, format (default B8G8R8A8_UNORM for color).
    // Outputs: Image, image view, memory (null-initialized on entry).
    // Throws std::runtime_error on failure.
    // Developer Note: Use for ray traced effects (e.g., reflections); composite with raster image in post-processing.
    // Best Practice: Use device-local memory for performance; transition layout before/after tracing.
    void createStorageImage(
        VkPhysicalDevice physicalDevice, VkExtent2D extent, VkFormat format,
        VkImage& image, VkImageView& imageView, VkDeviceMemory& memory
    );

    // Extension: Records ray tracing commands into a command buffer for hybrid rendering.
    // Inputs: Command buffer (assumed begun), image extent for dispatch, output storage image, TLAS for scene.
    // Assumes pipeline is bound and push constants set before call (e.g., viewProj, camPos).
    // Steps:
    // 1. Transition output image to GENERAL layout for storage.
    // 2. Bind ray tracing pipeline.
    // 3. Trace rays using SBT regions.
    // 4. Transition output image back to OPTIMAL for post-processing/compositing.
    // Throws std::runtime_error on pipeline bind or trace failure.
    // Developer Note: Call after raster pass for hybrid effects; extend with descriptors for input textures.
    // Best Practice: Minimize ray queries (from Khronos); use for targeted effects like shadows/reflections.
    void recordRayTracingCommands(
        VkCommandBuffer cmdBuffer, VkExtent2D extent, VkImage outputImage, VkAccelerationStructureKHR tlas
    );

private:
    VkDevice device_;  // Stored device handle for all operations.

    // Static mutexes for thread-safe initialization of function pointers and shared resources.
    static std::mutex functionPtrMutex_;   // Protects function pointer loading.
    static std::mutex shaderModuleMutex_;  // Protects shader module creation (if shared).

    // Function pointers for core ray tracing extensions (loaded lazily).
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

    // Builds bottom-level acceleration structure (BLAS) from triangle geometry.
    void createBottomLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                             VkBuffer vertexBuffer, VkBuffer indexBuffer, uint32_t vertexCount, uint32_t indexCount,
                             VkAccelerationStructureKHR& as, VkBuffer& asBuffer, VkDeviceMemory& asMemory);

    // Builds top-level acceleration structure (TLAS) instancing the BLAS.
    void createTopLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                          VkAccelerationStructureKHR blas, VkAccelerationStructureKHR& as, VkBuffer& asBuffer, VkDeviceMemory& asMemory);

    // Generic buffer creation utility.
    void createBuffer(VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer& buffer, VkDeviceMemory& memory);

    // Creates shader module from SPIR-V file.
    VkShaderModule createShaderModule(const std::string& filename);

    // Checks if shader file exists without full loading.
    bool shaderFileExists(const std::string& filename) const;
};

#endif // VULKAN_RTX_HPP