// src/engine/Vulkan_init.cpp
#include "engine/Vulkan_types.hpp"
#include "engine/Vulkan_init.hpp"
#include "engine/Vulkan/Vulkan_func_swapchain.hpp"
#include "engine/Vulkan/Vulkan_func_pipe.hpp"
#include "engine/Vulkan_init_pipeline.hpp"
#include "engine/Vulkan_init_buffers.hpp"
#include "engine/Vulkan_utils.hpp"
#include "engine/logging.hpp"
#include <stdexcept>
#include <source_location>
#include <vector>
#include <fstream>

namespace VulkanInitializer {

VkDeviceAddress getBufferDeviceAddress(VkDevice device, VkBuffer buffer) {
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
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type");
}

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

    VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create buffer: result={}",
                      std::source_location::current(), VulkanInitializer::vkResultToString(result));
        throw std::runtime_error("Failed to create buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties)
    };

    result = vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to allocate buffer memory: result={}",
                      std::source_location::current(), VulkanInitializer::vkResultToString(result));
        throw std::runtime_error("Failed to allocate buffer memory");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

VkShaderModule loadShader(VkDevice device, const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to open shader file: {}", std::source_location::current(), filepath);
        throw std::runtime_error("Failed to open shader file");
    }
    size_t size = file.tellg();
    std::vector<char> buffer(size);
    file.seekg(0);
    file.read(buffer.data(), size);
    file.close();
    VkShaderModuleCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .codeSize = size,
        .pCode = reinterpret_cast<const uint32_t*>(buffer.data())
    };
    VkShaderModule module;
    VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, &module);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to create shader module: result={}",
                      std::source_location::current(), VulkanInitializer::vkResultToString(result));
        throw std::runtime_error("Failed to create shader module");
    }
    return module;
}

void createRayTracingPipeline(VulkanContext& context) {
    LOG_INFO_CAT("VulkanRenderer", "Creating ray tracing pipeline", std::source_location::current());

    VkShaderModule rayGenModule = loadShader(context.device, "shaders/raygen.rgen.spv");
    VkShaderModule missModule = loadShader(context.device, "shaders/miss.rmiss.spv");
    VkShaderModule closestHitModule = loadShader(context.device, "shaders/closesthit.rchit.spv");

    std::vector<VkPipelineShaderStageCreateInfo> stages = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
            .module = rayGenModule,
            .pName = "main",
            .pSpecializationInfo = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_MISS_BIT_KHR,
            .module = missModule,
            .pName = "main",
            .pSpecializationInfo = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
            .module = closestHitModule,
            .pName = "main",
            .pSpecializationInfo = nullptr
        }
    };

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups = {
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

    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr},
        {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr}
    };
    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };
    VkResult result = vkCreateDescriptorSetLayout(context.device, &layoutInfo, nullptr, &context.rayTracingDescriptorSetLayout);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to create ray tracing descriptor set layout: result={}",
                      std::source_location::current(), VulkanInitializer::vkResultToString(result));
        throw std::runtime_error("Failed to create ray tracing descriptor set layout");
    }

    VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = context.descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &context.rayTracingDescriptorSetLayout
    };
    result = vkAllocateDescriptorSets(context.device, &allocInfo, &context.rayTracingDescriptorSet);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to allocate ray tracing descriptor set: result={}",
                      std::source_location::current(), VulkanInitializer::vkResultToString(result));
        throw std::runtime_error("Failed to allocate ray tracing descriptor set");
    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = &context.rayTracingDescriptorSetLayout,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr
    };
    result = vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr, &context.rayTracingPipelineLayout);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to create ray tracing pipeline layout: result={}",
                      std::source_location::current(), VulkanInitializer::vkResultToString(result));
        throw std::runtime_error("Failed to create ray tracing pipeline layout");
    }

    VkRayTracingPipelineCreateInfoKHR pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .stageCount = static_cast<uint32_t>(stages.size()),
        .pStages = stages.data(),
        .groupCount = static_cast<uint32_t>(groups.size()),
        .pGroups = groups.data(),
        .maxPipelineRayRecursionDepth = 1,
        .layout = context.rayTracingPipelineLayout
    };
    result = vkCreateRayTracingPipelinesKHR(context.device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &context.rayTracingPipeline);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to create ray tracing pipeline: result={}",
                      std::source_location::current(), VulkanInitializer::vkResultToString(result));
        throw std::runtime_error("Failed to create ray tracing pipeline");
    }

    vkDestroyShaderModule(context.device, rayGenModule, nullptr);
    vkDestroyShaderModule(context.device, missModule, nullptr);
    vkDestroyShaderModule(context.device, closestHitModule, nullptr);
    LOG_DEBUG_CAT("VulkanRenderer", "Ray tracing pipeline created", std::source_location::current());
}

void createAccelerationStructures(VulkanContext& context, std::span<const glm::vec3> vertices, std::span<const uint32_t> indices) {
    LOG_INFO_CAT("VulkanRenderer", "Creating acceleration structures", std::source_location::current());

    VkAccelerationStructureGeometryTrianglesDataKHR triangles{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .pNext = nullptr,
        .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
        .vertexData = { .deviceAddress = getBufferDeviceAddress(context.device, context.vertexBuffer) },
        .vertexStride = sizeof(glm::vec3),
        .maxVertex = static_cast<uint32_t>(vertices.size()),
        .indexType = VK_INDEX_TYPE_UINT32,
        .indexData = { .deviceAddress = getBufferDeviceAddress(context.device, context.indexBuffer) },
        .transformData = { .deviceAddress = 0 }
    };

    VkAccelerationStructureGeometryKHR geometry{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .pNext = nullptr,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry = { .triangles = triangles },
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
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
        .scratchData = { .deviceAddress = 0 }
    };

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
        .pNext = nullptr,
        .accelerationStructureSize = 0,
        .updateScratchSize = 0,
        .buildScratchSize = 0
    };
    uint32_t primitiveCount = static_cast<uint32_t>(indices.size() / 3);
    vkGetAccelerationStructureBuildSizesKHR(context.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                           &buildInfo, &primitiveCount, &sizeInfo);

    createBuffer(context.device, context.physicalDevice, sizeInfo.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, context.bottomLevelASBuffer, context.bottomLevelASBufferMemory);

    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .createFlags = 0,
        .buffer = context.bottomLevelASBuffer,
        .offset = 0,
        .size = sizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .deviceAddress = 0
    };
    VkResult result = vkCreateAccelerationStructureKHR(context.device, &createInfo, nullptr, &context.bottomLevelAS);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to create bottom level AS: result={}",
                      std::source_location::current(), VulkanInitializer::vkResultToString(result));
        throw std::runtime_error("Failed to create bottom level AS");
    }

    VkBuffer scratchBuffer;
    VkDeviceMemory scratchBufferMemory;
    createBuffer(context.device, context.physicalDevice, sizeInfo.buildScratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer, scratchBufferMemory);
    buildInfo.scratchData.deviceAddress = getBufferDeviceAddress(context.device, scratchBuffer);
    buildInfo.dstAccelerationStructure = context.bottomLevelAS;

    VkAccelerationStructureBuildRangeInfoKHR buildRange{
        .primitiveCount = primitiveCount,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0
    };
    VkAccelerationStructureBuildRangeInfoKHR* buildRanges[] = { &buildRange };

    VkCommandBuffer commandBuffer;
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = context.commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    result = vkAllocateCommandBuffers(context.device, &allocInfo, &commandBuffer);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to allocate command buffer: result={}",
                      std::source_location::current(), VulkanInitializer::vkResultToString(result));
        throw std::runtime_error("Failed to allocate command buffer");
    }

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to begin command buffer: result={}",
                      std::source_location::current(), VulkanInitializer::vkResultToString(result));
        throw std::runtime_error("Failed to begin command buffer");
    }

    vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfo, buildRanges);
    result = vkEndCommandBuffer(commandBuffer);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to end command buffer: result={}",
                      std::source_location::current(), VulkanInitializer::vkResultToString(result));
        throw std::runtime_error("Failed to end command buffer");
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
    result = vkQueueSubmit(context.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to submit command buffer: result={}",
                      std::source_location::current(), VulkanInitializer::vkResultToString(result));
        throw std::runtime_error("Failed to submit command buffer");
    }

    vkQueueWaitIdle(context.graphicsQueue);
    vkFreeCommandBuffers(context.device, context.commandPool, 1, &commandBuffer);
    vkDestroyBuffer(context.device, scratchBuffer, nullptr);
    vkFreeMemory(context.device, scratchBufferMemory, nullptr);

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
    createBuffer(context.device, context.physicalDevice, sizeof(instance),
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 instanceBuffer, instanceBufferMemory);

    void* data;
    result = vkMapMemory(context.device, instanceBufferMemory, 0, sizeof(instance), 0, &data);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to map instance buffer memory: result={}",
                      std::source_location::current(), VulkanInitializer::vkResultToString(result));
        throw std::runtime_error("Failed to map instance buffer memory");
    }
    memcpy(data, &instance, sizeof(instance));
    vkUnmapMemory(context.device, instanceBufferMemory);

    VkAccelerationStructureGeometryInstancesDataKHR instancesData{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        .pNext = nullptr,
        .arrayOfPointers = VK_FALSE,
        .data = { .deviceAddress = getBufferDeviceAddress(context.device, instanceBuffer) }
    };

    VkAccelerationStructureGeometryKHR topLevelGeometry{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .pNext = nullptr,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry = { .instances = instancesData },
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
        .scratchData = { .deviceAddress = 0 }
    };

    VkAccelerationStructureBuildSizesInfoKHR topLevelSizeInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
        .pNext = nullptr,
        .accelerationStructureSize = 0,
        .updateScratchSize = 0,
        .buildScratchSize = 0
    };
    uint32_t instanceCount = 1;
    vkGetAccelerationStructureBuildSizesKHR(context.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                           &topLevelBuildInfo, &instanceCount, &topLevelSizeInfo);

    createBuffer(context.device, context.physicalDevice, topLevelSizeInfo.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, context.topLevelASBuffer, context.topLevelASBufferMemory);

    VkAccelerationStructureCreateInfoKHR topLevelCreateInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .createFlags = 0,
        .buffer = context.topLevelASBuffer,
        .offset = 0,
        .size = topLevelSizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .deviceAddress = 0
    };
    result = vkCreateAccelerationStructureKHR(context.device, &topLevelCreateInfo, nullptr, &context.topLevelAS);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to create top level AS: result={}",
                      std::source_location::current(), VulkanInitializer::vkResultToString(result));
        throw std::runtime_error("Failed to create top level AS");
    }

    VkBuffer topLevelScratchBuffer;
    VkDeviceMemory topLevelScratchBufferMemory;
    createBuffer(context.device, context.physicalDevice, topLevelSizeInfo.buildScratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, topLevelScratchBuffer, topLevelScratchBufferMemory);
    topLevelBuildInfo.scratchData.deviceAddress = getBufferDeviceAddress(context.device, topLevelScratchBuffer);
    topLevelBuildInfo.dstAccelerationStructure = context.topLevelAS;

    VkAccelerationStructureBuildRangeInfoKHR topLevelBuildRange{
        .primitiveCount = instanceCount,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0
    };
    VkAccelerationStructureBuildRangeInfoKHR* topLevelBuildRanges[] = { &topLevelBuildRange };

    result = vkAllocateCommandBuffers(context.device, &allocInfo, &commandBuffer);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to allocate command buffer for TLAS: result={}",
                      std::source_location::current(), VulkanInitializer::vkResultToString(result));
        throw std::runtime_error("Failed to allocate command buffer for TLAS");
    }

    result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to begin command buffer for TLAS: result={}",
                      std::source_location::current(), VulkanInitializer::vkResultToString(result));
        throw std::runtime_error("Failed to begin command buffer for TLAS");
    }

    vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &topLevelBuildInfo, topLevelBuildRanges);
    result = vkEndCommandBuffer(commandBuffer);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to end command buffer for TLAS: result={}",
                      std::source_location::current(), VulkanInitializer::vkResultToString(result));
        throw std::runtime_error("Failed to end command buffer for TLAS");
    }

    result = vkQueueSubmit(context.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to submit command buffer for TLAS: result={}",
                      std::source_location::current(), VulkanInitializer::vkResultToString(result));
        throw std::runtime_error("Failed to submit command buffer for TLAS");
    }

    vkQueueWaitIdle(context.graphicsQueue);
    vkFreeCommandBuffers(context.device, context.commandPool, 1, &commandBuffer);
    vkDestroyBuffer(context.device, instanceBuffer, nullptr);
    vkFreeMemory(context.device, instanceBufferMemory, nullptr);
    vkDestroyBuffer(context.device, topLevelScratchBuffer, nullptr);
    vkFreeMemory(context.device, topLevelScratchBufferMemory, nullptr);
    LOG_DEBUG_CAT("VulkanRenderer", "Acceleration structures created", std::source_location::current());
}

void createShaderBindingTable(VulkanContext& context) {
    LOG_INFO_CAT("VulkanRenderer", "Creating shader binding table", std::source_location::current());

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

    const uint32_t groupCount = 3;
    const uint32_t handleSize = rtProperties.shaderGroupHandleSize;
    const uint32_t handleAlignment = rtProperties.shaderGroupHandleAlignment;
    const uint32_t alignedHandleSize = (handleSize + handleAlignment - 1) & ~(handleAlignment - 1);
    const uint32_t sbtSize = groupCount * alignedHandleSize;

    std::vector<uint8_t> shaderHandles(sbtSize);
    VkResult result = vkGetRayTracingShaderGroupHandlesKHR(context.device, context.rayTracingPipeline, 0, groupCount, sbtSize, shaderHandles.data());
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to get shader group handles: result={}",
                      std::source_location::current(), VulkanInitializer::vkResultToString(result));
        throw std::runtime_error("Failed to get shader group handles");
    }

    createBuffer(context.device, context.physicalDevice, sbtSize,
                 VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 context.shaderBindingTable, context.shaderBindingTableMemory);

    void* data;
    result = vkMapMemory(context.device, context.shaderBindingTableMemory, 0, sbtSize, 0, &data);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to map SBT memory: result={}",
                      std::source_location::current(), VulkanInitializer::vkResultToString(result));
        throw std::runtime_error("Failed to map SBT memory");
    }
    memcpy(data, shaderHandles.data(), sbtSize);
    vkUnmapMemory(context.device, context.shaderBindingTableMemory);
    LOG_DEBUG_CAT("VulkanRenderer", "Shader binding table created", std::source_location::current());
}

void createStorageImage(VulkanContext& context, uint32_t width, uint32_t height) {
    LOG_INFO_CAT("VulkanRenderer", "Creating storage image: width={}, height={}",
                 std::source_location::current(), width, height);

    VkImageCreateInfo imageCreateInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_B8G8R8A8_SRGB,
        .extent = {width, height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VkResult result = vkCreateImage(context.device, &imageCreateInfo, nullptr, &context.storageImage);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to create storage image: result={}",
                      std::source_location::current(), VulkanInitializer::vkResultToString(result));
        throw std::runtime_error("Failed to create storage image");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(context.device, context.storageImage, &memRequirements);

    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = findMemoryType(context.physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    result = vkAllocateMemory(context.device, &allocInfo, nullptr, &context.storageImageMemory);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to allocate storage image memory: result={}",
                      std::source_location::current(), VulkanInitializer::vkResultToString(result));
        throw std::runtime_error("Failed to allocate storage image memory");
    }

    vkBindImageMemory(context.device, context.storageImage, context.storageImageMemory, 0);

    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = context.storageImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_B8G8R8A8_SRGB,
        .components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    result = vkCreateImageView(context.device, &viewInfo, nullptr, &context.storageImageView);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to create storage image view: result={}",
                      std::source_location::current(), VulkanInitializer::vkResultToString(result));
        throw std::runtime_error("Failed to create storage image view");
    }

    VkWriteDescriptorSetAccelerationStructureKHR accelInfo{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .pNext = nullptr,
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &context.topLevelAS
    };

    VkDescriptorImageInfo descriptorImageInfo{
        .sampler = VK_NULL_HANDLE,
        .imageView = context.storageImageView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };

    VkDescriptorBufferInfo bufferInfo{
        .buffer = context.uniformBuffer,
        .offset = 0,
        .range = sizeof(UniformBufferObject)
    };

    std::vector<VkWriteDescriptorSet> descriptorWrites(3);
    descriptorWrites[0] = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = &accelInfo,
        .dstSet = context.rayTracingDescriptorSet,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
        .pImageInfo = nullptr,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr
    };
    descriptorWrites[1] = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = context.rayTracingDescriptorSet,
        .dstBinding = 1,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .pImageInfo = &descriptorImageInfo,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr
    };
    descriptorWrites[2] = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = context.rayTracingDescriptorSet,
        .dstBinding = 2,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pImageInfo = nullptr,
        .pBufferInfo = &bufferInfo,
        .pTexelBufferView = nullptr
    };

    vkUpdateDescriptorSets(context.device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    LOG_DEBUG_CAT("VulkanRenderer", "Storage image and descriptor set updated", std::source_location::current());
}

} // namespace VulkanInitializer

VulkanRenderer::VulkanRenderer(VkInstance instance, VkSurfaceKHR surface, std::span<const glm::vec3> vertices,
                               std::span<const uint32_t> indices, int width, int height) : context_{} {
    context_.instance = instance;
    context_.surface = surface;
    LOG_INFO_CAT("VulkanRenderer", "Constructing VulkanRenderer with instance={:p}, surface={:p}, width={}, height={}",
                 std::source_location::current(), static_cast<void*>(instance), static_cast<void*>(surface), width, height);

    swapchainManager_ = std::make_unique<VulkanSwapchainManager>(context_, context_.surface);
    bufferManager_ = std::make_unique<VulkanBufferManager>(context_);
    swapchainManager_->initializeSwapchain(width, height);
    bufferManager_->initializeBuffers(vertices, indices);
    VulkanInitializer::createRayTracingPipeline(context_);
    VulkanInitializer::createAccelerationStructures(context_, vertices, indices);
    VulkanInitializer::createStorageImage(context_, width, height);
    VulkanInitializer::createDescriptorPoolAndSet(context_.device, context_.descriptorSetLayout,
                                                context_.descriptorPool, context_.descriptorSet,
                                                context_.sampler, context_.uniformBuffer);
    LOG_INFO_CAT("VulkanRenderer", "VulkanRenderer initialized", std::source_location::current());
}

VulkanRenderer::~VulkanRenderer() {
    LOG_INFO_CAT("VulkanRenderer", "Destroying VulkanRenderer", std::source_location::current());
    if (context_.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(context_.device);
    }

    if (context_.storageImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(context_.device, context_.storageImageView, nullptr);
        context_.storageImageView = VK_NULL_HANDLE;
    }
    if (context_.storageImage != VK_NULL_HANDLE) {
        vkDestroyImage(context_.device, context_.storageImage, nullptr);
        context_.storageImage = VK_NULL_HANDLE;
    }
    if (context_.storageImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context_.device, context_.storageImageMemory, nullptr);
        context_.storageImageMemory = VK_NULL_HANDLE;
    }
    if (context_.shaderBindingTable != VK_NULL_HANDLE) {
        vkDestroyBuffer(context_.device, context_.shaderBindingTable, nullptr);
        context_.shaderBindingTable = VK_NULL_HANDLE;
    }
    if (context_.shaderBindingTableMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context_.device, context_.shaderBindingTableMemory, nullptr);
        context_.shaderBindingTableMemory = VK_NULL_HANDLE;
    }
    if (context_.topLevelAS != VK_NULL_HANDLE) {
        vkDestroyAccelerationStructureKHR(context_.device, context_.topLevelAS, nullptr);
        context_.topLevelAS = VK_NULL_HANDLE;
    }
    if (context_.topLevelASBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(context_.device, context_.topLevelASBuffer, nullptr);
        context_.topLevelASBuffer = VK_NULL_HANDLE;
    }
    if (context_.topLevelASBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context_.device, context_.topLevelASBufferMemory, nullptr);
        context_.topLevelASBufferMemory = VK_NULL_HANDLE;
    }
    if (context_.bottomLevelAS != VK_NULL_HANDLE) {
        vkDestroyAccelerationStructureKHR(context_.device, context_.bottomLevelAS, nullptr);
        context_.bottomLevelAS = VK_NULL_HANDLE;
    }
    if (context_.bottomLevelASBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(context_.device, context_.bottomLevelASBuffer, nullptr);
        context_.bottomLevelASBuffer = VK_NULL_HANDLE;
    }
    if (context_.bottomLevelASBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context_.device, context_.bottomLevelASBufferMemory, nullptr);
        context_.bottomLevelASBufferMemory = VK_NULL_HANDLE;
    }
    if (context_.rayTracingPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(context_.device, context_.rayTracingPipeline, nullptr);
        context_.rayTracingPipeline = VK_NULL_HANDLE;
    }
    if (context_.rayTracingPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(context_.device, context_.rayTracingPipelineLayout, nullptr);
        context_.rayTracingPipelineLayout = VK_NULL_HANDLE;
    }
    if (context_.rayTracingDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(context_.device, context_.rayTracingDescriptorSetLayout, nullptr);
        context_.rayTracingDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (context_.descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(context_.device, context_.descriptorPool, nullptr);
        context_.descriptorPool = VK_NULL_HANDLE;
    }
    if (context_.commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(context_.device, context_.commandPool, nullptr);
        context_.commandPool = VK_NULL_HANDLE;
    }
    if (context_.device != VK_NULL_HANDLE) {
        vkDestroyDevice(context_.device, nullptr);
        context_.device = VK_NULL_HANDLE;
    }
    if (context_.instance != VK_NULL_HANDLE && context_.surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(context_.instance, context_.surface, nullptr);
        context_.surface = VK_NULL_HANDLE;
    }
    if (context_.instance != VK_NULL_HANDLE) {
        vkDestroyInstance(context_.instance, nullptr);
        context_.instance = VK_NULL_HANDLE;
    }
}

void VulkanRenderer::renderFrame(const AMOURANTH& camera) {
    LOG_DEBUG_CAT("VulkanRenderer", "Rendering frame", std::source_location::current());

    VkSemaphore imageAvailableSemaphore;
    VkSemaphoreCreateInfo semaphoreInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0
    };
    VkResult result = vkCreateSemaphore(context_.device, &semaphoreInfo, nullptr, &imageAvailableSemaphore);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to create image available semaphore: result={}",
                      std::source_location::current(), VulkanInitializer::vkResultToString(result));
        throw std::runtime_error("Failed to create image available semaphore");
    }

    VkSemaphore renderFinishedSemaphore;
    result = vkCreateSemaphore(context_.device, &semaphoreInfo, nullptr, &renderFinishedSemaphore);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to create render finished semaphore: result={}",
                      std::source_location::current(), VulkanInitializer::vkResultToString(result));
        vkDestroySemaphore(context_.device, imageAvailableSemaphore, nullptr);
        throw std::runtime_error("Failed to create render finished semaphore");
    }

    uint32_t imageIndex;
    result = vkAcquireNextImageKHR(context_.device, swapchainManager_->swapchain(), UINT64_MAX,
                                   imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to acquire swapchain image: result={}",
                      std::source_location::current(), VulkanInitializer::vkResultToString(result));
        vkDestroySemaphore(context_.device, imageAvailableSemaphore, nullptr);
        vkDestroySemaphore(context_.device, renderFinishedSemaphore, nullptr);
        throw std::runtime_error("Failed to acquire swapchain image");
    }

    UniformBufferObject ubo{
        .model = glm::mat4(1.0f),
        .view = camera.viewMatrix,
        .proj = camera.projMatrix
    };
    void* data;
    result = vkMapMemory(context_.device, context_.uniformBufferMemory, 0, sizeof(ubo), 0, &data);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to map uniform buffer memory: result={}",
                      std::source_location::current(), VulkanInitializer::vkResultToString(result));
        throw std::runtime_error("Failed to map uniform buffer memory");
    }
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(context_.device, context_.uniformBufferMemory);

    VkCommandBuffer commandBuffer;
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = context_.commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    result = vkAllocateCommandBuffers(context_.device, &allocInfo, &commandBuffer);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to allocate command buffer: result={}",
                      std::source_location::current(), VulkanInitializer::vkResultToString(result));
        throw std::runtime_error("Failed to allocate command buffer");
    }

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to begin command buffer: result={}",
                      std::source_location::current(), VulkanInitializer::vkResultToString(result));
        throw std::runtime_error("Failed to begin command buffer");
    }

    VkImageMemoryBarrier storageImageBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = context_.storageImage,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         0, 0, nullptr, 0, nullptr, 1, &storageImageBarrier);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, context_.rayTracingPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, context_.rayTracingPipelineLayout,
                            0, 1, &context_.rayTracingDescriptorSet, 0, nullptr);

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
    vkGetPhysicalDeviceProperties2(context_.physicalDevice, &properties);

    VkStridedDeviceAddressRegionKHR raygenSBT{
        .deviceAddress = VulkanInitializer::getBufferDeviceAddress(context_.device, context_.shaderBindingTable),
        .stride = rtProperties.shaderGroupHandleSize,
        .size = rtProperties.shaderGroupHandleSize
    };
    VkStridedDeviceAddressRegionKHR missSBT{
        .deviceAddress = raygenSBT.deviceAddress + rtProperties.shaderGroupHandleSize,
        .stride = rtProperties.shaderGroupHandleSize,
        .size = rtProperties.shaderGroupHandleSize
    };
    VkStridedDeviceAddressRegionKHR hitSBT{
        .deviceAddress = missSBT.deviceAddress + rtProperties.shaderGroupHandleSize,
        .stride = rtProperties.shaderGroupHandleSize,
        .size = rtProperties.shaderGroupHandleSize
    };
    VkStridedDeviceAddressRegionKHR callableSBT{0, 0, 0};

    vkCmdTraceRaysKHR(commandBuffer, &raygenSBT, &missSBT, &hitSBT, &callableSBT,
                      swapchainManager_->swapchainExtent().width, swapchainManager_->swapchainExtent().height, 1);

    storageImageBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    storageImageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    storageImageBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    storageImageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &storageImageBarrier);

    VkImageMemoryBarrier swapchainImageBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = swapchainManager_->swapchainImages()[imageIndex],
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &swapchainImageBarrier);

    VkImageCopy copyRegion{
        .srcSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        .srcOffset = {0, 0, 0},
        .dstSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        .dstOffset = {0, 0, 0},
        .extent = {swapchainManager_->swapchainExtent().width, swapchainManager_->swapchainExtent().height, 1}
    };
    vkCmdCopyImage(commandBuffer, context_.storageImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   swapchainManager_->swapchainImages()[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    swapchainImageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    swapchainImageBarrier.dstAccessMask = 0;
    swapchainImageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    swapchainImageBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &swapchainImageBarrier);

    result = vkEndCommandBuffer(commandBuffer);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to end command buffer: result={}",
                      std::source_location::current(), VulkanInitializer::vkResultToString(result));
        throw std::runtime_error("Failed to end command buffer");
    }

    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &imageAvailableSemaphore,
        .pWaitDstStageMask = waitStages,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &renderFinishedSemaphore
    };
    result = vkQueueSubmit(context_.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to submit command buffer: result={}",
                      std::source_location::current(), VulkanInitializer::vkResultToString(result));
        throw std::runtime_error("Failed to submit command buffer");
    }

    VkSwapchainKHR swapchain = swapchainManager_->swapchain();
    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &renderFinishedSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &imageIndex,
        .pResults = nullptr
    };
    result = vkQueuePresentKHR(context_.presentQueue, &presentInfo);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to present swapchain image: result={}",
                      std::source_location::current(), VulkanInitializer::vkResultToString(result));
        throw std::runtime_error("Failed to present swapchain image");
    }

    vkQueueWaitIdle(context_.presentQueue);
    vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &commandBuffer);
    vkDestroySemaphore(context_.device, imageAvailableSemaphore, nullptr);
    vkDestroySemaphore(context_.device, renderFinishedSemaphore, nullptr);
    LOG_DEBUG_CAT("VulkanRenderer", "Frame rendered", std::source_location::current());
}

void VulkanRenderer::handleResize(int width, int height) {
    LOG_INFO_CAT("VulkanRenderer", "Handling resize to width={}, height={}",
                 std::source_location::current(), width, height);
    if (context_.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(context_.device);
    }
    swapchainManager_->handleResize(width, height);
    VulkanInitializer::createStorageImage(context_, width, height);
}