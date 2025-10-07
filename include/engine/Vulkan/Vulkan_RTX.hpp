#ifndef VULKAN_RTX_HPP
#define VULKAN_RTX_HPP

// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <stdexcept>
#include <atomic>
#include <mutex> // Added for mutex
#include <glm/glm.hpp>
#include <dlfcn.h>
#include "engine/logging.hpp"

#define VK_CHECK(result, msg) do { \
    if ((result) != VK_SUCCESS) { \
        logger_.log(Logging::LogLevel::Error, "{} (Error code: {})", std::source_location::current(), (msg), (result)); \
        throw VulkanRTXException(std::string(msg) + " (Error code: " + std::to_string(static_cast<int>(result)) + ")"); \
    } \
} while (0)

// Define operator== for VkAccelerationStructureBuildRangeInfoKHR in global namespace
inline bool operator==(const VkAccelerationStructureBuildRangeInfoKHR& lhs, const VkAccelerationStructureBuildRangeInfoKHR& rhs) {
    return lhs.primitiveCount == rhs.primitiveCount &&
           lhs.primitiveOffset == rhs.primitiveOffset &&
           lhs.firstVertex == rhs.firstVertex &&
           lhs.transformOffset == rhs.transformOffset;
}

class VulkanRTXException : public std::runtime_error {
public:
    explicit VulkanRTXException(const std::string& msg) : std::runtime_error(msg) {}
};

namespace VulkanRTX {

enum class ShaderFeatures : uint32_t {
    None = 0,
    AnyHit = 1 << 0,
    Intersection = 1 << 1,
    Callable = 1 << 2
};

enum class DescriptorBindings : uint32_t {
    TLAS = 0,
    StorageImage = 1,
    CameraUBO = 2,
    MaterialSSBO = 3,
    DimensionDataSSBO = 4,
    DenoiseImage = 5
};

struct DimensionData {
    uint32_t dimension;
    float value;
    bool operator==(const DimensionData& other) const {
        return dimension == other.dimension && value == other.value;
    }
};

struct PushConstants {
    glm::vec4 clearColor;
    float lightIntensity;
    uint32_t samplesPerPixel;
    uint32_t maxDepth;
};

template<typename T, typename DestroyFuncType>
class VulkanResource {
public:
    VulkanResource(VkDevice device, T resource, DestroyFuncType destroyFunc)
        : device_(device), resource_(resource), destroyFunc_(destroyFunc) {}
    ~VulkanResource() { if (resource_ != VK_NULL_HANDLE && destroyFunc_) destroyFunc_(device_, resource_, nullptr); }
    VulkanResource(const VulkanResource&) = delete;
    VulkanResource& operator=(const VulkanResource&) = delete;
    VulkanResource(VulkanResource&& other) noexcept
        : device_(other.device_), resource_(other.resource_), destroyFunc_(other.destroyFunc_) {
        other.resource_ = VK_NULL_HANDLE;
    }
    VulkanResource& operator=(VulkanResource&& other) noexcept {
        if (this != &other) {
            if (resource_ != VK_NULL_HANDLE && destroyFunc_) destroyFunc_(device_, resource_, nullptr);
            device_ = other.device_;
            resource_ = other.resource_;
            destroyFunc_ = other.destroyFunc_;
            other.resource_ = VK_NULL_HANDLE;
        }
        return *this;
    }
    T get() const { return resource_; }
    T* getPtr() { return &resource_; }
private:
    VkDevice device_;
    T resource_;
    DestroyFuncType destroyFunc_;
};

class VulkanDescriptorSet {
public:
    VulkanDescriptorSet(VkDevice device, VkDescriptorPool pool, VkDescriptorSet set, PFN_vkFreeDescriptorSets freeFunc)
        : device_(device), pool_(pool), set_(set), vkFreeDescriptorSets_(freeFunc) {}
    ~VulkanDescriptorSet();
    VulkanDescriptorSet(const VulkanDescriptorSet&) = delete;
    VulkanDescriptorSet& operator=(const VulkanDescriptorSet&) = delete;
    VulkanDescriptorSet(VulkanDescriptorSet&& other) noexcept
        : device_(other.device_), pool_(other.pool_), set_(other.set_), vkFreeDescriptorSets_(other.vkFreeDescriptorSets_) {
        other.set_ = VK_NULL_HANDLE;
        other.vkFreeDescriptorSets_ = nullptr;
    }
    VulkanDescriptorSet& operator=(VulkanDescriptorSet&& other) noexcept;
    VkDescriptorSet get() const { return set_; }
private:
    VkDevice device_;
    VkDescriptorPool pool_;
    VkDescriptorSet set_;
    PFN_vkFreeDescriptorSets vkFreeDescriptorSets_;
};

class VulkanRTX {
public:
    struct ShaderBindingTable {
        VkStridedDeviceAddressRegionKHR raygen{};
        VkStridedDeviceAddressRegionKHR miss{};
        VkStridedDeviceAddressRegionKHR hit{};
        VkStridedDeviceAddressRegionKHR callable{};
        VulkanRTX* parent;

        ShaderBindingTable(VkDevice device, VulkanRTX* parent_);
        ~ShaderBindingTable();
        ShaderBindingTable(ShaderBindingTable&& other) noexcept;
        ShaderBindingTable& operator=(ShaderBindingTable&& other) noexcept;

        VulkanResource<VkBuffer, PFN_vkDestroyBuffer> buffer;
        VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> memory;
    };

    VulkanRTX(VkDevice device, const std::vector<std::string>& shaderPaths, Logging::Logger logger = Logging::Logger());
    ~VulkanRTX() = default;

    void createDescriptorSetLayout();
    void createDescriptorPoolAndSet();
    void createRayTracingPipeline(uint32_t maxRayRecursionDepth);
    void createShaderBindingTable(VkPhysicalDevice physicalDevice);
    void updateDescriptors(VkBuffer cameraBuffer, VkBuffer materialBuffer, VkBuffer dimensionBuffer);
    VkShaderModule createShaderModule(const std::string& filename);
    bool shaderFileExists(const std::string& filename) const;
    void loadShadersAsync(std::vector<VkShaderModule>& modules, const std::vector<std::string>& paths);
    void buildShaderGroups(std::vector<VkRayTracingShaderGroupCreateInfoKHR>& groups,
                           const std::vector<VkPipelineShaderStageCreateInfo>& stages);
    bool hasShaderFeature(ShaderFeatures feature) const { return (static_cast<uint32_t>(shaderFeatures_) & static_cast<uint32_t>(feature)) != 0; }
    void createBuffer(VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
                     VkMemoryPropertyFlags props, VulkanResource<VkBuffer, PFN_vkDestroyBuffer>& buffer,
                     VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>& memory);
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags props);
    VkDeviceAddress getBufferDeviceAddress(VkBuffer buffer);
    VkDeviceAddress getAccelerationStructureDeviceAddress(VkAccelerationStructureKHR as);
    void createBottomLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                             const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries);
    void createTopLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                          const std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4>>& instances);
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
    VkCommandBuffer allocateTransientCommandBuffer(VkCommandPool commandPool);
    void submitAndWaitTransient(VkCommandBuffer cmdBuffer, VkQueue queue, VkCommandPool commandPool);
    void updateDescriptorSetForTLAS(VkAccelerationStructureKHR tlas);
    void compactAccelerationStructures(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue);

private:
    static std::atomic<bool> functionPtrInitialized_;
    static std::atomic<bool> shaderModuleInitialized_;
    static std::mutex functionPtrMutex_; // Added static mutex
    static std::mutex shaderModuleMutex_; // Added static mutex

    VkDevice device_;
    std::vector<std::string> shaderPaths_;
    Logging::Logger logger_;
    VulkanResource<VkDescriptorSetLayout, PFN_vkDestroyDescriptorSetLayout> dsLayout_;
    VulkanResource<VkDescriptorPool, PFN_vkDestroyDescriptorPool> dsPool_;
    VulkanDescriptorSet ds_;
    VulkanResource<VkPipelineLayout, PFN_vkDestroyPipelineLayout> rtPipelineLayout_;
    VulkanResource<VkPipeline, PFN_vkDestroyPipeline> rtPipeline_;
    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> blasBuffer_;
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> blasMemory_;
    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> tlasBuffer_;
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> tlasMemory_;
    VulkanResource<VkAccelerationStructureKHR, PFN_vkDestroyAccelerationStructureKHR> blas_;
    VulkanResource<VkAccelerationStructureKHR, PFN_vkDestroyAccelerationStructureKHR> tlas_;
    VkExtent2D extent_;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> primitiveCounts_;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> previousPrimitiveCounts_;
    std::vector<DimensionData> previousDimensionCache_;
    bool supportsCompaction_;
    ShaderFeatures shaderFeatures_;
    ShaderBindingTable sbt_;

    PFN_vkGetDeviceProcAddr vkGetDeviceProcAddrFunc;
    PFN_vkGetBufferDeviceAddress vkGetBufferDeviceAddressFunc;
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;
    PFN_vkCreateAccelerationStructureKHR vkCreateASFunc;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyASFunc;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetASBuildSizesFunc;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildASFunc;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetASDeviceAddressFunc;
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;
    PFN_vkCmdCopyAccelerationStructureKHR vkCmdCopyAccelerationStructureKHR;
    PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayoutFunc;
    PFN_vkAllocateDescriptorSets vkAllocateDescriptorSetsFunc;
    PFN_vkCreateDescriptorPool vkCreateDescriptorPoolFunc;
    PFN_vkGetPhysicalDeviceProperties2 vkGetPhysicalDeviceProperties2Func;
    PFN_vkCreateShaderModule vkCreateShaderModuleFunc;
    PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout;
    PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool;
    PFN_vkFreeDescriptorSets vkFreeDescriptorSets;
    PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout;
    PFN_vkDestroyPipeline vkDestroyPipeline;
    PFN_vkDestroyBuffer vkDestroyBuffer;
    PFN_vkFreeMemory vkFreeMemory;
    PFN_vkCreateQueryPool vkCreateQueryPool;
    PFN_vkDestroyQueryPool vkDestroyQueryPool;
    PFN_vkGetQueryPoolResults vkGetQueryPoolResults;
    PFN_vkCmdWriteAccelerationStructuresPropertiesKHR vkCmdWriteAccelerationStructuresPropertiesKHR;
    PFN_vkCreateBuffer vkCreateBuffer;
    PFN_vkAllocateMemory vkAllocateMemory;
    PFN_vkBindBufferMemory vkBindBufferMemory;
    PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties;
    PFN_vkBeginCommandBuffer vkBeginCommandBuffer;
    PFN_vkEndCommandBuffer vkEndCommandBuffer;
    PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers;
    PFN_vkQueueSubmit vkQueueSubmit;
    PFN_vkQueueWaitIdle vkQueueWaitIdle;
    PFN_vkFreeCommandBuffers vkFreeCommandBuffers;
    PFN_vkCmdResetQueryPool vkCmdResetQueryPool;
    PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements;
    PFN_vkMapMemory vkMapMemory;
    PFN_vkUnmapMemory vkUnmapMemory;
    PFN_vkCreateImage vkCreateImage;
    PFN_vkDestroyImage vkDestroyImage;
    PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements;
    PFN_vkBindImageMemory vkBindImageMemory;
    PFN_vkCreateImageView vkCreateImageView;
    PFN_vkDestroyImageView vkDestroyImageView;
    PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets;
    PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier;
    PFN_vkCmdBindPipeline vkCmdBindPipeline;
    PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets;
    PFN_vkCmdPushConstants vkCmdPushConstants;
    PFN_vkCmdCopyBuffer vkCmdCopyBuffer;
    PFN_vkCreatePipelineLayout vkCreatePipelineLayout;
    PFN_vkCreateComputePipelines vkCreateComputePipelines;
    PFN_vkCmdDispatch vkCmdDispatch;
    PFN_vkDestroyShaderModule vkDestroyShaderModule;
};

// Define VulkanDescriptorSet methods inline
inline VulkanDescriptorSet::~VulkanDescriptorSet() {
    if (set_ != VK_NULL_HANDLE && vkFreeDescriptorSets_) {
        vkFreeDescriptorSets_(device_, pool_, 1, &set_);
    }
}

inline VulkanDescriptorSet& VulkanDescriptorSet::operator=(VulkanDescriptorSet&& other) noexcept {
    if (this != &other) {
        if (set_ != VK_NULL_HANDLE && vkFreeDescriptorSets_) {
            vkFreeDescriptorSets_(device_, pool_, 1, &set_);
        }
        device_ = other.device_;
        pool_ = other.pool_;
        set_ = other.set_;
        vkFreeDescriptorSets_ = other.vkFreeDescriptorSets_;
        other.set_ = VK_NULL_HANDLE;
        other.vkFreeDescriptorSets_ = nullptr;
    }
    return *this;
}

} // namespace VulkanRTX

#endif // VULKAN_RTX_HPP