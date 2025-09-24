#include "Vulkan_RTX.hpp"
#include <fstream>
#include <cstring>
#include <array>

// AMOURANTH RTX engine - Implementation of ray tracing pipeline management.
// This file manages the creation and cleanup of Vulkan ray tracing resources, including:
// - Ray tracing pipeline with shaders (raygen, miss, closest-hit, optional any-hit, intersection, callable).
// - Shader Binding Table (SBT) for mapping shaders to ray tracing stages.
// - Bottom-Level Acceleration Structure (BLAS) for geometry.
// - Top-Level Acceleration Structure (TLAS) for instances.
// Key features:
// - Thread-safe: Mutexes protect function pointer loading and shader module creation.
// - Memory-safe: Comprehensive cleanup with null checks in cleanupRTX.
// - Developer-friendly: Detailed error messages, extensive comments, and extensible design.
// - Performance: Parallel shader loading, device-local memory, aligned SBT handles, one-time command buffers.
// - Extensibility: Ready for multiple TLAS instances or descriptor sets for textures/uniforms.
// Requirements:
// - Vulkan 1.2+ with VK_KHR_ray_tracing_pipeline and VK_KHR_acceleration_structure extensions.
// - SPIR-V shader files in assets/shaders/ (raygen.spv, miss.spv, closest_hit.spv, etc.).

// Static initializations for mutexes (thread safety) and Vulkan extension function pointers.
std::mutex VulkanRTX::functionPtrMutex_;
std::mutex VulkanRTX::shaderModuleMutex_;
PFN_vkGetBufferDeviceAddress VulkanRTX::vkGetBufferDeviceAddress = nullptr;
PFN_vkCmdTraceRaysKHR VulkanRTX::vkCmdTraceRaysKHR = nullptr;
PFN_vkCreateAccelerationStructureKHR VulkanRTX::vkCreateAccelerationStructureKHR = nullptr;
PFN_vkDestroyAccelerationStructureKHR VulkanRTX::vkDestroyAccelerationStructureKHR = nullptr;
PFN_vkGetAccelerationStructureBuildSizesKHR VulkanRTX::vkGetAccelerationStructureBuildSizesKHR = nullptr;
PFN_vkCmdBuildAccelerationStructuresKHR VulkanRTX::vkCmdBuildAccelerationStructuresKHR = nullptr;
PFN_vkGetAccelerationStructureDeviceAddressKHR VulkanRTX::vkGetAccelerationStructureDeviceAddressKHR = nullptr;
PFN_vkCreateRayTracingPipelinesKHR VulkanRTX::vkCreateRayTracingPipelinesKHR = nullptr;
PFN_vkGetRayTracingShaderGroupHandlesKHR VulkanRTX::vkGetRayTracingShaderGroupHandlesKHR = nullptr;

// Constructor: Initializes the Vulkan device and loads ray tracing extension function pointers.
// Thread-safe: Uses a mutex to prevent concurrent function pointer loading.
// Throws: std::runtime_error if any function pointer fails to load.
VulkanRTX::VulkanRTX(VkDevice device) : device_(device) {
    std::lock_guard<std::mutex> lock(functionPtrMutex_);
    // Load Vulkan extension functions required for ray tracing.
    vkGetBufferDeviceAddress = reinterpret_cast<PFN_vkGetBufferDeviceAddress>(vkGetDeviceProcAddr(device_, "vkGetBufferDeviceAddress"));
    vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(device_, "vkCmdTraceRaysKHR"));
    vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(device_, "vkCreateAccelerationStructureKHR"));
    vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(device_, "vkDestroyAccelerationStructureKHR"));
    vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(device_, "vkGetAccelerationStructureBuildSizesKHR"));
    vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(device_, "vkCmdBuildAccelerationStructuresKHR"));
    vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(device_, "vkGetAccelerationStructureDeviceAddressKHR"));
    vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddr(device_, "vkCreateRayTracingPipelinesKHR"));
    vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddr(device_, "vkGetRayTracingShaderGroupHandlesKHR"));

    // Validate that all required function pointers were loaded.
    if (!vkGetBufferDeviceAddress || !vkCmdTraceRaysKHR || !vkCreateAccelerationStructureKHR ||
        !vkDestroyAccelerationStructureKHR || !vkGetAccelerationStructureBuildSizesKHR ||
        !vkCmdBuildAccelerationStructuresKHR || !vkGetAccelerationStructureDeviceAddressKHR ||
        !vkCreateRayTracingPipelinesKHR || !vkGetRayTracingShaderGroupHandlesKHR) {
        throw std::runtime_error("Failed to load ray tracing extension functions");
    }
}

// Initializes all ray tracing resources: pipeline, BLAS, TLAS, and SBT.
// Parameters:
// - physicalDevice: Vulkan physical device for memory properties.
// - commandPool: Command pool for temporary command buffers.
// - graphicsQueue: Queue for submitting AS build commands.
// - vertexBuffer, indexBuffer: Buffers containing geometry data.
// - vertexCount, indexCount: Number of vertices and indices (must be valid).
// - rtPipeline, rtPipelineLayout: Output pipeline and layout.
// - sbt: Output shader binding table.
// - tlas, tlasBuffer, tlasMemory: Output top-level acceleration structure and its buffer/memory.
// - blas, blasBuffer, blasMemory: Output bottom-level acceleration structure and its buffer/memory.
// Throws: std::runtime_error on invalid input or resource creation failure.
// Cleanup: Calls cleanupRTX on failure to free partially allocated resources.
void VulkanRTX::initializeRTX(
    VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
    VkBuffer vertexBuffer, VkBuffer indexBuffer, uint32_t vertexCount, uint32_t indexCount,
    VkPipeline& rtPipeline, VkPipelineLayout& rtPipelineLayout, ShaderBindingTable& sbt,
    VkAccelerationStructureKHR& tlas, VkBuffer& tlasBuffer, VkDeviceMemory& tlasMemory,
    VkAccelerationStructureKHR& blas, VkBuffer& blasBuffer, VkDeviceMemory& blasMemory
) {
    // Validate input geometry counts.
    if (vertexCount == 0 || indexCount == 0 || indexCount % 3 != 0) {
        throw std::runtime_error("Invalid vertex or index count for RTX initialization");
    }

    try {
        // Create resources in order: pipeline, BLAS, TLAS, SBT.
        createRayTracingPipeline(rtPipeline, rtPipelineLayout);
        createBottomLevelAS(physicalDevice, commandPool, graphicsQueue, vertexBuffer, indexBuffer, vertexCount, indexCount, blas, blasBuffer, blasMemory);
        createTopLevelAS(physicalDevice, commandPool, graphicsQueue, blas, tlas, tlasBuffer, tlasMemory);
        createShaderBindingTable(physicalDevice, rtPipeline, sbt);
    } catch (...) {
        // Clean up all resources on failure to prevent leaks.
        cleanupRTX(rtPipeline, rtPipelineLayout, sbt.buffer, sbt.memory, tlas, tlasBuffer, tlasMemory, blas, blasBuffer, blasMemory);
        throw; // Re-throw to propagate the error.
    }
}

// Cleans up all ray tracing resources in reverse order of creation.
// Parameters: References to all resources to be destroyed (null-checked internally).
// Thread-safe: Safe to call concurrently as it only accesses device_ (set in constructor).
// Memory-safe: Nulls handles after destruction to prevent double-free.
void VulkanRTX::cleanupRTX(
    VkPipeline& rtPipeline, VkPipelineLayout& rtPipelineLayout,
    VkBuffer& sbtBuffer, VkDeviceMemory& sbtMemory, VkAccelerationStructureKHR& tlas,
    VkBuffer& tlasBuffer, VkDeviceMemory& tlasMemory, VkAccelerationStructureKHR& blas,
    VkBuffer& blasBuffer, VkDeviceMemory& blasMemory
) {
    if (device_ == VK_NULL_HANDLE) return; // Early exit if device is invalid.

    // Helper lambda to destroy a buffer and its memory, nulling handles.
    auto destroyBuffer = [this](VkBuffer& buffer, VkDeviceMemory& memory) {
        if (buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device_, buffer, nullptr);
            buffer = VK_NULL_HANDLE;
        }
        if (memory != VK_NULL_HANDLE) {
            vkFreeMemory(device_, memory, nullptr);
            memory = VK_NULL_HANDLE;
        }
    };

    // Destroy resources in reverse order of creation.
    destroyBuffer(sbtBuffer, sbtMemory);
    if (tlas != VK_NULL_HANDLE) {
        vkDestroyAccelerationStructureKHR(device_, tlas, nullptr);
        tlas = VK_NULL_HANDLE;
    }
    destroyBuffer(tlasBuffer, tlasMemory);
    if (blas != VK_NULL_HANDLE) {
        vkDestroyAccelerationStructureKHR(device_, blas, nullptr);
        blas = VK_NULL_HANDLE;
    }
    destroyBuffer(blasBuffer, blasMemory);
    if (rtPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, rtPipeline, nullptr);
        rtPipeline = VK_NULL_HANDLE;
    }
    if (rtPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, rtPipelineLayout, nullptr);
        rtPipelineLayout = VK_NULL_HANDLE;
    }
}

// Creates the ray tracing pipeline and its layout.
// Parameters:
// - pipeline: Output pipeline handle.
// - rtPipelineLayout: Output pipeline layout handle.
// Features:
// - Loads required shaders (raygen, miss, closest-hit) and optional shaders (any-hit, intersection, callable).
// - Uses std::async for parallel shader loading to improve performance.
// - Supports push constants for dynamic data (viewProj, camPos, etc.).
// - Cleans up shader modules after pipeline creation to save memory.
// Throws: std::runtime_error on shader loading or pipeline creation failure.
// Extensibility: Add descriptor set layouts for textures/uniforms in layoutInfo.
void VulkanRTX::createRayTracingPipeline(VkPipeline& pipeline, VkPipelineLayout& rtPipelineLayout) {
    // Define required and optional shader paths (SPIR-V files).
    constexpr std::array<const char*, 3> requiredShaders = {
        "assets/shaders/raygen.spv",
        "assets/shaders/miss.spv",
        "assets/shaders/closest_hit.spv"
    };
    constexpr std::array<const char*, 3> optionalShaders = {
        "assets/shaders/any_hit.spv",
        "assets/shaders/intersection.spv",
        "assets/shaders/callable.spv"
    };

    // Initialize shader module array (filled with VK_NULL_HANDLE initially).
    std::vector<VkShaderModule> shaderModules(requiredShaders.size() + optionalShaders.size(), VK_NULL_HANDLE);
    std::vector<std::future<VkShaderModule>> shaderFutures;
    std::vector<bool> optionalExists(optionalShaders.size(), false);

    // Load required shaders asynchronously.
    for (size_t i = 0; i < requiredShaders.size(); ++i) {
        shaderFutures.emplace_back(std::async(std::launch::async, [this, i, requiredShaders]() {
            return createShaderModule(requiredShaders[i]);
        }));
    }

    // Load optional shaders if they exist.
    for (size_t i = 0; i < optionalShaders.size(); ++i) {
        if (shaderFileExists(optionalShaders[i])) {
            optionalExists[i] = true;
            shaderFutures.emplace_back(std::async(std::launch::async, [this, i, optionalShaders]() {
                return createShaderModule(optionalShaders[i]);
            }));
        }
    }

    // Collect required shader modules.
    for (size_t i = 0; i < requiredShaders.size(); ++i) {
        shaderModules[i] = shaderFutures[i].get();
        if (shaderModules[i] == VK_NULL_HANDLE) {
            throw std::runtime_error("Failed to load required shader: " + std::string(requiredShaders[i]));
        }
    }
    // Collect optional shader modules.
    size_t futureIdx = requiredShaders.size();
    for (size_t i = 0; i < optionalShaders.size(); ++i) {
        if (optionalExists[i]) {
            shaderModules[requiredShaders.size() + i] = shaderFutures[futureIdx++].get();
        }
    }

    // Assign shader modules to specific roles.
    VkShaderModule raygenShader = shaderModules[0];
    VkShaderModule missShader = shaderModules[1];
    VkShaderModule closestHitShader = shaderModules[2];
    VkShaderModule anyHitShader = optionalExists[0] ? shaderModules[3] : VK_NULL_HANDLE;
    VkShaderModule intersectionShader = optionalExists[1] ? shaderModules[4] : VK_NULL_HANDLE;
    VkShaderModule callableShader = optionalExists[2] ? shaderModules[5] : VK_NULL_HANDLE;

    // Define pipeline shader stages.
    std::vector<VkPipelineShaderStageCreateInfo> stages = {
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_RAYGEN_BIT_KHR, raygenShader, "main", nullptr },
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_MISS_BIT_KHR, missShader, "main", nullptr },
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, closestHitShader, "main", nullptr }
    };
    uint32_t anyHitIndex = static_cast<uint32_t>(stages.size());
    if (anyHitShader != VK_NULL_HANDLE) {
        stages.push_back({ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_ANY_HIT_BIT_KHR, anyHitShader, "main", nullptr });
    }
    uint32_t intersectionIndex = static_cast<uint32_t>(stages.size());
    if (intersectionShader != VK_NULL_HANDLE) {
        stages.push_back({ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_INTERSECTION_BIT_KHR, intersectionShader, "main", nullptr });
    }
    uint32_t callableIndex = static_cast<uint32_t>(stages.size());
    if (callableShader != VK_NULL_HANDLE) {
        stages.push_back({ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_CALLABLE_BIT_KHR, callableShader, "main", nullptr });
    }

    // Define shader groups for ray tracing (mapping shaders to ray types).
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups = {
        { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 0, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, nullptr },
        { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 1, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, nullptr },
        { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR, VK_SHADER_UNUSED_KHR, 2, anyHitShader != VK_NULL_HANDLE ? anyHitIndex : VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, nullptr }
    };
    if (intersectionShader != VK_NULL_HANDLE) {
        groups.push_back({ VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR, intersectionIndex, 2, anyHitShader != VK_NULL_HANDLE ? anyHitIndex : VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, nullptr });
    }
    if (callableShader != VK_NULL_HANDLE) {
        groups.push_back({ VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, callableIndex, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, nullptr });
    }

    // Define push constant range for dynamic data (e.g., viewProj, camPos).
    VkPushConstantRange pushConstant = { VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR, 0, sizeof(PushConstants) };
    VkPipelineLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 0, nullptr, 1, &pushConstant };
    if (vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &rtPipelineLayout) != VK_SUCCESS) {
        // Clean up shader modules on failure.
        for (auto module : shaderModules) {
            if (module != VK_NULL_HANDLE) vkDestroyShaderModule(device_, module, nullptr);
        }
        throw std::runtime_error("Failed to create ray tracing pipeline layout");
    }

    // Create the ray tracing pipeline.
    VkRayTracingPipelineCreateInfoKHR createInfo = {
        VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        nullptr, 0, static_cast<uint32_t>(stages.size()), stages.data(),
        static_cast<uint32_t>(groups.size()), groups.data(),
        2, nullptr, nullptr, nullptr, rtPipelineLayout, VK_NULL_HANDLE, 0
    };
    if (vkCreateRayTracingPipelinesKHR(device_, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline) != VK_SUCCESS) {
        // Clean up pipeline layout and shader modules on failure.
        vkDestroyPipelineLayout(device_, rtPipelineLayout, nullptr);
        for (auto module : shaderModules) {
            if (module != VK_NULL_HANDLE) vkDestroyShaderModule(device_, module, nullptr);
        }
        throw std::runtime_error("Failed to create ray tracing pipeline");
    }

    // Clean up shader modules after pipeline creation to save memory.
    for (auto module : shaderModules) {
        if (module != VK_NULL_HANDLE) vkDestroyShaderModule(device_, module, nullptr);
    }
}

// Creates the Shader Binding Table (SBT) for mapping shaders to ray tracing stages.
// Parameters:
// - physicalDevice: For querying ray tracing properties.
// - pipeline: The ray tracing pipeline.
// - sbt: Output SBT structure with buffer, memory, and region addresses.
// Features:
// - Queries shader group handle size and alignment from device properties.
// - Allocates a host-visible buffer for SBT data.
// - Sets up SBT regions (raygen, miss, hit, callable) with proper alignment.
// Throws: std::runtime_error on failure to get handles or allocate memory.
// Cleanup: Calls cleanupRTX on failure to free partially allocated resources.
void VulkanRTX::createShaderBindingTable(VkPhysicalDevice physicalDevice, VkPipeline pipeline, ShaderBindingTable& sbt) {
    // Query ray tracing pipeline properties (e.g., shader group handle size).
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR, nullptr, 0, 0, 0, 0, 0, 0, 0, 0
    };
    VkPhysicalDeviceProperties2 properties = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &rtProperties, {}
    };
    vkGetPhysicalDeviceProperties2(physicalDevice, &properties);

    // Calculate number of shader groups (3 required + optional intersection/callable).
    uint32_t groupCount = 3;
    if (shaderFileExists("assets/shaders/intersection.spv")) ++groupCount;
    if (shaderFileExists("assets/shaders/callable.spv")) ++groupCount;

    // Compute SBT size with proper alignment for each shader group handle.
    const uint32_t handleSize = rtProperties.shaderGroupHandleSize;
    const uint32_t handleSizeAligned = (handleSize + rtProperties.shaderGroupBaseAlignment - 1) & ~(rtProperties.shaderGroupBaseAlignment - 1);
    const VkDeviceSize sbtSize = groupCount * handleSizeAligned;

    // Get shader group handles from the pipeline.
    std::vector<uint8_t> shaderHandleStorage(sbtSize);
    if (vkGetRayTracingShaderGroupHandlesKHR(device_, pipeline, 0, groupCount, sbtSize, shaderHandleStorage.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to get shader group handles");
    }

    // Allocate a host-visible buffer for the SBT.
    createBuffer(physicalDevice, sbtSize, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sbt.buffer, sbt.memory);
    void* data;
    if (vkMapMemory(device_, sbt.memory, 0, sbtSize, 0, &data) != VK_SUCCESS) {
        // Initialize null variables for unused resources.
        VkPipeline nullPipeline = VK_NULL_HANDLE;
        VkPipelineLayout nullPipelineLayout = VK_NULL_HANDLE;
        VkAccelerationStructureKHR nullTlas = VK_NULL_HANDLE;
        VkBuffer nullTlasBuffer = VK_NULL_HANDLE;
        VkDeviceMemory nullTlasMemory = VK_NULL_HANDLE;
        VkAccelerationStructureKHR nullBlas = VK_NULL_HANDLE;
        VkBuffer nullBlasBuffer = VK_NULL_HANDLE;
        VkDeviceMemory nullBlasMemory = VK_NULL_HANDLE;
        // Clean up SBT buffer and memory on failure.
        cleanupRTX(nullPipeline, nullPipelineLayout, sbt.buffer, sbt.memory, nullTlas, nullTlasBuffer, nullTlasMemory, nullBlas, nullBlasBuffer, nullBlasMemory);
        throw std::runtime_error("Failed to map SBT memory");
    }
    std::memcpy(data, shaderHandleStorage.data(), sbtSize);
    vkUnmapMemory(device_, sbt.memory);

    // Get the device address of the SBT buffer.
    VkBufferDeviceAddressInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, sbt.buffer };
    VkDeviceAddress sbtAddress = vkGetBufferDeviceAddress(device_, &bufferInfo);

    // Set up SBT regions with aligned addresses.
    sbt.raygen = { sbtAddress + 0 * handleSizeAligned, handleSize, handleSizeAligned };
    sbt.miss = { sbtAddress + 1 * handleSizeAligned, handleSize, handleSizeAligned };
    sbt.hit = { sbtAddress + 2 * handleSizeAligned, handleSize, handleSizeAligned };
    sbt.callable = { groupCount > 3 ? sbtAddress + (groupCount - 1) * handleSizeAligned : 0, groupCount > 3 ? handleSize : 0, groupCount > 3 ? handleSizeAligned : 0 };
}

// Creates the Bottom-Level Acceleration Structure (BLAS) for geometry.
// Parameters:
// - physicalDevice: For memory properties and buffer allocation.
// - commandPool: For temporary command buffers.
// - queue: For submitting AS build commands.
// - vertexBuffer, indexBuffer: Geometry data buffers.
// - vertexCount, indexCount: Geometry counts (indices must be divisible by 3).
// - as, asBuffer, asMemory: Output BLAS and its buffer/memory.
// Features:
// - Builds a triangle-based BLAS using vertex and index buffers.
// - Uses a scratch buffer for temporary build data.
// - Submits build commands via a one-time command buffer.
// Throws: std::runtime_error on failure to create resources or submit commands.
// Cleanup: Calls cleanupRTX on failure to free partially allocated resources.
void VulkanRTX::createBottomLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                                    VkBuffer vertexBuffer, VkBuffer indexBuffer, uint32_t vertexCount, uint32_t indexCount,
                                    VkAccelerationStructureKHR& as, VkBuffer& asBuffer, VkDeviceMemory& asMemory) {
    // Get device addresses for vertex and index buffers.
    VkBufferDeviceAddressInfo vertexBufferInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, vertexBuffer };
    VkDeviceAddress vertexAddress = vkGetBufferDeviceAddress(device_, &vertexBufferInfo);
    VkBufferDeviceAddressInfo indexBufferInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, indexBuffer };
    VkDeviceAddress indexAddress = vkGetBufferDeviceAddress(device_, &indexBufferInfo);

    // Define triangle geometry for the BLAS.
    VkAccelerationStructureGeometryTrianglesDataKHR triangles = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR, nullptr,
        VK_FORMAT_R32G32B32_SFLOAT, vertexAddress, sizeof(glm::vec3), vertexCount, VK_INDEX_TYPE_UINT32, indexAddress, {0}
    };
    VkAccelerationStructureGeometryKHR geometry = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR, nullptr,
        VK_GEOMETRY_TYPE_TRIANGLES_KHR, { .triangles = triangles }, VK_GEOMETRY_OPAQUE_BIT_KHR
    };
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR, nullptr,
        VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
        VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &geometry, nullptr, {0}
    };
    uint32_t primitiveCount = indexCount / 3;
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR, nullptr, 0, 0, 0
    };
    vkGetAccelerationStructureBuildSizesKHR(device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &primitiveCount, &sizeInfo);

    // Allocate buffer and memory for the BLAS.
    createBuffer(physicalDevice, sizeInfo.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, asBuffer, asMemory);
    VkAccelerationStructureCreateInfoKHR createInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR, nullptr, 0, asBuffer, 0,
        sizeInfo.accelerationStructureSize, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, 0
    };
    if (vkCreateAccelerationStructureKHR(device_, &createInfo, nullptr, &as) != VK_SUCCESS) {
        // Initialize null variables for unused resources.
        VkPipeline nullPipeline = VK_NULL_HANDLE;
        VkPipelineLayout nullPipelineLayout = VK_NULL_HANDLE;
        VkBuffer nullSbtBuffer = VK_NULL_HANDLE;
        VkDeviceMemory nullSbtMemory = VK_NULL_HANDLE;
        VkAccelerationStructureKHR nullTlas = VK_NULL_HANDLE;
        VkBuffer nullTlasBuffer = VK_NULL_HANDLE;
        VkDeviceMemory nullTlasMemory = VK_NULL_HANDLE;
        VkAccelerationStructureKHR nullBlas = VK_NULL_HANDLE;
        // Clean up BLAS buffer and memory on failure.
        cleanupRTX(nullPipeline, nullPipelineLayout, nullSbtBuffer, nullSbtMemory, nullTlas, nullTlasBuffer, nullTlasMemory, nullBlas, asBuffer, asMemory);
        throw std::runtime_error("Failed to create bottom-level acceleration structure");
    }

    // Allocate scratch buffer for BLAS build.
    VkBuffer scratchBuffer;
    VkDeviceMemory scratchMemory;
    createBuffer(physicalDevice, sizeInfo.buildScratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer, scratchMemory);
    VkBufferDeviceAddressInfo scratchAddressInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, scratchBuffer };
    buildInfo.dstAccelerationStructure = as;
    buildInfo.scratchData.deviceAddress = vkGetBufferDeviceAddress(device_, &scratchAddressInfo);

    // Allocate and record command buffer for BLAS build.
    VkCommandBufferAllocateInfo cmdAllocInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1
    };
    VkCommandBuffer cmdBuffer;
    if (vkAllocateCommandBuffers(device_, &cmdAllocInfo, &cmdBuffer) != VK_SUCCESS) {
        // Initialize null variables for unused resources.
        VkPipeline nullPipeline = VK_NULL_HANDLE;
        VkPipelineLayout nullPipelineLayout = VK_NULL_HANDLE;
        VkBuffer nullSbtBuffer = VK_NULL_HANDLE;
        VkDeviceMemory nullSbtMemory = VK_NULL_HANDLE;
        VkAccelerationStructureKHR nullTlas = VK_NULL_HANDLE;
        VkBuffer nullTlasBuffer = VK_NULL_HANDLE;
        VkDeviceMemory nullTlasMemory = VK_NULL_HANDLE;
        VkAccelerationStructureKHR nullBlas = VK_NULL_HANDLE;
        // Clean up BLAS and scratch buffers on failure.
        cleanupRTX(nullPipeline, nullPipelineLayout, nullSbtBuffer, nullSbtMemory, nullTlas, nullTlasBuffer, nullTlasMemory, nullBlas, asBuffer, asMemory);
        vkDestroyBuffer(device_, scratchBuffer, nullptr);
        vkFreeMemory(device_, scratchMemory, nullptr);
        throw std::runtime_error("Failed to allocate command buffer for BLAS build");
    }
    VkCommandBufferBeginInfo cmdBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr
    };
    if (vkBeginCommandBuffer(cmdBuffer, &cmdBeginInfo) != VK_SUCCESS) {
        // Initialize null variables for unused resources.
        VkPipeline nullPipeline = VK_NULL_HANDLE;
        VkPipelineLayout nullPipelineLayout = VK_NULL_HANDLE;
        VkBuffer nullSbtBuffer = VK_NULL_HANDLE;
        VkDeviceMemory nullSbtMemory = VK_NULL_HANDLE;
        VkAccelerationStructureKHR nullTlas = VK_NULL_HANDLE;
        VkBuffer nullTlasBuffer = VK_NULL_HANDLE;
        VkDeviceMemory nullTlasMemory = VK_NULL_HANDLE;
        VkAccelerationStructureKHR nullBlas = VK_NULL_HANDLE;
        // Clean up BLAS and scratch buffers, free command buffer.
        cleanupRTX(nullPipeline, nullPipelineLayout, nullSbtBuffer, nullSbtMemory, nullTlas, nullTlasBuffer, nullTlasMemory, nullBlas, asBuffer, asMemory);
        vkFreeCommandBuffers(device_, commandPool, 1, &cmdBuffer);
        vkDestroyBuffer(device_, scratchBuffer, nullptr);
        vkFreeMemory(device_, scratchMemory, nullptr);
        throw std::runtime_error("Failed to begin command buffer for BLAS build");
    }
    VkAccelerationStructureBuildRangeInfoKHR buildRange = { primitiveCount, 0, 0, 0 };
    const VkAccelerationStructureBuildRangeInfoKHR* buildRangePtr = &buildRange;
    vkCmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &buildInfo, &buildRangePtr);
    if (vkEndCommandBuffer(cmdBuffer) != VK_SUCCESS) {
        // Initialize null variables for unused resources.
        VkPipeline nullPipeline = VK_NULL_HANDLE;
        VkPipelineLayout nullPipelineLayout = VK_NULL_HANDLE;
        VkBuffer nullSbtBuffer = VK_NULL_HANDLE;
        VkDeviceMemory nullSbtMemory = VK_NULL_HANDLE;
        VkAccelerationStructureKHR nullTlas = VK_NULL_HANDLE;
        VkBuffer nullTlasBuffer = VK_NULL_HANDLE;
        VkDeviceMemory nullTlasMemory = VK_NULL_HANDLE;
        VkAccelerationStructureKHR nullBlas = VK_NULL_HANDLE;
        // Clean up BLAS and scratch buffers, free command buffer.
        cleanupRTX(nullPipeline, nullPipelineLayout, nullSbtBuffer, nullSbtMemory, nullTlas, nullTlasBuffer, nullTlasMemory, nullBlas, asBuffer, asMemory);
        vkFreeCommandBuffers(device_, commandPool, 1, &cmdBuffer);
        vkDestroyBuffer(device_, scratchBuffer, nullptr);
        vkFreeMemory(device_, scratchMemory, nullptr);
        throw std::runtime_error("Failed to end command buffer for BLAS build");
    }
    VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &cmdBuffer, 0, nullptr };
    if (vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        // Initialize null variables for unused resources.
        VkPipeline nullPipeline = VK_NULL_HANDLE;
        VkPipelineLayout nullPipelineLayout = VK_NULL_HANDLE;
        VkBuffer nullSbtBuffer = VK_NULL_HANDLE;
        VkDeviceMemory nullSbtMemory = VK_NULL_HANDLE;
        VkAccelerationStructureKHR nullTlas = VK_NULL_HANDLE;
        VkBuffer nullTlasBuffer = VK_NULL_HANDLE;
        VkDeviceMemory nullTlasMemory = VK_NULL_HANDLE;
        VkAccelerationStructureKHR nullBlas = VK_NULL_HANDLE;
        // Clean up BLAS and scratch buffers, free command buffer.
        cleanupRTX(nullPipeline, nullPipelineLayout, nullSbtBuffer, nullSbtMemory, nullTlas, nullTlasBuffer, nullTlasMemory, nullBlas, asBuffer, asMemory);
        vkFreeCommandBuffers(device_, commandPool, 1, &cmdBuffer);
        vkDestroyBuffer(device_, scratchBuffer, nullptr);
        vkFreeMemory(device_, scratchMemory, nullptr);
        throw std::runtime_error("Failed to submit command buffer for BLAS build");
    }
    // Wait for build completion.
    vkQueueWaitIdle(queue);
    vkFreeCommandBuffers(device_, commandPool, 1, &cmdBuffer);
    vkDestroyBuffer(device_, scratchBuffer, nullptr);
    vkFreeMemory(device_, scratchMemory, nullptr);
}

// Creates the Top-Level Acceleration Structure (TLAS) for instances.
// Parameters:
// - physicalDevice: For memory properties and buffer allocation.
// - commandPool: For temporary command buffers.
// - queue: For submitting AS build commands.
// - blas: Input BLAS to reference in the TLAS.
// - as, asBuffer, asMemory: Output TLAS and its buffer/memory.
// Features:
// - Creates a single instance referencing the input BLAS (extensible to multiple instances).
// - Uses a host-visible instance buffer for instance data.
// - Submits build commands via a one-time command buffer.
// Throws: std::runtime_error on failure to create resources or submit commands.
// Cleanup: Calls cleanupRTX on failure to free partially allocated resources.
// Extensibility: Modify to support multiple instances by using a vector of VkAccelerationStructureInstanceKHR.
void VulkanRTX::createTopLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                                 VkAccelerationStructureKHR blas, VkAccelerationStructureKHR& as, VkBuffer& asBuffer, VkDeviceMemory& asMemory) {
    // Define a 3x4 identity transform matrix for the BLAS instance.
    VkTransformMatrixKHR transform = {{
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f}
    }};
    VkAccelerationStructureInstanceKHR instance = {
        transform, 0, 0xFF, 0, VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR, {0}
    };
    VkAccelerationStructureDeviceAddressInfoKHR blasAddressInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR, nullptr, blas
    };
    instance.accelerationStructureReference = vkGetAccelerationStructureDeviceAddressKHR(device_, &blasAddressInfo);

    // Allocate a host-visible buffer for instance data.
    VkDeviceSize instanceBufferSize = sizeof(VkAccelerationStructureInstanceKHR);
    VkBuffer instanceBuffer;
    VkDeviceMemory instanceMemory;
    createBuffer(physicalDevice, instanceBufferSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, instanceBuffer, instanceMemory);
    void* mappedData;
    if (vkMapMemory(device_, instanceMemory, 0, instanceBufferSize, 0, &mappedData) != VK_SUCCESS) {
        // Initialize null variables for unused resources.
        VkPipeline nullPipeline = VK_NULL_HANDLE;
        VkPipelineLayout nullPipelineLayout = VK_NULL_HANDLE;
        VkBuffer nullSbtBuffer = VK_NULL_HANDLE;
        VkDeviceMemory nullSbtMemory = VK_NULL_HANDLE;
        VkAccelerationStructureKHR nullTlas = VK_NULL_HANDLE;
        VkBuffer nullTlasBuffer = VK_NULL_HANDLE;
        VkDeviceMemory nullTlasMemory = VK_NULL_HANDLE;
        VkAccelerationStructureKHR nullBlas = VK_NULL_HANDLE;
        // Clean up instance buffer on failure.
        cleanupRTX(nullPipeline, nullPipelineLayout, nullSbtBuffer, nullSbtMemory, nullTlas, nullTlasBuffer, nullTlasMemory, nullBlas, instanceBuffer, instanceMemory);
        vkDestroyBuffer(device_, instanceBuffer, nullptr);
        vkFreeMemory(device_, instanceMemory, nullptr);
        throw std::runtime_error("Failed to map TLAS instance memory");
    }
    std::memcpy(mappedData, &instance, instanceBufferSize);
    vkUnmapMemory(device_, instanceMemory);
    VkBufferDeviceAddressInfo instanceBufferAddressInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, instanceBuffer };
    VkDeviceAddress instanceBufferAddress = vkGetBufferDeviceAddress(device_, &instanceBufferAddressInfo);

    // Define TLAS geometry (instances).
    VkAccelerationStructureGeometryKHR geometry = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR, nullptr, VK_GEOMETRY_TYPE_INSTANCES_KHR, {}, VK_GEOMETRY_OPAQUE_BIT_KHR
    };
    geometry.geometry.instances = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR, nullptr, VK_FALSE, instanceBufferAddress };
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR, nullptr, VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR, VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &geometry, nullptr, {0}
    };
    uint32_t instanceCount = 1;
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR, nullptr, 0, 0, 0
    };
    vkGetAccelerationStructureBuildSizesKHR(device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &instanceCount, &sizeInfo);
    createBuffer(physicalDevice, sizeInfo.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, asBuffer, asMemory);
    VkAccelerationStructureCreateInfoKHR createInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR, nullptr, 0, asBuffer, 0, sizeInfo.accelerationStructureSize, VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, 0
    };
    if (vkCreateAccelerationStructureKHR(device_, &createInfo, nullptr, &as) != VK_SUCCESS) {
        // Initialize null variables for unused resources.
        VkPipeline nullPipeline = VK_NULL_HANDLE;
        VkPipelineLayout nullPipelineLayout = VK_NULL_HANDLE;
        VkBuffer nullSbtBuffer = VK_NULL_HANDLE;
        VkDeviceMemory nullSbtMemory = VK_NULL_HANDLE;
        VkAccelerationStructureKHR nullTlas = VK_NULL_HANDLE;
        VkBuffer nullTlasBuffer = VK_NULL_HANDLE;
        VkDeviceMemory nullTlasMemory = VK_NULL_HANDLE;
        VkAccelerationStructureKHR nullBlas = VK_NULL_HANDLE;
        // Clean up TLAS and instance buffers on failure.
        cleanupRTX(nullPipeline, nullPipelineLayout, nullSbtBuffer, nullSbtMemory, nullTlas, nullTlasBuffer, nullTlasMemory, nullBlas, asBuffer, asMemory);
        vkDestroyBuffer(device_, instanceBuffer, nullptr);
        vkFreeMemory(device_, instanceMemory, nullptr);
        throw std::runtime_error("Failed to create top-level acceleration structure");
    }
    VkBuffer scratchBuffer;
    VkDeviceMemory scratchMemory;
    createBuffer(physicalDevice, sizeInfo.buildScratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer, scratchMemory);
    VkBufferDeviceAddressInfo scratchAddressInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, scratchBuffer };
    buildInfo.dstAccelerationStructure = as;
    buildInfo.scratchData.deviceAddress = vkGetBufferDeviceAddress(device_, &scratchAddressInfo);
    VkCommandBufferAllocateInfo cmdAllocInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1
    };
    VkCommandBuffer cmdBuffer;
    if (vkAllocateCommandBuffers(device_, &cmdAllocInfo, &cmdBuffer) != VK_SUCCESS) {
        // Initialize null variables for unused resources.
        VkPipeline nullPipeline = VK_NULL_HANDLE;
        VkPipelineLayout nullPipelineLayout = VK_NULL_HANDLE;
        VkBuffer nullSbtBuffer = VK_NULL_HANDLE;
        VkDeviceMemory nullSbtMemory = VK_NULL_HANDLE;
        VkAccelerationStructureKHR nullTlas = VK_NULL_HANDLE;
        VkBuffer nullTlasBuffer = VK_NULL_HANDLE;
        VkDeviceMemory nullTlasMemory = VK_NULL_HANDLE;
        VkAccelerationStructureKHR nullBlas = VK_NULL_HANDLE;
        // Clean up TLAS, instance, and scratch buffers on failure.
        cleanupRTX(nullPipeline, nullPipelineLayout, nullSbtBuffer, nullSbtMemory, nullTlas, nullTlasBuffer, nullTlasMemory, nullBlas, asBuffer, asMemory);
        vkDestroyBuffer(device_, instanceBuffer, nullptr);
        vkFreeMemory(device_, instanceMemory, nullptr);
        vkDestroyBuffer(device_, scratchBuffer, nullptr);
        vkFreeMemory(device_, scratchMemory, nullptr);
        throw std::runtime_error("Failed to allocate command buffer for TLAS build");
    }
    VkCommandBufferBeginInfo cmdBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr
    };
    if (vkBeginCommandBuffer(cmdBuffer, &cmdBeginInfo) != VK_SUCCESS) {
        // Initialize null variables for unused resources.
        VkPipeline nullPipeline = VK_NULL_HANDLE;
        VkPipelineLayout nullPipelineLayout = VK_NULL_HANDLE;
        VkBuffer nullSbtBuffer = VK_NULL_HANDLE;
        VkDeviceMemory nullSbtMemory = VK_NULL_HANDLE;
        VkAccelerationStructureKHR nullTlas = VK_NULL_HANDLE;
        VkBuffer nullTlasBuffer = VK_NULL_HANDLE;
        VkDeviceMemory nullTlasMemory = VK_NULL_HANDLE;
        VkAccelerationStructureKHR nullBlas = VK_NULL_HANDLE;
        // Clean up TLAS, instance, and scratch buffers, free command buffer.
        cleanupRTX(nullPipeline, nullPipelineLayout, nullSbtBuffer, nullSbtMemory, nullTlas, nullTlasBuffer, nullTlasMemory, nullBlas, asBuffer, asMemory);
        vkFreeCommandBuffers(device_, commandPool, 1, &cmdBuffer);
        vkDestroyBuffer(device_, instanceBuffer, nullptr);
        vkFreeMemory(device_, instanceMemory, nullptr);
        vkDestroyBuffer(device_, scratchBuffer, nullptr);
        vkFreeMemory(device_, scratchMemory, nullptr);
        throw std::runtime_error("Failed to begin command buffer for TLAS build");
    }
    VkAccelerationStructureBuildRangeInfoKHR buildRange = { instanceCount, 0, 0, 0 };
    const VkAccelerationStructureBuildRangeInfoKHR* buildRangePtr = &buildRange;
    vkCmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &buildInfo, &buildRangePtr);
    if (vkEndCommandBuffer(cmdBuffer) != VK_SUCCESS) {
        // Initialize null variables for unused resources.
        VkPipeline nullPipeline = VK_NULL_HANDLE;
        VkPipelineLayout nullPipelineLayout = VK_NULL_HANDLE;
        VkBuffer nullSbtBuffer = VK_NULL_HANDLE;
        VkDeviceMemory nullSbtMemory = VK_NULL_HANDLE;
        VkAccelerationStructureKHR nullTlas = VK_NULL_HANDLE;
        VkBuffer nullTlasBuffer = VK_NULL_HANDLE;
        VkDeviceMemory nullTlasMemory = VK_NULL_HANDLE;
        VkAccelerationStructureKHR nullBlas = VK_NULL_HANDLE;
        // Clean up TLAS, instance, and scratch buffers, free command buffer.
        cleanupRTX(nullPipeline, nullPipelineLayout, nullSbtBuffer, nullSbtMemory, nullTlas, nullTlasBuffer, nullTlasMemory, nullBlas, asBuffer, asMemory);
        vkFreeCommandBuffers(device_, commandPool, 1, &cmdBuffer);
        vkDestroyBuffer(device_, instanceBuffer, nullptr);
        vkFreeMemory(device_, instanceMemory, nullptr);
        vkDestroyBuffer(device_, scratchBuffer, nullptr);
        vkFreeMemory(device_, scratchMemory, nullptr);
        throw std::runtime_error("Failed to end command buffer for TLAS build");
    }
    VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &cmdBuffer, 0, nullptr };
    if (vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        // Initialize null variables for unused resources.
        VkPipeline nullPipeline = VK_NULL_HANDLE;
        VkPipelineLayout nullPipelineLayout = VK_NULL_HANDLE;
        VkBuffer nullSbtBuffer = VK_NULL_HANDLE;
        VkDeviceMemory nullSbtMemory = VK_NULL_HANDLE;
        VkAccelerationStructureKHR nullTlas = VK_NULL_HANDLE;
        VkBuffer nullTlasBuffer = VK_NULL_HANDLE;
        VkDeviceMemory nullTlasMemory = VK_NULL_HANDLE;
        VkAccelerationStructureKHR nullBlas = VK_NULL_HANDLE;
        // Clean up TLAS, instance, and scratch buffers, free command buffer.
        cleanupRTX(nullPipeline, nullPipelineLayout, nullSbtBuffer, nullSbtMemory, nullTlas, nullTlasBuffer, nullTlasMemory, nullBlas, asBuffer, asMemory);
        vkFreeCommandBuffers(device_, commandPool, 1, &cmdBuffer);
        vkDestroyBuffer(device_, instanceBuffer, nullptr);
        vkFreeMemory(device_, instanceMemory, nullptr);
        vkDestroyBuffer(device_, scratchBuffer, nullptr);
        vkFreeMemory(device_, scratchMemory, nullptr);
        throw std::runtime_error("Failed to submit command buffer for TLAS build");
    }
    // Wait for build completion.
    vkQueueWaitIdle(queue);
    vkFreeCommandBuffers(device_, commandPool, 1, &cmdBuffer);
    vkDestroyBuffer(device_, instanceBuffer, nullptr);
    vkFreeMemory(device_, instanceMemory, nullptr);
    vkDestroyBuffer(device_, scratchBuffer, nullptr);
    vkFreeMemory(device_, scratchMemory, nullptr);
}

// Creates a Vulkan buffer with specified properties.
// Parameters:
// - physicalDevice: For memory properties.
// - size: Buffer size in bytes.
// - usage: Buffer usage flags (e.g., VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT).
// - props: Memory property flags (e.g., VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT).
// - buffer, memory: Output buffer and memory handles.
// Throws: std::runtime_error on failure to create or bind buffer/memory.
// Cleanup: Frees partially allocated resources on failure.
void VulkanRTX::createBuffer(VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer& buffer, VkDeviceMemory& memory) {
    VkBufferCreateInfo bufferInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, size, usage, VK_SHARING_MODE_EXCLUSIVE, 0, nullptr
    };
    if (vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) throw std::runtime_error("Buffer creation failed");
    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device_, buffer, &memReqs);
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
    uint32_t memType = UINT32_MAX;
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((memReqs.memoryTypeBits & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props) {
            memType = i;
            break;
        }
    }
    if (memType == UINT32_MAX) {
        vkDestroyBuffer(device_, buffer, nullptr);
        throw std::runtime_error("No suitable memory type");
    }
    VkMemoryAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, memReqs.size, memType };
    if (vkAllocateMemory(device_, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        vkDestroyBuffer(device_, buffer, nullptr);
        throw std::runtime_error("Memory allocation failed");
    }
    if (vkBindBufferMemory(device_, buffer, memory, 0) != VK_SUCCESS) {
        vkDestroyBuffer(device_, buffer, nullptr);
        vkFreeMemory(device_, memory, nullptr);
        throw std::runtime_error("Buffer memory binding failed");
    }
}

// Creates a shader module from a SPIR-V file.
// Parameters:
// - filename: Path to the SPIR-V file.
// Returns: VkShaderModule handle.
// Thread-safe: Uses a mutex to protect shader module creation.
// Throws: std::runtime_error if the file cannot be opened or module creation fails.
VkShaderModule VulkanRTX::createShaderModule(const std::string& filename) {
    std::lock_guard<std::mutex> lock(shaderModuleMutex_);
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("Failed to open shader: " + filename);
    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    VkShaderModuleCreateInfo info = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr, 0, fileSize, reinterpret_cast<const uint32_t*>(buffer.data()) };
    VkShaderModule module;
    if (vkCreateShaderModule(device_, &info, nullptr, &module) != VK_SUCCESS) throw std::runtime_error("Shader module creation failed for " + filename);
    return module;
}

// Checks if a shader file exists.
// Parameters:
// - filename: Path to the SPIR-V file.
// Returns: True if the file exists and is readable, false otherwise.
bool VulkanRTX::shaderFileExists(const std::string& filename) const {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    return file.is_open();
}