#ifndef VULKAN_RTX_HPP
#define VULKAN_RTX_HPP
// AMOURANTH RTX engine
#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>
#include <fstream>
#include <glm/glm.hpp>

// Class for managing Vulkan ray tracing resources and operations.
class VulkanRTX {
public:
    // Structure for the Shader Binding Table (SBT) used in ray tracing.
    struct ShaderBindingTable {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkStridedDeviceAddressRegionKHR raygen = {};
        VkStridedDeviceAddressRegionKHR miss = {};
        VkStridedDeviceAddressRegionKHR hit = {};
        VkStridedDeviceAddressRegionKHR callable = {};
    };

    // Push constants structure for passing data to shaders.
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

    // Initializes ray tracing resources including pipeline, acceleration structures, and SBT.
    static void initializeRTX(
        VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
        VkBuffer vertexBuffer, VkBuffer indexBuffer, uint32_t vertexCount, uint32_t indexCount,
        VkPipeline& rtPipeline, VkPipelineLayout& rtPipelineLayout, ShaderBindingTable& sbt,
        VkAccelerationStructureKHR& tlas, VkBuffer& tlasBuffer, VkDeviceMemory& tlasMemory,
        VkAccelerationStructureKHR& blas, VkBuffer& blasBuffer, VkDeviceMemory& blasMemory
    );

    // Cleans up ray tracing resources.
    static void cleanupRTX(
        VkDevice device, VkPipeline& rtPipeline, VkPipelineLayout& rtPipelineLayout,
        VkBuffer& sbtBuffer, VkDeviceMemory& sbtMemory, VkAccelerationStructureKHR& tlas,
        VkBuffer& tlasBuffer, VkDeviceMemory& tlasMemory, VkAccelerationStructureKHR& blas,
        VkBuffer& blasBuffer, VkDeviceMemory& blasMemory
    );

private:
    // Creates the ray tracing pipeline.
    static void createRayTracingPipeline(VkDevice device, VkPipeline& pipeline, VkPipelineLayout& rtPipelineLayout);

    // Creates the Shader Binding Table (SBT).
    static void createShaderBindingTable(VkDevice device, VkPhysicalDevice physicalDevice, VkPipeline pipeline, ShaderBindingTable& sbt);

    // Creates the Bottom-Level Acceleration Structure (BLAS).
    static void createBottomLevelAS(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                                    VkBuffer vertexBuffer, VkBuffer indexBuffer, uint32_t vertexCount, uint32_t indexCount,
                                    VkAccelerationStructureKHR& as, VkBuffer& asBuffer, VkDeviceMemory& asMemory);

    // Creates the Top-Level Acceleration Structure (TLAS).
    static void createTopLevelAS(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                                 VkAccelerationStructureKHR blas, VkAccelerationStructureKHR& as, VkBuffer& asBuffer, VkDeviceMemory& asMemory);

    // Utility to create a Vulkan buffer.
    static void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer& buffer, VkDeviceMemory& memory);

    // Utility to create a shader module from a SPIR-V file.
    static VkShaderModule createShaderModule(VkDevice device, const std::string& filename);

    // Function pointers for ray tracing extensions (declarations only).
    static PFN_vkGetBufferDeviceAddress vkGetBufferDeviceAddress;
    static PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;
    static PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
    static PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
    static PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
    static PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
    static PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
};

#endif // VULKAN_RTX_HPP