// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// Vulkan_RTX.cpp: Core Vulkan ray tracing implementation for the AMOURANTH RTX Engine.
// Supports real-time ray tracing of voxel-based geometry on n-dimensional lattices, with strong integration of DimensionData for voxel grid metadata.
// Leverages Vulkan ray tracing pipelines, acceleration structures, and compute shaders for denoising.
// Optimized for single-threaded operation with no mutexes, using C++20 features for robust and efficient rendering.

// How this ties into the voxel world:
// The VulkanRTX class manages Vulkan ray tracing resources to render voxel-based scenes, where each voxel is represented as a cube with 12 triangles (8 vertices, 36 indices).
// DimensionData (grid dimensions, voxel size) is stored in a storage buffer for shader access, enabling ray-voxel intersection tests or procedural geometry.
// The ray tracing pipeline supports optional intersection shaders for procedural voxels, and the top-level acceleration structure (TLAS) handles a single static voxel grid instance.
// The denoising compute shader processes the ray-traced output, ensuring high-quality rendering of voxel surfaces with dynamic resolution support.

#include "engine/Vulkan/Vulkan_RTX.hpp"
#include <array>
#include <vector>
#include <ranges>
#include <cstring>
#include <format>
#include <syncstream>
#include <iostream>

// ANSI color codes
#define RESET "\033[0m"
#define MAGENTA "\033[1;35m" // Bold magenta for errors
#define CYAN "\033[1;36m"    // Bold cyan for debug
#define YELLOW "\033[1;33m"  // Bold yellow for warnings
#define GREEN "\033[1;36m"   // Bold green for info
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
    std::osyncstream(std::cout) << GREEN << "[INFO] Created buffer with size=" << size << RESET << std::endl;
}

uint32_t VulkanRTX::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i : std::views::iota(0u, memProperties.memoryTypeCount)) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & props) == props) {
            std::osyncstream(std::cout) << CYAN << "[DEBUG] Found memory type index=" << i << " for properties=" << props << RESET << std::endl;
            return i;
        }
    }

    std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to find suitable memory type for filter=" << typeFilter << RESET << std::endl;
    throw VulkanRTXException("Failed to find suitable memory type.");
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
    VK_CHECK(vkCreateASFunc(device_, &createInfo, nullptr, &asHandle), "Failed to create BLAS.");
    blas_ = VulkanResource<VkAccelerationStructureKHR, PFN_vkDestroyAccelerationStructureKHR>(device_, asHandle, vkDestroyASFunc);

    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> scratchBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> scratchMemory(device_, VK_NULL_HANDLE, vkFreeMemory);
    createBuffer(physicalDevice, sizeInfo.buildScratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer, scratchMemory);

    buildInfo.dstAccelerationStructure = blas_.get();
    buildInfo.scratchData.deviceAddress = getBufferDeviceAddress(scratchBuffer.get());

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
    vkCmdBuildASFunc(cmdBuffer, 1, &buildInfo, buildRangePtrs.data());

    VK_CHECK(vkEndCommandBuffer(cmdBuffer), "Failed to end command buffer");
    submitAndWaitTransient(cmdBuffer, queue, commandPool);
    std::osyncstream(std::cout) << GREEN << "[INFO] Bottom-level AS created with " << geometries.size() << " geometries" << RESET << std::endl;
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
    }

    VkDeviceSize instanceSize = instanceData.size() * sizeof(VkAccelerationStructureInstanceKHR);
    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> instanceBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> instanceMemory(device_, VK_NULL_HANDLE, vkFreeMemory);

    createBuffer(physicalDevice, instanceSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 instanceBuffer, instanceMemory);

    void* data;
    VK_CHECK(vkMapMemory(device_, instanceMemory.get(), 0, instanceSize, 0, &data), "Instance buffer map failed.");
    std::memcpy(data, instanceData.data(), instanceSize);
    vkUnmapMemory(device_, instanceMemory.get());

    VkAccelerationStructureGeometryKHR geometry{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry = {
            .instances = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                .data = { .deviceAddress = getBufferDeviceAddress(instanceBuffer.get()) }
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

    buildInfo.dstAccelerationStructure = tlas_.get();
    buildInfo.scratchData.deviceAddress = getBufferDeviceAddress(scratchBuffer.get());

    VkAccelerationStructureBuildRangeInfoKHR buildRange{ .primitiveCount = primitiveCount };
    VkAccelerationStructureBuildRangeInfoKHR* buildRanges = &buildRange;

    VkCommandBuffer cmdBuffer = allocateTransientCommandBuffer(commandPool);
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo), "TLAS begin failed.");

    vkCmdBuildASFunc(cmdBuffer, 1, &buildInfo, &buildRanges);

    VK_CHECK(vkEndCommandBuffer(cmdBuffer), "TLAS end failed.");
    submitAndWaitTransient(cmdBuffer, queue, commandPool);
    updateDescriptorSetForTLAS(tlas_.get());
    std::osyncstream(std::cout) << GREEN << "[INFO] Top-level AS created with " << instances.size() << " instances" << RESET << std::endl;
}

VkDeviceAddress VulkanRTX::getBufferDeviceAddress(VkBuffer buffer) const {
    VkBufferDeviceAddressInfo info{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = buffer };
    VkDeviceAddress address = vkGetBufferDeviceAddressFunc(device_, &info);
    if (address == 0) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Invalid buffer device address" << RESET << std::endl;
        throw VulkanRTXException("Invalid buffer device address.");
    }
    return address;
}

VkDeviceAddress VulkanRTX::getAccelerationStructureDeviceAddress(VkAccelerationStructureKHR as) const {
    VkAccelerationStructureDeviceAddressInfoKHR info{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = as
    };
    VkDeviceAddress address = vkGetASDeviceAddressFunc(device_, &info);
    if (address == 0) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Invalid AS device address" << RESET << std::endl;
        throw VulkanRTXException("Invalid AS device address.");
    }
    return address;
}

VkCommandBuffer VulkanRTX::allocateTransientCommandBuffer(VkCommandPool commandPool) {
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    VkCommandBuffer cmdBuffer;
    VK_CHECK(vkAllocateCommandBuffers(device_, &allocInfo, &cmdBuffer), "Transient command buffer allocation failed.");
    std::osyncstream(std::cout) << GREEN << "[INFO] Allocated transient command buffer" << RESET << std::endl;
    return cmdBuffer;
}

void VulkanRTX::submitAndWaitTransient(VkCommandBuffer cmdBuffer, VkQueue queue, VkCommandPool commandPool) {
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
    std::osyncstream(std::cout) << GREEN << "[INFO] Submitted and waited for transient command buffer" << RESET << std::endl;
}

void VulkanRTX::updateDescriptorSetForTLAS(VkAccelerationStructureKHR tlas) {
    if (!tlas) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Null TLAS provided for descriptor update" << RESET << std::endl;
        throw VulkanRTXException("Null TLAS provided for descriptor update.");
    }

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
    std::osyncstream(std::cout) << GREEN << "[INFO] Updated descriptor set for TLAS" << RESET << std::endl;
}

void VulkanRTX::createStorageImage(
    VkPhysicalDevice physicalDevice, VkExtent2D extent, VkFormat format,
    VulkanResource<VkImage, PFN_vkDestroyImage>& image,
    VulkanResource<VkImageView, PFN_vkDestroyImageView>& imageView,
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>& memory) {
    if (!physicalDevice || extent.width == 0 || extent.height == 0) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Invalid image params: null device or zero extent (" << extent.width << "x" << extent.height << ")" << RESET << std::endl;
        throw VulkanRTXException(std::format("Invalid image params: null device or extent={}x{}.", extent.width, extent.height));
    }

    VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {extent.width, extent.height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                 VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VkImage tempImage = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImage(device_, &imageInfo, nullptr, &tempImage), std::format("Storage image creation failed for extent={}x{}.", extent.width, extent.height));
    image = VulkanResource<VkImage, PFN_vkDestroyImage>(device_, tempImage, vkDestroyImage);

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device_, image.get(), &memReqs);
    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = findMemoryType(physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    VkDeviceMemory tempMemory = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(device_, &allocInfo, nullptr, &tempMemory), "Storage image memory allocation failed.");
    memory = VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>(device_, tempMemory, vkFreeMemory);
    VK_CHECK(vkBindImageMemory(device_, image.get(), memory.get(), 0), "Storage image memory binding failed.");

    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image.get(),
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };

    VkImageView tempImageView = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImageView(device_, &viewInfo, nullptr, &tempImageView), "Storage image view creation failed.");
    imageView = VulkanResource<VkImageView, PFN_vkDestroyImageView>(device_, tempImageView, vkDestroyImageView);
    extent_ = extent; // Store extent for denoiseImage
    std::osyncstream(std::cout) << GREEN << "[INFO] Created storage image with extent=" << extent.width << "x" << extent.height << RESET << std::endl;
}

void VulkanRTX::recordRayTracingCommands(
    VkCommandBuffer cmdBuffer, VkExtent2D extent, VkImage outputImage, VkImageView outputImageView,
    const PushConstants& pc, VkAccelerationStructureKHR tlas) {
    if (!cmdBuffer || !outputImage || !outputImageView) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Null cmd buffer/image/view for ray tracing" << RESET << std::endl;
        throw VulkanRTXException("Null cmd buffer/image/view for ray tracing.");
    }
    if (tlas == VK_NULL_HANDLE) tlas = tlas_.get();
    if (tlas != tlas_.get()) updateDescriptorSetForTLAS(tlas);

    VkDescriptorImageInfo imageInfo{ .imageView = outputImageView, .imageLayout = VK_IMAGE_LAYOUT_GENERAL };
    VkWriteDescriptorSet imageWrite{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = ds_.get(),
        .dstBinding = static_cast<uint32_t>(DescriptorBindings::StorageImage),
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .pImageInfo = &imageInfo
    };
    vkUpdateDescriptorSets(device_, 1, &imageWrite, 0, nullptr);

    VkImageMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = outputImage,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline_.get());
    VkDescriptorSet dsHandle = ds_.get();
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipelineLayout_.get(),
                            0, 1, &dsHandle, 0, nullptr);
    vkCmdPushConstants(cmdBuffer, rtPipelineLayout_.get(),
                       VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
                       VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR |
                       VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR,
                       0, sizeof(PushConstants), &pc);
    vkCmdTraceRaysKHR(cmdBuffer, &sbt_.raygen, &sbt_.miss, &sbt_.hit, &sbt_.callable,
                      extent.width, extent.height, 1);

    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    std::osyncstream(std::cout) << GREEN << "[INFO] Recorded ray tracing commands for extent=" << extent.width << "x" << extent.height << RESET << std::endl;
}

void VulkanRTX::initializeRTX(
    VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
    const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
    uint32_t maxRayRecursionDepth,
    const std::vector<DimensionData>& dimensionCache) {
    if (!physicalDevice || !commandPool || !graphicsQueue || geometries.empty()) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Invalid init params: null device/pool/queue or empty geometries" << RESET << std::endl;
        throw VulkanRTXException("Invalid init params: null device/pool/queue or empty geometries.");
    }
    for (size_t i : std::views::iota(0u, geometries.size())) {
        auto [vertexBuffer, indexBuffer, vertexCount, indexCount, vertexStride] = geometries[i];
        if (!vertexBuffer || !indexBuffer || vertexCount == 0 || indexCount == 0 || vertexStride == 0) {
            std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Invalid geometry at index " << i << ": null buffer or zero count/stride" << RESET << std::endl;
            throw VulkanRTXException(std::format("Invalid geometry at index {}: null buffer or zero count/stride.", i));
        }
    }

    std::osyncstream(std::cout) << GREEN << "[INFO] Starting RTX initialization with " << geometries.size() << " geometries, max recursion depth=" << maxRayRecursionDepth << RESET << std::endl;

    createDescriptorSetLayout();
    createDescriptorPoolAndSet();
    createRayTracingPipeline(maxRayRecursionDepth);
    createShaderBindingTable(physicalDevice);

    primitiveCounts_.clear();
    primitiveCounts_.reserve(geometries.size());
    for (const auto& [_, __, ___, indexCount, ____] : geometries) {
        primitiveCounts_.push_back({
            .primitiveCount = indexCount / 3,
            .primitiveOffset = 0,
            .firstVertex = 0,
            .transformOffset = 0
        });
    }
    createBottomLevelAS(physicalDevice, commandPool, graphicsQueue, geometries);

    std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4>> instances = {
        {blas_.get(), glm::mat4(1.0f)}
    };
    createTopLevelAS(physicalDevice, commandPool, graphicsQueue, instances);

    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> dimensionBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> dimensionMemory(device_, VK_NULL_HANDLE, vkFreeMemory);
    if (!dimensionCache.empty()) {
        VkDeviceSize dimSize = static_cast<VkDeviceSize>(dimensionCache.size()) * sizeof(DimensionData);
        createBuffer(physicalDevice, dimSize,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, dimensionBuffer, dimensionMemory);

        VulkanResource<VkBuffer, PFN_vkDestroyBuffer> stagingBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
        VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> stagingMemory(device_, VK_NULL_HANDLE, vkFreeMemory);
        createBuffer(physicalDevice, dimSize,
                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingBuffer, stagingMemory);

        void* data;
        VK_CHECK(vkMapMemory(device_, stagingMemory.get(), 0, dimSize, 0, &data), "Dimension staging map failed.");
        std::memcpy(data, dimensionCache.data(), static_cast<size_t>(dimSize));
        vkUnmapMemory(device_, stagingMemory.get());

        VkCommandBuffer cmdBuffer = allocateTransientCommandBuffer(commandPool);
        VkCommandBufferBeginInfo beginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        };
        VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo), "Failed to begin command buffer");
        VkBufferCopy copyRegion{ .srcOffset = 0, .dstOffset = 0, .size = dimSize };
        vkCmdCopyBuffer(cmdBuffer, stagingBuffer.get(), dimensionBuffer.get(), 1, &copyRegion);
        VK_CHECK(vkEndCommandBuffer(cmdBuffer), "Failed to end command buffer");
        submitAndWaitTransient(cmdBuffer, graphicsQueue, commandPool);
        std::osyncstream(std::cout) << GREEN << "[INFO] Uploaded dimension cache with size=" << dimSize << " bytes" << RESET << std::endl;
    }

    updateDescriptors(nullptr, nullptr, dimensionBuffer.get());
    std::osyncstream(std::cout) << GREEN << "[INFO] RTX initialization completed successfully" << RESET << std::endl;
}

void VulkanRTX::updateRTX(
    VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
    const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
    const std::vector<DimensionData>& dimensionCache) {
    if (!physicalDevice || !commandPool || !graphicsQueue || geometries.empty()) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Invalid update params: null device/pool/queue or empty geometries" << RESET << std::endl;
        throw VulkanRTXException("Invalid update params: null device/pool/queue or empty geometries.");
    }

    std::osyncstream(std::cout) << GREEN << "[INFO] Starting RTX update with " << geometries.size() << " geometries" << RESET << std::endl;

    std::vector<VkAccelerationStructureBuildRangeInfoKHR> newPrimitiveCounts;
    newPrimitiveCounts.reserve(geometries.size());
    bool geometriesChanged = false;
    for (size_t i : std::views::iota(0u, geometries.size())) {
        const auto& [_, __, ___, indexCount, ____] = geometries[i];
        uint32_t newCount = indexCount / 3;
        newPrimitiveCounts.push_back({
            .primitiveCount = newCount,
            .primitiveOffset = 0,
            .firstVertex = 0,
            .transformOffset = 0
        });
        if (i < previousPrimitiveCounts_.size() && newCount != previousPrimitiveCounts_[i].primitiveCount) {
            geometriesChanged = true;
        }
    }
    primitiveCounts_ = std::move(newPrimitiveCounts);
    previousPrimitiveCounts_ = primitiveCounts_;

    if (geometriesChanged) {
        createBottomLevelAS(physicalDevice, commandPool, graphicsQueue, geometries);
        std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4>> instances = {
            {blas_.get(), glm::mat4(1.0f)}
        };
        createTopLevelAS(physicalDevice, commandPool, graphicsQueue, instances);
        std::osyncstream(std::cout) << YELLOW << "[WARNING] Geometries changed, rebuilt BLAS and TLAS" << RESET << std::endl;
    }

    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> dimensionBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> dimensionMemory(device_, VK_NULL_HANDLE, vkFreeMemory);
    if (!dimensionCache.empty() && dimensionCache != previousDimensionCache_) {
        VkDeviceSize dimSize = static_cast<VkDeviceSize>(dimensionCache.size()) * sizeof(DimensionData);
        createBuffer(physicalDevice, dimSize,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, dimensionBuffer, dimensionMemory);

        VulkanResource<VkBuffer, PFN_vkDestroyBuffer> stagingBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
        VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> stagingMemory(device_, VK_NULL_HANDLE, vkFreeMemory);
        createBuffer(physicalDevice, dimSize,
                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingBuffer, stagingMemory);

        void* data;
        VK_CHECK(vkMapMemory(device_, stagingMemory.get(), 0, dimSize, 0, &data), "Dimension staging map failed.");
        std::memcpy(data, dimensionCache.data(), static_cast<size_t>(dimSize));
        vkUnmapMemory(device_, stagingMemory.get());

        VkCommandBuffer cmdBuffer = allocateTransientCommandBuffer(commandPool);
        VkCommandBufferBeginInfo beginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        };
        VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo), "Failed to begin command buffer");
        VkBufferCopy copyRegion{ .srcOffset = 0, .dstOffset = 0, .size = dimSize };
        vkCmdCopyBuffer(cmdBuffer, stagingBuffer.get(), dimensionBuffer.get(), 1, &copyRegion);
        VK_CHECK(vkEndCommandBuffer(cmdBuffer), "Failed to end command buffer");
        submitAndWaitTransient(cmdBuffer, graphicsQueue, commandPool);
        previousDimensionCache_ = dimensionCache;
        std::osyncstream(std::cout) << GREEN << "[INFO] Updated dimension cache with size=" << dimSize << " bytes" << RESET << std::endl;
    }

    updateDescriptors(nullptr, nullptr, dimensionBuffer.get());
    std::osyncstream(std::cout) << GREEN << "[INFO] RTX update completed successfully" << RESET << std::endl;
}

void VulkanRTX::denoiseImage(VkCommandBuffer cmdBuffer, VkImage inputImage, VkImageView inputImageView, VkImage outputImage, VkImageView outputImageView) {
    if (!cmdBuffer || !inputImageView || !outputImageView) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Null denoise params: cmd buffer, input view, or output view" << RESET << std::endl;
        throw VulkanRTXException("Null denoise params: cmd buffer, input view, or output view.");
    }

    std::osyncstream(std::cout) << GREEN << "[INFO] Starting image denoising" << RESET << std::endl;

    VkShaderModule denoiseModule = createShaderModule("assets/shaders/denoise.spv");
    VkPipelineShaderStageCreateInfo stage{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = denoiseModule,
        .pName = "main"
    };

    VkPipelineLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = dsLayout_.getPtr()
    };
    VkPipelineLayout denoiseLayout;
    VK_CHECK(vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &denoiseLayout), "Denoise layout creation failed.");

    VkComputePipelineCreateInfo pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = stage,
        .layout = denoiseLayout
    };
    VkPipeline denoisePipeline;
    VK_CHECK(vkCreateComputePipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &denoisePipeline), "Denoise pipeline creation failed.");

    std::array imageInfos{
        VkDescriptorImageInfo{ .imageView = inputImageView, .imageLayout = VK_IMAGE_LAYOUT_GENERAL },
        VkDescriptorImageInfo{ .imageView = outputImageView, .imageLayout = VK_IMAGE_LAYOUT_GENERAL }
    };
    std::array writes{
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = ds_.get(),
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::StorageImage),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &imageInfos[0]
        },
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = ds_.get(),
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::DenoiseImage),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &imageInfos[1]
        }
    };
    vkUpdateDescriptorSets(device_, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

    std::array barriers{
        VkImageMemoryBarrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = inputImage,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        },
        VkImageMemoryBarrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = outputImage,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        }
    };
    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 2, barriers.data());

    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, denoisePipeline);
    VkDescriptorSet dsHandle = ds_.get();
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, denoiseLayout, 0, 1, &dsHandle, 0, nullptr);
    constexpr uint32_t WORKGROUP_SIZE = 16;
    vkCmdDispatch(cmdBuffer,
                  (extent_.width + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE,
                  (extent_.height + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE,
                  1);

    barriers[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barriers[0].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    barriers[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barriers[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barriers[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    barriers[1].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barriers[1].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 2, barriers.data());

    vkDestroyPipeline(device_, denoisePipeline, nullptr);
    vkDestroyPipelineLayout(device_, denoiseLayout, nullptr);
    vkDestroyShaderModule(device_, denoiseModule, nullptr);
    std::osyncstream(std::cout) << GREEN << "[INFO] Image denoising completed for extent=" << extent_.width << "x" << extent_.height << RESET << std::endl;
}