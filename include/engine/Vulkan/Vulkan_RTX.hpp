// Vulkan_RTX.hpp
#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <stdexcept>
#include <string>
#include <format>
#include <syncstream>

#define RESET "\033[0m"
#define MAGENTA "\033[1;35m"
#define CYAN "\033[1;36m"
#define YELLOW "\033[1;33m"
#define GREEN "\033[1;32m"

namespace VulkanRTX {

#define VK_CHECK(result, msg) do { if ((result) != VK_SUCCESS) { throw VulkanRTXException(std::string(msg) + " (Error code: " + std::to_string(result) + ")"); } } while (0)

class VulkanRTXException : public std::runtime_error {
public:
    explicit VulkanRTXException(const std::string& message) : std::runtime_error(message) {}
};

template<typename T, typename DestroyFuncType>
class VulkanResource {
public:
    VulkanResource() = default;
    VulkanResource(T resource, VkDevice device, DestroyFuncType destroyFunc)
        : resource_(resource), device_(device), destroyFunc_(destroyFunc) {}
    ~VulkanResource() { reset(); }

    VulkanResource(const VulkanResource&) = delete;
    VulkanResource& operator=(const VulkanResource&) = delete;

    VulkanResource(VulkanResource&& other) noexcept
        : resource_(other.resource_), device_(other.device_), destroyFunc_(other.destroyFunc_) {
        other.resource_ = nullptr;
    }
    VulkanResource& operator=(VulkanResource&& other) noexcept {
        if (this != &other) {
            reset();
            resource_ = other.resource_;
            device_ = other.device_;
            destroyFunc_ = other.destroyFunc_;
            other.resource_ = nullptr;
        }
        return *this;
    }

    T get() const { return resource_; }
    T* getPtr() { return &resource_; }
    void reset() {
        if (resource_ && device_ && destroyFunc_) {
            destroyFunc_(device_, resource_, nullptr);
            resource_ = nullptr;
        }
    }

private:
    T resource_ = nullptr;
    VkDevice device_ = nullptr;
    DestroyFuncType destroyFunc_ = nullptr;
};

class VulkanDescriptorSet {
public:
    VulkanDescriptorSet() = default;
    VulkanDescriptorSet(VkDescriptorSet set, VkDevice device)
        : set_(set), device_(device) {}
    ~VulkanDescriptorSet() { reset(); }

    VulkanDescriptorSet(const VulkanDescriptorSet&) = delete;
    VulkanDescriptorSet& operator=(const VulkanDescriptorSet&) = delete;

    VulkanDescriptorSet(VulkanDescriptorSet&& other) noexcept
        : set_(other.set_), device_(other.device_) {
        other.set_ = nullptr;
    }
    VulkanDescriptorSet& operator=(VulkanDescriptorSet&& other) noexcept {
        if (this != &other) {
            reset();
            set_ = other.set_;
            device_ = other.device_;
            other.set_ = nullptr;
        }
        return *this;
    }

    VkDescriptorSet get() const { return set_; }
    void reset() {
        if (set_ && device_) {
            set_ = nullptr; // Descriptor sets are freed via descriptor pool
        }
    }

private:
    VkDescriptorSet set_ = nullptr;
    VkDevice device_ = nullptr;
};

struct ShaderBindingTable {
    VkStridedDeviceAddressRegionKHR raygen;
    VkStridedDeviceAddressRegionKHR miss;
    VkStridedDeviceAddressRegionKHR hit;
    VkStridedDeviceAddressRegionKHR callable;
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
    uint32_t frame;
    // Add other necessary fields
};

struct DimensionData {
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    float voxelSize;
};

class VulkanRTX {
public:
    VulkanRTX(VkDevice device) : device_(device) {}
    ~VulkanRTX() = default;

    void setDescriptorSetLayout(VulkanResource<VkDescriptorSetLayout, PFN_vkDestroyDescriptorSetLayout>&& layout) {
        dsLayout_ = std::move(layout);
    }
    void setDescriptorPool(VulkanResource<VkDescriptorPool, PFN_vkDestroyDescriptorPool>&& pool) {
        dsPool_ = std::move(pool);
    }
    void setDescriptorSet(VulkanDescriptorSet&& set) {
        ds_ = std::move(set);
    }
    void setRayTracingPipelineLayout(VulkanResource<VkPipelineLayout, PFN_vkDestroyPipelineLayout>&& layout) {
        rtPipelineLayout_ = std::move(layout);
    }
    void setRayTracingPipeline(VulkanResource<VkPipeline, PFN_vkDestroyPipeline>&& pipeline) {
        rtPipeline_ = std::move(pipeline);
    }
    void setBottomLevelASBuffer(VulkanResource<VkBuffer, PFN_vkDestroyBuffer>&& buffer) {
        blasBuffer_ = std::move(buffer);
    }
    void setBottomLevelASMemory(VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>&& memory) {
        blasMemory_ = std::move(memory);
    }
    void setTopLevelASBuffer(VulkanResource<VkBuffer, PFN_vkDestroyBuffer>&& buffer) {
        tlasBuffer_ = std::move(buffer);
    }
    void setTopLevelASMemory(VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>&& memory) {
        tlasMemory_ = std::move(memory);
    }
    void setShaderBindingTable(ShaderBindingTable&& sbt) {
        sbt_ = std::move(sbt);
    }
    void setBottomLevelAS(VulkanResource<VkAccelerationStructureKHR, PFN_vkDestroyAccelerationStructureKHR>&& blas) {
        blas_ = std::move(blas);
    }
    void setTopLevelAS(VulkanResource<VkAccelerationStructureKHR, PFN_vkDestroyAccelerationStructureKHR>&& tlas) {
        tlas_ = std::move(tlas);
    }

    void createBuffer(VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
                     VkMemoryPropertyFlags properties, VulkanResource<VkBuffer, PFN_vkDestroyBuffer>& buffer,
                     VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>& memory);
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags props);
    void createBottomLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                             const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries);
    void createTopLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                          const std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4>>& instances);
    VkDeviceAddress getBufferDeviceAddress(VkBuffer buffer) const;
    VkDeviceAddress getAccelerationStructureDeviceAddress(VkAccelerationStructureKHR as) const;
    VkCommandBuffer allocateTransientCommandBuffer(VkCommandPool commandPool);
    void submitAndWaitTransient(VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool commandPool);
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
    VkShaderModule createShaderModule(const std::string& filename);
    void createDescriptorSetLayout();
    void createDescriptorPoolAndSet();
    void createRayTracingPipeline(uint32_t maxRayRecursionDepth);
    void createShaderBindingTable(VkPhysicalDevice physicalDevice);
    void updateDescriptors(VkBuffer cameraBuffer, VkBuffer materialBuffer, VkBuffer dimensionBuffer);

private:
    VkDevice device_;
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
    VulkanResource<VkAccelerationStructureKHR, PFN_vkDestroyAccelerationStructureKHR> blas_;
    VulkanResource<VkAccelerationStructureKHR, PFN_vkDestroyAccelerationStructureKHR> tlas_;
    VkExtent2D extent_;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> primitiveCounts_;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> previousPrimitiveCounts_;
    std::vector<DimensionData> previousDimensionCache_;
};

} // namespace VulkanRTX

#endif // VULKAN_RTX_HPP