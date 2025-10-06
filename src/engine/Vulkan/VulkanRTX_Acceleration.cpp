// AMOURANTH RTX Engine, October 2025 - Vulkan ray-tracing acceleration structure management.
// Dependencies: Vulkan 1.3+, GLM, C++20 standard library.
// Zachary Geurts 2025

#include "engine/Vulkan/Vulkan_RTX.hpp"
#include <stdexcept>
#include <format>
#include <syncstream>
#include <vector>
#include <iostream>
#include <cstring>

// ANSI color codes for consistent logging
#define RESET "\033[0m"
#define MAGENTA "\033[1;35m" // Bold magenta for errors
#define CYAN "\033[1;36m"    // Bold cyan for debug
#define YELLOW "\033[1;33m"  // Bold yellow for warnings
#define GREEN "\033[1;32m"   // Bold green for info

namespace std {
// Custom formatter for VkResult to enable std::format support
template <>
struct formatter<VkResult, char> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(VkResult value, FormatContext& ctx) const {
        std::string_view name;
        switch (value) {
            case VK_SUCCESS: name = "VK_SUCCESS"; break;
            case VK_NOT_READY: name = "VK_NOT_READY"; break;
            case VK_TIMEOUT: name = "VK_TIMEOUT"; break;
            case VK_EVENT_SET: name = "VK_EVENT_SET"; break;
            case VK_EVENT_RESET: name = "VK_EVENT_RESET"; break;
            case VK_INCOMPLETE: name = "VK_INCOMPLETE"; break;
            case VK_ERROR_OUT_OF_HOST_MEMORY: name = "VK_ERROR_OUT_OF_HOST_MEMORY"; break;
            case VK_ERROR_OUT_OF_DEVICE_MEMORY: name = "VK_ERROR_OUT_OF_DEVICE_MEMORY"; break;
            case VK_ERROR_INITIALIZATION_FAILED: name = "VK_ERROR_INITIALIZATION_FAILED"; break;
            case VK_ERROR_DEVICE_LOST: name = "VK_ERROR_DEVICE_LOST"; break;
            case VK_ERROR_MEMORY_MAP_FAILED: name = "VK_ERROR_MEMORY_MAP_FAILED"; break;
            case VK_ERROR_LAYER_NOT_PRESENT: name = "VK_ERROR_LAYER_NOT_PRESENT"; break;
            case VK_ERROR_EXTENSION_NOT_PRESENT: name = "VK_ERROR_EXTENSION_NOT_PRESENT"; break;
            case VK_ERROR_FEATURE_NOT_PRESENT: name = "VK_ERROR_FEATURE_NOT_PRESENT"; break;
            case VK_ERROR_INCOMPATIBLE_DRIVER: name = "VK_ERROR_INCOMPATIBLE_DRIVER"; break;
            case VK_ERROR_TOO_MANY_OBJECTS: name = "VK_ERROR_TOO_MANY_OBJECTS"; break;
            case VK_ERROR_FORMAT_NOT_SUPPORTED: name = "VK_ERROR_FORMAT_NOT_SUPPORTED"; break;
            default: name = "UNKNOWN_VK_RESULT"; break;
        }
        return format_to(ctx.out(), "{}", name);
    }
};
} // namespace std

namespace VulkanRTX {

void VulkanRTX::createBuffer(VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
                             VkMemoryPropertyFlags properties, VulkanResource<VkBuffer, PFN_vkDestroyBuffer>& buffer,
                             VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>& memory) {
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
    VkResult result = vkCreateBuffer(device_, &bufferInfo, nullptr, &vkBuffer);
    if (result != VK_SUCCESS) {
        auto error = std::format("vkCreateBuffer failed (Error code: {})", result);
        std::osyncstream(std::cout) << MAGENTA << "[ERROR] " << error << RESET << std::endl;
        throw std::runtime_error(error);
    }
    buffer = VulkanResource<VkBuffer, PFN_vkDestroyBuffer>(vkBuffer, device_, vkDestroyBuffer);

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device_, vkBuffer, &memRequirements);

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    uint32_t memoryTypeIndex = 0;
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((memRequirements.memoryTypeBits & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            memoryTypeIndex = i;
            break;
        }
    }

    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = memoryTypeIndex
    };
    VkDeviceMemory vkMemory;
    result = vkAllocateMemory(device_, &allocInfo, nullptr, &vkMemory);
    if (result != VK_SUCCESS) {
        auto error = std::format("vkAllocateMemory failed (Error code: {})", result);
        std::osyncstream(std::cout) << MAGENTA << "[ERROR] " << error << RESET << std::endl;
        throw std::runtime_error(error);
    }
    memory = VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>(vkMemory, device_, vkFreeMemory);

    result = vkBindBufferMemory(device_, vkBuffer, vkMemory, 0);
    if (result != VK_SUCCESS) {
        auto error = std::format("vkBindBufferMemory failed (Error code: {})", result);
        std::osyncstream(std::cout) << MAGENTA << "[ERROR] " << error << RESET << std::endl;
        throw std::runtime_error(error);
    }
}

void VulkanRTX::compactAccelerationStructures(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue) {
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
    VkResult result = vkCreateQueryPool(device_, &queryPoolInfo, nullptr, &queryPool);
    if (result != VK_SUCCESS) {
        auto error = std::format("vkCreateQueryPool failed (Error code: {})", result);
        std::osyncstream(std::cout) << MAGENTA << "[ERROR] " << error << RESET << std::endl;
        throw std::runtime_error(error);
    }

    VkCommandBuffer commandBuffer = allocateTransientCommandBuffer(commandPool);
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (result != VK_SUCCESS) {
        auto error = std::format("vkBeginCommandBuffer failed (Error code: {})", result);
        std::osyncstream(std::cout) << MAGENTA << "[ERROR] " << error << RESET << std::endl;
        throw std::runtime_error(error);
    }

    std::vector<VkAccelerationStructureKHR> structures = { blas_.get(), tlas_.get() };
    vkCmdResetQueryPool(commandBuffer, queryPool, 0, 2);
    for (uint32_t i = 0; i < structures.size(); ++i) {
        vkCmdWriteAccelerationStructuresPropertiesKHR(
            commandBuffer, 1, &structures[i], VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryPool, i);
    }

    result = vkEndCommandBuffer(commandBuffer);
    if (result != VK_SUCCESS) {
        auto error = std::format("vkEndCommandBuffer failed (Error code: {})", result);
        std::osyncstream(std::cout) << MAGENTA << "[ERROR] " << error << RESET << std::endl;
        throw std::runtime_error(error);
    }

    submitAndWaitTransient(commandBuffer, queue, commandPool);

    VkDeviceSize compactedSizes[2];
    result = vkGetQueryPoolResults(device_, queryPool, 0, 2, sizeof(compactedSizes), compactedSizes, sizeof(VkDeviceSize), VK_QUERY_RESULT_64_BIT);
    if (result != VK_SUCCESS) {
        auto error = std::format("vkGetQueryPoolResults failed (Error code: {})", result);
        std::osyncstream(std::cout) << MAGENTA << "[ERROR] " << error << RESET << std::endl;
        throw std::runtime_error(error);
    }

    for (uint32_t i = 0; i < structures.size(); ++i) {
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
        result = vkCreateAccelerationStructureKHR(device_, &createInfo, nullptr, &newAS);
        if (result != VK_SUCCESS) {
            auto error = std::format("vkCreateAccelerationStructureKHR failed (Error code: {})", result);
            std::osyncstream(std::cout) << MAGENTA << "[ERROR] " << error << RESET << std::endl;
            throw std::runtime_error(error);
        }

        commandBuffer = allocateTransientCommandBuffer(commandPool);
        beginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr
        };
        result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
        if (result != VK_SUCCESS) {
            auto error = std::format("vkBeginCommandBuffer failed (Error code: {})", result);
            std::osyncstream(std::cout) << MAGENTA << "[ERROR] " << error << RESET << std::endl;
            throw std::runtime_error(error);
        }

        VkCopyAccelerationStructureInfoKHR copyInfo{
            .sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR,
            .pNext = nullptr,
            .src = structures[i],
            .dst = newAS,
            .mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR
        };
        vkCmdCopyAccelerationStructureKHR(commandBuffer, &copyInfo);

        result = vkEndCommandBuffer(commandBuffer);
        if (result != VK_SUCCESS) {
            auto error = std::format("vkEndCommandBuffer failed (Error code: {})", result);
            std::osyncstream(std::cout) << MAGENTA << "[ERROR] " << error << RESET << std::endl;
            throw std::runtime_error(error);
        }

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

    vkDestroyQueryPool(device_, queryPool, nullptr);
}

void VulkanRTX::createBottomLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                                    const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Creating bottom-level acceleration structure" << RESET << std::endl;

    std::vector<VkAccelerationStructureGeometryKHR> asGeometries;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> buildRanges;
    for (const auto& [vertexBuffer, indexBuffer, vertexCount, indexCount, stride] : geometries) {
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
                    .maxVertex = vertexCount,
                    .indexType = VK_INDEX_TYPE_UINT32,
                    .indexData = { .deviceAddress = getBufferDeviceAddress(indexBuffer) },
                    .transformData = { .deviceAddress = 0 }
                }
            },
            .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
        };
        asGeometries.push_back(geometry);
        buildRanges.push_back({
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
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .srcAccelerationStructure = VK_NULL_HANDLE,
        .dstAccelerationStructure = VK_NULL_HANDLE,
        .geometryCount = static_cast<uint32_t>(asGeometries.size()),
        .pGeometries = asGeometries.data(),
        .ppGeometries = nullptr,
        .scratchData = { .deviceAddress = 0 }
    };

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
        .pNext = nullptr,
        .accelerationStructureSize = 0,
        .updateScratchSize = 0,
        .buildScratchSize = 0
    };
    std::vector<uint32_t> primitiveCounts(asGeometries.size());
    for (size_t i = 0; i < asGeometries.size(); ++i) {
        primitiveCounts[i] = buildRanges[i].primitiveCount;
    }
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
    VkResult result = vkCreateAccelerationStructureKHR(device_, &createInfo, nullptr, &blas);
    if (result != VK_SUCCESS) {
        auto error = std::format("vkCreateAccelerationStructureKHR failed (Error code: {})", result);
        std::osyncstream(std::cout) << MAGENTA << "[ERROR] " << error << RESET << std::endl;
        throw std::runtime_error(error);
    }
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
    result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (result != VK_SUCCESS) {
        auto error = std::format("vkBeginCommandBuffer failed (Error code: {})", result);
        std::osyncstream(std::cout) << MAGENTA << "[ERROR] " << error << RESET << std::endl;
        throw std::runtime_error(error);
    }

    std::vector<VkAccelerationStructureBuildRangeInfoKHR*> buildRangePtrs;
    for (auto& range : buildRanges) {
        buildRangePtrs.push_back(&range);
    }
    vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfo, buildRangePtrs.data());

    result = vkEndCommandBuffer(commandBuffer);
    if (result != VK_SUCCESS) {
        auto error = std::format("vkEndCommandBuffer failed (Error code: {})", result);
        std::osyncstream(std::cout) << MAGENTA << "[ERROR] " << error << RESET << std::endl;
        throw std::runtime_error(error);
    }

    submitAndWaitTransient(commandBuffer, queue, commandPool);
}

void VulkanRTX::createTopLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                                 const std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4>>& instances) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Creating top-level acceleration structure" << RESET << std::endl;

    std::vector<VkAccelerationStructureInstanceKHR> asInstances;
    for (const auto& [blas, transform] : instances) {
        VkTransformMatrixKHR transformMatrix;
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 4; ++j) {
                transformMatrix.matrix[i][j] = transform[j][i]; // GLM is column-major, Vulkan expects row-major
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
    createBuffer(physicalDevice, asInstances.size() * sizeof(VkAccelerationStructureInstanceKHR),
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, instanceBuffer, instanceMemory);

    void* mappedData;
    vkMapMemory(device_, instanceMemory.get(), 0, asInstances.size() * sizeof(VkAccelerationStructureInstanceKHR), 0, &mappedData);
    std::memcpy(mappedData, asInstances.data(), asInstances.size() * sizeof(VkAccelerationStructureInstanceKHR));
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
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR,
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
    VkResult result = vkCreateAccelerationStructureKHR(device_, &createInfo, nullptr, &tlas);
    if (result != VK_SUCCESS) {
        auto error = std::format("vkCreateAccelerationStructureKHR failed (Error code: {})", result);
        std::osyncstream(std::cout) << MAGENTA << "[ERROR] " << error << RESET << std::endl;
        throw std::runtime_error(error);
    }
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
    result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (result != VK_SUCCESS) {
        auto error = std::format("vkBeginCommandBuffer failed (Error code: {})", result);
        std::osyncstream(std::cout) << MAGENTA << "[ERROR] " << error << RESET << std::endl;
        throw std::runtime_error(error);
    }

    VkAccelerationStructureBuildRangeInfoKHR buildRange{
        .primitiveCount = primitiveCount,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0
    };
    VkAccelerationStructureBuildRangeInfoKHR* buildRangePtr = &buildRange;
    vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfo, &buildRangePtr);

    result = vkEndCommandBuffer(commandBuffer);
    if (result != VK_SUCCESS) {
        auto error = std::format("vkEndCommandBuffer failed (Error code: {})", result);
        std::osyncstream(std::cout) << MAGENTA << "[ERROR] " << error << RESET << std::endl;
        throw std::runtime_error(error);
    }

    submitAndWaitTransient(commandBuffer, queue, commandPool);
}

VkDeviceAddress VulkanRTX::getBufferDeviceAddress(VkBuffer buffer) const {
    VkBufferDeviceAddressInfo info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = nullptr,
        .buffer = buffer
    };
    return vkGetBufferDeviceAddress(device_, &info);
}

VkDeviceAddress VulkanRTX::getAccelerationStructureDeviceAddress(VkAccelerationStructureKHR as) const {
    VkAccelerationStructureDeviceAddressInfoKHR info{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .pNext = nullptr,
        .accelerationStructure = as
    };
    return vkGetAccelerationStructureDeviceAddressKHR(device_, &info);
}

VkCommandBuffer VulkanRTX::allocateTransientCommandBuffer(VkCommandPool commandPool) {
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VkCommandBuffer commandBuffer;
    VkResult result = vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer);
    if (result != VK_SUCCESS) {
        auto error = std::format("vkAllocateCommandBuffers failed (Error code: {})", result);
        std::osyncstream(std::cout) << MAGENTA << "[ERROR] " << error << RESET << std::endl;
        throw std::runtime_error(error);
    }
    return commandBuffer;
}

void VulkanRTX::submitAndWaitTransient(VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool commandPool) {
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
    VkResult result = vkCreateFence(device_, &fenceInfo, nullptr, &fence);
    if (result != VK_SUCCESS) {
        auto error = std::format("vkCreateFence failed (Error code: {})", result);
        std::osyncstream(std::cout) << MAGENTA << "[ERROR] " << error << RESET << std::endl;
        throw std::runtime_error(error);
    }

    result = vkQueueSubmit(queue, 1, &submitInfo, fence);
    if (result != VK_SUCCESS) {
        auto error = std::format("vkQueueSubmit failed (Error code: {})", result);
        std::osyncstream(std::cout) << MAGENTA << "[ERROR] " << error << RESET << std::endl;
        throw std::runtime_error(error);
    }

    result = vkWaitForFences(device_, 1, &fence, VK_TRUE, UINT64_MAX);
    if (result != VK_SUCCESS) {
        auto error = std::format("vkWaitForFences failed (Error code: {})", result);
        std::osyncstream(std::cout) << MAGENTA << "[ERROR] " << error << RESET << std::endl;
        throw std::runtime_error(error);
    }

    vkDestroyFence(device_, fence, nullptr);
    vkFreeCommandBuffers(device_, commandPool, 1, &commandBuffer);
}

void VulkanRTX::updateDescriptorSetForTLAS(VkAccelerationStructureKHR tlas) {
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
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
        .pImageInfo = nullptr,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr
    };

    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
}

} // namespace VulkanRTX