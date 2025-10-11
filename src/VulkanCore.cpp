// VulkanCore.cpp
// AMOURANTH RTX Engine, October 2025 - Implementation of core Vulkan utilities.
// Dependencies: Vulkan 1.3+, GLM, logging.hpp, ue_init.hpp.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#include "VulkanCore.hpp"
#include "engine/logging.hpp"
#include "ue_init.hpp"
#include <stdexcept>
#include <source_location>
#include <fstream>
#include <algorithm>

namespace VulkanInitializer {
    void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size,
                     VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                     VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
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
        if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            LOG_ERROR("Failed to create buffer size: {}", std::source_location::current(), size);
            throw std::runtime_error("Failed to create buffer");
        }
        LOG_INFO("Created buffer: {:p}", std::source_location::current(), static_cast<void*>(buffer));
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, buffer, &memRequirements);
        VkMemoryAllocateFlagsInfo allocFlagsInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
            .pNext = nullptr,
            .flags = static_cast<VkMemoryAllocateFlags>((usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) ? VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR : 0),
            .deviceMask = 0
        };
        VkMemoryAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) ? &allocFlagsInfo : nullptr,
            .allocationSize = memRequirements.size,
            .memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties)
        };
        if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            LOG_ERROR("Failed to allocate buffer memory size: {}", std::source_location::current(), memRequirements.size);
            throw std::runtime_error("Failed to allocate buffer memory");
        }
        LOG_INFO("Allocated buffer memory: {:p}", std::source_location::current(), static_cast<void*>(bufferMemory));
        vkBindBufferMemory(device, buffer, bufferMemory, 0);
    }

    VkDeviceAddress getBufferDeviceAddress(VkDevice device, VkBuffer buffer) {
        auto vkGetBufferDeviceAddress = (PFN_vkGetBufferDeviceAddress)vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddress");
        if (!vkGetBufferDeviceAddress) {
            LOG_ERROR("Failed to load vkGetBufferDeviceAddress", std::source_location::current());
            throw std::runtime_error("Failed to load vkGetBufferDeviceAddress");
        }
        VkBufferDeviceAddressInfo addressInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .pNext = nullptr,
            .buffer = buffer
        };
        return vkGetBufferDeviceAddress(device, &addressInfo);
    }

    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
        LOG_ERROR("Failed to find suitable memory type", std::source_location::current());
        throw std::runtime_error("Failed to find suitable memory type");
    }

    void createAccelerationStructures(VulkanContext& context, std::span<const glm::vec3> vertices, std::span<const uint32_t> indices) {
        auto vkCreateAccelerationStructureKHR = (PFN_vkCreateAccelerationStructureKHR)vkGetDeviceProcAddr(context.device, "vkCreateAccelerationStructureKHR");
        auto vkCmdBuildAccelerationStructuresKHR = (PFN_vkCmdBuildAccelerationStructuresKHR)vkGetDeviceProcAddr(context.device, "vkCmdBuildAccelerationStructuresKHR");
        auto vkGetAccelerationStructureBuildSizesKHR = (PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetDeviceProcAddr(context.device, "vkGetAccelerationStructureBuildSizesKHR");
        if (!vkCreateAccelerationStructureKHR || !vkCmdBuildAccelerationStructuresKHR || !vkGetAccelerationStructureBuildSizesKHR) {
            LOG_ERROR("Failed to load acceleration structure functions", std::source_location::current());
            throw std::runtime_error("Failed to load acceleration structure functions");
        }

        // Create vertex buffer
        createBuffer(context.device, context.physicalDevice, vertices.size_bytes(),
                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     context.vertexBuffer_, context.vertexBufferMemory);
        void* vertexData;
        vkMapMemory(context.device, context.vertexBufferMemory, 0, vertices.size_bytes(), 0, &vertexData);
        memcpy(vertexData, vertices.data(), vertices.size_bytes());
        vkUnmapMemory(context.device, context.vertexBufferMemory);

        // Create index buffer
        createBuffer(context.device, context.physicalDevice, indices.size_bytes(),
                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     context.indexBuffer_, context.indexBufferMemory);
        void* indexData;
        vkMapMemory(context.device, context.indexBufferMemory, 0, indices.size_bytes(), 0, &indexData);
        memcpy(indexData, indices.data(), indices.size_bytes());
        vkUnmapMemory(context.device, context.indexBufferMemory);

        // Bottom-level acceleration structure
        VkAccelerationStructureGeometryTrianglesDataKHR triangleData{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
            .pNext = nullptr,
            .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
            .vertexData = {getBufferDeviceAddress(context.device, context.vertexBuffer_)},
            .vertexStride = sizeof(glm::vec3),
            .maxVertex = static_cast<uint32_t>(vertices.size()),
            .indexType = VK_INDEX_TYPE_UINT32,
            .indexData = {getBufferDeviceAddress(context.device, context.indexBuffer_)},
            .transformData = {0}
        };
        VkAccelerationStructureGeometryKHR geometry{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .pNext = nullptr,
            .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
            .geometry = {.triangles = triangleData},
            .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
        };
        VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
            .pNext = nullptr,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
            .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
            .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
            .srcAccelerationStructure = VK_NULL_HANDLE,
            .dstAccelerationStructure = VK_NULL_HANDLE,
            .geometryCount = 1,
            .pGeometries = &geometry,
            .ppGeometries = nullptr,
            .scratchData = {0}
        };
        const uint32_t primitiveCount = static_cast<uint32_t>(indices.size() / 3);
        VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
            .pNext = nullptr,
            .accelerationStructureSize = 0,
            .updateScratchSize = 0,
            .buildScratchSize = 0
        };
        vkGetAccelerationStructureBuildSizesKHR(context.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildGeometryInfo, &primitiveCount, &buildSizesInfo);

        // Create bottom-level AS buffer
        createBuffer(context.device, context.physicalDevice, buildSizesInfo.accelerationStructureSize,
                     VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, context.bottomLevelASBuffer, context.bottomLevelASBufferMemory);
        VkAccelerationStructureCreateInfoKHR createInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .pNext = nullptr,
            .createFlags = 0,
            .buffer = context.bottomLevelASBuffer,
            .offset = 0,
            .size = buildSizesInfo.accelerationStructureSize,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
            .deviceAddress = 0
        };
        if (vkCreateAccelerationStructureKHR(context.device, &createInfo, nullptr, &context.bottomLevelAS) != VK_SUCCESS) {
            LOG_ERROR("Failed to create bottom-level AS", std::source_location::current());
            throw std::runtime_error("Failed to create bottom-level AS");
        }
        LOG_INFO("Created bottom-level AS: {:p}", std::source_location::current(), static_cast<void*>(context.bottomLevelAS));

        // Create scratch buffer for BLAS
        VkBuffer bottomLevelScratchBuffer;
        VkDeviceMemory bottomLevelScratchBufferMemory;
        createBuffer(context.device, context.physicalDevice, buildSizesInfo.buildScratchSize,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, bottomLevelScratchBuffer, bottomLevelScratchBufferMemory);
        buildGeometryInfo.dstAccelerationStructure = context.bottomLevelAS;
        buildGeometryInfo.scratchData.deviceAddress = getBufferDeviceAddress(context.device, bottomLevelScratchBuffer);
        VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{
            .primitiveCount = primitiveCount,
            .primitiveOffset = 0,
            .firstVertex = 0,
            .transformOffset = 0
        };
        VkAccelerationStructureBuildRangeInfoKHR* buildRangeInfos[] = {&buildRangeInfo};
        VkCommandBuffer commandBuffer;
        VkCommandBufferAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = context.commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };
        if (vkAllocateCommandBuffers(context.device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
            LOG_ERROR("Failed to allocate BLAS command buffer", std::source_location::current());
            throw std::runtime_error("Failed to allocate BLAS command buffer");
        }
        VkCommandBufferBeginInfo beginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr
        };
        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            LOG_ERROR("Failed to begin BLAS command buffer", std::source_location::current());
            throw std::runtime_error("Failed to begin BLAS command buffer");
        }
        vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildGeometryInfo, buildRangeInfos);
        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            LOG_ERROR("Failed to end BLAS command buffer", std::source_location::current());
            throw std::runtime_error("Failed to end BLAS command buffer");
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
        if (vkQueueSubmit(context.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
            LOG_ERROR("Failed to submit BLAS command buffer", std::source_location::current());
            throw std::runtime_error("Failed to submit BLAS command buffer");
        }
        vkQueueWaitIdle(context.graphicsQueue);
        vkFreeCommandBuffers(context.device, context.commandPool, 1, &commandBuffer);
        vkDestroyBuffer(context.device, bottomLevelScratchBuffer, nullptr);
        vkFreeMemory(context.device, bottomLevelScratchBufferMemory, nullptr);

        // Top-level acceleration structure
        VkAccelerationStructureInstanceKHR instance{
            .transform = {
                .matrix = {
                    {1.0f, 0.0f, 0.0f, 0.0f},
                    {0.0f, 1.0f, 0.0f, 0.0f},
                    {0.0f, 0.0f, 1.0f, 0.0f}
                }
            },
            .instanceCustomIndex = 0,
            .mask = 0xFF,
            .instanceShaderBindingTableRecordOffset = 0,
            .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
            .accelerationStructureReference = getBufferDeviceAddress(context.device, context.bottomLevelASBuffer)
        };
        VkBuffer instanceBuffer;
        VkDeviceMemory instanceBufferMemory;
        createBuffer(context.device, context.physicalDevice, sizeof(VkAccelerationStructureInstanceKHR),
                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     instanceBuffer, instanceBufferMemory);
        void* instanceData;
        vkMapMemory(context.device, instanceBufferMemory, 0, sizeof(VkAccelerationStructureInstanceKHR), 0, &instanceData);
        memcpy(instanceData, &instance, sizeof(VkAccelerationStructureInstanceKHR));
        vkUnmapMemory(context.device, instanceBufferMemory);

        VkAccelerationStructureGeometryInstancesDataKHR instancesData{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
            .pNext = nullptr,
            .arrayOfPointers = VK_FALSE,
            .data = {getBufferDeviceAddress(context.device, instanceBuffer)}
        };
        VkAccelerationStructureGeometryKHR topLevelGeometry{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .pNext = nullptr,
            .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
            .geometry = {.instances = instancesData},
            .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
        };
        VkAccelerationStructureBuildGeometryInfoKHR topLevelBuildInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
            .pNext = nullptr,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
            .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
            .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
            .srcAccelerationStructure = VK_NULL_HANDLE,
            .dstAccelerationStructure = VK_NULL_HANDLE,
            .geometryCount = 1,
            .pGeometries = &topLevelGeometry,
            .ppGeometries = nullptr,
            .scratchData = {0}
        };
        const uint32_t topLevelPrimitiveCount = 1;
        vkGetAccelerationStructureBuildSizesKHR(context.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &topLevelBuildInfo, &topLevelPrimitiveCount, &buildSizesInfo);

        // Create top-level AS buffer
        createBuffer(context.device, context.physicalDevice, buildSizesInfo.accelerationStructureSize,
                     VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, context.topLevelASBuffer, context.topLevelASBufferMemory);
        createInfo.buffer = context.topLevelASBuffer;
        createInfo.size = buildSizesInfo.accelerationStructureSize;
        createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        if (vkCreateAccelerationStructureKHR(context.device, &createInfo, nullptr, &context.topLevelAS) != VK_SUCCESS) {
            LOG_ERROR("Failed to create top-level AS", std::source_location::current());
            throw std::runtime_error("Failed to create top-level AS");
        }
        LOG_INFO("Created top-level AS: {:p}", std::source_location::current(), static_cast<void*>(context.topLevelAS));

        // Create scratch buffer for TLAS
        VkBuffer topLevelScratchBuffer;
        VkDeviceMemory topLevelScratchBufferMemory;
        createBuffer(context.device, context.physicalDevice, buildSizesInfo.buildScratchSize,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, topLevelScratchBuffer, topLevelScratchBufferMemory);
        topLevelBuildInfo.dstAccelerationStructure = context.topLevelAS;
        topLevelBuildInfo.scratchData.deviceAddress = getBufferDeviceAddress(context.device, topLevelScratchBuffer);
        VkAccelerationStructureBuildRangeInfoKHR topLevelBuildRangeInfo{
            .primitiveCount = topLevelPrimitiveCount,
            .primitiveOffset = 0,
            .firstVertex = 0,
            .transformOffset = 0
        };
        VkAccelerationStructureBuildRangeInfoKHR* topLevelBuildRangeInfos[] = {&topLevelBuildRangeInfo};
        VkCommandBuffer topLevelCommandBuffer;
        VkCommandBufferAllocateInfo topLevelAllocInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = context.commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };
        if (vkAllocateCommandBuffers(context.device, &topLevelAllocInfo, &topLevelCommandBuffer) != VK_SUCCESS) {
            LOG_ERROR("Failed to allocate TLAS command buffer", std::source_location::current());
            throw std::runtime_error("Failed to allocate TLAS command buffer");
        }
        VkCommandBufferBeginInfo topLevelBeginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr
        };
        if (vkBeginCommandBuffer(topLevelCommandBuffer, &topLevelBeginInfo) != VK_SUCCESS) {
            LOG_ERROR("Failed to begin TLAS command buffer", std::source_location::current());
            throw std::runtime_error("Failed to begin TLAS command buffer");
        }
        vkCmdBuildAccelerationStructuresKHR(topLevelCommandBuffer, 1, &topLevelBuildInfo, topLevelBuildRangeInfos);
        if (vkEndCommandBuffer(topLevelCommandBuffer) != VK_SUCCESS) {
            LOG_ERROR("Failed to end TLAS command buffer", std::source_location::current());
            throw std::runtime_error("Failed to end TLAS command buffer");
        }
        VkSubmitInfo topLevelSubmitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = nullptr,
            .pWaitDstStageMask = nullptr,
            .commandBufferCount = 1,
            .pCommandBuffers = &topLevelCommandBuffer,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores = nullptr
        };
        if (vkQueueSubmit(context.graphicsQueue, 1, &topLevelSubmitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
            LOG_ERROR("Failed to submit TLAS command buffer", std::source_location::current());
            throw std::runtime_error("Failed to submit TLAS command buffer");
        }
        vkQueueWaitIdle(context.graphicsQueue);
        vkFreeCommandBuffers(context.device, context.commandPool, 1, &topLevelCommandBuffer);
        vkDestroyBuffer(context.device, topLevelScratchBuffer, nullptr);
        vkFreeMemory(context.device, topLevelScratchBufferMemory, nullptr);
        vkDestroyBuffer(context.device, instanceBuffer, nullptr);
        vkFreeMemory(context.device, instanceBufferMemory, nullptr);
    }

    void createRayTracingPipeline(VulkanContext& context) {
        auto vkCreateRayTracingPipelinesKHR = (PFN_vkCreateRayTracingPipelinesKHR)vkGetDeviceProcAddr(context.device, "vkCreateRayTracingPipelinesKHR");
        if (!vkCreateRayTracingPipelinesKHR) {
            LOG_ERROR("Failed to load vkCreateRayTracingPipelinesKHR", std::source_location::current());
            throw std::runtime_error("Failed to load vkCreateRayTracingPipelinesKHR");
        }
        auto loadShader = [&](const std::string& filepath) -> VkShaderModule {
            std::ifstream file(filepath, std::ios::ate | std::ios::binary);
            if (!file.is_open()) {
                LOG_ERROR("Failed to open shader file: {}", std::source_location::current(), filepath);
                throw std::runtime_error("Failed to open shader file");
            }
            size_t fileSize = static_cast<size_t>(file.tellg());
            std::vector<char> buffer(fileSize);
            file.seekg(0);
            file.read(buffer.data(), fileSize);
            file.close();
            VkShaderModuleCreateInfo createInfo{
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .codeSize = buffer.size(),
                .pCode = reinterpret_cast<const uint32_t*>(buffer.data())
            };
            VkShaderModule shaderModule;
            if (vkCreateShaderModule(context.device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
                LOG_ERROR("Failed to create shader module: {}", std::source_location::current(), filepath);
                throw std::runtime_error("Failed to create shader module");
            }
            return shaderModule;
        };
        VkShaderModule raygenShader = loadShader("shaders/raygen.spv");
        VkShaderModule missShader = loadShader("shaders/miss.spv");
        VkShaderModule closestHitShader = loadShader("shaders/closesthit.spv");
        if (raygenShader == VK_NULL_HANDLE || missShader == VK_NULL_HANDLE || closestHitShader == VK_NULL_HANDLE) {
            LOG_ERROR("Failed to load ray-tracing shader modules", std::source_location::current());
            if (raygenShader != VK_NULL_HANDLE) vkDestroyShaderModule(context.device, raygenShader, nullptr);
            if (missShader != VK_NULL_HANDLE) vkDestroyShaderModule(context.device, missShader, nullptr);
            if (closestHitShader != VK_NULL_HANDLE) vkDestroyShaderModule(context.device, closestHitShader, nullptr);
            throw std::runtime_error("Failed to load ray-tracing shader modules");
        }
        VkPipelineShaderStageCreateInfo shaderStages[] = {
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                .module = raygenShader,
                .pName = "main",
                .pSpecializationInfo = nullptr
            },
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .stage = VK_SHADER_STAGE_MISS_BIT_KHR,
                .module = missShader,
                .pName = "main",
                .pSpecializationInfo = nullptr
            },
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                .module = closestHitShader,
                .pName = "main",
                .pSpecializationInfo = nullptr
            }
        };
        VkRayTracingShaderGroupCreateInfoKHR shaderGroups[] = {
            {
                .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                .pNext = nullptr,
                .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
                .generalShader = 0,
                .closestHitShader = VK_SHADER_UNUSED_KHR,
                .anyHitShader = VK_SHADER_UNUSED_KHR,
                .intersectionShader = VK_SHADER_UNUSED_KHR,
                .pShaderGroupCaptureReplayHandle = nullptr
            },
            {
                .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                .pNext = nullptr,
                .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
                .generalShader = 1,
                .closestHitShader = VK_SHADER_UNUSED_KHR,
                .anyHitShader = VK_SHADER_UNUSED_KHR,
                .intersectionShader = VK_SHADER_UNUSED_KHR,
                .pShaderGroupCaptureReplayHandle = nullptr
            },
            {
                .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                .pNext = nullptr,
                .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
                .generalShader = VK_SHADER_UNUSED_KHR,
                .closestHitShader = 2,
                .anyHitShader = VK_SHADER_UNUSED_KHR,
                .intersectionShader = VK_SHADER_UNUSED_KHR,
                .pShaderGroupCaptureReplayHandle = nullptr
            }
        };
        VkDescriptorSetLayoutBinding bindings[] = {
            {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                .pImmutableSamplers = nullptr
            },
            {
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                .pImmutableSamplers = nullptr
            },
            {
                .binding = 2,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                .pImmutableSamplers = nullptr
            }
        };
        VkDescriptorSetLayoutCreateInfo layoutInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .bindingCount = 3,
            .pBindings = bindings
        };
        if (vkCreateDescriptorSetLayout(context.device, &layoutInfo, nullptr, &context.rayTracingDescriptorSetLayout) != VK_SUCCESS) {
            LOG_ERROR("Failed to create ray-tracing descriptor set layout", std::source_location::current());
            vkDestroyShaderModule(context.device, raygenShader, nullptr);
            vkDestroyShaderModule(context.device, missShader, nullptr);
            vkDestroyShaderModule(context.device, closestHitShader, nullptr);
            throw std::runtime_error("Failed to create ray-tracing descriptor set layout");
        }
        LOG_INFO("Created ray-tracing descriptor set layout: {:p}", std::source_location::current(), static_cast<void*>(context.rayTracingDescriptorSetLayout));
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .setLayoutCount = 1,
            .pSetLayouts = &context.rayTracingDescriptorSetLayout,
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = nullptr
        };
        if (vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr, &context.rayTracingPipelineLayout) != VK_SUCCESS) {
            LOG_ERROR("Failed to create ray-tracing pipeline layout", std::source_location::current());
            vkDestroyShaderModule(context.device, raygenShader, nullptr);
            vkDestroyShaderModule(context.device, missShader, nullptr);
            vkDestroyShaderModule(context.device, closestHitShader, nullptr);
            vkDestroyDescriptorSetLayout(context.device, context.rayTracingDescriptorSetLayout, nullptr);
            throw std::runtime_error("Failed to create ray-tracing pipeline layout");
        }
        LOG_INFO("Created ray-tracing pipeline layout: {:p}", std::source_location::current(), static_cast<void*>(context.rayTracingPipelineLayout));
        VkRayTracingPipelineCreateInfoKHR pipelineInfo{
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .stageCount = 3,
            .pStages = shaderStages,
            .groupCount = 3,
            .pGroups = shaderGroups,
            .maxPipelineRayRecursionDepth = 1,
            .pLibraryInfo = nullptr,
            .pLibraryInterface = nullptr,
            .pDynamicState = nullptr,
            .layout = context.rayTracingPipelineLayout,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = -1
        };
        if (vkCreateRayTracingPipelinesKHR(context.device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &context.rayTracingPipeline) != VK_SUCCESS) {
            LOG_ERROR("Failed to create ray-tracing pipeline", std::source_location::current());
            vkDestroyShaderModule(context.device, raygenShader, nullptr);
            vkDestroyShaderModule(context.device, missShader, nullptr);
            vkDestroyShaderModule(context.device, closestHitShader, nullptr);
            vkDestroyDescriptorSetLayout(context.device, context.rayTracingDescriptorSetLayout, nullptr);
            vkDestroyPipelineLayout(context.device, context.rayTracingPipelineLayout, nullptr);
            throw std::runtime_error("Failed to create ray-tracing pipeline");
        }
        LOG_INFO("Created ray-tracing pipeline: {:p}", std::source_location::current(), static_cast<void*>(context.rayTracingPipeline));
        vkDestroyShaderModule(context.device, raygenShader, nullptr);
        vkDestroyShaderModule(context.device, missShader, nullptr);
        vkDestroyShaderModule(context.device, closestHitShader, nullptr);
        LOG_INFO("Destroyed ray-tracing shader modules", std::source_location::current());
    }

    void createShaderBindingTable(VulkanContext& context) {
        auto vkGetRayTracingShaderGroupHandlesKHR = (PFN_vkGetRayTracingShaderGroupHandlesKHR)vkGetDeviceProcAddr(context.device, "vkGetRayTracingShaderGroupHandlesKHR");
        if (!vkGetRayTracingShaderGroupHandlesKHR) {
            LOG_ERROR("Failed to load vkGetRayTracingShaderGroupHandlesKHR", std::source_location::current());
            throw std::runtime_error("Failed to load vkGetRayTracingShaderGroupHandlesKHR");
        }
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR,
            .pNext = nullptr,
            .shaderGroupHandleSize = 0,
            .maxRayRecursionDepth = 0,
            .maxShaderGroupStride = 0,
            .shaderGroupBaseAlignment = 0,
            .shaderGroupHandleCaptureReplaySize = 0,
            .maxRayDispatchInvocationCount = 0,
            .shaderGroupHandleAlignment = 0,
            .maxRayHitAttributeSize = 0
        };
        VkPhysicalDeviceProperties2 properties{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            .pNext = &rtProperties,
            .properties = {}
        };
        vkGetPhysicalDeviceProperties2(context.physicalDevice, &properties);
        LOG_INFO("SBT properties: handle size: {}, alignment: {}", std::source_location::current(),
                 rtProperties.shaderGroupHandleSize, rtProperties.shaderGroupBaseAlignment);
        const uint32_t groupCount = 3;
        const uint32_t sbtSize = rtProperties.shaderGroupHandleSize * groupCount;
        std::vector<uint8_t> shaderHandleStorage(sbtSize);
        if (vkGetRayTracingShaderGroupHandlesKHR(context.device, context.rayTracingPipeline, 0, groupCount, sbtSize, shaderHandleStorage.data()) != VK_SUCCESS) {
            LOG_ERROR("Failed to get shader group handles", std::source_location::current());
            throw std::runtime_error("Failed to get shader group handles");
        }
        createBuffer(context.device, context.physicalDevice, sbtSize,
                     VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     context.shaderBindingTable, context.shaderBindingTableMemory);
        LOG_INFO("Created shader binding table: {:p}", std::source_location::current(), static_cast<void*>(context.shaderBindingTable));
        void* data;
        vkMapMemory(context.device, context.shaderBindingTableMemory, 0, sbtSize, 0, &data);
        memcpy(data, shaderHandleStorage.data(), sbtSize);
        vkUnmapMemory(context.device, context.shaderBindingTableMemory);
        LOG_INFO("Populated shader binding table with size: {}", std::source_location::current(), sbtSize);
    }
}