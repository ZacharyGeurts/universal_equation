#ifndef VULKAN_RTX_HPP
#define VULKAN_RTX_HPP

// AMOURANTH RTX Engine - September 2025
// Core header for ray tracing (RTX) pipeline management.
// Manages pipeline, descriptors, and rendering commands for hybrid rendering.
// Supports triangle-based geometry (quads as 2 triangles, spheres as ~32,000 triangles).
// Requires Vulkan 1.2+ with VK_KHR_ray_tracing_pipeline, VK_KHR_acceleration_structure,
// and optional VK_KHR_ray_tracing_maintenance1 for compaction.
// Author: Zachary Geurts, 2025

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <mutex>
#include <cstdint>
#include <spdlog/spdlog.h>  // For logging
#include "engine/core.hpp"  // For AMOURANTH integration

// Custom exception for Vulkan errors
class VulkanRTXException : public std::runtime_error {
public:
    VulkanRTXException(const std::string& msg, VkResult result = VK_SUCCESS)
        : std::runtime_error(msg + " (VkResult: " + std::to_string(result) + ")"), result_(result) {}
    VkResult getResult() const { return result_; }
private:
    VkResult result_;
};

// RAII wrapper for Vulkan resources
template<typename T, void(*DestroyFunc)(VkDevice, T, const VkAllocationCallbacks*)>
class VulkanResource {
public:
    VulkanResource(VkDevice device, T handle = VK_NULL_HANDLE)
        : device_(device), handle_(handle) {}
    ~VulkanResource() {
        if (handle_ != VK_NULL_HANDLE && DestroyFunc) {
            DestroyFunc(device_, handle_, nullptr);
        }
    }
    VulkanResource(const VulkanResource&) = delete;
    VulkanResource& operator=(const VulkanResource&) = delete;
    VulkanResource(VulkanResource&& other) noexcept : device_(other.device_), handle_(other.handle_) {
        other.handle_ = VK_NULL_HANDLE;
    }
    VulkanResource& operator=(VulkanResource&& other) noexcept {
        if (this != &other) {
            if (handle_ != VK_NULL_HANDLE && DestroyFunc) {
                DestroyFunc(device_, handle_, nullptr);
            }
            device_ = other.device_;
            handle_ = other.handle_;
            other.handle_ = VK_NULL_HANDLE;
        }
        return *this;
    }
    T& get() { return handle_; }
    const T& get() const { return handle_; }
private:
    VkDevice device_;
    T handle_;
};

// RAII wrapper for VkDescriptorSet
class VulkanDescriptorSet {
public:
    VulkanDescriptorSet(VkDevice device, VkDescriptorPool pool, VkDescriptorSet handle = VK_NULL_HANDLE)
        : device_(device), pool_(pool), handle_(handle) {}
    ~VulkanDescriptorSet() {
        if (handle_ != VK_NULL_HANDLE) {
            vkFreeDescriptorSets(device_, pool_, 1, &handle_);
        }
    }
    VulkanDescriptorSet(const VulkanDescriptorSet&) = delete;
    VulkanDescriptorSet& operator=(const VulkanDescriptorSet&) = delete;
    VulkanDescriptorSet(VulkanDescriptorSet&& other) noexcept
        : device_(other.device_), pool_(other.pool_), handle_(other.handle_) {
        other.handle_ = VK_NULL_HANDLE;
    }
    VulkanDescriptorSet& operator=(VulkanDescriptorSet&& other) noexcept {
        if (this != &other) {
            if (handle_ != VK_NULL_HANDLE) {
                vkFreeDescriptorSets(device_, pool_, 1, &handle_);
            }
            device_ = other.device_;
            pool_ = other.pool_;
            handle_ = other.handle_;
            other.handle_ = VK_NULL_HANDLE;
        }
        return *this;
    }
    VkDescriptorSet& get() { return handle_; }
    const VkDescriptorSet& get() const { return handle_; }
private:
    VkDevice device_;
    VkDescriptorPool pool_;
    VkDescriptorSet handle_;
};

// Forward declaration of VulkanRTX
class VulkanRTX;

// RAII wrapper for VkAccelerationStructureKHR_T*
class VulkanASResource {
public:
    VulkanASResource(VkDevice device, VkAccelerationStructureKHR_T* handle = VK_NULL_HANDLE)
        : device_(device), handle_(handle) {}
    ~VulkanASResource();
    VulkanASResource(const VulkanASResource&) = delete;
    VulkanASResource& operator=(const VulkanASResource&) = delete;
    VulkanASResource(VulkanASResource&& other) noexcept : device_(other.device_), handle_(other.handle_) {
        other.handle_ = VK_NULL_HANDLE;
    }
    VulkanASResource& operator=(VulkanASResource&& other) noexcept;
    VkAccelerationStructureKHR_T*& get() { return handle_; }
    const VkAccelerationStructureKHR_T* get() const { return handle_; }
private:
    VkDevice device_;
    VkAccelerationStructureKHR_T* handle_;
};

// Macro for Vulkan call checking
#define VK_CHECK(call, msg) do { \
    VkResult result = call; \
    if (result != VK_SUCCESS) { \
        spdlog::error("{}: VkResult {}", msg, result); \
        throw VulkanRTXException(msg, result); \
    } \
} while (0)

// Shader binding table structure
struct SBT {
    VulkanResource<VkBuffer_T*, vkDestroyBuffer> buffer;
    VulkanResource<VkDeviceMemory, vkFreeMemory> memory;
    VkStridedDeviceAddressRegionKHR raygen = {};
    VkStridedDeviceAddressRegionKHR miss = {};
    VkStridedDeviceAddressRegionKHR hit = {};
    VkStridedDeviceAddressRegionKHR callable = {};
    SBT(VkDevice device) : buffer(device, VK_NULL_HANDLE), memory(device, VK_NULL_HANDLE) {}
};

// Descriptor binding indices
enum class DescriptorBindings : uint32_t {
    TLAS = 0,
    StorageImage = 1,
    CameraUBO = 2,
    MaterialSSBO = 3,
    DimensionDataSSBO = 4
};

// Shader feature flags
enum class ShaderFeatures : uint32_t {
    None = 0,
    AnyHit = 1 << 0,
    Intersection = 1 << 1,
    Callable = 1 << 2
};

class VulkanRTX {
    friend class VulkanASResource; // Allow VulkanASResource to access vkDestroyASFunc
private:
    VkDevice device_ = VK_NULL_HANDLE;
    std::vector<std::string> shaderPaths_;
    std::vector<uint32_t> primitiveCounts_; // Store primitive counts for BLAS
    VulkanResource<VkDescriptorSetLayout, vkDestroyDescriptorSetLayout> dsLayout_;
    VulkanResource<VkDescriptorPool, vkDestroyDescriptorPool> dsPool_;
    VulkanDescriptorSet ds_;
    VulkanResource<VkPipelineLayout, vkDestroyPipelineLayout> rtPipelineLayout_;
    VulkanResource<VkPipeline, vkDestroyPipeline> rtPipeline_;
    VulkanASResource blas_;
    VulkanResource<VkBuffer_T*, vkDestroyBuffer> blasBuffer_;
    VulkanResource<VkDeviceMemory, vkFreeMemory> blasMemory_;
    VulkanASResource tlas_;
    VulkanResource<VkBuffer_T*, vkDestroyBuffer> tlasBuffer_;
    VulkanResource<VkDeviceMemory, vkFreeMemory> tlasMemory_;
    SBT sbt_;
    bool supportsCompaction_ = false;
    ShaderFeatures shaderFeatures_ = ShaderFeatures::None;

    static std::mutex functionPtrMutex_;
    static std::mutex shaderModuleMutex_;
    static PFN_vkGetBufferDeviceAddress vkGetBufferDeviceAddress;
    static PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;
    static PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
    static PFN_vkDestroyAccelerationStructureKHR vkDestroyASFunc;
    static PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
    static PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
    static PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
    static PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;
    static PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;
    static PFN_vkCmdCopyAccelerationStructureKHR vkCmdCopyAccelerationStructureKHR;

public:
    explicit VulkanRTX(VkDevice device, const std::vector<std::string>& shaderPaths = {
        "assets/shaders/raygen.spv", "assets/shaders/miss.spv", "assets/shaders/closest_hit.spv",
        "assets/shaders/any_hit.spv", "assets/shaders/intersection.spv", "assets/shaders/callable.spv"
    });

    // Initialize RTX pipeline with geometries and optional dimension cache
    void initializeRTX(
        VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
        const std::vector<std::tuple<VkBuffer_T*, VkBuffer_T*, uint32_t, uint32_t, uint64_t>>& geometries,
        uint32_t maxRayRecursionDepth = 2,
        const std::vector<DimensionData>& dimensionCache = {}
    );

    // Update RTX pipeline with new geometries and dimension cache
    void updateRTX(
        VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
        const std::vector<std::tuple<VkBuffer_T*, VkBuffer_T*, uint32_t, uint32_t, uint64_t>>& geometries,
        const std::vector<DimensionData>& dimensionCache
    );

    // Compact BLAS if supported (requires VK_KHR_ray_tracing_maintenance1)
    void compactAccelerationStructures(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue);

    // Update descriptor sets with camera, material, and dimension buffers
    void updateDescriptors(VkBuffer_T* cameraBuffer, VkBuffer_T* materialBuffer, VkBuffer_T* dimensionBuffer);

    // Create storage image for ray tracing output
    void createStorageImage(VkPhysicalDevice physicalDevice, VkExtent2D extent, VkFormat format,
                           VkImage& image, VkImageView& imageView, VkDeviceMemory& memory);

    // Record ray tracing commands with push constants for model transformations
    void recordRayTracingCommands(VkCommandBuffer cmdBuffer, VkExtent2D extent, VkImage outputImage,
                                 VkImageView outputImageView, const PushConstants& pc,
                                 VkAccelerationStructureKHR_T* tlas = VK_NULL_HANDLE);

    // Denoise image using compute shader (16x16 dispatch grid for 1920x1080)
    void denoiseImage(VkCommandBuffer cmdBuffer, VkImageView inputImageView, VkImageView outputImageView);

    // Getters
    VkPipeline getPipeline() const { return rtPipeline_.get(); }
    VkPipelineLayout getPipelineLayout() const { return rtPipelineLayout_.get(); }
    const SBT& getSBT() const { return sbt_; }
    const VkAccelerationStructureKHR_T* getTLAS() const { return tlas_.get(); }
    const VkAccelerationStructureKHR_T* getBLAS() const { return blas_.get(); }
    VkDescriptorSet getDescriptorSet() const { return ds_.get(); }
    bool hasShaderFeature(ShaderFeatures feature) const {
        return (static_cast<uint32_t>(shaderFeatures_) & static_cast<uint32_t>(feature)) != 0;
    }

private:
    void createDescriptorSetLayout();
    void createDescriptorPoolAndSet();
    void createRayTracingPipeline(uint32_t maxRayRecursionDepth);
    void createShaderBindingTable(VkPhysicalDevice physicalDevice);
    void updateDescriptorSetForTLAS(VkAccelerationStructureKHR_T* tlas);
    void createBuffer(VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
                     VkMemoryPropertyFlags props, VulkanResource<VkBuffer_T*, vkDestroyBuffer>& buffer,
                     VulkanResource<VkDeviceMemory, vkFreeMemory>& memory);
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags props);
    VkShaderModule createShaderModule(const std::string& filename);
    bool shaderFileExists(const std::string& filename) const;
    void loadShadersAsync(std::vector<VkShaderModule>& modules, const std::vector<std::string>& paths);
    void buildShaderGroups(std::vector<VkRayTracingShaderGroupCreateInfoKHR>& groups,
                           const std::vector<VkPipelineShaderStageCreateInfo>& stages);
    void createBottomLevelAS(
        VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
        const std::vector<std::tuple<VkBuffer_T*, VkBuffer_T*, uint32_t, uint32_t, uint64_t>>& geometries
    );
    void createTopLevelAS(
        VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
        const std::vector<std::tuple<VkAccelerationStructureKHR_T*, glm::mat4>>& instances
    );
};

#endif // VULKAN_RTX_HPP