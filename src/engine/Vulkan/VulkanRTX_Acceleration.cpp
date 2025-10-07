// VulkanRTX_Acceleration.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
#include "engine/Vulkan/Vulkan_RTX.hpp"
#include <stdexcept>
#include <format>
#include <syncstream>
#include <vector>
#include <iostream>
#include <cstring>

// Define color macros for logging
#define GREEN "\033[32m"
#define MAGENTA "\033[35m"
#define RESET "\033[0m"

namespace VulkanRTX {

void VulkanRTX::compactAccelerationStructures(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue) {
    if (!physicalDevice || !commandPool || !queue) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Invalid compact params: null device, pool, or queue" << RESET << std::endl;
        throw VulkanRTXException("Invalid compact params: null device, pool, or queue.");
    }

    std::osyncstream(std::cout) << GREEN << "[INFO] Compacting acceleration structures" << RESET << std::endl;

    VkQueryPoolCreateInfo queryPoolInfo = {
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
    VkCommandBufferBeginInfo beginInfo = {
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

    VkDeviceSize compactedSizes[2] = {0, 0};
    VK_CHECK(vkGetQueryPoolResults(device_, queryPool, 0, 2, sizeof(compactedSizes), compactedSizes, sizeof(VkDeviceSize), VK_QUERY_RESULT_64_BIT),
             "Failed to get query pool results.");

    for (uint32_t i = 0; i < structures.size(); ++i) {
        if (structures[i] && compactedSizes[i] > 0) {
            VulkanResource<VkBuffer, PFN_vkDestroyBuffer> newBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
            VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> newMemory(device_, VK_NULL_HANDLE, vkFreeMemory);
            createBuffer(physicalDevice, compactedSizes[i],
                         VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, newBuffer, newMemory);

            VkAccelerationStructureCreateInfoKHR createInfo = {
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
            VK_CHECK(vkCreateASFunc(device_, &createInfo, nullptr, &newAS), "Failed to create compacted acceleration structure.");

            commandBuffer = allocateTransientCommandBuffer(commandPool);
            VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo), "Failed to begin command buffer for copy.");

            VkCopyAccelerationStructureInfoKHR copyInfo = {
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
                blas_ = VulkanResource<VkAccelerationStructureKHR, PFN_vkDestroyAccelerationStructureKHR>(device_, newAS, vkDestroyASFunc);
            } else {
                tlasBuffer_ = std::move(newBuffer);
                tlasMemory_ = std::move(newMemory);
                tlas_ = VulkanResource<VkAccelerationStructureKHR, PFN_vkDestroyAccelerationStructureKHR>(device_, newAS, vkDestroyASFunc);
            }
        }
    }

    vkDestroyQueryPool(device_, queryPool, nullptr);
    std::osyncstream(std::cout) << GREEN << "[INFO] Compacted acceleration structures" << RESET << std::endl;
}

} // namespace VulkanRTX