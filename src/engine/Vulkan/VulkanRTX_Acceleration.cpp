// VulkanRTX_Acceleration.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// Manages Vulkan ray-tracing acceleration structure compaction using KHR extensions.
// Dependencies: Vulkan 1.3+, VK_KHR_acceleration_structure, C++20 standard library, logging.hpp.
// Supported platforms: Windows, Linux (AMD, NVIDIA, Intel GPUs only).
// Zachary Geurts 2025

#include "engine/logging.hpp"
#include "engine/Vulkan/Vulkan_RTX.hpp"
#include <stdexcept>
#include <vector>
#include <source_location>
#include <string>

// Define VK_CHECK if not already defined, converting VkResult to int for formatting
#ifndef VK_CHECK
#define VK_CHECK(result, msg) do { if ((result) != VK_SUCCESS) { LOG_ERROR_CAT("Vulkan", "{} (VkResult: {})", std::source_location::current(), (msg), static_cast<int>(result)); throw VulkanRTXException((msg)); } } while (0)
#endif

namespace VulkanRTX {

void VulkanRTX::compactAccelerationStructures(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue) {
    if (!physicalDevice || !commandPool || !queue) {
        LOG_ERROR_CAT("Vulkan", "Invalid compact params: physicalDevice={:p}, commandPool={:p}, queue={:p}", 
                      std::source_location::current(), static_cast<void*>(physicalDevice), 
                      static_cast<void*>(commandPool), static_cast<void*>(queue));
        throw VulkanRTXException("Invalid compact params: null device, pool, or queue.");
    }

    if (!getSupportsCompaction()) {
        LOG_WARNING_CAT("Vulkan", "Acceleration structure compaction not supported on this device", std::source_location::current());
        return;
    }

    LOG_INFO_CAT("Vulkan", "Compacting acceleration structures", std::source_location::current());

    VkQueryPoolCreateInfo queryPoolInfo = {
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
        .queryCount = 2,
        .pipelineStatistics = 0
    };
    VkQueryPool queryPool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateQueryPool(device_, &queryPoolInfo, nullptr, &queryPool), 
             "Query pool creation failed.");
    LOG_DEBUG_CAT("Vulkan", "Created query pool: queryPool={:p}", 
                  std::source_location::current(), static_cast<void*>(queryPool));

    VkCommandBuffer commandBuffer = allocateTransientCommandBuffer(commandPool);
    LOG_DEBUG_CAT("Vulkan", "Allocated transient command buffer: commandBuffer={:p}", 
                  std::source_location::current(), static_cast<void*>(commandBuffer));

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo), 
             "Failed to begin command buffer.");
    LOG_DEBUG_CAT("Vulkan", "Began command buffer: commandBuffer={:p}", 
                  std::source_location::current(), static_cast<void*>(commandBuffer));

    std::vector<VkAccelerationStructureKHR> structures = { getBLAS(), getTLAS() };
    LOG_DEBUG_CAT("Vulkan", "Processing {} acceleration structures", 
                  std::source_location::current(), structures.size());

    vkCmdResetQueryPool(commandBuffer, queryPool, 0, 2);
    LOG_DEBUG_CAT("Vulkan", "Reset query pool: queryPool={:p}", 
                  std::source_location::current(), static_cast<void*>(queryPool));

    for (uint32_t i = 0; i < structures.size(); ++i) {
        if (structures[i]) {
            LOG_DEBUG_CAT("Vulkan", "Writing properties for acceleration structure {}: as={:p}", 
                          std::source_location::current(), i, static_cast<void*>(structures[i]));
            vkCmdWriteAccelerationStructuresPropertiesKHR(
                commandBuffer, 1, &structures[i], VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryPool, i);
        }
    }

    VK_CHECK(vkEndCommandBuffer(commandBuffer), "Failed to end command buffer.");
    LOG_DEBUG_CAT("Vulkan", "Ended command buffer: commandBuffer={:p}", 
                  std::source_location::current(), static_cast<void*>(commandBuffer));

    submitAndWaitTransient(commandBuffer, queue, commandPool);
    LOG_DEBUG_CAT("Vulkan", "Submitted and waited for command buffer: commandBuffer={:p}", 
                  std::source_location::current(), static_cast<void*>(commandBuffer));

    VkDeviceSize compactedSizes[2] = {0, 0};
    VK_CHECK(vkGetQueryPoolResults(device_, queryPool, 0, 2, sizeof(compactedSizes), compactedSizes, sizeof(VkDeviceSize),
                                   VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT),
             "Failed to get query pool results.");
    LOG_DEBUG_CAT("Vulkan", "Retrieved compacted sizes: BLAS={:d}, TLAS={:d}", 
                  std::source_location::current(), compactedSizes[0], compactedSizes[1]);

    // Initialize vectors with default-constructed VulkanResource objects
    std::vector<VulkanResource<VkAccelerationStructureKHR, PFN_vkDestroyAccelerationStructureKHR>> compactedAS;
    std::vector<VulkanResource<VkBuffer, PFN_vkDestroyBuffer>> compactedBuffers;
    std::vector<VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>> compactedMemories;
    compactedAS.reserve(2);
    compactedBuffers.reserve(2);
    compactedMemories.reserve(2);
    for (size_t i = 0; i < 2; ++i) {
        compactedAS.emplace_back(device_, VK_NULL_HANDLE, vkDestroyASFunc);
        compactedBuffers.emplace_back(device_, VK_NULL_HANDLE, vkDestroyBuffer);
        compactedMemories.emplace_back(device_, VK_NULL_HANDLE, vkFreeMemory);
    }

    for (uint32_t i = 0; i < structures.size(); ++i) {
        if (structures[i] && compactedSizes[i] > 0) {
            LOG_DEBUG_CAT("Vulkan", "Creating compacted acceleration structure {}: size={:d}", 
                          std::source_location::current(), i, compactedSizes[i]);

            createBuffer(physicalDevice, compactedSizes[i], 
                         VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                         compactedBuffers[i], compactedMemories[i]);

            VkAccelerationStructureCreateInfoKHR asCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
                .pNext = nullptr,
                .createFlags = 0, // No flags, as PREFER_FAST_TRACE is for build, not create
                .buffer = compactedBuffers[i].get(),
                .offset = 0,
                .size = compactedSizes[i],
                .type = (i == 0) ? VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR : VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
                .deviceAddress = 0
            };

            VK_CHECK(vkCreateASFunc(device_, &asCreateInfo, nullptr, compactedAS[i].getPtr()),
                     "Failed to create compacted acceleration structure.");
            LOG_DEBUG_CAT("Vulkan", "Created compacted acceleration structure {}: as={:p}", 
                          std::source_location::current(), i, static_cast<void*>(compactedAS[i].get()));

            VkCommandBuffer copyCommandBuffer = allocateTransientCommandBuffer(commandPool);
            VK_CHECK(vkBeginCommandBuffer(copyCommandBuffer, &beginInfo), 
                     "Failed to begin copy command buffer.");
            LOG_DEBUG_CAT("Vulkan", "Began copy command buffer: commandBuffer={:p}", 
                          std::source_location::current(), static_cast<void*>(copyCommandBuffer));

            VkCopyAccelerationStructureInfoKHR copyInfo = {
                .sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR,
                .pNext = nullptr,
                .src = structures[i],
                .dst = compactedAS[i].get(),
                .mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR
            };
            vkCmdCopyAccelerationStructureKHR(copyCommandBuffer, &copyInfo);
            LOG_DEBUG_CAT("Vulkan", "Issued copy command for acceleration structure {}", 
                          std::source_location::current(), i);

            VK_CHECK(vkEndCommandBuffer(copyCommandBuffer), "Failed to end copy command buffer.");
            LOG_DEBUG_CAT("Vulkan", "Ended copy command buffer: commandBuffer={:p}", 
                          std::source_location::current(), static_cast<void*>(copyCommandBuffer));

            submitAndWaitTransient(copyCommandBuffer, queue, commandPool);
            LOG_DEBUG_CAT("Vulkan", "Submitted and waited for copy command buffer: commandBuffer={:p}", 
                          std::source_location::current(), static_cast<void*>(copyCommandBuffer));
        }
    }

    // Update BLAS and TLAS with compacted versions
    if (compactedAS[0].get() != VK_NULL_HANDLE) {
        setBLAS(compactedAS[0].get());
        setBLASBuffer(compactedBuffers[0].get());
        setBLASMemory(compactedMemories[0].get());
        compactedAS[0] = VulkanResource<VkAccelerationStructureKHR, PFN_vkDestroyAccelerationStructureKHR>(device_, VK_NULL_HANDLE, vkDestroyASFunc);
        compactedBuffers[0] = VulkanResource<VkBuffer, PFN_vkDestroyBuffer>(device_, VK_NULL_HANDLE, vkDestroyBuffer);
        compactedMemories[0] = VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>(device_, VK_NULL_HANDLE, vkFreeMemory);
        LOG_INFO_CAT("Vulkan", "Updated BLAS with compacted version", std::source_location::current());
    }
    if (compactedAS[1].get() != VK_NULL_HANDLE) {
        setTLAS(compactedAS[1].get());
        setTLASBuffer(compactedBuffers[1].get());
        setTLASMemory(compactedMemories[1].get());
        compactedAS[1] = VulkanResource<VkAccelerationStructureKHR, PFN_vkDestroyAccelerationStructureKHR>(device_, VK_NULL_HANDLE, vkDestroyASFunc);
        compactedBuffers[1] = VulkanResource<VkBuffer, PFN_vkDestroyBuffer>(device_, VK_NULL_HANDLE, vkDestroyBuffer);
        compactedMemories[1] = VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>(device_, VK_NULL_HANDLE, vkFreeMemory);
        LOG_INFO_CAT("Vulkan", "Updated TLAS with compacted version", std::source_location::current());
        updateDescriptorSetForTLAS(getTLAS());
        LOG_DEBUG_CAT("Vulkan", "Updated descriptor set for compacted TLAS", std::source_location::current());
    }

    vkDestroyQueryPool(device_, queryPool, nullptr);
    LOG_DEBUG_CAT("Vulkan", "Destroyed query pool: queryPool={:p}", 
                  std::source_location::current(), static_cast<void*>(queryPool));

    LOG_INFO_CAT("Vulkan", "Completed acceleration structure compaction", std::source_location::current());
}

} // namespace VulkanRTX