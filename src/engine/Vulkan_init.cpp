// src/engine/Vulkan_init.cpp
#include "engine/Vulkan_init.hpp"
#include "engine/core.hpp"
#include "engine/logging.hpp"
#include <stdexcept>
#include <source_location>
#include <vector>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>

namespace VulkanInitializer {

VkDeviceAddress getBufferDeviceAddress(VkDevice device, VkBuffer buffer) {
    VkBufferDeviceAddressInfo addressInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
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
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create buffer: result={}", std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to create buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties)
    };

    result = vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to allocate buffer memory: result={}", std::source_location::current(), vkResultToString(result));
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
        .codeSize = size,
        .pCode = reinterpret_cast<const uint32_t*>(buffer.data())
    };
    VkShaderModule module;
    VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, &module);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to create shader module: result={}", std::source_location::current(), vkResultToString(result));
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
            .stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
            .module = rayGenModule,
            .pName = "main"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_MISS_BIT_KHR,
            .module = missModule,
            .pName = "main"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
            .module = closestHitModule,
            .pName = "main"
        }
    };

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups = {
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = 0,
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR
        },
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = 1,
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR
        },
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
            .generalShader = VK_SHADER_UNUSED_KHR,
            .closestHitShader = 2,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR
        }
    };

    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr},
        {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr}
    };
    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };
    VkResult result = vkCreateDescriptorSetLayout(context.device, &layoutInfo, nullptr, &context.rayTracingDescriptorSetLayout);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to create ray tracing descriptor set layout: result={}",
                      std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to create ray tracing descriptor set layout");
    }

    VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = context.descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &context.rayTracingDescriptorSetLayout
    };
    result = vkAllocateDescriptorSets(context.device, &allocInfo, &context.rayTracingDescriptorSet);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to allocate ray tracing descriptor set: result={}",
                      std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to allocate ray tracing descriptor set");
    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &context.rayTracingDescriptorSetLayout
    };
    result = vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr, &context.rayTracingPipelineLayout);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to create ray tracing pipeline layout: result={}",
                      std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to create ray tracing pipeline layout");
    }

    VkRayTracingPipelineCreateInfoKHR pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
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
                      std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to create ray tracing pipeline");
    }

    vkDestroyShaderModule(context.device, rayGenModule, nullptr);
    vkDestroyShaderModule(context.device, missModule, nullptr);
    vkDestroyShaderModule(context.device, closestHitModule, nullptr);
    LOG_DEBUG_CAT("VulkanRenderer", "Ray tracing pipeline created", std::source_location::current());
}

void createAccelerationStructures(VulkanContext& context, std::span<const glm::vec3> vertices, std::span<const uint32_t> indices) {
    VkDeviceSize vertexBufferSize = vertices.size() * sizeof(glm::vec3);
    VkDeviceSize indexBufferSize = indices.size() * sizeof(uint32_t);
    VkBuffer stagingVertexBuffer, stagingIndexBuffer;
    VkDeviceMemory stagingVertexMemory, stagingIndexMemory;

    createBuffer(context.device, context.physicalDevice, vertexBufferSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingVertexBuffer, stagingVertexMemory);
    createBuffer(context.device, context.physicalDevice, indexBufferSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingIndexBuffer, stagingIndexMemory);

    void* data;
    vkMapMemory(context.device, stagingVertexMemory, 0, vertexBufferSize, 0, &data);
    memcpy(data, vertices.data(), vertexBufferSize);
    vkUnmapMemory(context.device, stagingVertexMemory);
    vkMapMemory(context.device, stagingIndexMemory, 0, indexBufferSize, 0, &data);
    memcpy(data, indices.data(), indexBufferSize);
    vkUnmapMemory(context.device, stagingIndexMemory);

    VkBuffer vertexBuffer, indexBuffer;
    VkDeviceMemory vertexMemory, indexMemory;
    createBuffer(context.device, context.physicalDevice, vertexBufferSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexMemory);
    createBuffer(context.device, context.physicalDevice, indexBufferSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexMemory);

    VkCommandBuffer commandBuffer;
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = context.commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    vkAllocateCommandBuffers(context.device, &allocInfo, &commandBuffer);
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkBufferCopy copyRegion = {};
    copyRegion.size = vertexBufferSize;
    vkCmdCopyBuffer(commandBuffer, stagingVertexBuffer, vertexBuffer, 1, &copyRegion);
    copyRegion.size = indexBufferSize;
    vkCmdCopyBuffer(commandBuffer, stagingIndexBuffer, indexBuffer, 1, &copyRegion);

    VkMemoryBarrier memoryBarrier{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR
    };
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                         0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

    VkAccelerationStructureGeometryKHR geometry{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry.triangles = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
            .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
            .vertexData.deviceAddress = getBufferDeviceAddress(context.device, vertexBuffer),
            .vertexStride = sizeof(glm::vec3),
            .maxVertex = static_cast<uint32_t>(vertices.size()),
            .indexType = VK_INDEX_TYPE_UINT32,
            .indexData.deviceAddress = getBufferDeviceAddress(context.device, indexBuffer)
        }
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .geometryCount = 1,
        .pGeometries = &geometry
    };

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };
    vkGetAccelerationStructureBuildSizesKHR(context.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, nullptr, &sizeInfo);

    createBuffer(context.device, context.physicalDevice, sizeInfo.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, context.bottomLevelASBuffer, context.bottomLevelASBufferMemory);

    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = context.bottomLevelASBuffer,
        .size = sizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
    };
    vkCreateAccelerationStructureKHR(context.device, &createInfo, nullptr, &context.bottomLevelAS);

    VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{
        .primitiveCount = static_cast<uint32_t>(indices.size() / 3),
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0
    };
    VkAccelerationStructureBuildRangeInfoKHR* buildRangeInfos[] = {&buildRangeInfo};

    VkBuffer scratchBuffer;
    VkDeviceMemory scratchMemory;
    createBuffer(context.device, context.physicalDevice, sizeInfo.buildScratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer, scratchMemory);
    buildInfo.scratchData.deviceAddress = getBufferDeviceAddress(context.device, scratchBuffer);
    buildInfo.dstAccelerationStructure = context.bottomLevelAS;

    vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfo, buildRangeInfos);

    vkEndCommandBuffer(commandBuffer);
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer
    };
    vkQueueSubmit(context.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(context.graphicsQueue);
    vkFreeCommandBuffers(context.device, context.commandPool, 1, &commandBuffer);
    vkDestroyBuffer(context.device, scratchBuffer, nullptr);
    vkFreeMemory(context.device, scratchMemory, nullptr);
    vkDestroyBuffer(context.device, stagingVertexBuffer, nullptr);
    vkFreeMemory(context.device, stagingVertexMemory, nullptr);
    vkDestroyBuffer(context.device, stagingIndexBuffer, nullptr);
    vkFreeMemory(context.device, stagingIndexMemory, nullptr);
}

void createShaderBindingTable(VulkanContext& context) {
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };
    VkPhysicalDeviceProperties2 properties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &rtProperties
    };
    vkGetPhysicalDeviceProperties2(context.physicalDevice, &properties);

    const uint32_t handleSize = rtProperties.shaderGroupHandleSize;
    const uint32_t groupCount = 3; // Raygen, miss, hit
    const uint32_t sbtSize = handleSize * groupCount;

    createBuffer(context.device, context.physicalDevice, sbtSize,
                 VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 context.shaderBindingTable, context.shaderBindingTableMemory);

    std::vector<uint8_t> shaderHandleStorage(sbtSize);
    vkGetRayTracingShaderGroupHandlesKHR(context.device, context.rayTracingPipeline, 0, groupCount, sbtSize, shaderHandleStorage.data());

    void* data;
    vkMapMemory(context.device, context.shaderBindingTableMemory, 0, sbtSize, 0, &data);
    memcpy(data, shaderHandleStorage.data(), sbtSize);
    vkUnmapMemory(context.device, context.shaderBindingTableMemory);
}

void createStorageImage(VulkanContext& context, uint32_t width, uint32_t height) {
    VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = {width, height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VkResult result = vkCreateImage(context.device, &imageInfo, nullptr, &context.storageImage);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to create storage image: result={}", std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to create storage image");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(context.device, context.storageImage, &memRequirements);
    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = findMemoryType(context.physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    result = vkAllocateMemory(context.device, &allocInfo, nullptr, &context.storageImageMemory);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to allocate storage image memory: result={}", std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to allocate storage image memory");
    }
    vkBindImageMemory(context.device, context.storageImage, context.storageImageMemory, 0);

    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = context.storageImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
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
        LOG_ERROR_CAT("VulkanRenderer", "Failed to create storage image view: result={}", std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to create storage image view");
    }
}

void createDescriptorPoolAndSet(VkDevice device, VkDescriptorSetLayout descriptorSetLayout,
                               VkDescriptorPool& descriptorPool, VkDescriptorSet& descriptorSet,
                               VkSampler& sampler, VkBuffer uniformBuffer) {
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
        {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1}
    };
    VkDescriptorPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = 3,
        .pPoolSizes = poolSizes
    };
    VkResult result = vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to create descriptor pool: result={}", std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to create descriptor pool");
    }

    VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &descriptorSetLayout
    };
    result = vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to allocate descriptor set: result={}", std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to allocate descriptor set");
    }

    VkSamplerCreateInfo samplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
    };
    result = vkCreateSampler(device, &samplerInfo, nullptr, &sampler);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to create sampler: result={}", std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to create sampler");
    }

    VkDescriptorBufferInfo bufferInfo{
        .buffer = uniformBuffer,
        .offset = 0,
        .range = sizeof(UniformBufferObject)
    };
    VkWriteDescriptorSet descriptorWrites[1] = {};
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = descriptorSet;
    descriptorWrites[0].dstBinding = 2;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(device, 1, descriptorWrites, 0, nullptr);
}

void createRenderPass(VkDevice device, VkRenderPass& renderPass, VkFormat format) {
    VkAttachmentDescription colorAttachment{
        .format = format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };

    VkAttachmentReference colorAttachmentRef{
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subpass{
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentRef
    };

    VkRenderPassCreateInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colorAttachment,
        .subpassCount = 1,
        .pSubpasses = &subpass
    };

    VkResult result = vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to create render pass: result={}", std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to create render pass");
    }
}

void createDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout& descriptorSetLayout) {
    VkDescriptorSetLayoutBinding uboLayoutBinding{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &uboLayoutBinding
    };

    VkResult result = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to create descriptor set layout: result={}", std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to create descriptor set layout");
    }
}

void createGraphicsPipeline(VkDevice device, VkRenderPass renderPass,
                           VkPipeline& pipeline, VkPipelineLayout& pipelineLayout,
                           VkDescriptorSetLayout& descriptorSetLayout,
                           int width, int height,
                           VkShaderModule& vertexShaderModule,
                           VkShaderModule& fragmentShaderModule) {
    VkPipelineShaderStageCreateInfo shaderStages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertexShaderModule,
            .pName = "main"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragmentShaderModule,
            .pName = "main"
        }
    };

    VkVertexInputBindingDescription bindingDescription{
        .binding = 0,
        .stride = sizeof(glm::vec3),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };
    VkVertexInputAttributeDescription attributeDescription{
        .binding = 0,
        .location = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = 0
    };
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindingDescription,
        .vertexAttributeDescriptionCount = 1,
        .pVertexAttributeDescriptions = &attributeDescription
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };

    VkViewport viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(width),
        .height = static_cast<float>(height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    VkRect2D scissor{
        .offset = {0, 0},
        .extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}
    };
    VkPipelineViewportStateCreateInfo viewportState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor
    };

    VkPipelineRasterizationStateCreateInfo rasterizer{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.0f,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE
    };

    VkPipelineMultisampleStateCreateInfo multisampling{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };

    VkPipelineColorBlendAttachmentState colorBlendAttachment{
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_FALSE
    };
    VkPipelineColorBlendStateCreateInfo colorBlending{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &descriptorSetLayout
    };
    VkResult result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to create pipeline layout: result={}", std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to create pipeline layout");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shaderStages,
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pColorBlendState = &colorBlending,
        .layout = pipelineLayout,
        .renderPass = renderPass,
        .subpass = 0
    };
    result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to create graphics pipeline: result={}", std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to create graphics pipeline");
    }
}

} // namespace VulkanInitializer

VulkanRenderer::VulkanRenderer(VkInstance instance, VkSurfaceKHR surface, std::span<const glm::vec3> vertices,
                              std::span<const uint32_t> indices, int width, int height)
    : context_{}, width_(width), height_(height) {
    context_.instance = instance;
    context_.surface = surface;
    LOG_INFO_CAT("VulkanRenderer", "Constructing VulkanRenderer with instance={:p}, surface={:p}, width={}, height={}",
                 std::source_location::current(), static_cast<void*>(instance), static_cast<void*>(surface), width, height);

    // Initialize Vulkan instance, physical device, device, queues, command pool
    // (Add your existing initialization code here)

    swapchainManager_ = std::make_unique<VulkanSwapchainManager>(context_, context_.surface);
    pipelineManager_ = std::make_unique<VulkanPipelineManager>(context_, width, height);
    bufferManager_ = std::make_unique<VulkanBufferManager>(context_, vertices, indices);

    swapchainManager_->initializeSwapchain(width, height);
    pipelineManager_->createRenderPass();
    pipelineManager_->createPipelineLayout();
    pipelineManager_->createGraphicsPipeline();
    VulkanInitializer::createRayTracingPipeline(context_);
    VulkanInitializer::createAccelerationStructures(context_, vertices, indices);
    VulkanInitializer::createStorageImage(context_, width, height);
    VulkanInitializer::createDescriptorPoolAndSet(context_.device, context_.descriptorSetLayout,
                                                context_.descriptorPool, context_.descriptorSet,
                                                context_.sampler, context_.uniformBuffer);

    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = context_.commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    commandBuffers_.resize(1);
    VkResult result = vkAllocateCommandBuffers(context_.device, &allocInfo, commandBuffers_.data());
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to allocate command buffers: result={}", std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to allocate command buffers");
    }

    LOG_INFO_CAT("VulkanRenderer", "VulkanRenderer initialized", std::source_location::current());
}

VulkanRenderer::~VulkanRenderer() {
    LOG_INFO_CAT("VulkanRenderer", "Destroying VulkanRenderer", std::source_location::current());
    if (context_.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(context_.device);
    }

    if (context_.storageImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(context_.device, context_.storageImageView, nullptr);
    }
    if (context_.storageImage != VK_NULL_HANDLE) {
        vkDestroyImage(context_.device, context_.storageImage, nullptr);
    }
    if (context_.storageImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context_.device, context_.storageImageMemory, nullptr);
    }
    if (context_.shaderBindingTable != VK_NULL_HANDLE) {
        vkDestroyBuffer(context_.device, context_.shaderBindingTable, nullptr);
    }
    if (context_.shaderBindingTableMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context_.device, context_.shaderBindingTableMemory, nullptr);
    }
    if (context_.topLevelAS != VK_NULL_HANDLE) {
        vkDestroyAccelerationStructureKHR(context_.device, context_.topLevelAS, nullptr);
    }
    if (context_.topLevelASBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(context_.device, context_.topLevelASBuffer, nullptr);
    }
    if (context_.topLevelASBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context_.device, context_.topLevelASBufferMemory, nullptr);
    }
    if (context_.bottomLevelAS != VK_NULL_HANDLE) {
        vkDestroyAccelerationStructureKHR(context_.device, context_.bottomLevelAS, nullptr);
    }
    if (context_.bottomLevelASBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(context_.device, context_.bottomLevelASBuffer, nullptr);
    }
    if (context_.bottomLevelASBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context_.device, context_.bottomLevelASBufferMemory, nullptr);
    }
    if (context_.rayTracingPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(context_.device, context_.rayTracingPipeline, nullptr);
    }
    if (context_.rayTracingPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(context_.device, context_.rayTracingPipelineLayout, nullptr);
    }
    if (context_.rayTracingDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(context_.device, context_.rayTracingDescriptorSetLayout, nullptr);
    }
    if (context_.descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(context_.device, context_.descriptorPool, nullptr);
    }
    if (context_.commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(context_.device, context_.commandPool, nullptr);
    }
    if (context_.device != VK_NULL_HANDLE) {
        vkDestroyDevice(context_.device, nullptr);
    }
    if (context_.instance != VK_NULL_HANDLE && context_.surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(context_.instance, context_.surface, nullptr);
    }
    if (context_.instance != VK_NULL_HANDLE) {
        vkDestroyInstance(context_.instance, nullptr);
    }
}

void VulkanRenderer::renderFrame(const AMOURANTH& camera) {
    LOG_DEBUG_CAT("VulkanRenderer", "Rendering frame", std::source_location::current());

    VkSemaphore imageAvailableSemaphore;
    VkSemaphoreCreateInfo semaphoreInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };
    VkResult result = vkCreateSemaphore(context_.device, &semaphoreInfo, nullptr, &imageAvailableSemaphore);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to create image available semaphore: result={}",
                      std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to create image available semaphore");
    }

    VkSemaphore renderFinishedSemaphore;
    result = vkCreateSemaphore(context_.device, &semaphoreInfo, nullptr, &renderFinishedSemaphore);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to create render finished semaphore: result={}",
                      std::source_location::current(), vkResultToString(result));
        vkDestroySemaphore(context_.device, imageAvailableSemaphore, nullptr);
        throw std::runtime_error("Failed to create render finished semaphore");
    }

    uint32_t imageIndex;
    result = vkAcquireNextImageKHR(context_.device, swapchainManager_->getSwapchain(), UINT64_MAX,
                                   imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to acquire swapchain image: result={}",
                      std::source_location::current(), vkResultToString(result));
        vkDestroySemaphore(context_.device, imageAvailableSemaphore, nullptr);
        vkDestroySemaphore(context_.device, renderFinishedSemaphore, nullptr);
        throw std::runtime_error("Failed to acquire swapchain image");
    }

    UniformBufferObject ubo{
        .model = glm::mat4(1.0f),
        .view = camera.getTransform(),
        .proj = glm::perspective(glm::radians(45.0f), (float)camera.getWidth() / camera.getHeight(), 0.1f, 100.0f)
    };
    void* data;
    result = vkMapMemory(context_.device, context_.uniformBufferMemory, 0, sizeof(ubo), 0, &data);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to map uniform buffer memory: result={}",
                      std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to map uniform buffer memory");
    }
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(context_.device, context_.uniformBufferMemory);

    VkCommandBuffer commandBuffer = commandBuffers_[0];
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to begin command buffer: result={}",
                      std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to begin command buffer");
    }

    VkImageMemoryBarrier storageImageBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
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
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };
    VkPhysicalDeviceProperties2 properties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &rtProperties
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
    VkStridedDeviceAddressRegionKHR callableSBT{};

    vkCmdTraceRaysKHR(commandBuffer, &raygenSBT, &missSBT, &hitSBT, &callableSBT,
                      swapchainManager_->getSwapchainExtent().width, swapchainManager_->getSwapchainExtent().height, 1);

    storageImageBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    storageImageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    storageImageBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    storageImageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &storageImageBarrier);

    VkImageMemoryBarrier swapchainImageBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = swapchainManager_->getSwapchainImages()[imageIndex],
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
        .dstSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        .extent = {swapchainManager_->getSwapchainExtent().width, swapchainManager_->getSwapchainExtent().height, 1}
    };
    vkCmdCopyImage(commandBuffer, context_.storageImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   swapchainManager_->getSwapchainImages()[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    swapchainImageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    swapchainImageBarrier.dstAccessMask = 0;
    swapchainImageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    swapchainImageBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &swapchainImageBarrier);

    result = vkEndCommandBuffer(commandBuffer);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to end command buffer: result={}",
                      std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to end command buffer");
    }

    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
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
                      std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to submit command buffer");
    }

    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &renderFinishedSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &swapchainManager_->getSwapchain(),
        .pImageIndices = &imageIndex
    };
    result = vkQueuePresentKHR(context_.presentQueue, &presentInfo);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to present swapchain image: result={}",
                      std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to present swapchain image");
    }

    vkQueueWaitIdle(context_.presentQueue);
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
    width_ = width;
    height_ = height;
}