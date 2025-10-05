// AMOURANTH RTX © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
#include "engine/Vulkan/Vulkan_RTX.hpp"
#include <cstring>
#include <ranges>

void VulkanRTX::createBuffer(
    VkPhysicalDevice physicalDevice,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags props,
    VulkanResource<VkBuffer, PFN_vkDestroyBuffer>& buffer,
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>& memory) {
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer tempBuffer = VK_NULL_HANDLE;
    VK_CHECK(vkCreateBuffer(device_, &bufferInfo, nullptr, &tempBuffer), "Buffer creation failed.");
    buffer = VulkanResource<VkBuffer, PFN_vkDestroyBuffer>(device_, tempBuffer, vkDestroyBuffer);

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device_, tempBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, props);

    VkDeviceMemory tempMemory = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(device_, &allocInfo, nullptr, &tempMemory), "Memory allocation failed.");
    memory = VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>(device_, tempMemory, vkFreeMemory);

    VK_CHECK(vkBindBufferMemory(device_, tempBuffer, tempMemory, 0), "Buffer memory binding failed.");
}

uint32_t VulkanRTX::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }

    throw VulkanRTXException("Failed to find suitable memory type.");
}

void VulkanRTX::compactAccelerationStructures(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue) {
    if (!supportsCompaction_) {
        throw VulkanRTXException("Acceleration structure compaction not supported.");
    }

    VkAccelerationStructureKHR blasHandle = blas_.get();
    if (!blasHandle) {
        throw VulkanRTXException("No BLAS available for compaction.");
    }

    // Create a query pool to get the compacted size
    VkQueryPoolCreateInfo queryPoolInfo = {};
    queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    queryPoolInfo.queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
    queryPoolInfo.queryCount = 1;

    VkQueryPool queryPool;
    VK_CHECK(vkCreateQueryPool(device_, &queryPoolInfo, nullptr, &queryPool), "Query pool creation failed.");

    // Record command to query compacted size
    VkCommandBuffer cmdBuffer = allocateTransientCommandBuffer(commandPool);
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo), "Failed to begin command buffer");

    vkCmdResetQueryPool(cmdBuffer, queryPool, 0, 1);
    vkCmdWriteASPropertiesFunc(
        cmdBuffer, 1, &blasHandle, VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryPool, 0);

    VK_CHECK(vkEndCommandBuffer(cmdBuffer), "Failed to end command buffer");
    submitAndWaitTransient(cmdBuffer, queue, commandPool);

    // Retrieve the compacted size
    VkDeviceSize compactedSize;
    VK_CHECK(vkGetQueryPoolResults(device_, queryPool, 0, 1, sizeof(VkDeviceSize), &compactedSize, sizeof(VkDeviceSize), VK_QUERY_RESULT_WAIT_BIT),
             "Failed to get query pool results");

    // Create a new buffer for the compacted BLAS
    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> compactedBlasBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> compactedBlasMemory(device_, VK_NULL_HANDLE, vkFreeMemory);
    createBuffer(physicalDevice, compactedSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, compactedBlasBuffer, compactedBlasMemory);

    // Create the compacted BLAS
    VkAccelerationStructureCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = compactedBlasBuffer.get();
    createInfo.size = compactedSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

    VkAccelerationStructureKHR compactedAsHandle = VK_NULL_HANDLE;
    VK_CHECK(vkCreateASFunc(device_, &createInfo, nullptr, &compactedAsHandle), "Failed to create compacted acceleration structure");
    VulkanResource<VkAccelerationStructureKHR, PFN_vkDestroyAccelerationStructureKHR> compactedBlas(device_, compactedAsHandle, vkDestroyASFunc);

    // Copy the original BLAS to the compacted BLAS
    cmdBuffer = allocateTransientCommandBuffer(commandPool);
    beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo), "Failed to begin command buffer");

    VkCopyAccelerationStructureInfoKHR copyInfo = {};
    copyInfo.sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR;
    copyInfo.src = blas_.get();
    copyInfo.dst = compactedBlas.get();
    copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;

    vkCmdCopyAccelerationStructureKHR(cmdBuffer, &copyInfo);

    VK_CHECK(vkEndCommandBuffer(cmdBuffer), "Failed to end command buffer");
    submitAndWaitTransient(cmdBuffer, queue, commandPool);

    // Update the BLAS to the compacted version
    blas_ = std::move(compactedBlas);
    blasBuffer_ = std::move(compactedBlasBuffer);
    blasMemory_ = std::move(compactedBlasMemory);

    vkDestroyQueryPool(device_, queryPool, nullptr);
}

void VulkanRTX::createBottomLevelAS(
    VkPhysicalDevice physicalDevice,
    VkCommandPool commandPool,
    VkQueue queue,
    const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries) {
    if (geometries.empty()) {
        throw VulkanRTXException("Invalid BLAS params.");
    }

    std::vector<VkAccelerationStructureGeometryKHR> geometriesKHR(geometries.size());
    primitiveCounts_.resize(geometries.size());

    for (size_t i : std::views::iota(0u, geometries.size())) {
        auto [vertexBuffer, indexBuffer, vertexCount, indexCount, vertexStride] = geometries[i];
        if (!vertexBuffer || !indexBuffer || vertexCount == 0 || indexCount == 0 || vertexStride == 0) {
            throw VulkanRTXException("Invalid geometry: null buffer or zero count/stride.");
        }

        geometriesKHR[i] = {};
        geometriesKHR[i].sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        geometriesKHR[i].geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        geometriesKHR[i].geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        geometriesKHR[i].geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        geometriesKHR[i].geometry.triangles.vertexData.deviceAddress = getBufferDeviceAddress(vertexBuffer);
        geometriesKHR[i].geometry.triangles.vertexStride = vertexStride;
        geometriesKHR[i].geometry.triangles.maxVertex = vertexCount - 1; // Corrected to maxVertex = vertexCount - 1
        geometriesKHR[i].geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
        geometriesKHR[i].geometry.triangles.indexData.deviceAddress = getBufferDeviceAddress(indexBuffer);
        geometriesKHR[i].flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

        primitiveCounts_[i].primitiveCount = indexCount / 3;
        primitiveCounts_[i].primitiveOffset = 0;
        primitiveCounts_[i].firstVertex = 0;
        primitiveCounts_[i].transformOffset = 0;
    }

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
    buildInfo.geometryCount = static_cast<uint32_t>(geometries.size());
    buildInfo.pGeometries = geometriesKHR.data();

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    std::vector<uint32_t> maxPrimitives(geometries.size());
    for (size_t i = 0; i < geometries.size(); ++i) {
        maxPrimitives[i] = primitiveCounts_[i].primitiveCount;
    }
    vkGetASBuildSizesFunc(device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, maxPrimitives.data(), &sizeInfo);

    createBuffer(physicalDevice, sizeInfo.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, blasBuffer_, blasMemory_);

    VkAccelerationStructureCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = blasBuffer_.get();
    createInfo.size = sizeInfo.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    VkAccelerationStructureKHR asHandle = VK_NULL_HANDLE;
    VK_CHECK(vkCreateASFunc(device_, &createInfo, nullptr, &asHandle), "Failed to create acceleration structure");
    blas_ = VulkanResource<VkAccelerationStructureKHR, PFN_vkDestroyAccelerationStructureKHR>(device_, asHandle, vkDestroyASFunc);

    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> scratchBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> scratchMemory(device_, VK_NULL_HANDLE, vkFreeMemory);
    createBuffer(physicalDevice, sizeInfo.buildScratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer, scratchMemory);

    VkDeviceAddress scratchAddress = getBufferDeviceAddress(scratchBuffer.get());
    buildInfo.dstAccelerationStructure = blas_.get();
    buildInfo.scratchData.deviceAddress = scratchAddress;

    VkCommandBuffer cmdBuffer = allocateTransientCommandBuffer(commandPool);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo), "Failed to begin command buffer");

    std::vector<VkAccelerationStructureBuildRangeInfoKHR*> buildRangePtrs(geometries.size());
    for (size_t i = 0; i < geometries.size(); ++i) {
        buildRangePtrs[i] = &primitiveCounts_[i];
    }
    vkCmdBuildASFunc(cmdBuffer, 1, &buildInfo, buildRangePtrs.data());

    VK_CHECK(vkEndCommandBuffer(cmdBuffer), "Failed to end command buffer");

    submitAndWaitTransient(cmdBuffer, queue, commandPool);
}

void VulkanRTX::createTopLevelAS(
    VkPhysicalDevice physicalDevice,
    VkCommandPool commandPool,
    VkQueue queue,
    const std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4>>& instances) {
    if (instances.empty()) {
        throw VulkanRTXException("Invalid TLAS params.");
    }

    std::vector<VkAccelerationStructureInstanceKHR> instanceData(instances.size());

    for (size_t i : std::views::iota(0u, instances.size())) {
        auto [as, transform] = instances[i];
        if (!as) throw VulkanRTXException("Null AS in instances.");

        VkTransformMatrixKHR transformMatrix = {};
        std::memcpy(&transformMatrix, &transform, sizeof(VkTransformMatrixKHR));

        instanceData[i] = {};
        instanceData[i].transform = transformMatrix;
        instanceData[i].instanceCustomIndex = 0;
        instanceData[i].mask = 0xFF;
        instanceData[i].instanceShaderBindingTableRecordOffset = 0;
        instanceData[i].flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instanceData[i].accelerationStructureReference = getAccelerationStructureDeviceAddress(as);
    }

    VkDeviceSize instanceSize = instanceData.size() * sizeof(VkAccelerationStructureInstanceKHR);
    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> instanceBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> instanceMemory(device_, VK_NULL_HANDLE, vkFreeMemory);

    createBuffer(physicalDevice, instanceSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 instanceBuffer, instanceMemory);

    void* data;
    VK_CHECK(vkMapMemory(device_, instanceMemory.get(), 0, instanceSize, 0, &data), "Instance buffer map failed.");
    memcpy(data, instanceData.data(), instanceSize);
    vkUnmapMemory(device_, instanceMemory.get());

    VkDeviceAddress instanceAddress = getBufferDeviceAddress(instanceBuffer.get());

    VkAccelerationStructureGeometryKHR geometry = {};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    geometry.geometry.instances.data.deviceAddress = instanceAddress;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;

    uint32_t primitiveCount = static_cast<uint32_t>(instances.size());
    VkAccelerationStructureBuildSizesInfoKHR buildSizes = {};
    buildSizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    vkGetASBuildSizesFunc(device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                          &buildInfo, &primitiveCount, &buildSizes);

    createBuffer(physicalDevice, buildSizes.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, tlasBuffer_, tlasMemory_);

    VkAccelerationStructureCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = tlasBuffer_.get();
    createInfo.size = buildSizes.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

    VkAccelerationStructureKHR tempAS = VK_NULL_HANDLE;
    VK_CHECK(vkCreateASFunc(device_, &createInfo, nullptr, &tempAS), "TLAS creation failed.");
    tlas_ = VulkanResource<VkAccelerationStructureKHR, PFN_vkDestroyAccelerationStructureKHR>(device_, tempAS, vkDestroyASFunc);

    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> scratchBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> scratchMemory(device_, VK_NULL_HANDLE, vkFreeMemory);
    createBuffer(physicalDevice, buildSizes.buildScratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer, scratchMemory);

    VkDeviceAddress scratchAddress = getBufferDeviceAddress(scratchBuffer.get());
    buildInfo.dstAccelerationStructure = tlas_.get();
    buildInfo.scratchData.deviceAddress = scratchAddress;

    VkAccelerationStructureBuildRangeInfoKHR buildRange = {};
    buildRange.primitiveCount = primitiveCount;
    VkAccelerationStructureBuildRangeInfoKHR* buildRanges = &buildRange;

    VkCommandBuffer cmdBuffer = allocateTransientCommandBuffer(commandPool);
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo), "TLAS begin failed.");

    vkCmdBuildASFunc(cmdBuffer, 1, &buildInfo, &buildRanges);

    VK_CHECK(vkEndCommandBuffer(cmdBuffer), "TLAS end failed.");

    submitAndWaitTransient(cmdBuffer, queue, commandPool);

    updateDescriptorSetForTLAS(tlas_.get());
}

VkDeviceAddress VulkanRTX::getBufferDeviceAddress(VkBuffer buffer) const {
    VkBufferDeviceAddressInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    info.buffer = buffer;
    return vkGetBufferDeviceAddressFunc(device_, &info);
}

VkDeviceAddress VulkanRTX::getAccelerationStructureDeviceAddress(VkAccelerationStructureKHR as) const {
    VkAccelerationStructureDeviceAddressInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    info.accelerationStructure = as;
    return vkGetASDeviceAddressFunc(device_, &info);
}

VkCommandBuffer VulkanRTX::allocateTransientCommandBuffer(VkCommandPool commandPool) {
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmdBuffer;
    VK_CHECK(vkAllocateCommandBuffers(device_, &allocInfo, &cmdBuffer), "Transient command buffer allocation failed.");
    return cmdBuffer;
}

void VulkanRTX::submitAndWaitTransient(VkCommandBuffer cmdBuffer, VkQueue queue, VkCommandPool commandPool) {
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence;
    VK_CHECK(vkCreateFence(device_, &fenceInfo, nullptr, &fence), "Fence creation failed.");

    VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, fence), "Queue submit failed.");
    VK_CHECK(vkWaitForFences(device_, 1, &fence, VK_TRUE, UINT64_MAX), "Fence wait failed.");

    vkDestroyFence(device_, fence, nullptr);
    vkFreeCommandBuffers(device_, commandPool, 1, &cmdBuffer);
}

void VulkanRTX::updateDescriptorSetForTLAS(VkAccelerationStructureKHR tlas) {
    if (!tlas) {
        throw VulkanRTXException("Null TLAS provided for descriptor update.");
    }
    VkWriteDescriptorSetAccelerationStructureKHR asInfo = {};
    asInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    asInfo.accelerationStructureCount = 1;
    asInfo.pAccelerationStructures = &tlas;

    VkWriteDescriptorSet accelWrite = {};
    accelWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    accelWrite.pNext = &asInfo;
    accelWrite.dstSet = ds_.get();
    accelWrite.dstBinding = static_cast<uint32_t>(DescriptorBindings::TLAS);
    accelWrite.dstArrayElement = 0;
    accelWrite.descriptorCount = 1;
    accelWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

    vkUpdateDescriptorSets(device_, 1, &accelWrite, 0, nullptr);
}