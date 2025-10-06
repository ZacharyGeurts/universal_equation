#ifndef VULKAN_RTX_HPP
#define VULKAN_RTX_HPP

// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// Vulkan ray-tracing utilities for acceleration structures, shader binding tables, and pipeline setup.
// Supports Windows/Linux (X11/Wayland); no mutexes; voxel geometry via glm::vec3 vertices.
// Dependencies: Vulkan 1.3+, GLM, C++20 standard library.
// Usage: Manages ray-tracing pipeline, BLAS/TLAS, and descriptors for voxel rendering.
// Zachary Geurts 2025

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <tuple>
#include <memory>
#include <stdexcept>
#include <span>
#include <string>
#include <format>

#define VK_CHECK(call, msg) do { \
    VkResult result = (call); \
    if (result != VK_SUCCESS) { \
        throw VulkanRTXException(std::format("{} (Error code: {})", msg, result)); \
    } \
} while (0)

class VulkanRTXException : public std::runtime_error {
public:
    explicit VulkanRTXException(const std::string& msg) : std::runtime_error(msg) {}
};

enum class DescriptorBindings {
    TLAS = 0,
    StorageImage = 1,
    CameraUBO = 2,
    MaterialSSBO = 3,
    DimensionDataSSBO = 4,
    DenoiseImage = 5
};

struct PushConstants {
    glm::mat4 viewInverse;
    glm::mat4 projInverse;
    uint32_t frameIndex;
    uint32_t maxBounces;
};

struct DimensionData {
    float width;
    float height;
    float depth;
};

inline bool operator==(const DimensionData& lhs, const DimensionData& rhs) {
    return lhs.width == rhs.width && lhs.height == rhs.height && lhs.depth == rhs.depth;
}

enum class ShaderFeatures : uint32_t {
    None = 0,
    AnyHit = 1 << 0,
    Intersection = 1 << 1,
    Callable = 1 << 2
};

class VulkanRTX {
public:
    template <typename T, typename DestroyFuncType>
    class VulkanResource {
    public:
        VulkanResource(VkDevice device, T handle, DestroyFuncType destroyFunc)
            : device_(device), handle_(handle), destroyFunc_(destroyFunc) {}

        ~VulkanResource() {
            if (handle_ != VK_NULL_HANDLE) {
                destroyFunc_(device_, handle_, nullptr);
            }
        }

        VulkanResource(const VulkanResource&) = delete;
        VulkanResource& operator=(const VulkanResource&) = delete;

        VulkanResource(VulkanResource&& other) noexcept
            : device_(other.device_), handle_(other.handle_), destroyFunc_(other.destroyFunc_) {
            other.handle_ = VK_NULL_HANDLE;
        }

        VulkanResource& operator=(VulkanResource&& other) noexcept {
            if (this != &other) {
                if (handle_ != VK_NULL_HANDLE) {
                    destroyFunc_(device_, handle_, nullptr);
                }
                device_ = other.device_;
                handle_ = other.handle_;
                destroyFunc_ = other.destroyFunc_;
                other.handle_ = VK_NULL_HANDLE;
            }
            return *this;
        }

        T get() const { return handle_; }
        T* getPtr() { return &handle_; }

    private:
        VkDevice device_;
        T handle_;
        DestroyFuncType destroyFunc_;
    };

    class VulkanDescriptorSet {
    public:
        VulkanDescriptorSet(VkDevice device, VkDescriptorPool pool, VkDescriptorSet set)
            : device_(device), pool_(pool), set_(set) {}

        ~VulkanDescriptorSet() {
            if (set_ != VK_NULL_HANDLE) {
                vkFreeDescriptorSets(device_, pool_, 1, &set_);
            }
        }

        VulkanDescriptorSet(const VulkanDescriptorSet&) = delete;
        VulkanDescriptorSet& operator=(const VulkanDescriptorSet&) = delete;

        VulkanDescriptorSet(VulkanDescriptorSet&& other) noexcept
            : device_(other.device_), pool_(other.pool_), set_(other.set_) {
            other.set_ = VK_NULL_HANDLE;
        }

        VulkanDescriptorSet& operator=(VulkanDescriptorSet&& other) noexcept {
            if (this != &other) {
                if (set_ != VK_NULL_HANDLE) {
                    vkFreeDescriptorSets(device_, pool_, 1, &set_);
                }
                device_ = other.device_;
                pool_ = other.pool_;
                set_ = other.set_;
                other.set_ = VK_NULL_HANDLE;
            }
            return *this;
        }

        VkDescriptorSet get() const { return set_; }
        VkDescriptorSet* getPtr() { return &set_; }

    private:
        VkDevice device_;
        VkDescriptorPool pool_;
        VkDescriptorSet set_;
    };

    class ShaderBindingTable {
    public:
        VkStridedDeviceAddressRegionKHR raygen{};
        VkStridedDeviceAddressRegionKHR miss{};
        VkStridedDeviceAddressRegionKHR hit{};
        VkStridedDeviceAddressRegionKHR callable{};
        VulkanRTX* parent;
        VulkanResource<VkBuffer, PFN_vkDestroyBuffer> buffer;
        VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> memory;

        ShaderBindingTable(VkDevice device, VulkanRTX* parent_);
        ~ShaderBindingTable();
        ShaderBindingTable(ShaderBindingTable&& other) noexcept;
        ShaderBindingTable& operator=(ShaderBindingTable&& other) noexcept;
    };

    VulkanRTX(VkDevice device, const std::vector<std::string>& shaderPaths);

    void setDevice(VkDevice device) { device_ = device; }
    VkDevice getDevice() const { return device_; }

    void setShaderPaths(const std::vector<std::string>& paths) { shaderPaths_ = paths; }
    const std::vector<std::string>& getShaderPaths() const { return shaderPaths_; }

    void setDescriptorSetLayout(const VulkanResource<VkDescriptorSetLayout, PFN_vkDestroyDescriptorSetLayout>& layout) { dsLayout_ = layout; }
    VulkanResource<VkDescriptorSetLayout, PFN_vkDestroyDescriptorSetLayout>& getDescriptorSetLayout() { return dsLayout_; }

    void setDescriptorPool(const VulkanResource<VkDescriptorPool, PFN_vkDestroyDescriptorPool>& pool) { dsPool_ = pool; }
    VulkanResource<VkDescriptorPool, PFN_vkDestroyDescriptorPool>& getDescriptorPool() { return dsPool_; }

    void setDescriptorSet(const VulkanDescriptorSet& set) { ds_ = set; }
    VulkanDescriptorSet& getDescriptorSet() { return ds_; }

    void setRayTracingPipelineLayout(const VulkanResource<VkPipelineLayout, PFN_vkDestroyPipelineLayout>& layout) { rtPipelineLayout_ = layout; }
    VulkanResource<VkPipelineLayout, PFN_vkDestroyPipelineLayout>& getRayTracingPipelineLayout() { return rtPipelineLayout_; }

    void setRayTracingPipeline(const VulkanResource<VkPipeline, PFN_vkDestroyPipeline>& pipeline) { rtPipeline_ = pipeline; }
    VulkanResource<VkPipeline, PFN_vkDestroyPipeline>& getRayTracingPipeline() { return rtPipeline_; }

    void setBottomLevelASBuffer(const VulkanResource<VkBuffer, PFN_vkDestroyBuffer>& buffer) { blasBuffer_ = buffer; }
    VulkanResource<VkBuffer, PFN_vkDestroyBuffer>& getBottomLevelASBuffer() { return blasBuffer_; }

    void setBottomLevelASMemory(const VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>& memory) { blasMemory_ = memory; }
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>& getBottomLevelASMemory() { return blasMemory_; }

    void setTopLevelASBuffer(const VulkanResource<VkBuffer, PFN_vkDestroyBuffer>& buffer) { tlasBuffer_ = buffer; }
    VulkanResource<VkBuffer, PFN_vkDestroyBuffer>& getTopLevelASBuffer() { return tlasBuffer_; }

    void setTopLevelASMemory(const VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>& memory) { tlasMemory_ = memory; }
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>& getTopLevelASMemory() { return tlasMemory_; }

    void setShaderBindingTable(const ShaderBindingTable& sbt) { sbt_ = sbt; }
    ShaderBindingTable& getShaderBindingTable() { return sbt_; }

    void setBottomLevelAS(const VulkanResource<VkAccelerationStructureKHR, PFN_vkDestroyAccelerationStructureKHR>& blas) { blas_ = blas; }
    VulkanResource<VkAccelerationStructureKHR, PFN_vkDestroyAccelerationStructureKHR>& getBottomLevelAS() { return blas_; }

    void setTopLevelAS(const VulkanResource<VkAccelerationStructureKHR, PFN_vkDestroyAccelerationStructureKHR>& tlas) { tlas_ = tlas; }
    VulkanResource<VkAccelerationStructureKHR, PFN_vkDestroyAccelerationStructureKHR>& getTopLevelAS() { return tlas_; }

    void createBottomLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                             const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries);
    void createTopLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                          const std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4>>& instances);
    void createBuffer(VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags props, VulkanResource<VkBuffer, PFN_vkDestroyBuffer>& buffer,
                      VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>& memory);
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags props);
    void compactAccelerationStructures(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue);
    VkDeviceAddress getBufferDeviceAddress(VkBuffer buffer) const;
    VkDeviceAddress getAccelerationStructureDeviceAddress(VkAccelerationStructureKHR as) const;
    VkCommandBuffer allocateTransientCommandBuffer(VkCommandPool commandPool);
    void submitAndWaitTransient(VkCommandBuffer cmdBuffer, VkQueue queue, VkCommandPool commandPool);
    void updateDescriptorSetForTLAS(VkAccelerationStructureKHR tlas);
    void createStorageImage(VkPhysicalDevice physicalDevice, VkExtent2D extent, VkFormat format,
                           VulkanResource<VkImage, PFN_vkDestroyImage>& image,
                           VulkanResource<VkImageView, PFN_vkDestroyImageView>& imageView,
                           VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>& memory);
    void recordRayTracingCommands(VkCommandBuffer cmdBuffer, VkExtent2D extent, VkImage outputImage,
                                  VkImageView outputImageView, const PushConstants& pc, VkAccelerationStructureKHR tlas);
    void initializeRTX(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                       const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                       uint32_t maxRayRecursionDepth, const std::vector<DimensionData>& dimensionCache);
    void updateRTX(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                   const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                   const std::vector<DimensionData>& dimensionCache);
    void denoiseImage(VkCommandBuffer cmdBuffer, VkImage inputImage, VkImageView inputImageView,
                      VkImage outputImage, VkImageView outputImageView);
    void createDescriptorSetLayout();
    void createDescriptorPoolAndSet();
    void createRayTracingPipeline(uint32_t maxRayRecursionDepth);
    void createShaderBindingTable(VkPhysicalDevice physicalDevice);
    void updateDescriptors(VkBuffer cameraBuffer, VkBuffer materialBuffer, VkBuffer dimensionBuffer);
    VkShaderModule createShaderModule(const std::string& filename);
    bool shaderFileExists(const std::string& filename) const;
    void buildShaderGroups(std::vector<VkRayTracingShaderGroupCreateInfoKHR>& groups,
                           const std::vector<VkPipelineShaderStageCreateInfo>& stages);
    bool hasShaderFeature(ShaderFeatures feature) const {
        return (static_cast<uint32_t>(shaderFeatures_) & static_cast<uint32_t>(feature)) != static_cast<uint32_t>(ShaderFeatures::None);
    }

private:
    VkDevice device_;
    std::vector<std::string> shaderPaths_;
    VulkanResource<VkDescriptorSetLayout, PFN_vkDestroyDescriptorSetLayout> dsLayout_;
    VulkanResource<VkDescriptorPool, PFN_vkDestroyDescriptorPool> dsPool_;
    VulkanDescriptorSet ds_;
    VulkanResource<VkPipelineLayout, PFN_vkDestroyPipelineLayout> rtPipelineLayout_;
    VulkanResource<VkPipeline, PFN_vkDestroyPipeline> rtPipeline_;
    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> blasBuffer_;
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> blasMemory_;
    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> tlasBuffer_;
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> tlasMemory_;
    ShaderBindingTable sbt_;
    bool supportsCompaction_;
    ShaderFeatures shaderFeatures_;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> primitiveCounts_;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> previousPrimitiveCounts_;
    std::vector<DimensionData> previousDimensionCache_;
    VulkanResource<VkAccelerationStructureKHR, PFN_vkDestroyAccelerationStructureKHR> blas_;
    VulkanResource<VkAccelerationStructureKHR, PFN_vkDestroyAccelerationStructureKHR> tlas_;

    // Vulkan extension function pointers
    PFN_vkGetBufferDeviceAddress vkGetBufferDeviceAddressFunc = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetASDeviceAddressFunc = nullptr;
    PFN_vkCreateAccelerationStructureKHR vkCreateASFunc = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildASFunc = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetASBuildSizesFunc = nullptr;
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = nullptr;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyASFunc = nullptr;
    PFN_vkCmdCopyAccelerationStructureKHR vkCmdCopyAccelerationStructureKHR = nullptr;
    PFN_vkCmdWriteAccelerationStructuresPropertiesKHR vkCmdWriteASPropertiesFunc = nullptr;
};

#endif // VULKAN_RTX_HPP