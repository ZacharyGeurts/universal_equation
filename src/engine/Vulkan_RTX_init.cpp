#include "Vulkan_RTX_init.hpp"
#include <spdlog/spdlog.h>
// AMOURANTH RTX Engine - September 2025
// Author: Zachary Geurts, 2025
void VulkanRTX::initializeRTX(
    VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
    const std::vector<std::tuple<VkBuffer_T*, VkBuffer_T*, uint32_t, uint32_t, uint64_t>>& geometries,
    uint32_t maxRayRecursionDepth, const std::vector<DimensionData>& dimensionCache
) {
    if (!physicalDevice || !commandPool || !graphicsQueue) {
        throw VulkanRTXException("Invalid physical device, command pool, or queue");
    }
    if (geometries.empty() || std::any_of(geometries.begin(), geometries.end(),
        [](const auto& g) { return std::get<2>(g) == 0 || std::get<3>(g) == 0 || std::get<3>(g) % 3 != 0; })) {
        throw VulkanRTXException("Invalid geometry data");
    }
    dimensionCache_ = dimensionCache;
    try {
        createDescriptorSetLayout();
        createRayTracingPipeline(maxRayRecursionDepth);
        createBottomLevelAS(physicalDevice, commandPool, graphicsQueue, geometries);
        createTopLevelAS(physicalDevice, commandPool, graphicsQueue, {});
        createShaderBindingTable(physicalDevice);
        createDescriptorPoolAndSet();
        updateDescriptorSetForTLAS(tlas_.get());
        if (supportsCompaction_) {
            compactAccelerationStructures(physicalDevice, commandPool, graphicsQueue);
        }
    } catch (const VulkanRTXException& e) {
        spdlog::error("RTX initialization failed: {}", e.what());
        throw;
    }
}

void VulkanRTX::updateRTX(
    VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
    const std::vector<std::tuple<VkBuffer_T*, VkBuffer_T*, uint32_t, uint32_t, uint64_t>>& geometries,
    const std::vector<DimensionData>& dimensionCache
) {
    dimensionCache_ = dimensionCache;
    createBottomLevelAS(physicalDevice, commandPool, graphicsQueue, geometries);
    createTopLevelAS(physicalDevice, commandPool, graphicsQueue, {});
    updateDescriptorSetForTLAS(tlas_.get());
    if (supportsCompaction_) {
        compactAccelerationStructures(physicalDevice, commandPool, graphicsQueue);
    }
}

void VulkanRTX::compactAccelerationStructures(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue) {
    if (!supportsCompaction_) {
        spdlog::warn("AS compaction not supported");
        return;
    }
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR, nullptr, 0, 0, 0 };
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = { 
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR, nullptr,
        VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, 0, VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        blas_.get(), VK_NULL_HANDLE, 0, nullptr, nullptr, {0}
    };
    uint32_t primCount = 0;
    for (uint32_t count : primitiveCounts_) primCount += count; // Sum stored primitive counts
    if (primCount == 0) {
        spdlog::warn("No primitives available for compaction");
        return;
    }
    vkGetAccelerationStructureBuildSizesKHR(device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &primCount, &sizeInfo);

    VulkanResource<VkBuffer_T*, vkDestroyBuffer> compactedBuffer(device_, VK_NULL_HANDLE);
    VulkanResource<VkDeviceMemory, vkFreeMemory> compactedMemory(device_, VK_NULL_HANDLE);
    createBuffer(physicalDevice, sizeInfo.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, compactedBuffer, compactedMemory);

    VkAccelerationStructureCreateInfoKHR createInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR, nullptr, 0,
        compactedBuffer.get(), 0, sizeInfo.accelerationStructureSize,
        VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, 0
    };
    VulkanASResource compactedAS(device_, VK_NULL_HANDLE);
    VK_CHECK(vkCreateAccelerationStructureKHR(device_, &createInfo, nullptr, &compactedAS.get()), "Failed to create compacted BLAS");

    VkCommandBufferAllocateInfo cmdAllocInfo = { 
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, commandPool, 
        VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 
    };
    VkCommandBuffer cmdBuffer;
    VK_CHECK(vkAllocateCommandBuffers(device_, &cmdAllocInfo, &cmdBuffer), "Failed to allocate compaction command buffer");
    VkCommandBufferBeginInfo cmdBeginInfo = { 
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, 
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr 
    };
    VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &cmdBeginInfo), "Failed to begin compaction command buffer");
    VkCopyAccelerationStructureInfoKHR copyInfo = {
        VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR, nullptr,
        blas_.get(), compactedAS.get(), VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR
    };
    vkCmdCopyAccelerationStructureKHR(cmdBuffer, &copyInfo);
    VK_CHECK(vkEndCommandBuffer(cmdBuffer), "Failed to end compaction command buffer");
    VkSubmitInfo submitInfo = { 
        VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &cmdBuffer, 0, nullptr 
    };
    VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE), "Failed to submit compaction command");
    vkQueueWaitIdle(queue);
    vkFreeCommandBuffers(device_, commandPool, 1, &cmdBuffer);
    blas_ = std::move(compactedAS);
    blasBuffer_ = std::move(compactedBuffer);
    blasMemory_ = std::move(compactedMemory);
}

void VulkanRTX::createBottomLevelAS(
    VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
    const std::vector<std::tuple<VkBuffer_T*, VkBuffer_T*, uint32_t, uint32_t, uint64_t>>& geometries
) {
    std::vector<VkAccelerationStructureGeometryKHR> geoInfos;
    primitiveCounts_.clear(); // Clear previous counts
    for (const auto& [vBuf, iBuf, vCount, iCount, stride] : geometries) {
        VkBufferDeviceAddressInfo vInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, vBuf };
        VkDeviceAddress vAddr = vkGetBufferDeviceAddress(device_, &vInfo);
        if (vAddr == 0) throw VulkanRTXException("Invalid vertex buffer address");
        VkBufferDeviceAddressInfo iInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, iBuf };
        VkDeviceAddress iAddr = vkGetBufferDeviceAddress(device_, &iInfo);
        if (iAddr == 0) throw VulkanRTXException("Invalid index buffer address");

        VkAccelerationStructureGeometryTrianglesDataKHR triangles = {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR, nullptr,
            VK_FORMAT_R32G32B32_SFLOAT, vAddr, stride, vCount - 1, VK_INDEX_TYPE_UINT32, iAddr, {0}
        };
        geoInfos.push_back({ 
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR, nullptr, 
            VK_GEOMETRY_TYPE_TRIANGLES_KHR, { .triangles = triangles }, VK_GEOMETRY_OPAQUE_BIT_KHR 
        });
        primitiveCounts_.push_back(iCount / 3); // Store primitive count
    }

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR, nullptr,
        VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, 
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
        VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR, blas_.get(), VK_NULL_HANDLE, 
        static_cast<uint32_t>(geoInfos.size()), geoInfos.data(), nullptr, {0}
    };
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo = { 
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR, nullptr, 0, 0, 0 
    };
    vkGetAccelerationStructureBuildSizesKHR(device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, 
                                            &buildInfo, primitiveCounts_.data(), &sizeInfo);

    createBuffer(physicalDevice, sizeInfo.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, blasBuffer_, blasMemory_);
    VkAccelerationStructureCreateInfoKHR createInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR, nullptr, 0,
        blasBuffer_.get(), 0, sizeInfo.accelerationStructureSize,
        VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, 0
    };
    VK_CHECK(vkCreateAccelerationStructureKHR(device_, &createInfo, nullptr, &blas_.get()), 
             "Failed to create BLAS");

    VulkanResource<VkBuffer_T*, vkDestroyBuffer> scratchBuffer(device_, VK_NULL_HANDLE);
    VulkanResource<VkDeviceMemory, vkFreeMemory> scratchMemory(device_, VK_NULL_HANDLE);
    createBuffer(physicalDevice, sizeInfo.buildScratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer, scratchMemory);
    VkBufferDeviceAddressInfo scratchInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, scratchBuffer.get() };
    buildInfo.scratchData.deviceAddress = vkGetBufferDeviceAddress(device_, &scratchInfo);
    buildInfo.dstAccelerationStructure = blas_.get();

    VkCommandBufferAllocateInfo cmdAllocInfo = { 
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, commandPool, 
        VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 
    };
    VkCommandBuffer cmdBuffer;
    VK_CHECK(vkAllocateCommandBuffers(device_, &cmdAllocInfo, &cmdBuffer), 
             "Failed to allocate BLAS command buffer");
    VkCommandBufferBeginInfo cmdBeginInfo = { 
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, 
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr 
    };
    VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &cmdBeginInfo), "Failed to begin BLAS command buffer");

    std::vector<VkAccelerationStructureBuildRangeInfoKHR> buildRanges;
    for (uint32_t count : primitiveCounts_) {
        buildRanges.push_back({ count, 0, 0, 0 });
    }
    const VkAccelerationStructureBuildRangeInfoKHR* buildRangePtr = buildRanges.data();
    vkCmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &buildInfo, &buildRangePtr);
    VK_CHECK(vkEndCommandBuffer(cmdBuffer), "Failed to end BLAS command buffer");
    VkSubmitInfo submitInfo = { 
        VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &cmdBuffer, 0, nullptr 
    };
    VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE), "Failed to submit BLAS command");
    vkQueueWaitIdle(queue);
    vkFreeCommandBuffers(device_, commandPool, 1, &cmdBuffer);
}

void VulkanRTX::createTopLevelAS(
    VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
    const std::vector<std::tuple<VkAccelerationStructureKHR_T*, glm::mat<4, 4, float, glm::packed_highp>>>& instances
) {
    std::vector<VkAccelerationStructureInstanceKHR> instanceData;
    for (const auto& [as, transform] : instances.empty() ? 
         std::vector<std::tuple<VkAccelerationStructureKHR_T*, glm::mat<4, 4, float, glm::packed_highp>>>{{blas_.get(), glm::mat<4, 4, float, glm::packed_highp>(1.0f)}} : instances) {
        VkAccelerationStructureDeviceAddressInfoKHR addrInfo = { 
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR, nullptr, as 
        };
        VkDeviceAddress accelRef = vkGetAccelerationStructureDeviceAddressKHR(device_, &addrInfo);
        if (accelRef == 0) throw VulkanRTXException("Invalid BLAS address for TLAS");

        VkTransformMatrixKHR vkTransform;
        for (int i = 0; i < 3; ++i) for (int j = 0; j < 4; ++j) vkTransform.matrix[i][j] = transform[j][i];
        instanceData.push_back({ 
            vkTransform, 0, 0xFF, 0, VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR, accelRef 
        });
    }

    VkDeviceSize instanceBufferSize = instanceData.size() * sizeof(VkAccelerationStructureInstanceKHR);
    VulkanResource<VkBuffer_T*, vkDestroyBuffer> instanceBuffer(device_, VK_NULL_HANDLE);
    VulkanResource<VkDeviceMemory, vkFreeMemory> instanceMemory(device_, VK_NULL_HANDLE);
    createBuffer(physicalDevice, instanceBufferSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                 instanceBuffer, instanceMemory);
    void* mappedData;
    VK_CHECK(vkMapMemory(device_, instanceMemory.get(), 0, instanceBufferSize, 0, &mappedData), 
             "Failed to map TLAS instance memory");
    std::memcpy(mappedData, instanceData.data(), instanceBufferSize);
    vkUnmapMemory(device_, instanceMemory.get());

    VkBufferDeviceAddressInfo instanceInfo = { 
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, instanceBuffer.get() 
    };
    VkDeviceAddress instanceAddr = vkGetBufferDeviceAddress(device_, &instanceInfo);
    if (instanceAddr == 0) throw VulkanRTXException("Invalid TLAS instance buffer address");

    VkAccelerationStructureGeometryKHR geometry = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR, nullptr, VK_GEOMETRY_TYPE_INSTANCES_KHR,
        { .instances = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR, nullptr, VK_FALSE, instanceAddr } },
        VK_GEOMETRY_OPAQUE_BIT_KHR
    };
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR, nullptr, 
        VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
        VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR, tlas_.get(), VK_NULL_HANDLE, 1, &geometry, nullptr, {0}
    };
    uint32_t instanceCount = static_cast<uint32_t>(instanceData.size());
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo = { 
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR, nullptr, 0, 0, 0 
    };
    vkGetAccelerationStructureBuildSizesKHR(device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, 
                                            &buildInfo, &instanceCount, &sizeInfo);

    createBuffer(physicalDevice, sizeInfo.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, tlasBuffer_, tlasMemory_);
    VkAccelerationStructureCreateInfoKHR createInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR, nullptr, 0,
        tlasBuffer_.get(), 0, sizeInfo.accelerationStructureSize,
        VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, 0
    };
    VK_CHECK(vkCreateAccelerationStructureKHR(device_, &createInfo, nullptr, &tlas_.get()), 
             "Failed to create TLAS");

    VulkanResource<VkBuffer_T*, vkDestroyBuffer> scratchBuffer(device_, VK_NULL_HANDLE);
    VulkanResource<VkDeviceMemory, vkFreeMemory> scratchMemory(device_, VK_NULL_HANDLE);
    createBuffer(physicalDevice, sizeInfo.buildScratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer, scratchMemory);
    VkBufferDeviceAddressInfo scratchInfo = { 
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, scratchBuffer.get() 
    };
    buildInfo.scratchData.deviceAddress = vkGetBufferDeviceAddress(device_, &scratchInfo);
    buildInfo.dstAccelerationStructure = tlas_.get();

    VkCommandBufferAllocateInfo cmdAllocInfo = { 
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, commandPool, 
        VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 
    };
    VkCommandBuffer cmdBuffer;
    VK_CHECK(vkAllocateCommandBuffers(device_, &cmdAllocInfo, &cmdBuffer), 
             "Failed to allocate TLAS command buffer");
    VkCommandBufferBeginInfo cmdBeginInfo = { 
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, 
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr 
    };
    VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &cmdBeginInfo), "Failed to begin TLAS command buffer");
    VkAccelerationStructureBuildRangeInfoKHR buildRange = { instanceCount, 0, 0, 0 };
    const VkAccelerationStructureBuildRangeInfoKHR* buildRangePtr = &buildRange;
    vkCmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &buildInfo, &buildRangePtr);
    VK_CHECK(vkEndCommandBuffer(cmdBuffer), "Failed to end TLAS command buffer");
    VkSubmitInfo submitInfo = { 
        VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &cmdBuffer, 0, nullptr 
    };
    VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE), "Failed to submit TLAS command");
    vkQueueWaitIdle(queue);
    vkFreeCommandBuffers(device_, commandPool, 1, &cmdBuffer);
}