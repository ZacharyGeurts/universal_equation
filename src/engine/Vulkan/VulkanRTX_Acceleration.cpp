// VulkanRTX_Acceleration.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
#include "engine/Vulkan/Vulkan_RTX.hpp"
#include <stdexcept>
#include <format>
#include <syncstream>
#include <vector>
#include <iostream>
#include <cstring>
#include <dlfcn.h>

// Define color macros for logging
#define GREEN "\033[32m"
#define MAGENTA "\033[35m"
#define RESET "\033[0m"

namespace VulkanRTX {

VulkanRTX::VulkanRTX(VkDevice device, const std::vector<std::string>& shaderPaths)
    : device_(device),
      shaderPaths_(shaderPaths),
      dsLayout_(device, VK_NULL_HANDLE, vkDestroyDescriptorSetLayout),
      dsPool_(device, VK_NULL_HANDLE, vkDestroyDescriptorPool),
      ds_(device, VK_NULL_HANDLE, VK_NULL_HANDLE, vkFreeDescriptorSets),
      rtPipelineLayout_(device, VK_NULL_HANDLE, vkDestroyPipelineLayout),
      rtPipeline_(device, VK_NULL_HANDLE, vkDestroyPipeline),
      blasBuffer_(device, VK_NULL_HANDLE, vkDestroyBuffer),
      blasMemory_(device, VK_NULL_HANDLE, vkFreeMemory),
      tlasBuffer_(device, VK_NULL_HANDLE, vkDestroyBuffer),
      tlasMemory_(device, VK_NULL_HANDLE, vkFreeMemory),
      blas_(device, VK_NULL_HANDLE, vkDestroyASFunc),
      tlas_(device, VK_NULL_HANDLE, vkDestroyASFunc),
      extent_{0, 0},
      primitiveCounts_(),
      previousPrimitiveCounts_(),
      previousDimensionCache_(),
      supportsCompaction_(false),
      shaderFeatures_(ShaderFeatures::None),
      sbt_(device, this) {
    if (!device) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Null Vulkan device provided" << RESET << std::endl;
        throw VulkanRTXException("Null Vulkan device provided.");
    }

    std::osyncstream(std::cout) << GREEN << std::format("[INFO] Starting VulkanRTX initialization with {} shader paths", shaderPaths.size()) << RESET << std::endl;

    // Load Vulkan function pointers
    std::lock_guard<std::mutex> lock(functionPtrMutex_);
    vkGetDeviceProcAddrFunc = reinterpret_cast<PFN_vkGetDeviceProcAddr>(dlsym(dlopen(nullptr, RTLD_LAZY), "vkGetDeviceProcAddr"));
    if (!vkGetDeviceProcAddrFunc) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to load vkGetDeviceProcAddr" << RESET << std::endl;
        throw VulkanRTXException("Failed to load vkGetDeviceProcAddr.");
    }

    vkGetBufferDeviceAddressFunc = reinterpret_cast<PFN_vkGetBufferDeviceAddress>(
        vkGetDeviceProcAddrFunc(device_, "vkGetBufferDeviceAddress"));
    vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(
        vkGetDeviceProcAddrFunc(device_, "vkCmdTraceRaysKHR"));
    vkCreateASFunc = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
        vkGetDeviceProcAddrFunc(device_, "vkCreateAccelerationStructureKHR"));
    vkDestroyASFunc = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
        vkGetDeviceProcAddrFunc(device_, "vkDestroyAccelerationStructureKHR"));
    vkGetASBuildSizesFunc = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
        vkGetDeviceProcAddrFunc(device_, "vkGetAccelerationStructureBuildSizesKHR"));
    vkCmdBuildASFunc = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(
        vkGetDeviceProcAddrFunc(device_, "vkCmdBuildAccelerationStructuresKHR"));
    vkGetASDeviceAddressFunc = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
        vkGetDeviceProcAddrFunc(device_, "vkGetAccelerationStructureDeviceAddressKHR"));
    vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(
        vkGetDeviceProcAddrFunc(device_, "vkCreateRayTracingPipelinesKHR"));
    vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(
        vkGetDeviceProcAddrFunc(device_, "vkGetRayTracingShaderGroupHandlesKHR"));
    vkCmdCopyAccelerationStructureKHR = reinterpret_cast<PFN_vkCmdCopyAccelerationStructureKHR>(
        vkGetDeviceProcAddrFunc(device_, "vkCmdCopyAccelerationStructureKHR"));
    vkCreateDescriptorSetLayoutFunc = reinterpret_cast<PFN_vkCreateDescriptorSetLayout>(
        vkGetDeviceProcAddrFunc(device_, "vkCreateDescriptorSetLayout"));
    vkAllocateDescriptorSetsFunc = reinterpret_cast<PFN_vkAllocateDescriptorSets>(
        vkGetDeviceProcAddrFunc(device_, "vkAllocateDescriptorSets"));
    vkCreateDescriptorPoolFunc = reinterpret_cast<PFN_vkCreateDescriptorPool>(
        vkGetDeviceProcAddrFunc(device_, "vkCreateDescriptorPool"));
    vkGetPhysicalDeviceProperties2Func = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
        vkGetDeviceProcAddrFunc(device_, "vkGetPhysicalDeviceProperties2"));
    vkCreateShaderModuleFunc = reinterpret_cast<PFN_vkCreateShaderModule>(
        vkGetDeviceProcAddrFunc(device_, "vkCreateShaderModule"));
    vkDestroyDescriptorSetLayout = reinterpret_cast<PFN_vkDestroyDescriptorSetLayout>(
        vkGetDeviceProcAddrFunc(device_, "vkDestroyDescriptorSetLayout"));
    vkDestroyDescriptorPool = reinterpret_cast<PFN_vkDestroyDescriptorPool>(
        vkGetDeviceProcAddrFunc(device_, "vkDestroyDescriptorPool"));
    vkFreeDescriptorSets = reinterpret_cast<PFN_vkFreeDescriptorSets>(
        vkGetDeviceProcAddrFunc(device_, "vkFreeDescriptorSets"));
    vkDestroyPipelineLayout = reinterpret_cast<PFN_vkDestroyPipelineLayout>(
        vkGetDeviceProcAddrFunc(device_, "vkDestroyPipelineLayout"));
    vkDestroyPipeline = reinterpret_cast<PFN_vkDestroyPipeline>(
        vkGetDeviceProcAddrFunc(device_, "vkDestroyPipeline"));
    vkDestroyBuffer = reinterpret_cast<PFN_vkDestroyBuffer>(
        vkGetDeviceProcAddrFunc(device_, "vkDestroyBuffer"));
    vkFreeMemory = reinterpret_cast<PFN_vkFreeMemory>(
        vkGetDeviceProcAddrFunc(device_, "vkFreeMemory"));
    vkCreateQueryPool = reinterpret_cast<PFN_vkCreateQueryPool>(
        vkGetDeviceProcAddrFunc(device_, "vkCreateQueryPool"));
    vkDestroyQueryPool = reinterpret_cast<PFN_vkDestroyQueryPool>(
        vkGetDeviceProcAddrFunc(device_, "vkDestroyQueryPool"));
    vkGetQueryPoolResults = reinterpret_cast<PFN_vkGetQueryPoolResults>(
        vkGetDeviceProcAddrFunc(device_, "vkGetQueryPoolResults"));
    vkCmdWriteAccelerationStructuresPropertiesKHR = reinterpret_cast<PFN_vkCmdWriteAccelerationStructuresPropertiesKHR>(
        vkGetDeviceProcAddrFunc(device_, "vkCmdWriteAccelerationStructuresPropertiesKHR"));
    vkCreateBuffer = reinterpret_cast<PFN_vkCreateBuffer>(
        vkGetDeviceProcAddrFunc(device_, "vkCreateBuffer"));
    vkAllocateMemory = reinterpret_cast<PFN_vkAllocateMemory>(
        vkGetDeviceProcAddrFunc(device_, "vkAllocateMemory"));
    vkBindBufferMemory = reinterpret_cast<PFN_vkBindBufferMemory>(
        vkGetDeviceProcAddrFunc(device_, "vkBindBufferMemory"));
    vkGetPhysicalDeviceMemoryProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
        vkGetDeviceProcAddrFunc(device_, "vkGetPhysicalDeviceMemoryProperties"));
    vkBeginCommandBuffer = reinterpret_cast<PFN_vkBeginCommandBuffer>(
        vkGetDeviceProcAddrFunc(device_, "vkBeginCommandBuffer"));
    vkEndCommandBuffer = reinterpret_cast<PFN_vkEndCommandBuffer>(
        vkGetDeviceProcAddrFunc(device_, "vkEndCommandBuffer"));
    vkAllocateCommandBuffers = reinterpret_cast<PFN_vkAllocateCommandBuffers>(
        vkGetDeviceProcAddrFunc(device_, "vkAllocateCommandBuffers"));
    vkQueueSubmit = reinterpret_cast<PFN_vkQueueSubmit>(
        vkGetDeviceProcAddrFunc(device_, "vkQueueSubmit"));
    vkQueueWaitIdle = reinterpret_cast<PFN_vkQueueWaitIdle>(
        vkGetDeviceProcAddrFunc(device_, "vkQueueWaitIdle"));
    vkFreeCommandBuffers = reinterpret_cast<PFN_vkFreeCommandBuffers>(
        vkGetDeviceProcAddrFunc(device_, "vkFreeCommandBuffers"));
    vkCmdResetQueryPool = reinterpret_cast<PFN_vkCmdResetQueryPool>(
        vkGetDeviceProcAddrFunc(device_, "vkCmdResetQueryPool"));
    vkGetBufferMemoryRequirements = reinterpret_cast<PFN_vkGetBufferMemoryRequirements>(
        vkGetDeviceProcAddrFunc(device_, "vkGetBufferMemoryRequirements"));
    vkMapMemory = reinterpret_cast<PFN_vkMapMemory>(
        vkGetDeviceProcAddrFunc(device_, "vkMapMemory"));
    vkUnmapMemory = reinterpret_cast<PFN_vkUnmapMemory>(
        vkGetDeviceProcAddrFunc(device_, "vkUnmapMemory"));
    vkCreateImage = reinterpret_cast<PFN_vkCreateImage>(
        vkGetDeviceProcAddrFunc(device_, "vkCreateImage"));
    vkDestroyImage = reinterpret_cast<PFN_vkDestroyImage>(
        vkGetDeviceProcAddrFunc(device_, "vkDestroyImage"));
    vkGetImageMemoryRequirements = reinterpret_cast<PFN_vkGetImageMemoryRequirements>(
        vkGetDeviceProcAddrFunc(device_, "vkGetImageMemoryRequirements"));
    vkBindImageMemory = reinterpret_cast<PFN_vkBindImageMemory>(
        vkGetDeviceProcAddrFunc(device_, "vkBindImageMemory"));
    vkCreateImageView = reinterpret_cast<PFN_vkCreateImageView>(
        vkGetDeviceProcAddrFunc(device_, "vkCreateImageView"));
    vkDestroyImageView = reinterpret_cast<PFN_vkDestroyImageView>(
        vkGetDeviceProcAddrFunc(device_, "vkDestroyImageView"));
    vkUpdateDescriptorSets = reinterpret_cast<PFN_vkUpdateDescriptorSets>(
        vkGetDeviceProcAddrFunc(device_, "vkUpdateDescriptorSets"));
    vkCmdPipelineBarrier = reinterpret_cast<PFN_vkCmdPipelineBarrier>(
        vkGetDeviceProcAddrFunc(device_, "vkCmdPipelineBarrier"));
    vkCmdBindPipeline = reinterpret_cast<PFN_vkCmdBindPipeline>(
        vkGetDeviceProcAddrFunc(device_, "vkCmdBindPipeline"));
    vkCmdBindDescriptorSets = reinterpret_cast<PFN_vkCmdBindDescriptorSets>(
        vkGetDeviceProcAddrFunc(device_, "vkCmdBindDescriptorSets"));
    vkCmdPushConstants = reinterpret_cast<PFN_vkCmdPushConstants>(
        vkGetDeviceProcAddrFunc(device_, "vkCmdPushConstants"));
    vkCmdCopyBuffer = reinterpret_cast<PFN_vkCmdCopyBuffer>(
        vkGetDeviceProcAddrFunc(device_, "vkCmdCopyBuffer"));
    vkCreatePipelineLayout = reinterpret_cast<PFN_vkCreatePipelineLayout>(
        vkGetDeviceProcAddrFunc(device_, "vkCreatePipelineLayout"));
    vkCreateComputePipelines = reinterpret_cast<PFN_vkCreateComputePipelines>(
        vkGetDeviceProcAddrFunc(device_, "vkCreateComputePipelines"));
    vkCmdDispatch = reinterpret_cast<PFN_vkCmdDispatch>(
        vkGetDeviceProcAddrFunc(device_, "vkCmdDispatch"));
    vkDestroyShaderModule = reinterpret_cast<PFN_vkDestroyShaderModule>(
        vkGetDeviceProcAddrFunc(device_, "vkDestroyShaderModule"));

    if (!vkGetBufferDeviceAddressFunc || !vkCmdTraceRaysKHR || !vkCreateASFunc ||
        !vkDestroyASFunc || !vkGetASBuildSizesFunc || !vkCmdBuildASFunc ||
        !vkGetASDeviceAddressFunc || !vkCreateRayTracingPipelinesKHR ||
        !vkGetRayTracingShaderGroupHandlesKHR) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Device lacks required ray tracing extensions" << RESET << std::endl;
        throw VulkanRTXException("Device lacks required ray tracing extensions (Vulkan 1.2+ with VK_KHR_ray_tracing_pipeline).");
    }
    supportsCompaction_ = (vkCmdCopyAccelerationStructureKHR != nullptr);

    std::osyncstream(std::cout) << GREEN << std::format("[INFO] VulkanRTX initialized successfully, supportsCompaction={}", supportsCompaction_) << RESET << std::endl;
}

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