// VulkanRTX_Acceleration.cpp
#include "engine/Vulkan/Vulkan_RTX.hpp"
#include <stdexcept>
#include <format>
#include <syncstream>
#include <vector>
#include <iostream>
#include <cstring>

namespace VulkanRTX {

void VulkanRTX::createBuffer(VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
                             VkMemoryPropertyFlags properties, VulkanResource<VkBuffer, PFN_vkDestroyBuffer>& buffer,
                             VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>& memory) {
    if (!physicalDevice || size == 0) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Invalid buffer params: null device or zero size" << RESET << std::endl;
        throw VulkanRTXException(std::format("Invalid buffer params: null device or size={}.", size));
    }

    std::osyncstream(std::cout) << GREEN << "[INFO] Creating Vulkan buffer of size: " << size << RESET << std::endl;

    VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr
    };
    VkBuffer vkBuffer;
    VK_CHECK(vkCreateBuffer(device_, &bufferInfo, nullptr, &vkBuffer), std::format("Buffer creation failed for size={}.", size));
    buffer = VulkanResource<VkBuffer, PFN_vkDestroyBuffer>(vkBuffer, device_, vkDestroyBuffer);

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device_, vkBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties)
    };
    VkDeviceMemory vkMemory;
    VK_CHECK(vkAllocateMemory(device_, &allocInfo, nullptr, &vkMemory), "Memory allocation failed.");
    memory = VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>(vkMemory, device_, vkFreeMemory);

    VK_CHECK(vkBindBufferMemory(device_, vkBuffer, vkMemory, 0), "Buffer memory binding failed.");
    std::osyncstream(std::cout) << GREEN << "[INFO] Created buffer with size=" << size << RESET << std::endl;
}

void VulkanRTX::compactAccelerationStructures(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue) {
    if (!physicalDevice || !commandPool || !queue) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Invalid compact params: null device, pool, or queue" << RESET << std::endl;
        throw VulkanRTXException("Invalid compact params: null device, pool, or queue.");
    }

    std::osyncstream(std::cout) << GREEN << "[INFO] Compacting acceleration structures" << RESET << std::endl;

    VkQueryPoolCreateInfo queryPoolInfo{
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
        .queryCount = 2,
        .pipelineStatistics = 0
    };
    VkQueryPool queryPool;
    VK_CHECK(vkCreateQueryPool(device_, &queryPoolInfo, nullptr, &queryPool), "Query pool creation failed.");

    VkCommandBuffer commandBuffer = allocateTransientCommandBuffer(commandPool);
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo), "Failed to begin command buffer.");

    std::vector<VkAccelerationStructureKHR> structures = { blas_.get(), tlas_.get() };
    vkCmdResetQueryPool(commandBuffer, queryPool, 0, 2);
    for (uint32_t i = 0; i < structures.size(); ++i) {
        if (structures[i]) {
            vkCmdWriteAccelerationStructuresPropertiesKHR(
                commandBuffer, 1, &structures[i], VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryPool, i);
        }
    }

    VK_CHECK(vkEndCommandBuffer(commandBuffer), "Failed to end command buffer.");
    submitAndWaitTransient(commandBuffer, queue, commandPool);

    VkDeviceSize compactedSizes[2];
    VK_CHECK(vkGetQueryPoolResults(device_, queryPool, 0, 2, sizeof(compactedSizes), compactedSizes, sizeof(VkDeviceSize), VK_QUERY_RESULT_64_BIT),
             "Failed to get query pool results.");

    for (uint32_t i = 0; i < structures.size(); ++i) {
        if (structures[i] && compactedSizes[i] > 0) {
            VulkanResource<VkBuffer, PFN_vkDestroyBuffer> newBuffer;
            VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> newMemory;
            createBuffer(physicalDevice, compactedSizes[i],
                         VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, newBuffer, newMemory);

            VkAccelerationStructureCreateInfoKHR createInfo{
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
                .pNext = nullptr,
                .createFlags = 0,
                .buffer = newBuffer.get(),
                .offset = 0,
                .size = compactedSizes[i],
                .type = (i == 0) ? VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR : VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
                .deviceAddress = 0
            };
            VkAccelerationStructureKHR newAS;
            VK_CHECK(vkCreateAccelerationStructureKHR(device_, &createInfo, nullptr, &newAS), "Failed to create compacted acceleration structure.");

            commandBuffer = allocateTransientCommandBuffer(commandPool);
            VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo), "Failed to begin command buffer for copy.");

            VkCopyAccelerationStructureInfoKHR copyInfo{
                .sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR,
                .pNext = nullptr,
                .src = structures[i],
                .dst = newAS,
                .mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR
            };
            vkCmdCopyAccelerationStructureKHR(commandBuffer, &copyInfo);

            VK_CHECK(vkEndCommandBuffer(commandBuffer), "Failed to end command buffer for copy.");
            submitAndWaitTransient(commandBuffer, queue, commandPool);

            if (i == 0) {
                blasBuffer_ = std::move(newBuffer);
                blasMemory_ = std::move(newMemory);
                blas_ = VulkanResource<VkAccelerationStructureKHR, PFN_vkDestroyAccelerationStructureKHR>(newAS, device_, vkDestroyAccelerationStructureKHR);
            } else {
                tlasBuffer_ = std::move(newBuffer);
                tlasMemory_ = std::move(newMemory);
                tlas_ = VulkanResource<VkAccelerationStructureKHR, PFN_vkDestroyAccelerationStructureKHR>(newAS, device_, vkDestroyAccelerationStructureKHR);
            }
        }
    }

    vkDestroyQueryPool(device_, queryPool, nullptr);
    std::osyncstream(std::cout) << GREEN << "[INFO] Compacted acceleration structures" << RESET << std::endl;
}

void VulkanRTX::createBottomLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                                    const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries) {
    if (geometries.empty()) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Invalid BLAS params: empty geometries" << RESET << std::endl;
        throw VulkanRTXException("Invalid BLAS params: empty geometries.");
    }

    std::osyncstream(std::cout) << GREEN << "[INFO] Creating bottom-level acceleration structure" << RESET << std::endl;

    std::vector<VkAccelerationStructureGeometryKHR> asGeometries;
    primitiveCounts_.clear();
    primitiveCounts_.reserve(geometries.size());
    for (const auto& [vertexBuffer, indexBuffer, vertexCount, indexCount, stride] : geometries) {
        if (!vertexBuffer || !indexBuffer || vertexCount == 0 || indexCount == 0 || stride == 0) {
            std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Invalid geometry: null buffer or zero count/stride" << RESET << std::endl;
            throw VulkanRTXException("Invalid geometry: null buffer or zero count/stride.");
        }
        VkAccelerationStructureGeometryKHR geometry{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .pNext = nullptr,
            .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
            .geometry = {
                .triangles = {
                    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
                    .pNext = nullptr,
                    .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
                    .vertexData = { .deviceAddress = getBufferDeviceAddress(vertexBuffer) },
                    .vertexStride = stride,
                    .maxVertex = vertexCount - 1,
                    .indexType = VK_INDEX_TYPE_UINT32,
                    .indexData = { .deviceAddress = getBufferDeviceAddress(indexBuffer) },
                    .transformData = { .deviceAddress = 0 }
                }
            },
            .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
        };
        asGeometries.push_back(geometry);
        primitiveCounts_.push_back({
            .primitiveCount = indexCount / 3,
            .primitiveOffset = 0,
            .firstVertex = 0,
            .transformOffset = 0
        });
    }

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .pNext = nullptr,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .srcAccelerationStructure = VK_NULL_HANDLE,
        .dstAccelerationStructure = VK_NULL_HANDLE,
        .geometryCount = static_cast<uint32_t>(asGeometries.size()),
        .pGeometries = asGeometries.data(),
        .ppGeometries = nullptr,
        .scratchData = { .deviceAddress = 0 }
    };

    std::vector<uint32_t> primitiveCounts(asGeometries.size());
    for (size_t i = 0; i < asGeometries.size(); ++i) {
        primitiveCounts[i] = primitiveCounts_[i].primitiveCount;
    }
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
        .pNext = nullptr,
        .accelerationStructureSize = 0,
        .updateScratchSize = 0,
        .buildScratchSize = 0
    };
    vkGetAccelerationStructureBuildSizesKHR(device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo,
                                           primitiveCounts.data(), &sizeInfo);

    createBuffer(physicalDevice, sizeInfo.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, blasBuffer_, blasMemory_);

    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .createFlags = 0,
        .buffer = blasBuffer_.get(),
        .offset = 0,
        .size = sizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .deviceAddress = 0
    };
    VkAccelerationStructureKHR blas;
    VK_CHECK(vkCreateAccelerationStructureKHR(device_, &createInfo, nullptr, &blas), "Failed to create BLAS.");
    blas_ = VulkanResource<VkAccelerationStructureKHR, PFN_vkDestroyAccelerationStructureKHR>(blas, device_, vkDestroyAccelerationStructureKHR);

    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> scratchBuffer;
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> scratchMemory;
    createBuffer(physicalDevice, sizeInfo.buildScratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer, scratchMemory);

    buildInfo.dstAccelerationStructure = blas_.get();
    buildInfo.scratchData.deviceAddress = getBufferDeviceAddress(scratchBuffer.get());

    VkCommandBuffer commandBuffer = allocateTransientCommandBuffer(commandPool);
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo), "Failed to begin command buffer.");

    std::vector<VkAccelerationStructureBuildRangeInfoKHR*> buildRangePtrs;
    for (auto& range : primitiveCounts_) {
        buildRangePtrs.push_back(&range);
    }
    vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfo, buildRangePtrs.data());

    VK_CHECK(vkEndCommandBuffer(commandBuffer), "Failed to end command buffer.");
    submitAndWaitTransient(commandBuffer, queue, commandPool);
    std::osyncstream(std::cout) << GREEN << "[INFO] Created BLAS with " << asGeometries.size() << " geometries" << RESET << std::endl;
}

void VulkanRTX::createTopLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                                 const std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4>>& instances) {
    if (instances.empty()) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Invalid TLAS params: empty instances" << RESET << std::endl;
        throw VulkanRTXException("Invalid TLAS params: empty instances.");
    }

    std::osyncstream(std::cout) << GREEN << "[INFO] Creating top-level acceleration structure" << RESET << std::endl;

    std::vector<VkAccelerationStructureInstanceKHR> asInstances;
    for (size_t i = 0; i < instances.size(); ++i) {
        const auto& [blas, transform] = instances[i];
        if (!blas) {
            std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Null AS in instance at index " << i << RESET << std::endl;
            throw VulkanRTXException(std::format("Null AS in instance at index {}.", i));
        }
        VkTransformMatrixKHR transformMatrix;
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 4; ++col) {
                transformMatrix.matrix[row][col] = transform[col][row]; // GLM is column-major, Vulkan expects row-major
            }
        }
        VkAccelerationStructureInstanceKHR instance{
            .transform = transformMatrix,
            .instanceCustomIndex = 0,
            .mask = 0xFF,
            .instanceShaderBindingTableRecordOffset = 0,
            .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
            .accelerationStructureReference = getAccelerationStructureDeviceAddress(blas)
        };
        asInstances.push_back(instance);
    }

    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> instanceBuffer;
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> instanceMemory;
    VkDeviceSize instanceSize = asInstances.size() * sizeof(VkAccelerationStructureInstanceKHR);
    createBuffer(physicalDevice, instanceSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, instanceBuffer, instanceMemory);

    void* mappedData;
    VK_CHECK(vkMapMemory(device_, instanceMemory.get(), 0, instanceSize, 0, &mappedData), "Instance buffer map failed.");
    std::memcpy(mappedData, asInstances.data(), instanceSize);
    vkUnmapMemory(device_, instanceMemory.get());

    VkAccelerationStructureGeometryKHR geometry{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .pNext = nullptr,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry = {
            .instances = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                .pNext = nullptr,
                .arrayOfPointers = VK_FALSE,
                .data = { .deviceAddress = getBufferDeviceAddress(instanceBuffer.get()) }
            }
        },
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .pNext = nullptr,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .srcAccelerationStructure = VK_NULL_HANDLE,
        .dstAccelerationStructure = VK_NULL_HANDLE,
        .geometryCount = 1,
        .pGeometries = &geometry,
        .ppGeometries = nullptr,
        .scratchData = { .deviceAddress = 0 }
    };

    VkAccelerationStructureBuildSizesInfoKHR buildSizes{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
        .pNext = nullptr,
        .accelerationStructureSize = 0,
        .updateScratchSize = 0,
        .buildScratchSize = 0
    };
    uint32_t primitiveCount = static_cast<uint32_t>(asInstances.size());
    vkGetAccelerationStructureBuildSizesKHR(device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &primitiveCount, &buildSizes);

    createBuffer(physicalDevice, buildSizes.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, tlasBuffer_, tlasMemory_);

    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .createFlags = 0,
        .buffer = tlasBuffer_.get(),
        .offset = 0,
        .size = buildSizes.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .deviceAddress = 0
    };
    VkAccelerationStructureKHR tlas;
    VK_CHECK(vkCreateAccelerationStructureKHR(device_, &createInfo, nullptr, &tlas), "Failed to create TLAS.");
    tlas_ = VulkanResource<VkAccelerationStructureKHR, PFN_vkDestroyAccelerationStructureKHR>(tlas, device_, vkDestroyAccelerationStructureKHR);

    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> scratchBuffer;
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> scratchMemory;
    createBuffer(physicalDevice, buildSizes.buildScratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer, scratchMemory);

    buildInfo.dstAccelerationStructure = tlas_.get();
    buildInfo.scratchData.deviceAddress = getBufferDeviceAddress(scratchBuffer.get());

    VkCommandBuffer commandBuffer = allocateTransientCommandBuffer(commandPool);
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo), "Failed to begin command buffer.");

    VkAccelerationStructureBuildRangeInfoKHR buildRange{
        .primitiveCount = primitiveCount,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0
    };
    VkAccelerationStructureBuildRangeInfoKHR* buildRangePtr = &buildRange;
    vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfo, &buildRangePtr);

    VK_CHECK(vkEndCommandBuffer(commandBuffer), "Failed to end command buffer.");
    submitAndWaitTransient(commandBuffer, queue, commandPool);
    updateDescriptorSetForTLAS(tlas_.get());
    std::osyncstream(std::cout) << GREEN << "[INFO] Created TLAS with " << asInstances.size() << " instances" << RESET << std::endl;
}

VkDeviceAddress VulkanRTX::getBufferDeviceAddress(VkBuffer buffer) const {
    if (!buffer) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Invalid buffer device address" << RESET << std::endl;
        throw VulkanRTXException("Invalid buffer device address.");
    }
    VkBufferDeviceAddressInfo info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = nullptr,
        .buffer = buffer
    };
    VkDeviceAddress address = vkGetBufferDeviceAddress(device_, &info);
    if (address == 0) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Invalid buffer device address" << RESET << std::endl;
        throw VulkanRTXException("Invalid buffer device address.");
    }
    return address;
}

VkDeviceAddress VulkanRTX::getAccelerationStructureDeviceAddress(VkAccelerationStructureKHR as) const {
    if (!as) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Invalid AS device address" << RESET << std::endl;
        throw VulkanRTXException("Invalid AS device address.");
    }
    VkAccelerationStructureDeviceAddressInfoKHR info{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .pNext = nullptr,
        .accelerationStructure = as
    };
    VkDeviceAddress address = vkGetAccelerationStructureDeviceAddressKHR(device_, &info);
    if (address == 0) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Invalid AS device address" << RESET << std::endl;
        throw VulkanRTXException("Invalid AS device address.");
    }
    return address;
}

VkCommandBuffer VulkanRTX::allocateTransientCommandBuffer(VkCommandPool commandPool) {
    if (!commandPool) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Invalid command pool" << RESET << std::endl;
        throw VulkanRTXException("Invalid command pool.");
    }
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VkCommandBuffer commandBuffer;
    VK_CHECK(vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer), "Transient command buffer allocation failed.");
    std::osyncstream(std::cout) << GREEN << "[INFO] Allocated transient command buffer" << RESET << std::endl;
    return commandBuffer;
}

void VulkanRTX::submitAndWaitTransient(VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool commandPool) {
    if (!commandBuffer || !queue || !commandPool) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Invalid submit params: null command buffer, queue, or pool" << RESET << std::endl;
        throw VulkanRTXException("Invalid submit params: null command buffer, queue, or pool.");
    }
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .pWaitDstStageMask = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = nullptr
    };
    VkFenceCreateInfo fenceInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0
    };
    VkFence fence;
    VK_CHECK(vkCreateFence(device_, &fenceInfo, nullptr, &fence), "Fence creation failed.");

    VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, fence), "Queue submit failed.");
    VK_CHECK(vkWaitForFences(device_, 1, &fence, VK_TRUE, UINT64_MAX), "Fence wait failed.");

    vkDestroyFence(device_, fence, nullptr);
    vkFreeCommandBuffers(device_, commandPool, 1, &commandBuffer);
    std::osyncstream(std::cout) << GREEN << "[INFO] Submitted and waited for transient command buffer" << RESET << std::endl;
}

void VulkanRTX::updateDescriptorSetForTLAS(VkAccelerationStructureKHR tlas) {
    if (!tlas) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Null TLAS provided for descriptor update" << RESET << std::endl;
        throw VulkanRTXException("Null TLAS provided for descriptor update.");
    }
    std::osyncstream(std::cout) << GREEN << "[INFO] Updating descriptor set for TLAS" << RESET << std::endl;

    VkWriteDescriptorSetAccelerationStructureKHR asInfo{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .pNext = nullptr,
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &tlas
    };

    VkWriteDescriptorSet write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = &asInfo,
        .dstSet = ds_.get(),
        .dstBinding = static_cast<uint32_t>(DescriptorBindings::TLAS),
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
        .pImageInfo = nullptr,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr
    };

    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
    std::osyncstream(std::cout) << GREEN << "[INFO] Updated descriptor set for TLAS" << RESET << std::endl;
}

} // namespace VulkanRTX