#ifndef VULKAN_RTX_HPP
#define VULKAN_RTX_HPP

// AMOURANTH RTX engine - Header for ray tracing (RTX) pipeline management.
// Provides a class for initializing, building, and managing Vulkan ray tracing resources,
// including acceleration structures (BLAS/TLAS), shader binding table (SBT), and ray tracing pipeline.
// Extended for hybrid rendering: Supports recording ray tracing commands into command buffers for integration with rasterization.
// Designed for developer-friendliness: Thread-safe function pointer loading, exception-based error handling,
// comprehensive cleanup, optional shader support (e.g., any-hit, intersection, callable), and extensive comments.
// Optimizations: Lazy loading of extensions, one-time command buffers for AS builds, aligned SBT handles, and efficient image transitions.
// Memory safety: All resources are RAII-managed internally; destructor automatically cleans up in reverse order.
// Note: Requires Vulkan 1.2+ with ray tracing extensions enabled in the device (VK_KHR_ray_tracing_pipeline, VK_KHR_acceleration_structure).
// Best Practices Incorporated:
// - Minimize active ray queries for performance (from Khronos Vulkan Ray Tracing Best Practices).
// - Use device-local memory for AS and scratch buffers.
// - Prefer fast-trace flags for AS builds.
// - Aligned SBT handles to avoid misalignment issues.
// - Hybrid integration: Record ray tracing into existing raster command buffers; use storage images for output.
// - Extensibility: Descriptor sets for TLAS, output image, camera, and material buffer; easy to add more bindings for textures/buffers.
// Usage Example:
//   VulkanRTX rtx(device);
//   rtx.initializeRTX(physicalDevice, commandPool, queue, vertexBuffer, indexBuffer, vertexCount, indexCount);
//   // Create storage image:
//   VkImage rtOutputImage = VK_NULL_HANDLE;
//   VkImageView rtOutputImageView = VK_NULL_HANDLE;
//   VkDeviceMemory rtOutputMemory = VK_NULL_HANDLE;
//   rtx.createStorageImage(physicalDevice, {width, height}, VK_FORMAT_R8G8B8A8_UNORM, rtOutputImage, rtOutputImageView, rtOutputMemory);
//   // Create camera and material buffers:
//   VkBuffer cameraBuffer = ...; VkDeviceMemory cameraMemory = ...;
//   VkBuffer materialBuffer = ...; VkDeviceMemory materialMemory = ...;
//   rtx.createRayTracingDescriptorSet(descriptorPool, cameraBuffer, materialBuffer);
//   // In render loop:
//   VkCommandBuffer cmd = ...; // Begin command buffer.
//   VulkanRTX::PushConstants pc = {viewProj, camPos, ...};
//   rtx.recordRayTracingCommands(cmd, {width, height}, rtOutputImage, rtOutputImageView, pc);
//   // End and submit cmd; composite rtOutputImage with raster result if hybrid.
//   // On shutdown or resize:
//   rtx.cleanupRTX(); // Or let destructor handle it.

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
struct VkDescriptorSetLayout_T;   // VkDescriptorSetLayout
struct VkDescriptorPool_T;        // VkDescriptorPool
struct VkDescriptorSet_T;         // VkDescriptorSet

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

    // Destructor: Automatically cleans up all RTX resources.
    ~VulkanRTX();

    // Non-copyable and non-movable for safety (Vulkan handles are not thread-safe to copy).
    VulkanRTX(const VulkanRTX&) = delete;
    VulkanRTX& operator=(const VulkanRTX&) = delete;
    VulkanRTX(VulkanRTX&&) = delete;
    VulkanRTX& operator=(VulkanRTX&&) = delete;

    // Initializes all RTX resources: pipeline, BLAS, TLAS, SBT, descriptor sets.
    // Inputs: Physical device, command pool/queue for AS builds, vertex/index buffers and counts for geometry.
    // Throws std::runtime_error on failure; cleans up partially allocated resources.
    // Developer Note: Use after rasterization setup for hybrid rendering; extend for dynamic geometry updates.
    void initializeRTX(
        VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
        VkBuffer vertexBuffer, VkBuffer indexBuffer, uint32_t vertexCount, uint32_t indexCount
    );

    // Cleans up all RTX resources: Destroys pipeline/layout, SBT buffer/memory, TLAS/BLAS and their buffers/memories, descriptors.
    // Safe to call multiple times; nulls all internal handles after destruction.
    // Developer Note: Call on resize if AS needs rebuilding; destructor calls automatically.
    void cleanupRTX();

    // Extension: Creates a storage image for ray tracing output (e.g., for hybrid rendering).
    // Inputs: Physical device for memory, image extent, format (e.g., VK_FORMAT_R8G8B8A8_UNORM for color).
    // Outputs: Image, image view, memory (null-initialized on entry).
    // Throws: std::runtime_error on failure.
    // Developer Note: Use for ray traced effects (e.g., reflections); composite with raster image in post-processing.
    // Best Practice: Use device-local memory for performance; transition layout before/after tracing.
    void createStorageImage(
        VkPhysicalDevice physicalDevice, VkExtent2D extent, VkFormat format,
        VkImage& image, VkImageView& imageView, VkDeviceMemory& memory
    );

    // Extension: Creates and updates descriptor set for ray tracing with camera and material buffers.
    // Inputs: Descriptor pool, camera buffer (uniform), material buffer (storage).
    // Throws: std::runtime_error on failure.
    // Developer Note: Call after initializeRTX to bind camera and material data.
    void createRayTracingDescriptorSet(
        VkDescriptorPool descriptorPool, VkBuffer cameraBuffer, VkBuffer materialBuffer
    );

    // Extension: Records ray tracing commands into a command buffer for hybrid rendering.
    // Inputs: Command buffer (assumed begun), image extent for dispatch, output storage image/view, push constants, optional TLAS (uses internal if null).
    // Steps:
    // 1. Transition output image to GENERAL layout for storage.
    // 2. Update descriptors for output image (and TLAS if provided).
    // 3. Bind ray tracing pipeline and descriptor set.
    // 4. Push constants.
    // 5. Trace rays using SBT regions.
    // 6. Transition output image back to COLOR_ATTACHMENT_OPTIMAL for post-processing/compositing.
    // Throws std::runtime_error on failure.
    // Developer Note: Call after raster pass for hybrid effects; extend with more descriptors for input textures.
    // Best Practice: Minimize ray queries (from Khronos); use for targeted effects like shadows/reflections.
    void recordRayTracingCommands(
        VkCommandBuffer cmdBuffer, VkExtent2D extent, VkImage outputImage, VkImageView outputImageView,
        const PushConstants& pc, VkAccelerationStructureKHR tlas = VK_NULL_HANDLE
    );

    // Getters for internal resources (for advanced usage or manual control).
    VkPipeline getPipeline() const { return rtPipeline_; }
    VkPipelineLayout getPipelineLayout() const { return rtPipelineLayout_; }
    const ShaderBindingTable& getSBT() const { return sbt_; }
    VkAccelerationStructureKHR getTLAS() const { return tlas_; }
    VkAccelerationStructureKHR getBLAS() const { return blas_; }
    VkDescriptorSet getDescriptorSet() const { return ds_; }

private:
    VkDevice device_;  // Stored device handle for all operations.

    // Internal resources (RAII-managed).
    VkPipeline rtPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout rtPipelineLayout_ = VK_NULL_HANDLE;
    ShaderBindingTable sbt_;
    VkAccelerationStructureKHR tlas_ = VK_NULL_HANDLE;
    VkBuffer tlasBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory tlasMemory_ = VK_NULL_HANDLE;
    VkAccelerationStructureKHR blas_ = VK_NULL_HANDLE;
    VkBuffer blasBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory blasMemory_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout dsLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool dsPool_ = VK_NULL_HANDLE;
    VkDescriptorSet ds_ = VK_NULL_HANDLE;

    // Flags for optional shaders.
    bool hasAnyHit_ = false;
    bool hasIntersection_ = false;
    bool hasCallable_ = false;

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

    // Creates descriptor set layout for ray tracing (TLAS, storage image, camera, material buffer).
    void createDescriptorSetLayout();

	void createDescriptorPoolAndSet();

    // Creates ray tracing pipeline with optional shaders.
    void createRayTracingPipeline();

    // Creates shader binding table with aligned handles.
    void createShaderBindingTable(VkPhysicalDevice physicalDevice);

    // Builds bottom-level acceleration structure (BLAS) from triangle geometry.
    void createBottomLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                             VkBuffer vertexBuffer, VkBuffer indexBuffer, uint32_t vertexCount, uint32_t indexCount);

    // Builds top-level acceleration structure (TLAS) instancing the BLAS.
    void createTopLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue);

    // Updates descriptor set for TLAS.
    void updateDescriptorSetForTLAS(VkAccelerationStructureKHR tlas);

    // Generic buffer creation utility.
    void createBuffer(VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer& buffer, VkDeviceMemory& memory);

    // Finds suitable memory type index.
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags props);

    // Creates shader module from SPIR-V file.
    VkShaderModule createShaderModule(const std::string& filename);

    // Checks if shader file exists without full loading.
    bool shaderFileExists(const std::string& filename) const;
};

#endif // VULKAN_RTX_HPP