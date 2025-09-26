#include "Vulkan_RTX.hpp"
#include <fstream>
#include <cstring>
#include <array>
#include <future>

// AMOURANTH RTX engine September 2025 - Implementation of ray tracing pipeline management.
// This file manages the creation and cleanup of Vulkan ray tracing resources, including:
// - Ray tracing pipeline with shaders (raygen, miss, closest-hit, optional any-hit, intersection, callable).
// - Shader Binding Table (SBT) for mapping shaders to ray tracing stages.
// - Bottom-Level Acceleration Structure (BLAS) for geometry.
// - Top-Level Acceleration Structure (TLAS) for instances.
// - Descriptor sets for TLAS, output storage image, camera uniform buffer, and material storage buffer.
// Key features:
// - Thread-safe: Mutexes protect function pointer loading and shader module creation.
// - Memory-safe: Comprehensive cleanup with null checks in destructor.
// - Developer-friendly: Detailed error messages, extensive comments, and extensible design.
// - Performance: Parallel shader loading, device-local memory, aligned SBT handles with proper padding, one-time command buffers.
// - Extensibility: Easy to add more descriptor bindings or multiple geometries/instances; supports dynamic TLAS override in recording.
// Requirements:
// - Vulkan 1.2+ with VK_KHR_ray_tracing_pipeline, VK_KHR_acceleration_structure, and VK_KHR_buffer_device_address extensions.
// - SPIR-V shader files in assets/shaders/ (raygen.spv, miss.spv, closest_hit.spv, etc.).
// Fixes:
// - Removed "assets/shaders/quad.spv" from requiredShaders to fix "too many initializers" error.
// - Added VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT for buffers with VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT to fix VUID-vkBindBufferMemory-bufferDeviceAddress-03339.
// - Fixed typos in createBottomLevelAS and createTopLevelAS (scratchMemory_ -> scratchMemory, instanceMemory_ -> instanceMemory).
// - Fixed narrowing conversion warning in createBuffer by casting to VkMemoryAllocateFlags.
// Zachary Geurts 2025

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

VulkanRTX::VulkanRTX(VkDevice device) : device_(device) {
    std::lock_guard<std::mutex> lock(functionPtrMutex_);
    vkGetBufferDeviceAddress = reinterpret_cast<PFN_vkGetBufferDeviceAddress>(vkGetDeviceProcAddr(device_, "vkGetBufferDeviceAddress"));
    vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(device_, "vkCmdTraceRaysKHR"));
    vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(device_, "vkCreateAccelerationStructureKHR"));
    vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(device_, "vkDestroyAccelerationStructureKHR"));
    vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(device_, "vkGetAccelerationStructureBuildSizesKHR"));
    vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(device_, "vkCmdBuildAccelerationStructuresKHR"));
    vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(device_, "vkGetAccelerationStructureDeviceAddressKHR"));
    vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddr(device_, "vkCreateRayTracingPipelinesKHR"));
    vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddr(device_, "vkGetRayTracingShaderGroupHandlesKHR"));

    if (!vkGetBufferDeviceAddress || !vkCmdTraceRaysKHR || !vkCreateAccelerationStructureKHR ||
        !vkDestroyAccelerationStructureKHR || !vkGetAccelerationStructureBuildSizesKHR ||
        !vkCmdBuildAccelerationStructuresKHR || !vkGetAccelerationStructureDeviceAddressKHR ||
        !vkCreateRayTracingPipelinesKHR || !vkGetRayTracingShaderGroupHandlesKHR) {
        throw std::runtime_error("Failed to load ray tracing extension functions");
    }
}

VulkanRTX::~VulkanRTX() {
    cleanupRTX();
}

void VulkanRTX::initializeRTX(
    VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
    VkBuffer vertexBuffer, VkBuffer indexBuffer, uint32_t vertexCount, uint32_t indexCount
) {
    if (vertexCount == 0 || indexCount == 0 || indexCount % 3 != 0) {
        throw std::runtime_error("Invalid vertex or index count for RTX initialization");
    }

    try {
        createDescriptorSetLayout();
        createRayTracingPipeline();
        createBottomLevelAS(physicalDevice, commandPool, graphicsQueue, vertexBuffer, indexBuffer, vertexCount, indexCount);
        createTopLevelAS(physicalDevice, commandPool, graphicsQueue);
        createShaderBindingTable(physicalDevice);
        createDescriptorPoolAndSet();
        updateDescriptorSetForTLAS(tlas_);
    } catch (...) {
        cleanupRTX();
        throw;
    }
}

void VulkanRTX::cleanupRTX() {
    if (device_ == VK_NULL_HANDLE) return;

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

    if (ds_ != VK_NULL_HANDLE) {
        ds_ = VK_NULL_HANDLE;
    }
    if (dsPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, dsPool_, nullptr);
        dsPool_ = VK_NULL_HANDLE;
    }
    if (dsLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, dsLayout_, nullptr);
        dsLayout_ = VK_NULL_HANDLE;
    }
    destroyBuffer(sbt_.buffer, sbt_.memory);
    if (tlas_ != VK_NULL_HANDLE) {
        vkDestroyAccelerationStructureKHR(device_, tlas_, nullptr);
        tlas_ = VK_NULL_HANDLE;
    }
    destroyBuffer(tlasBuffer_, tlasMemory_);
    if (blas_ != VK_NULL_HANDLE) {
        vkDestroyAccelerationStructureKHR(device_, blas_, nullptr);
        blas_ = VK_NULL_HANDLE;
    }
    destroyBuffer(blasBuffer_, blasMemory_);
    if (rtPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, rtPipeline_, nullptr);
        rtPipeline_ = VK_NULL_HANDLE;
    }
    if (rtPipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, rtPipelineLayout_, nullptr);
        rtPipelineLayout_ = VK_NULL_HANDLE;
    }
}

void VulkanRTX::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding bindings[4] = {};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 4;
    layoutInfo.pBindings = bindings;

    if (vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &dsLayout_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout for ray tracing");
    }
}

void VulkanRTX::createRayTracingPipeline() {
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

    std::vector<VkShaderModule> shaderModules(requiredShaders.size() + optionalShaders.size(), VK_NULL_HANDLE);
    std::vector<std::future<VkShaderModule>> shaderFutures;
    hasAnyHit_ = shaderFileExists(optionalShaders[0]);
    hasIntersection_ = shaderFileExists(optionalShaders[1]);
    hasCallable_ = shaderFileExists(optionalShaders[2]);

    for (size_t i = 0; i < requiredShaders.size(); ++i) {
        shaderFutures.emplace_back(std::async(std::launch::async, [this, path = requiredShaders[i]]() {
            return createShaderModule(path);
        }));
    }

    if (hasAnyHit_) {
        shaderFutures.emplace_back(std::async(std::launch::async, [this, path = optionalShaders[0]]() {
            return createShaderModule(path);
        }));
    }
    if (hasIntersection_) {
        shaderFutures.emplace_back(std::async(std::launch::async, [this, path = optionalShaders[1]]() {
            return createShaderModule(path);
        }));
    }
    if (hasCallable_) {
        shaderFutures.emplace_back(std::async(std::launch::async, [this, path = optionalShaders[2]]() {
            return createShaderModule(path);
        }));
    }

    for (size_t i = 0; i < requiredShaders.size(); ++i) {
        shaderModules[i] = shaderFutures[i].get();
        if (shaderModules[i] == VK_NULL_HANDLE) {
            throw std::runtime_error("Failed to load required shader: " + std::string(requiredShaders[i]));
        }
    }
    size_t futureIdx = requiredShaders.size();
    size_t moduleIdx = requiredShaders.size();
    if (hasAnyHit_) shaderModules[moduleIdx++] = shaderFutures[futureIdx++].get();
    if (hasIntersection_) shaderModules[moduleIdx++] = shaderFutures[futureIdx++].get();
    if (hasCallable_) shaderModules[moduleIdx++] = shaderFutures[futureIdx++].get();

    VkShaderModule raygenShader = shaderModules[0];
    VkShaderModule missShader = shaderModules[1];
    VkShaderModule closestHitShader = shaderModules[2];
    VkShaderModule anyHitShader = hasAnyHit_ ? shaderModules[3] : VK_NULL_HANDLE;
    VkShaderModule intersectionShader = hasIntersection_ ? shaderModules[hasAnyHit_ ? 4 : 3] : VK_NULL_HANDLE;
    VkShaderModule callableShader = hasCallable_ ? shaderModules.back() : VK_NULL_HANDLE;

    std::vector<VkPipelineShaderStageCreateInfo> stages = {
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_RAYGEN_BIT_KHR, raygenShader, "main", nullptr },
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_MISS_BIT_KHR, missShader, "main", nullptr },
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, closestHitShader, "main", nullptr }
    };
    uint32_t anyHitIndex = hasAnyHit_ ? static_cast<uint32_t>(stages.size()) : VK_SHADER_UNUSED_KHR;
    if (hasAnyHit_) {
        stages.push_back({ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_ANY_HIT_BIT_KHR, anyHitShader, "main", nullptr });
    }
    uint32_t intersectionIndex = hasIntersection_ ? static_cast<uint32_t>(stages.size()) : VK_SHADER_UNUSED_KHR;
    if (hasIntersection_) {
        stages.push_back({ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_INTERSECTION_BIT_KHR, intersectionShader, "main", nullptr });
    }
    uint32_t callableIndex = hasCallable_ ? static_cast<uint32_t>(stages.size()) : VK_SHADER_UNUSED_KHR;
    if (hasCallable_) {
        stages.push_back({ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_CALLABLE_BIT_KHR, callableShader, "main", nullptr });
    }

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups = {
        { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 0, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, nullptr },
        { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 1, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, nullptr },
        { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR, VK_SHADER_UNUSED_KHR, 2, anyHitIndex, VK_SHADER_UNUSED_KHR, nullptr }
    };
    if (hasIntersection_) {
        groups.push_back({ VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR, intersectionIndex, 2, anyHitIndex, VK_SHADER_UNUSED_KHR, nullptr });
    }
    if (hasCallable_) {
        groups.push_back({ VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, callableIndex, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, nullptr });
    }

    VkPushConstantRange pushConstant = { VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR, 0, sizeof(PushConstants) };

    VkPipelineLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 1, &dsLayout_, 1, &pushConstant };
    if (vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &rtPipelineLayout_) != VK_SUCCESS) {
        for (auto module : shaderModules) {
            if (module != VK_NULL_HANDLE) vkDestroyShaderModule(device_, module, nullptr);
        }
        throw std::runtime_error("Failed to create ray tracing pipeline layout");
    }

    VkRayTracingPipelineCreateInfoKHR createInfo = {
        VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        nullptr, 0, static_cast<uint32_t>(stages.size()), stages.data(),
        static_cast<uint32_t>(groups.size()), groups.data(),
        2, nullptr, nullptr, nullptr, rtPipelineLayout_, VK_NULL_HANDLE, 0
    };
    if (vkCreateRayTracingPipelinesKHR(device_, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &createInfo, nullptr, &rtPipeline_) != VK_SUCCESS) {
        vkDestroyPipelineLayout(device_, rtPipelineLayout_, nullptr);
        rtPipelineLayout_ = VK_NULL_HANDLE;
        for (auto module : shaderModules) {
            if (module != VK_NULL_HANDLE) vkDestroyShaderModule(device_, module, nullptr);
        }
        throw std::runtime_error("Failed to create ray tracing pipeline");
    }

    for (auto module : shaderModules) {
        if (module != VK_NULL_HANDLE) vkDestroyShaderModule(device_, module, nullptr);
    }
}

void VulkanRTX::createShaderBindingTable(VkPhysicalDevice physicalDevice) {
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR,
        nullptr, 0, 0, 0, 0, 0, 0, 0, 0
    };
    VkPhysicalDeviceProperties2 properties = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        &rtProperties, {}
    };
    vkGetPhysicalDeviceProperties2(physicalDevice, &properties);

    uint32_t numRaygen = 1;
    uint32_t numMiss = 1;
    uint32_t numHit = 1 + (hasIntersection_ ? 1 : 0);
    uint32_t numCallable = hasCallable_ ? 1 : 0;
    uint32_t groupCount = numRaygen + numMiss + numHit + numCallable;

    const uint32_t handleSize = rtProperties.shaderGroupHandleSize;
    const uint32_t handleAlignment = rtProperties.shaderGroupHandleAlignment;
    const uint32_t handleSizeAligned = ((handleSize + handleAlignment - 1) / handleAlignment) * handleAlignment;

    const VkDeviceSize handlesSize = groupCount * handleSize;
    std::vector<uint8_t> handles(handlesSize);
    if (vkGetRayTracingShaderGroupHandlesKHR(device_, rtPipeline_, 0, groupCount, handlesSize, handles.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to get shader group handles");
    }

    const VkDeviceSize sbtSize = groupCount * handleSizeAligned;
    createBuffer(physicalDevice, sbtSize, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sbt_.buffer, sbt_.memory);

    void* data;
    if (vkMapMemory(device_, sbt_.memory, 0, sbtSize, 0, &data) != VK_SUCCESS) {
        throw std::runtime_error("Failed to map SBT memory");
    }
    uint8_t* pData = static_cast<uint8_t*>(data);
    for (uint32_t g = 0; g < groupCount; ++g) {
        std::memcpy(pData + g * handleSizeAligned, handles.data() + g * handleSize, handleSize);
    }
    vkUnmapMemory(device_, sbt_.memory);

    VkBufferDeviceAddressInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, sbt_.buffer };
    VkDeviceAddress sbtAddress = vkGetBufferDeviceAddress(device_, &bufferInfo);

    uint32_t raygenStart = 0;
    uint32_t missStart = raygenStart + numRaygen;
    uint32_t hitStart = missStart + numMiss;
    uint32_t callableStart = hitStart + numHit;

    sbt_.raygen = { sbtAddress + raygenStart * handleSizeAligned, numRaygen * handleSizeAligned, handleSizeAligned };
    sbt_.miss = { sbtAddress + missStart * handleSizeAligned, numMiss * handleSizeAligned, handleSizeAligned };
    sbt_.hit = { sbtAddress + hitStart * handleSizeAligned, numHit * handleSizeAligned, handleSizeAligned };
    sbt_.callable = { numCallable ? sbtAddress + callableStart * handleSizeAligned : 0, numCallable * handleSizeAligned, handleSizeAligned };
}

void VulkanRTX::createBottomLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                                    VkBuffer vertexBuffer, VkBuffer indexBuffer, uint32_t vertexCount, uint32_t indexCount) {
    VkBufferDeviceAddressInfo vertexBufferInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, vertexBuffer };
    VkDeviceAddress vertexAddress = vkGetBufferDeviceAddress(device_, &vertexBufferInfo);
    VkBufferDeviceAddressInfo indexBufferInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, indexBuffer };
    VkDeviceAddress indexAddress = vkGetBufferDeviceAddress(device_, &indexBufferInfo);

    VkAccelerationStructureGeometryTrianglesDataKHR triangles = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR, nullptr,
        VK_FORMAT_R32G32B32_SFLOAT, vertexAddress, sizeof(glm::vec3) * 2, vertexCount / 2, VK_INDEX_TYPE_UINT32, indexAddress, {0}
    };
    VkAccelerationStructureGeometryKHR geometry = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR, nullptr,
        VK_GEOMETRY_TYPE_TRIANGLES_KHR, { .triangles = triangles }, VK_GEOMETRY_OPAQUE_BIT_KHR
    };
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR, nullptr,
        VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &geometry, nullptr, {0}
    };
    uint32_t primitiveCount = indexCount / 3;
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR, nullptr, 0, 0, 0
    };
    vkGetAccelerationStructureBuildSizesKHR(device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &primitiveCount, &sizeInfo);

    createBuffer(physicalDevice, sizeInfo.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, blasBuffer_, blasMemory_);
    VkAccelerationStructureCreateInfoKHR createInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR, nullptr, 0, blasBuffer_, 0,
        sizeInfo.accelerationStructureSize, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, 0
    };
    if (vkCreateAccelerationStructureKHR(device_, &createInfo, nullptr, &blas_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create bottom-level acceleration structure");
    }

    VkBuffer scratchBuffer = VK_NULL_HANDLE;
    VkDeviceMemory scratchMemory = VK_NULL_HANDLE;
    createBuffer(physicalDevice, sizeInfo.buildScratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer, scratchMemory);
    VkBufferDeviceAddressInfo scratchAddressInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, scratchBuffer };
    buildInfo.dstAccelerationStructure = blas_;
    buildInfo.scratchData.deviceAddress = vkGetBufferDeviceAddress(device_, &scratchAddressInfo);

    VkCommandBufferAllocateInfo cmdAllocInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1
    };
    VkCommandBuffer cmdBuffer;
    if (vkAllocateCommandBuffers(device_, &cmdAllocInfo, &cmdBuffer) != VK_SUCCESS) {
        vkDestroyBuffer(device_, scratchBuffer, nullptr);
        vkFreeMemory(device_, scratchMemory, nullptr);
        throw std::runtime_error("Failed to allocate command buffer for BLAS build");
    }
    VkCommandBufferBeginInfo cmdBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr
    };
    if (vkBeginCommandBuffer(cmdBuffer, &cmdBeginInfo) != VK_SUCCESS) {
        vkFreeCommandBuffers(device_, commandPool, 1, &cmdBuffer);
        vkDestroyBuffer(device_, scratchBuffer, nullptr);
        vkFreeMemory(device_, scratchMemory, nullptr);
        throw std::runtime_error("Failed to begin command buffer for BLAS build");
    }
    VkAccelerationStructureBuildRangeInfoKHR buildRange = { primitiveCount, 0, 0, 0 };
    const VkAccelerationStructureBuildRangeInfoKHR* buildRangePtr = &buildRange;
    vkCmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &buildInfo, &buildRangePtr);
    if (vkEndCommandBuffer(cmdBuffer) != VK_SUCCESS) {
        vkFreeCommandBuffers(device_, commandPool, 1, &cmdBuffer);
        vkDestroyBuffer(device_, scratchBuffer, nullptr);
        vkFreeMemory(device_, scratchMemory, nullptr);
        throw std::runtime_error("Failed to end command buffer for BLAS build");
    }
    VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &cmdBuffer, 0, nullptr };
    if (vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        vkFreeCommandBuffers(device_, commandPool, 1, &cmdBuffer);
        vkDestroyBuffer(device_, scratchBuffer, nullptr);
        vkFreeMemory(device_, scratchMemory, nullptr);
        throw std::runtime_error("Failed to submit command buffer for BLAS build");
    }
    vkQueueWaitIdle(queue);
    vkFreeCommandBuffers(device_, commandPool, 1, &cmdBuffer);
    vkDestroyBuffer(device_, scratchBuffer, nullptr);
    vkFreeMemory(device_, scratchMemory, nullptr);
}

void VulkanRTX::createTopLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue) {
    VkTransformMatrixKHR transform = {{
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f}
    }};
    VkAccelerationStructureInstanceKHR instance = {
        transform, 0, 0xFF, 0, VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR, {0}
    };
    VkAccelerationStructureDeviceAddressInfoKHR blasAddressInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR, nullptr, blas_
    };
    instance.accelerationStructureReference = vkGetAccelerationStructureDeviceAddressKHR(device_, &blasAddressInfo);

    VkDeviceSize instanceBufferSize = sizeof(VkAccelerationStructureInstanceKHR);
    VkBuffer instanceBuffer = VK_NULL_HANDLE;
    VkDeviceMemory instanceMemory = VK_NULL_HANDLE;
    createBuffer(physicalDevice, instanceBufferSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, instanceBuffer, instanceMemory);
    void* mappedData;
    if (vkMapMemory(device_, instanceMemory, 0, instanceBufferSize, 0, &mappedData) != VK_SUCCESS) {
        vkDestroyBuffer(device_, instanceBuffer, nullptr);
        vkFreeMemory(device_, instanceMemory, nullptr);
        throw std::runtime_error("Failed to map TLAS instance memory");
    }
    std::memcpy(mappedData, &instance, instanceBufferSize);
    vkUnmapMemory(device_, instanceMemory);
    VkBufferDeviceAddressInfo instanceBufferAddressInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, instanceBuffer };
    VkDeviceAddress instanceBufferAddress = vkGetBufferDeviceAddress(device_, &instanceBufferAddressInfo);

    VkAccelerationStructureGeometryKHR geometry = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR, nullptr, VK_GEOMETRY_TYPE_INSTANCES_KHR, {}, VK_GEOMETRY_OPAQUE_BIT_KHR
    };
    geometry.geometry.instances = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR, nullptr, VK_FALSE, instanceBufferAddress };
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR, nullptr, VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR, VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &geometry, nullptr, {0}
    };
    uint32_t instanceCount = 1;
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR, nullptr, 0, 0, 0
    };
    vkGetAccelerationStructureBuildSizesKHR(device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &instanceCount, &sizeInfo);
    createBuffer(physicalDevice, sizeInfo.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, tlasBuffer_, tlasMemory_);
    VkAccelerationStructureCreateInfoKHR createInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR, nullptr, 0, tlasBuffer_, 0, sizeInfo.accelerationStructureSize, VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, 0
    };
    if (vkCreateAccelerationStructureKHR(device_, &createInfo, nullptr, &tlas_) != VK_SUCCESS) {
        vkDestroyBuffer(device_, instanceBuffer, nullptr);
        vkFreeMemory(device_, instanceMemory, nullptr);
        throw std::runtime_error("Failed to create top-level acceleration structure");
    }
    VkBuffer scratchBuffer = VK_NULL_HANDLE;
    VkDeviceMemory scratchMemory = VK_NULL_HANDLE;
    createBuffer(physicalDevice, sizeInfo.buildScratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer, scratchMemory);
    VkBufferDeviceAddressInfo scratchAddressInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, scratchBuffer };
    buildInfo.dstAccelerationStructure = tlas_;
    buildInfo.scratchData.deviceAddress = vkGetBufferDeviceAddress(device_, &scratchAddressInfo);
    VkCommandBufferAllocateInfo cmdAllocInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1
    };
    VkCommandBuffer cmdBuffer;
    if (vkAllocateCommandBuffers(device_, &cmdAllocInfo, &cmdBuffer) != VK_SUCCESS) {
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
        vkFreeCommandBuffers(device_, commandPool, 1, &cmdBuffer);
        vkDestroyBuffer(device_, instanceBuffer, nullptr);
        vkFreeMemory(device_, instanceMemory, nullptr);
        vkDestroyBuffer(device_, scratchBuffer, nullptr);
        vkFreeMemory(device_, scratchMemory, nullptr);
        throw std::runtime_error("Failed to end command buffer for TLAS build");
    }
    VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &cmdBuffer, 0, nullptr };
    if (vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        vkFreeCommandBuffers(device_, commandPool, 1, &cmdBuffer);
        vkDestroyBuffer(device_, instanceBuffer, nullptr);
        vkFreeMemory(device_, instanceMemory, nullptr);
        vkDestroyBuffer(device_, scratchBuffer, nullptr);
        vkFreeMemory(device_, scratchMemory, nullptr);
        throw std::runtime_error("Failed to submit command buffer for TLAS build");
    }
    vkQueueWaitIdle(queue);
    vkFreeCommandBuffers(device_, commandPool, 1, &cmdBuffer);
    vkDestroyBuffer(device_, instanceBuffer, nullptr);
    vkFreeMemory(device_, instanceMemory, nullptr);
    vkDestroyBuffer(device_, scratchBuffer, nullptr);
    vkFreeMemory(device_, scratchMemory, nullptr);
}

void VulkanRTX::createDescriptorPoolAndSet() {
    VkDescriptorPoolSize poolSizes[4] = {
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 }
    };
    VkDescriptorPoolCreateInfo poolInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, nullptr, 0, 1, 4, poolSizes
    };
    if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &dsPool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool for ray tracing");
    }

    VkDescriptorSetAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, dsPool_, 1, &dsLayout_
    };
    if (vkAllocateDescriptorSets(device_, &allocInfo, &ds_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor set for ray tracing");
    }
}

void VulkanRTX::createRayTracingDescriptorSet(VkDescriptorPool descriptorPool, VkBuffer cameraBuffer, VkBuffer materialBuffer) {
    VkDescriptorSetAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, descriptorPool, 1, &dsLayout_
    };
    if (vkAllocateDescriptorSets(device_, &allocInfo, &ds_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate ray tracing descriptor set");
    }

    VkWriteDescriptorSetAccelerationStructureKHR asInfo = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR, nullptr, 1, &tlas_
    };
    VkDescriptorBufferInfo cameraInfo = { cameraBuffer, 0, VK_WHOLE_SIZE };
    VkDescriptorBufferInfo materialInfo = { materialBuffer, 0, VK_WHOLE_SIZE };

    std::vector<VkWriteDescriptorSet> writes(3);
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].pNext = &asInfo;
    writes[0].dstSet = ds_;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = ds_;
    writes[1].dstBinding = 2;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[1].pBufferInfo = &cameraInfo;

    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = ds_;
    writes[2].dstBinding = 3;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[2].pBufferInfo = &materialInfo;

    vkUpdateDescriptorSets(device_, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void VulkanRTX::updateDescriptorSetForTLAS(VkAccelerationStructureKHR tlas) {
    VkWriteDescriptorSetAccelerationStructureKHR asInfo = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        nullptr, 1, &tlas
    };
    VkWriteDescriptorSet accelWrite = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        &asInfo, ds_, 0, 0, 1, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
        nullptr, nullptr, nullptr
    };
    vkUpdateDescriptorSets(device_, 1, &accelWrite, 0, nullptr);
}

void VulkanRTX::createBuffer(VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer& buffer, VkDeviceMemory& memory) {
    VkBufferCreateInfo bufferInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, size, usage, VK_SHARING_MODE_EXCLUSIVE, 0, nullptr
    };
    if (vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Buffer creation failed");
    }
    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device_, buffer, &memReqs);
    uint32_t memType = findMemoryType(physicalDevice, memReqs.memoryTypeBits, props);

    VkMemoryAllocateFlagsInfo allocFlagsInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO, nullptr,
        static_cast<VkMemoryAllocateFlags>(usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT ? VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT : 0), 0
    };
    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) ? &allocFlagsInfo : nullptr,
        memReqs.size, memType
    };
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

uint32_t VulkanRTX::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    throw std::runtime_error("No suitable memory type found");
    return UINT32_MAX;
}

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

bool VulkanRTX::shaderFileExists(const std::string& filename) const {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    return file.is_open();
}

void VulkanRTX::createStorageImage(
    VkPhysicalDevice physicalDevice, VkExtent2D extent, VkFormat format,
    VkImage& image, VkImageView& imageView, VkDeviceMemory& memory
) {
    VkImageCreateInfo imageInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, nullptr, 0, VK_IMAGE_TYPE_2D, format,
        {extent.width, extent.height, 1}, 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_SHARING_MODE_EXCLUSIVE, 0, nullptr, VK_IMAGE_LAYOUT_UNDEFINED
    };
    if (vkCreateImage(device_, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create storage image");
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device_, image, &memReqs);
    uint32_t memType = findMemoryType(physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkMemoryAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, memReqs.size, memType };
    if (vkAllocateMemory(device_, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        vkDestroyImage(device_, image, nullptr);
        throw std::runtime_error("Failed to allocate memory for storage image");
    }
    if (vkBindImageMemory(device_, image, memory, 0) != VK_SUCCESS) {
        vkDestroyImage(device_, image, nullptr);
        vkFreeMemory(device_, memory, nullptr);
        throw std::runtime_error("Failed to bind memory for storage image");
    }

    VkImageViewCreateInfo viewInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, nullptr, 0, image, VK_IMAGE_VIEW_TYPE_2D, format,
        {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    if (vkCreateImageView(device_, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        vkDestroyImage(device_, image, nullptr);
        vkFreeMemory(device_, memory, nullptr);
        throw std::runtime_error("Failed to create image view for storage image");
    }
}

void VulkanRTX::recordRayTracingCommands(
    VkCommandBuffer cmdBuffer, VkExtent2D extent, VkImage outputImage, VkImageView outputImageView,
    const PushConstants& pc, VkAccelerationStructureKHR tlas
) {
    if (tlas == VK_NULL_HANDLE) tlas = tlas_;

    if (tlas != tlas_) {
        updateDescriptorSetForTLAS(tlas);
    }

    VkDescriptorImageInfo imageInfo = { VK_NULL_HANDLE, outputImageView, VK_IMAGE_LAYOUT_GENERAL };
    VkWriteDescriptorSet imageWrite = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, ds_, 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imageInfo, nullptr, nullptr
    };
    vkUpdateDescriptorSets(device_, 1, &imageWrite, 0, nullptr);

    VkImageMemoryBarrier barrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, 0, VK_ACCESS_SHADER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
        outputImage, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline_);
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipelineLayout_, 0, 1, &ds_, 0, nullptr);

    vkCmdPushConstants(cmdBuffer, rtPipelineLayout_, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR, 0, sizeof(PushConstants), &pc);

    vkCmdTraceRaysKHR(cmdBuffer, &sbt_.raygen, &sbt_.miss, &sbt_.hit, &sbt_.callable, extent.width, extent.height, 1);

    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    if (tlas != tlas_) {
        updateDescriptorSetForTLAS(tlas_);
    }
}