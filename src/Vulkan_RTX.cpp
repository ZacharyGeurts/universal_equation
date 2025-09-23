#include "Vulkan_RTX.hpp"
#include <algorithm>
#include <cstring>
// AMOURANTH RTX engine
// Static mutexes and function pointers
std::mutex VulkanRTX::functionPtrMutex_;
std::mutex VulkanRTX::shaderModuleMutex_;
PFN_vkGetBufferDeviceAddress VulkanRTX::vkGetBufferDeviceAddress = nullptr;
PFN_vkCmdTraceRaysKHR VulkanRTX::vkCmdTraceRaysKHR = nullptr;
PFN_vkCreateAccelerationStructureKHR VulkanRTX::vkCreateAccelerationStructureKHR = nullptr;
PFN_vkDestroyAccelerationStructureKHR VulkanRTX::vkDestroyAccelerationStructureKHR = nullptr;
PFN_vkGetAccelerationStructureBuildSizesKHR VulkanRTX::vkGetAccelerationStructureBuildSizesKHR = nullptr;
PFN_vkCmdBuildAccelerationStructuresKHR VulkanRTX::vkCmdBuildAccelerationStructuresKHR = nullptr;
PFN_vkGetAccelerationStructureDeviceAddressKHR VulkanRTX::vkGetAccelerationStructureDeviceAddressKHR = nullptr;

VulkanRTX::VulkanRTX(VkDevice device) : device_(device) {
    std::lock_guard<std::mutex> lock(functionPtrMutex_);
    if (vkGetBufferDeviceAddress == nullptr) vkGetBufferDeviceAddress = (PFN_vkGetBufferDeviceAddress)vkGetDeviceProcAddr(device_, "vkGetBufferDeviceAddress");
    if (vkCmdTraceRaysKHR == nullptr) vkCmdTraceRaysKHR = (PFN_vkCmdTraceRaysKHR)vkGetDeviceProcAddr(device_, "vkCmdTraceRaysKHR");
    if (vkCreateAccelerationStructureKHR == nullptr) vkCreateAccelerationStructureKHR = (PFN_vkCreateAccelerationStructureKHR)vkGetDeviceProcAddr(device_, "vkCreateAccelerationStructureKHR");
    if (vkDestroyAccelerationStructureKHR == nullptr) vkDestroyAccelerationStructureKHR = (PFN_vkDestroyAccelerationStructureKHR)vkGetDeviceProcAddr(device_, "vkDestroyAccelerationStructureKHR");
    if (vkGetAccelerationStructureBuildSizesKHR == nullptr) vkGetAccelerationStructureBuildSizesKHR = (PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetDeviceProcAddr(device_, "vkGetAccelerationStructureBuildSizesKHR");
    if (vkCmdBuildAccelerationStructuresKHR == nullptr) vkCmdBuildAccelerationStructuresKHR = (PFN_vkCmdBuildAccelerationStructuresKHR)vkGetDeviceProcAddr(device_, "vkCmdBuildAccelerationStructuresKHR");
    if (vkGetAccelerationStructureDeviceAddressKHR == nullptr) vkGetAccelerationStructureDeviceAddressKHR = (PFN_vkGetAccelerationStructureDeviceAddressKHR)vkGetDeviceProcAddr(device_, "vkGetAccelerationStructureDeviceAddressKHR");

    if (!vkGetBufferDeviceAddress || !vkCmdTraceRaysKHR || !vkCreateAccelerationStructureKHR ||
        !vkDestroyAccelerationStructureKHR || !vkGetAccelerationStructureBuildSizesKHR ||
        !vkCmdBuildAccelerationStructuresKHR || !vkGetAccelerationStructureDeviceAddressKHR) {
        throw std::runtime_error("Failed to load ray tracing extension functions");
    }
}

void VulkanRTX::initializeRTX(
    VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
    VkBuffer vertexBuffer, VkBuffer indexBuffer, uint32_t vertexCount, uint32_t indexCount,
    VkPipeline& rtPipeline, VkPipelineLayout& rtPipelineLayout, ShaderBindingTable& sbt,
    VkAccelerationStructureKHR& tlas, VkBuffer& tlasBuffer, VkDeviceMemory& tlasMemory,
    VkAccelerationStructureKHR& blas, VkBuffer& blasBuffer, VkDeviceMemory& blasMemory
) {
    try {
        createRayTracingPipeline(rtPipeline, rtPipelineLayout);
        createBottomLevelAS(physicalDevice, commandPool, graphicsQueue, vertexBuffer, indexBuffer, vertexCount, indexCount, blas, blasBuffer, blasMemory);
        createTopLevelAS(physicalDevice, commandPool, graphicsQueue, blas, tlas, tlasBuffer, tlasMemory);
        createShaderBindingTable(physicalDevice, rtPipeline, sbt);
    } catch (...) {
        cleanupRTX(rtPipeline, rtPipelineLayout, sbt.buffer, sbt.memory, tlas, tlasBuffer, tlasMemory, blas, blasBuffer, blasMemory);
        throw;
    }
}

void VulkanRTX::cleanupRTX(
    VkPipeline& rtPipeline, VkPipelineLayout& rtPipelineLayout,
    VkBuffer& sbtBuffer, VkDeviceMemory& sbtMemory, VkAccelerationStructureKHR& tlas,
    VkBuffer& tlasBuffer, VkDeviceMemory& tlasMemory, VkAccelerationStructureKHR& blas,
    VkBuffer& blasBuffer, VkDeviceMemory& blasMemory
) {
    auto destroyBuffer = [&](VkBuffer& buffer, VkDeviceMemory& memory) {
        if (buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device_, buffer, nullptr);
            buffer = VK_NULL_HANDLE;
        }
        if (memory != VK_NULL_HANDLE) {
            vkFreeMemory(device_, memory, nullptr);
            memory = VK_NULL_HANDLE;
        }
    };

    destroyBuffer(sbtBuffer, sbtMemory);
    destroyBuffer(tlasBuffer, tlasMemory);
    destroyBuffer(blasBuffer, blasMemory);

    if (tlas != VK_NULL_HANDLE) {
        vkDestroyAccelerationStructureKHR(device_, tlas, nullptr);
        tlas = VK_NULL_HANDLE;
    }
    if (blas != VK_NULL_HANDLE) {
        vkDestroyAccelerationStructureKHR(device_, blas, nullptr);
        blas = VK_NULL_HANDLE;
    }
    if (rtPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, rtPipeline, nullptr);
        rtPipeline = VK_NULL_HANDLE;
    }
    if (rtPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, rtPipelineLayout, nullptr);
        rtPipelineLayout = VK_NULL_HANDLE;
    }
}

void VulkanRTX::createRayTracingPipeline(VkPipeline& pipeline, VkPipelineLayout& rtPipelineLayout) {
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR = (PFN_vkCreateRayTracingPipelinesKHR)vkGetDeviceProcAddr(device_, "vkCreateRayTracingPipelinesKHR");
    if (!vkCreateRayTracingPipelinesKHR) {
        throw std::runtime_error("Failed to load vkCreateRayTracingPipelinesKHR");
    }

    // Parallelize shader module creation
    std::vector<std::future<VkShaderModule>> shaderFutures;
    std::vector<std::string> shaderFiles = {
        "assets/shaders/raygen.spv",
        "assets/shaders/miss.spv",
        "assets/shaders/closest_hit.spv",
        "assets/shaders/any_hit.spv",
        "assets/shaders/intersection.spv",
        "assets/shaders/callable.spv"
    };
    std::vector<VkShaderModule> shaderModules(6, VK_NULL_HANDLE);
    std::vector<bool> shaderExists(6, false);

    for (size_t i = 0; i < shaderFiles.size(); ++i) {
        shaderFutures.push_back(std::async(std::launch::async, [this, &shaderFiles, i]() {
            try {
                return createShaderModule(shaderFiles[i]);
            } catch (...) {
                return VkShaderModule(VK_NULL_HANDLE);
            }
        }));
    }

    for (size_t i = 0; i < shaderFutures.size(); ++i) {
        shaderModules[i] = shaderFutures[i].get();
        shaderExists[i] = shaderModules[i] != VK_NULL_HANDLE;
    }

    VkShaderModule raygenShader = shaderModules[0];
    VkShaderModule missShader = shaderModules[1];
    VkShaderModule closestHitShader = shaderModules[2];
    VkShaderModule anyHitShader = shaderModules[3];
    VkShaderModule intersectionShader = shaderModules[4];
    VkShaderModule callableShader = shaderModules[5];
    bool hasCallable = shaderExists[5];

    if (!raygenShader || !missShader || !closestHitShader) {
        for (auto module : shaderModules) {
            if (module != VK_NULL_HANDLE) vkDestroyShaderModule(device_, module, nullptr);
        }
        throw std::runtime_error("Required shader modules failed to load");
    }

    std::vector<VkPipelineShaderStageCreateInfo> stages = {
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_RAYGEN_BIT_KHR, raygenShader, "main", nullptr },
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_MISS_BIT_KHR, missShader, "main", nullptr },
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, closestHitShader, "main", nullptr }
    };
    uint32_t anyHitIndex = 3, intersectionIndex = 4, callableIndex = 5;
    if (anyHitShader != VK_NULL_HANDLE) {
        stages.push_back({ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_ANY_HIT_BIT_KHR, anyHitShader, "main", nullptr });
        intersectionIndex++;
        callableIndex++;
    }
    if (intersectionShader != VK_NULL_HANDLE) {
        stages.push_back({ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_INTERSECTION_BIT_KHR, intersectionShader, "main", nullptr });
        callableIndex++;
    }
    if (hasCallable) {
        stages.push_back({ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_CALLABLE_BIT_KHR, callableShader, "main", nullptr });
    }

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups = {
        { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 0, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, nullptr },
        { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 1, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, nullptr },
        { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR, VK_SHADER_UNUSED_KHR, 2, anyHitShader != VK_NULL_HANDLE ? anyHitIndex : VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, nullptr }
    };
    if (intersectionShader != VK_NULL_HANDLE) {
        groups.push_back({ VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR, intersectionIndex, 2, anyHitShader != VK_NULL_HANDLE ? anyHitIndex : VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, nullptr });
    }
    if (hasCallable) {
        groups.push_back({ VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, callableIndex, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, nullptr });
    }

    VkPushConstantRange pushConstant = { VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR, 0, sizeof(PushConstants) };
    VkPipelineLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 0, nullptr, 1, &pushConstant };
    if (vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &rtPipelineLayout) != VK_SUCCESS) {
        for (auto module : shaderModules) {
            if (module != VK_NULL_HANDLE) vkDestroyShaderModule(device_, module, nullptr);
        }
        throw std::runtime_error("Failed to create ray tracing pipeline layout");
    }

    VkRayTracingPipelineCreateInfoKHR createInfo = {
        VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        nullptr, 0, static_cast<uint32_t>(stages.size()), stages.data(),
        static_cast<uint32_t>(groups.size()), groups.data(),
        2, nullptr, nullptr, nullptr, rtPipelineLayout, VK_NULL_HANDLE, 0
    };
    if (vkCreateRayTracingPipelinesKHR(device_, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline) != VK_SUCCESS) {
        vkDestroyPipelineLayout(device_, rtPipelineLayout, nullptr);
        for (auto module : shaderModules) {
            if (module != VK_NULL_HANDLE) vkDestroyShaderModule(device_, module, nullptr);
        }
        throw std::runtime_error("Failed to create ray tracing pipeline");
    }

    for (auto module : shaderModules) {
        if (module != VK_NULL_HANDLE) vkDestroyShaderModule(device_, module, nullptr);
    }
}

void VulkanRTX::createShaderBindingTable(VkPhysicalDevice physicalDevice, VkPipeline pipeline, ShaderBindingTable& sbt) {
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = (PFN_vkGetRayTracingShaderGroupHandlesKHR)vkGetDeviceProcAddr(device_, "vkGetRayTracingShaderGroupHandlesKHR");
    if (!vkGetRayTracingShaderGroupHandlesKHR) {
        throw std::runtime_error("Failed to load vkGetRayTracingShaderGroupHandlesKHR");
    }

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR, nullptr, 0, 0, 0, 0, 0, 0, 0, 0
    };
    VkPhysicalDeviceProperties2 properties = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &rtProperties, {}
    };
    vkGetPhysicalDeviceProperties2(physicalDevice, &properties);

    uint32_t groupCount = 3;
    bool hasIntersection = false, hasCallable = false;
    {
        std::lock_guard<std::mutex> lock(shaderModuleMutex_);
        try {
            VkShaderModule mod = createShaderModule("assets/shaders/intersection.spv");
            vkDestroyShaderModule(device_, mod, nullptr);
            hasIntersection = true;
        } catch (...) {}
        try {
            VkShaderModule mod = createShaderModule("assets/shaders/callable.spv");
            vkDestroyShaderModule(device_, mod, nullptr);
            hasCallable = true;
        } catch (...) {}
    }
    if (hasIntersection) ++groupCount;
    if (hasCallable) ++groupCount;

    const uint32_t handleSize = rtProperties.shaderGroupHandleSize;
    const uint32_t handleSizeAligned = (handleSize + rtProperties.shaderGroupBaseAlignment - 1) & ~(rtProperties.shaderGroupBaseAlignment - 1);
    const uint32_t sbtSize = groupCount * handleSizeAligned;

    std::vector<uint8_t> shaderHandleStorage(sbtSize);
    if (vkGetRayTracingShaderGroupHandlesKHR(device_, pipeline, 0, groupCount, sbtSize, shaderHandleStorage.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to get shader group handles");
    }

    createBuffer(physicalDevice, sbtSize, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sbt.buffer, sbt.memory);
    void* data = nullptr;
    if (vkMapMemory(device_, sbt.memory, 0, sbtSize, 0, &data) != VK_SUCCESS) {
        vkDestroyBuffer(device_, sbt.buffer, nullptr);
        vkFreeMemory(device_, sbt.memory, nullptr);
        throw std::runtime_error("Failed to map SBT memory");
    }
    memcpy(data, shaderHandleStorage.data(), sbtSize);
    vkUnmapMemory(device_, sbt.memory);

    VkBufferDeviceAddressInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, sbt.buffer };
    VkDeviceAddress sbtAddress = vkGetBufferDeviceAddress(device_, &bufferInfo);

    sbt.raygen = { sbtAddress + 0 * handleSizeAligned, handleSize, handleSizeAligned };
    sbt.miss = { sbtAddress + 1 * handleSizeAligned, handleSize, handleSizeAligned };
    sbt.hit = { sbtAddress + 2 * handleSizeAligned, handleSize, handleSizeAligned };
    sbt.callable = { hasCallable ? sbtAddress + (groupCount - 1) * handleSizeAligned : 0, hasCallable ? handleSize : 0, hasCallable ? handleSizeAligned : 0 };
}

void VulkanRTX::createBottomLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                                    VkBuffer vertexBuffer, VkBuffer indexBuffer, uint32_t vertexCount, uint32_t indexCount,
                                    VkAccelerationStructureKHR& as, VkBuffer& asBuffer, VkDeviceMemory& asMemory) {
    if (vertexCount == 0 || indexCount == 0) throw std::runtime_error("Invalid vertex or index count for BLAS");
    VkBufferDeviceAddressInfo vertexBufferInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, vertexBuffer };
    VkDeviceAddress vertexAddress = vkGetBufferDeviceAddress(device_, &vertexBufferInfo);
    VkBufferDeviceAddressInfo indexBufferInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, indexBuffer };
    VkDeviceAddress indexAddress = vkGetBufferDeviceAddress(device_, &indexBufferInfo);

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

    createBuffer(physicalDevice, sizeInfo.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, asBuffer, asMemory);
    VkAccelerationStructureCreateInfoKHR createInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR, nullptr, 0, asBuffer, 0,
        sizeInfo.accelerationStructureSize, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, 0
    };
    if (vkCreateAccelerationStructureKHR(device_, &createInfo, nullptr, &as) != VK_SUCCESS) {
        vkDestroyBuffer(device_, asBuffer, nullptr);
        vkFreeMemory(device_, asMemory, nullptr);
        throw std::runtime_error("Failed to create bottom-level acceleration structure");
    }

    VkBuffer scratchBuffer;
    VkDeviceMemory scratchMemory;
    createBuffer(physicalDevice, sizeInfo.buildScratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer, scratchMemory);
    VkBufferDeviceAddressInfo scratchAddressInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, scratchBuffer };
    buildInfo.dstAccelerationStructure = as;
    buildInfo.scratchData.deviceAddress = vkGetBufferDeviceAddress(device_, &scratchAddressInfo);

    VkCommandBufferAllocateInfo cmdAllocInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1
    };
    VkCommandBuffer cmdBuffer;
    if (vkAllocateCommandBuffers(device_, &cmdAllocInfo, &cmdBuffer) != VK_SUCCESS) {
        vkDestroyBuffer(device_, asBuffer, nullptr);
        vkFreeMemory(device_, asMemory, nullptr);
        vkDestroyBuffer(device_, scratchBuffer, nullptr);
        vkFreeMemory(device_, scratchMemory, nullptr);
        throw std::runtime_error("Failed to allocate command buffer for BLAS build");
    }
    VkCommandBufferBeginInfo cmdBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr
    };
    if (vkBeginCommandBuffer(cmdBuffer, &cmdBeginInfo) != VK_SUCCESS) {
        vkFreeCommandBuffers(device_, commandPool, 1, &cmdBuffer);
        vkDestroyBuffer(device_, asBuffer, nullptr);
        vkFreeMemory(device_, asMemory, nullptr);
        vkDestroyBuffer(device_, scratchBuffer, nullptr);
        vkFreeMemory(device_, scratchMemory, nullptr);
        throw std::runtime_error("Failed to begin command buffer for BLAS build");
    }
    VkAccelerationStructureBuildRangeInfoKHR buildRange = { primitiveCount, 0, 0, 0 };
    const VkAccelerationStructureBuildRangeInfoKHR* buildRangePtr = &buildRange;
    vkCmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &buildInfo, &buildRangePtr);
    if (vkEndCommandBuffer(cmdBuffer) != VK_SUCCESS) {
        vkFreeCommandBuffers(device_, commandPool, 1, &cmdBuffer);
        vkDestroyBuffer(device_, asBuffer, nullptr);
        vkFreeMemory(device_, asMemory, nullptr);
        vkDestroyBuffer(device_, scratchBuffer, nullptr);
        vkFreeMemory(device_, scratchMemory, nullptr);
        throw std::runtime_error("Failed to end command buffer for BLAS build");
    }
    VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &cmdBuffer, 0, nullptr };
    if (vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        vkFreeCommandBuffers(device_, commandPool, 1, &cmdBuffer);
        vkDestroyBuffer(device_, asBuffer, nullptr);
        vkFreeMemory(device_, asMemory, nullptr);
        vkDestroyBuffer(device_, scratchBuffer, nullptr);
        vkFreeMemory(device_, scratchMemory, nullptr);
        throw std::runtime_error("Failed to submit command buffer for BLAS build");
    }
    vkQueueWaitIdle(queue);
    vkFreeCommandBuffers(device_, commandPool, 1, &cmdBuffer);
    vkDestroyBuffer(device_, scratchBuffer, nullptr);
    vkFreeMemory(device_, scratchMemory, nullptr);
}

void VulkanRTX::createTopLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                                 VkAccelerationStructureKHR blas, VkAccelerationStructureKHR& as, VkBuffer& asBuffer, VkDeviceMemory& asMemory) {
    VkTransformMatrixKHR transform = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f
    };
    VkAccelerationStructureInstanceKHR instance = {
        transform, 0, 0xFF, 0, VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR, {0}
    };
    VkAccelerationStructureDeviceAddressInfoKHR blasAddressInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR, nullptr, blas
    };
    instance.accelerationStructureReference = vkGetAccelerationStructureDeviceAddressKHR(device_, &blasAddressInfo);

    VkDeviceSize instanceBufferSize = sizeof(VkAccelerationStructureInstanceKHR);
    VkBuffer instanceBuffer;
    VkDeviceMemory instanceMemory;
    createBuffer(physicalDevice, instanceBufferSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, instanceBuffer, instanceMemory);
    void* mappedData = nullptr;
    if (vkMapMemory(device_, instanceMemory, 0, instanceBufferSize, 0, &mappedData) != VK_SUCCESS) {
        vkDestroyBuffer(device_, instanceBuffer, nullptr);
        vkFreeMemory(device_, instanceMemory, nullptr);
        throw std::runtime_error("Failed to map TLAS instance memory");
    }
    memcpy(mappedData, &instance, instanceBufferSize);
    vkUnmapMemory(device_, instanceMemory);
    VkBufferDeviceAddressInfo instanceBufferAddressInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, instanceBuffer };
    VkDeviceAddress instanceBufferAddress = vkGetBufferDeviceAddress(device_, &instanceBufferAddressInfo);

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
        vkDestroyBuffer(device_, instanceBuffer, nullptr);
        vkFreeMemory(device_, instanceMemory, nullptr);
        vkDestroyBuffer(device_, asBuffer, nullptr);
        vkFreeMemory(device_, asMemory, nullptr);
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
        vkDestroyBuffer(device_, instanceBuffer, nullptr);
        vkFreeMemory(device_, instanceMemory, nullptr);
        vkDestroyBuffer(device_, asBuffer, nullptr);
        vkFreeMemory(device_, asMemory, nullptr);
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
        vkDestroyBuffer(device_, asBuffer, nullptr);
        vkFreeMemory(device_, asMemory, nullptr);
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
        vkDestroyBuffer(device_, asBuffer, nullptr);
        vkFreeMemory(device_, asMemory, nullptr);
        vkDestroyBuffer(device_, scratchBuffer, nullptr);
        vkFreeMemory(device_, scratchMemory, nullptr);
        throw std::runtime_error("Failed to end command buffer for TLAS build");
    }
    VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &cmdBuffer, 0, nullptr };
    if (vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        vkFreeCommandBuffers(device_, commandPool, 1, &cmdBuffer);
        vkDestroyBuffer(device_, instanceBuffer, nullptr);
        vkFreeMemory(device_, instanceMemory, nullptr);
        vkDestroyBuffer(device_, asBuffer, nullptr);
        vkFreeMemory(device_, asMemory, nullptr);
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