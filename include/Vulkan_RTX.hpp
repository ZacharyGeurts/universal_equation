#ifndef VULKAN_RTX_HPP
#define VULKAN_RTX_HPP
// AMOURANTH RTX engine
#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>
#include <fstream>
#include <glm/glm.hpp>
#include <mutex>
#include <thread>
#include <future>

// Class for managing Vulkan ray tracing resources and operations.
class VulkanRTX {
public:
    struct ShaderBindingTable {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkStridedDeviceAddressRegionKHR raygen = {};
        VkStridedDeviceAddressRegionKHR miss = {};
        VkStridedDeviceAddressRegionKHR hit = {};
        VkStridedDeviceAddressRegionKHR callable = {};
    };

    struct PushConstants {
        glm::mat4 viewProj;
        glm::vec3 camPos;
        float wavePhase;
        float cycleProgress;
        float zoomFactor;
        float interactionStrength;
        float darkMatter;
        float darkEnergy;
    };

    // Constructor to initialize function pointers safely
    VulkanRTX(VkDevice device);

    // Initializes ray tracing resources
    void initializeRTX(
        VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
        VkBuffer vertexBuffer, VkBuffer indexBuffer, uint32_t vertexCount, uint32_t indexCount,
        VkPipeline& rtPipeline, VkPipelineLayout& rtPipelineLayout, ShaderBindingTable& sbt,
        VkAccelerationStructureKHR& tlas, VkBuffer& tlasBuffer, VkDeviceMemory& tlasMemory,
        VkAccelerationStructureKHR& blas, VkBuffer& blasBuffer, VkDeviceMemory& blasMemory
    );

    // Cleans up ray tracing resources with RAII
    void cleanupRTX(
        VkPipeline& rtPipeline, VkPipelineLayout& rtPipelineLayout,
        VkBuffer& sbtBuffer, VkDeviceMemory& sbtMemory, VkAccelerationStructureKHR& tlas,
        VkBuffer& tlasBuffer, VkDeviceMemory& tlasMemory, VkAccelerationStructureKHR& blas,
        VkBuffer& blasBuffer, VkDeviceMemory& blasMemory
    );

private:
    VkDevice device_; // Store device for internal use
    static std::mutex functionPtrMutex_; // Mutex for function pointer initialization
    static std::mutex shaderModuleMutex_; // Mutex for shader module creation

    // Function pointers for ray tracing extensions
    static PFN_vkGetBufferDeviceAddress vkGetBufferDeviceAddress;
    static PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;
    static PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
    static PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
    static PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
    static PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
    static PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;

    void createRayTracingPipeline(VkPipeline& pipeline, VkPipelineLayout& rtPipelineLayout);
    void createShaderBindingTable(VkPhysicalDevice physicalDevice, VkPipeline pipeline, ShaderBindingTable& sbt);
    void createBottomLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                             VkBuffer vertexBuffer, VkBuffer indexBuffer, uint32_t vertexCount, uint32_t indexCount,
                             VkAccelerationStructureKHR& as, VkBuffer& asBuffer, VkDeviceMemory& asMemory);
    void createTopLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                          VkAccelerationStructureKHR blas, VkAccelerationStructureKHR& as, VkBuffer& asBuffer, VkDeviceMemory& asMemory);
    void createBuffer(VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer& buffer, VkDeviceMemory& memory);
    VkShaderModule createShaderModule(const std::string& filename);
};

#endif // VULKAN_RTX_HPP