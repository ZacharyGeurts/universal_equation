// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// Vulkan_RTX.cpp: Core Vulkan ray tracing implementation for the AMOURANTH RTX Engine.
// Supports real-time ray tracing of voxel-based geometry on n-dimensional lattices, with integration of DimensionData for voxel grid metadata.
// Leverages Vulkan ray tracing pipelines, acceleration structures, and compute shaders for denoising.
// Optimized for single-threaded operation with no mutexes, using C++20 features and robust logging.

// How this ties into the voxel world:
// The VulkanRTX class manages Vulkan ray tracing resources to render voxel-based scenes, where each voxel is represented as a cube with 12 triangles (8 vertices, 36 indices).
// DimensionData (grid dimensions, voxel size) is stored in a storage buffer for shader access, enabling ray-voxel intersection tests or procedural geometry.
// The ray tracing pipeline supports optional intersection shaders for procedural voxels, and the top-level acceleration structure (TLAS) handles a single static voxel grid instance.

#include "engine/Vulkan/Vulkan_RTX.hpp"
#include <cstring>
#include <ranges>
#include <stdexcept>
#include <format>
#include <syncstream>
#include <iostream>

// ANSI color codes
#define RESET "\033[0m"
#define MAGENTA "\033[1;35m" // Bold magenta for errors
#define CYAN "\033[1;36m"    // Bold cyan for debug
#define YELLOW "\033[1;33m"  // Bold yellow for warnings
#define GREEN "\033[1;32m"   // Bold green for info
#define BOLD "\033[1m"

void VulkanRTX::createBuffer(
    VkPhysicalDevice physicalDevice,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags props,
    VulkanResource<VkBuffer, PFN_vkDestroyBuffer>& buffer,
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>& memory) {
    if (!physicalDevice || size == 0) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Invalid buffer params: null device or zero size" << RESET << std::endl;
        throw VulkanRTXException(std::format("Invalid buffer params: null device or size={}.", size));
    }

    std::osyncstream(std::cout) << GREEN << "[INFO] Creating buffer with size=" << size << ", usage=" << usage << RESET << std::endl;

    VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VkBuffer tempBuffer = VK_NULL_HANDLE;
    VK_CHECK(vkCreateBuffer(device_, &bufferInfo, nullptr, &tempBuffer), std::format("Buffer creation failed for size={}.", size));
    buffer = VulkanResource<VkBuffer, PFN_vkDestroyBuffer>(device_, tempBuffer, vkDestroyBuffer);

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device_, tempBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, props)
    };

    VkDeviceMemory tempMemory = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(device_, &allocInfo, nullptr, &tempMemory), "Memory allocation failed.");
    memory = VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>(device_, tempMemory, vkFreeMemory);

    VK_CHECK(vkBindBufferMemory(device_, tempBuffer, tempMemory, 0), "Buffer memory binding failed.");
    std::osyncstream(std::cout) << GREEN << "[INFO] Created buffer successfully with size=" << size << RESET << std::endl;
}

uint32_t VulkanRTX::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags props) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Finding memory type for filter=" << typeFilter << ", properties=" << props << RESET << std::endl;

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i : std::views::iota(0u, memProperties.memoryTypeCount)) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & props) == props) {
            std::osyncstream(std::cout) << CYAN << "[DEBUG] Found memory type index=" << i << RESET << std::endl;
            return i;
        }
    }

    std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to find suitable memory type for filter=" << typeFilter << RESET << std::endl;
    throw VulkanRTXException(std::format("Failed to find suitable memory type for filter={}.", typeFilter));
}

void VulkanRTX::compactAccelerationStructures(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue) {
    if (!supportsCompaction_) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Acceleration structure compaction not supported" << RESET << std::endl;
        throw VulkanRTXException("Acceleration structure compaction not supported.");
    }

    VkAccelerationStructureKHR blasHandle = blas_.get();
    if (!blasHandle) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] No BLAS available for compaction" << RESET << std::endl;
        throw VulkanRTXException("No BLAS available for compaction.");
    }

    std::osyncstream(std::cout) << GREEN << "[INFO] Starting acceleration structure compaction" << RESET << std::endl;

    VkQueryPoolCreateInfo queryPoolInfo{
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
        .queryCount = 1
    };

    VkQueryPool queryPool;
    VK_CHECK(vkCreateQueryPool(device_, &queryPoolInfo, nullptr, &queryPool), "Query pool creation failed.");

    VkCommandBuffer cmdBuffer = allocateTransientCommandBuffer(commandPool);
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo), "Failed to begin command buffer");

    std::osyncstream(std::cout) << CYAN << "[DEBUG] Resetting query pool and writing AS properties" << RESET << std::endl;
    vkCmdResetQueryPool(cmdBuffer, queryPool, 0, 1);
    vkCmdWriteASPropertiesFunc(
        cmdBuffer, 1, &blasHandle, VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryPool, 0);

    VK_CHECK(vkEndCommandBuffer(cmdBuffer), "Failed to end command buffer");
    submitAndWaitTransient(cmdBuffer, queue, commandPool);

    VkDeviceSize compactedSize;
    VK_CHECK(vkGetQueryPoolResults(device_, queryPool, 0, 1, sizeof(VkDeviceSize), &compactedSize, sizeof(VkDeviceSize), VK_QUERY_RESULT_WAIT_BIT),
             "Failed to get query pool results");
    std::osyncstream(std::cout) << CYAN << "[DEBUG] Compacted size=" << compactedSize << " bytes" << RESET << std::endl;

    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> compactedBlasBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> compactedBlasMemory(device_, VK_NULL_HANDLE, vkFreeMemory);
    createBuffer(physicalDevice, compactedSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, compactedBlasBuffer, compactedBlasMemory);

    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = compactedBlasBuffer.get(),
        .size = compactedSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
    };

    VkAccelerationStructureKHR compactedAsHandle = VK_NULL_HANDLE;
    VK_CHECK(vkCreateASFunc(device_, &createInfo, nullptr, &compactedAsHandle), "Failed to create compacted acceleration structure");
    VulkanResource<VkAccelerationStructureKHR, PFN_vkDestroyAccelerationStructureKHR> compactedBlas(device_, compactedAsHandle, vkDestroyASFunc);

    cmdBuffer = allocateTransientCommandBuffer(commandPool);
    beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo), "Failed to begin command buffer");

    VkCopyAccelerationStructureInfoKHR copyInfo{
        .sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR,
        .src = blas_.get(),
        .dst = compactedBlas.get(),
        .mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR
    };

    std::osyncstream(std::cout) << CYAN << "[DEBUG] Copying acceleration structure for compaction" << RESET << std::endl;
    vkCmdCopyAccelerationStructureKHR(cmdBuffer, &copyInfo);

    VK_CHECK(vkEndCommandBuffer(cmdBuffer), "Failed to end command buffer");
    submitAndWaitTransient(cmdBuffer, queue, commandPool);

    blas_ = std::move(compactedBlas);
    blasBuffer_ = std::move(compactedBlasBuffer);
    blasMemory_ = std::move(compactedBlasMemory);

    vkDestroyQueryPool(device_, queryPool, nullptr);
    std::osyncstream(std::cout) << GREEN << "[INFO] Acceleration structure compaction completed successfully" << RESET << std::endl;
}

void VulkanRTX::createBottomLevelAS(
    VkPhysicalDevice physicalDevice,
    VkCommandPool commandPool,
    VkQueue queue,
    const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries) {
    if (geometries.empty()) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Invalid BLAS params: empty geometries" << RESET << std::endl;
        throw VulkanRTXException("Invalid BLAS params: empty geometries.");
    }

    std::osyncstream(std::cout) << GREEN << "[INFO] Creating bottom-level AS with " << geometries.size() << " geometries" << RESET << std::endl;

    std::vector<VkAccelerationStructureGeometryKHR> geometriesKHR(geometries.size());
    primitiveCounts_.resize(geometries.size());

    for (size_t i : std::views::iota(0u, geometries.size())) {
        auto [vertexBuffer, indexBuffer, vertexCount, indexCount, vertexStride] = geometries[i];
        if (!vertexBuffer || !indexBuffer || vertexCount == 0 || indexCount == 0 || vertexStride == 0) {
            std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Invalid geometry at index " << i << ": null buffer or zero count/stride" << RESET << std::endl;
            throw VulkanRTXException(std::format("Invalid geometry at index {}: null buffer or zero count/stride.", i));
        }

        geometriesKHR[i] = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
            .geometry = {
                .triangles = {
                    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
                    .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
                    .vertexData = { .deviceAddress = getBufferDeviceAddress(vertexBuffer) },
                    .vertexStride = vertexStride,
                    .maxVertex = vertexCount - 1,
                    .indexType = VK_INDEX_TYPE_UINT32,
                    .indexData = { .deviceAddress = getBufferDeviceAddress(indexBuffer) }
                }
            },
            .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
        };

        primitiveCounts_[i] = {
            .primitiveCount = indexCount / 3,
            .primitiveOffset = 0,
            .firstVertex = 0,
            .transformOffset = 0
        };
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Geometry " << i << ": vertexCount=" << vertexCount << ", indexCount=" << indexCount << ", primitiveCount=" << (indexCount / 3) << RESET << std::endl;
    }

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR,
        .geometryCount = static_cast<uint32_t>(geometries.size()),
        .pGeometries = geometriesKHR.data()
    };

    std::vector<uint32_t> maxPrimitives(geometries.size());
    for (size_t i : std::views::iota(0u, geometries.size())) {
        maxPrimitives[i] = primitiveCounts_[i].primitiveCount;
    }
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    vkGetASBuildSizesFunc(device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, maxPrimitives.data(), &sizeInfo);
    std::osyncstream(std::cout) << CYAN << "[DEBUG] BLAS build sizes: accelerationStructureSize=" << sizeInfo.accelerationStructureSize << ", buildScratchSize=" << sizeInfo.buildScratchSize << RESET << std::endl;

    createBuffer(physicalDevice, sizeInfo.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, blasBuffer_, blasMemory_);

    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = blasBuffer_.get(),
        .size = sizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
    };

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
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo), "Failed to begin command buffer");

    std::vector<VkAccelerationStructureBuildRangeInfoKHR*> buildRangePtrs(geometries.size());
    for (size_t i : std::views::iota(0u, geometries.size())) {
        buildRangePtrs[i] = &primitiveCounts_[i];
    }
    std::osyncstream(std::cout) << CYAN << "[DEBUG] Building BLAS with " << geometries.size() << " geometries" << RESET << std::endl;
    vkCmdBuildASFunc(cmdBuffer, 1, &buildInfo, buildRangePtrs.data());

    VK_CHECK(vkEndCommandBuffer(cmdBuffer), "Failed to end command buffer");
    submitAndWaitTransient(cmdBuffer, queue, commandPool);
    std::osyncstream(std::cout) << GREEN << "[INFO] Bottom-level AS created successfully with " << geometries.size() << " geometries" << RESET << std::endl;
}

void VulkanRTX::createTopLevelAS(
    VkPhysicalDevice physicalDevice,
    VkCommandPool commandPool,
    VkQueue queue,
    const std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4>>& instances) {
    if (instances.empty()) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Invalid TLAS params: empty instances" << RESET << std::endl;
        throw VulkanRTXException("Invalid TLAS params: empty instances.");
    }

    std::osyncstream(std::cout) << GREEN << "[INFO] Creating top-level AS with " << instances.size() << " instances" << RESET << std::endl;

    std::vector<VkAccelerationStructureInstanceKHR> instanceData(instances.size());
    for (size_t i : std::views::iota(0u, instances.size())) {
        auto [as, transform] = instances[i];
        if (!as) {
            std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Null AS in instance at index " << i << RESET << std::endl;
            throw VulkanRTXException(std::format("Null AS in instance at index {}.", i));
        }

        VkTransformMatrixKHR transformMatrix;
        std::memcpy(&transformMatrix, &transform, sizeof(VkTransformMatrixKHR));

        instanceData[i] = {
            .transform = transformMatrix,
            .instanceCustomIndex = 0,
            .mask = 0xFF,
            .instanceShaderBindingTableRecordOffset = 0,
            .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
            .accelerationStructureReference = getAccelerationStructureDeviceAddress(as)
        };
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Instance " << i << ": AS address=" << instanceData[i].accelerationStructureReference << RESET << std::endl;
    }

    VkDeviceSize instanceSize = instanceData.size() * sizeof(VkAccelerationStructureInstanceKHR);
    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> instanceBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> instanceMemory(device_, VK_NULL_HANDLE, vkFreeMemory);

    std::osyncstream(std::cout) << CYAN << "[DEBUG] Creating instance buffer with size=" << instanceSize << RESET << std::endl;
    createBuffer(physicalDevice, instanceSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 instanceBuffer, instanceMemory);

    void* data;
    VK_CHECK(vkMapMemory(device_, instanceMemory.get(), 0, instanceSize, 0, &data), "Instance buffer map failed.");
    std::memcpy(data, instanceData.data(), instanceSize);
    vkUnmapMemory(device_, instanceMemory.get());
    std::osyncstream(std::cout) << CYAN << "[DEBUG] Mapped and copied instance data" << RESET << std::endl;

    VkDeviceAddress instanceAddress = getBufferDeviceAddress(instanceBuffer.get());

    VkAccelerationStructureGeometryKHR geometry{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry = {
            .instances = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                .data = { .deviceAddress = instanceAddress }
            }
        },
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .geometryCount = 1,
        .pGeometries = &geometry
    };

    uint32_t primitiveCount = static_cast<uint32_t>(instances.size());
    VkAccelerationStructureBuildSizesInfoKHR buildSizes{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    vkGetASBuildSizesFunc(device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &primitiveCount, &buildSizes);
    std::osyncstream(std::cout) << CYAN << "[DEBUG] TLAS build sizes: accelerationStructureSize=" << buildSizes.accelerationStructureSize << ", buildScratchSize=" << buildSizes.buildScratchSize << RESET << std::endl;

    createBuffer(physicalDevice, buildSizes.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, tlasBuffer_, tlasMemory_);

    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = tlasBuffer_.get(),
        .size = buildSizes.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
    };

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

    VkCommandBuffer cmdBuffer = allocateTransientCommandBuffer(commandPool);
    beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo), "TLAS begin failed.");

    VkAccelerationStructureBuildRangeInfoKHR buildRange{ .primitiveCount = primitiveCount };
    VkAccelerationStructureBuildRangeInfoKHR* buildRanges = &buildRange;
    std::osyncstream(std::cout) << CYAN << "[DEBUG] Building TLAS with " << primitiveCount << " instances" << RESET << std::endl;
    vkCmdBuildASFunc(cmdBuffer, 1, &buildInfo, &buildRanges);

    VK_CHECK(vkEndCommandBuffer(cmdBuffer), "TLAS end failed.");
    submitAndWaitTransient(cmdBuffer, queue, commandPool);

    updateDescriptorSetForTLAS(tlas_.get());
    std::osyncstream(std::cout) << GREEN << "[INFO] Top-level AS created successfully with " << instances.size() << " instances" << RESET << std::endl;
}

VkDeviceAddress VulkanRTX::getBufferDeviceAddress(VkBuffer buffer) const {
    std::osyncstream(std::cout) << GREEN << "[INFO] Getting device address for buffer" << RESET << std::endl;
    VkBufferDeviceAddressInfo info{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = buffer };
    VkDeviceAddress address = vkGetBufferDeviceAddressFunc(device_, &info);
    if (address == 0) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Invalid buffer device address" << RESET << std::endl;
        throw VulkanRTXException("Invalid buffer device address.");
    }
    std::osyncstream(std::cout) << CYAN << "[DEBUG] Buffer device address=" << address << RESET << std::endl;
    return address;
}

VkDeviceAddress VulkanRTX::getAccelerationStructureDeviceAddress(VkAccelerationStructureKHR as) const {
    std::osyncstream(std::cout) << GREEN << "[INFO] Getting device address for acceleration structure" << RESET << std::endl;
    VkAccelerationStructureDeviceAddressInfoKHR info{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = as
    };
    VkDeviceAddress address = vkGetASDeviceAddressFunc(device_, &info);
    if (address == 0) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Invalid AS device address" << RESET << std::endl;
        throw VulkanRTXException("Invalid AS device address.");
    }
    std::osyncstream(std::cout) << CYAN << "[DEBUG] AS device address=" << address << RESET << std::endl;
    return address;
}

VkCommandBuffer VulkanRTX::allocateTransientCommandBuffer(VkCommandPool commandPool) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Allocating transient command buffer" << RESET << std::endl;
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    VkCommandBuffer cmdBuffer;
    VK_CHECK(vkAllocateCommandBuffers(device_, &allocInfo, &cmdBuffer), "Transient command buffer allocation failed.");
    std::osyncstream(std::cout) << GREEN << "[INFO] Transient command buffer allocated successfully" << RESET << std::endl;
    return cmdBuffer;
}

void VulkanRTX::submitAndWaitTransient(VkCommandBuffer cmdBuffer, VkQueue queue, VkCommandPool commandPool) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Submitting transient command buffer" << RESET << std::endl;
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmdBuffer
    };

    VkFenceCreateInfo fenceInfo{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    VkFence fence;
    VK_CHECK(vkCreateFence(device_, &fenceInfo, nullptr, &fence), "Fence creation failed.");

    VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, fence), "Queue submit failed.");
    VK_CHECK(vkWaitForFences(device_, 1, &fence, VK_TRUE, UINT64_MAX), "Fence wait failed.");

    vkDestroyFence(device_, fence, nullptr);
    vkFreeCommandBuffers(device_, commandPool, 1, &cmdBuffer);
    std::osyncstream(std::cout) << GREEN << "[INFO] Transient command buffer submitted and completed" << RESET << std::endl;
}

void VulkanRTX::updateDescriptorSetForTLAS(VkAccelerationStructureKHR tlas) {
    if (!tlas) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Null TLAS provided for descriptor update" << RESET << std::endl;
        throw VulkanRTXException("Null TLAS provided for descriptor update.");
    }

    std::osyncstream(std::cout) << GREEN << "[INFO] Updating descriptor set for TLAS" << RESET << std::endl;
    VkWriteDescriptorSetAccelerationStructureKHR asInfo{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &tlas
    };

    VkWriteDescriptorSet accelWrite{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = &asInfo,
        .dstSet = ds_.get(),
        .dstBinding = static_cast<uint32_t>(DescriptorBindings::TLAS),
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
    };

    vkUpdateDescriptorSets(device_, 1, &accelWrite, 0, nullptr);
    std::osyncstream(std::cout) << GREEN << "[INFO] TLAS descriptor set updated successfully" << RESET << std::endl;
}