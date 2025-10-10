// src/engine/Vulkan_init.cpp
#include "engine/Vulkan_init.hpp"
#include "engine/Vulkan/Vulkan_func_swapchain.hpp"
#include "engine/Vulkan/Vulkan_func_pipe.hpp"
#include "engine/Vulkan_init_pipeline.hpp"
#include "engine/Vulkan_init_buffers.hpp"
#include "engine/logging.hpp"
#include <stdexcept>
#include <source_location>
#include <vector>

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

void createRayTracingPipeline(VulkanContext& context) {
    LOG_INFO_CAT("VulkanRenderer", "Creating ray tracing pipeline", std::source_location::current());

    // Load shaders (placeholder: assumes SPIR-V files exist)
    auto loadShader = [](const char* path) -> VkShaderModule {
        // TODO: Implement shader loading
        return VK_NULL_HANDLE; // Replace with actual shader loading
    };
    VkShaderModule rayGenModule = loadShader("shaders/raygen.rgen.spv");
    VkShaderModule missModule = loadShader("shaders/miss.rmiss.spv");
    VkShaderModule closestHitModule = loadShader("shaders/closesthit.rchit.spv");

    // Shader stages
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

    // Shader groups
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups = {
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = 0, // Raygen
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR
        },
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = 1, // Miss
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR
        },
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
            .generalShader = VK_SHADER_UNUSED_KHR,
            .closestHitShader = 2, // Closest hit
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR
        }
    };

    // Descriptor set layout
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

    // Allocate descriptor set
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

    // Pipeline layout
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

    // Ray tracing pipeline (placeholder)
    VkRayTracingPipelineCreateInfoKHR pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .stageCount = static_cast<uint32_t>(stages.size()),
        .pStages = stages.data(),
        .groupCount = static_cast<uint32_t>(groups.size()),
        .pGroups = groups.data(),
        .maxPipelineRayRecursionDepth = 1,
        .layout = context.rayTracingPipelineLayout
    };
    // TODO: Create pipeline using vkCreateRayTracingPipelinesKHR
    context.rayTracingPipeline = VK_NULL_HANDLE; // Replace with actual pipeline creation
    LOG_DEBUG_CAT("VulkanRenderer", "Ray tracing pipeline created", std::source_location::current());

    // Clean up shader modules
    vkDestroyShaderModule(context.device, rayGenModule, nullptr);
    vkDestroyShaderModule(context.device, missModule, nullptr);
    vkDestroyShaderModule(context.device, closestHitModule, nullptr);
}

void createAccelerationStructures(VulkanContext& context, std::span<const glm::vec3> vertices, std::span<const uint32_t> indices) {
    LOG_INFO_CAT("VulkanRenderer", "Creating acceleration structures", std::source_location::current());

    // Create BLAS
    VkAccelerationStructureGeometryKHR geometry{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry.triangles = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
            .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
            .vertexData.deviceAddress = getBufferDeviceAddress(context.device, context.vertexBuffer),
            .vertexStride = sizeof(glm::vec3),
            .maxVertex = static_cast<uint32_t>(vertices.size()),
            .indexType = VK_INDEX_TYPE_UINT32,
            .indexData.deviceAddress = getBufferDeviceAddress(context.device, context.indexBuffer)
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
    uint32_t primitiveCount = static_cast<uint32_t>(indices.size() / 3);
    vkGetAccelerationStructureBuildSizesKHR(context.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                           &buildInfo, &primitiveCount, &sizeInfo);

    // Create BLAS buffer
    createBuffer(context.device, context.physicalDevice, sizeInfo.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, context.topLevelASBuffer, context.topLevelASBufferMemory);

    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = context.topLevelASBuffer,
        .size = sizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
    };
    VkResult result = vkCreateAccelerationStructureKHR(context.device, &createInfo, nullptr, &context.topLevelAS);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to create BLAS: result={}",
                      std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to create BLAS");
    }

    // Build BLAS
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
        LOG_ERROR_CAT("VulkanRenderer", "Failed to allocate command buffer for BLAS: result={}",
                      std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to allocate command buffer for BLAS");
    }

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to begin command buffer for BLAS: result={}",
                      std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to begin command buffer for BLAS");
    }

    VkAccelerationStructureBuildRangeInfoKHR buildRange{
        .primitiveCount = primitiveCount,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0
    };
    VkAccelerationStructureBuildGeometryInfoKHR buildInfoFinal = buildInfo;
    buildInfoFinal.dstAccelerationStructure = context.topLevelAS;
    vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfoFinal, &buildRange);

    result = vkEndCommandBuffer(commandBuffer);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to end command buffer for BLAS: result={}",
                      std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to end command buffer for BLAS");
    }

    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer
    };
    result = vkQueueSubmit(context.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to submit command buffer for BLAS: result={}",
                      std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to submit command buffer for BLAS");
    }

    vkQueueWaitIdle(context.graphicsQueue);
    vkFreeCommandBuffers(context.device, context.commandPool, 1, &commandBuffer);

    // TODO: Create TLAS (instance referencing BLAS)
    LOG_DEBUG_CAT("VulkanRenderer", "Acceleration structures created", std::source_location::current());
}

void createShaderBindingTable(VulkanContext& context) {
    LOG_INFO_CAT("VulkanRenderer", "Creating shader binding table", std::source_location::current());

    const uint32_t groupCount = 3; // Raygen, miss, hit
    const uint32_t handleSize = 32; // Adjust based on device properties
    const uint32_t sbtSize = groupCount * handleSize;
    createBuffer(context.device, context.physicalDevice, sbtSize,
                 VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 context.shaderBindingTable, context.shaderBindingTableMemory);

    // TODO: Populate SBT with shader handles
    LOG_DEBUG_CAT("VulkanRenderer", "Shader binding table created", std::source_location::current());
}

void createStorageImage(VulkanContext& context, uint32_t width, uint32_t height) {
    LOG_INFO_CAT("VulkanRenderer", "Creating storage image with width={}, height={}",
                 std::source_location::current(), width, height);

    VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_B8G8R8A8_SRGB,
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
        LOG_ERROR_CAT("VulkanRenderer", "Failed to create storage image: result={}",
                      std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to create storage image");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(context.device, context.storageImage, &memRequirements);
    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = findMemoryType(context.physicalDevice, memRequirements.memoryTypeBits,
                                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    result = vkAllocateMemory(context.device, &allocInfo, nullptr, &context.storageImageMemory);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to allocate storage image memory: result={}",
                      std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to allocate storage image memory");
    }
    vkBindImageMemory(context.device, context.storageImage, context.storageImageMemory, 0);

    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = context.storageImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_B8G8R8A8_SRGB,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    result = vkCreateImageView(context.device, &viewInfo, nullptr, &context.storageImageView);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to create storage image view: result={}",
                      std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to create storage image view");
    }

    // Update descriptor set
    VkDescriptorImageInfo imageInfo{
        .imageView = context.storageImageView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };
    VkWriteDescriptorSetAccelerationStructureKHR accelInfo{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &context.topLevelAS
    };
    VkDescriptorBufferInfo bufferInfo{
        .buffer = context.uniformBuffer,
        .offset = 0,
        .range = sizeof(UniformBufferObject)
    };
    std::vector<VkWriteDescriptorSet> descriptorWrites = {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = &accelInfo,
            .dstSet = context.rayTracingDescriptorSet,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = context.rayTracingDescriptorSet,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &imageInfo
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = context.rayTracingDescriptorSet,
            .dstBinding = 2,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &bufferInfo
        }
    };
    vkUpdateDescriptorSets(context.device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    LOG_DEBUG_CAT("VulkanRenderer", "Storage image created", std::source_location::current());
}

} // namespace VulkanInitializer

VulkanRenderer::VulkanRenderer(VkInstance instance, VkSurfaceKHR surface,
                               std::span<const glm::vec3> vertices, std::span<const uint32_t> indices,
                               int width, int height)
    : context_{}, swapchainManager_(nullptr), pipelineManager_(nullptr), bufferManager_(nullptr) {
    LOG_INFO_CAT("VulkanRenderer", "Constructing VulkanRenderer with width={}, height={}",
                 std::source_location::current(), width, height);
    LOG_DEBUG_CAT("VulkanRenderer", "Received instance={:p}, surface={:p}, vertices.size={}, indices.size={}",
                  std::source_location::current(), static_cast<void*>(instance), static_cast<void*>(surface),
                  vertices.size(), indices.size());

    context_.instance = instance;
    context_.surface = surface;

    swapchainManager_ = std::make_unique<VulkanSwapchainManager>(context_, context_.surface);
    LOG_DEBUG_CAT("VulkanRenderer", "VulkanSwapchainManager created", std::source_location::current());
    swapchainManager_->initializeSwapchain(width, height);
    LOG_DEBUG_CAT("VulkanRenderer", "Swapchain initialized", std::source_location::current());

    pipelineManager_ = std::make_unique<VulkanPipelineManager>(context_);
    LOG_DEBUG_CAT("VulkanRenderer", "VulkanPipelineManager created", std::source_location::current());

    bufferManager_ = std::make_unique<VulkanBufferManager>(context_);
    bufferManager_->initializeBuffers(vertices, indices);
    LOG_DEBUG_CAT("VulkanRenderer", "VulkanBufferManager created and buffers initialized", std::source_location::current());

    VulkanInitializer::createDescriptorPoolAndSet(context_.device, context_.descriptorSetLayout,
                                                 context_.descriptorPool, context_.descriptorSet,
                                                 context_.sampler, context_.uniformBuffer);
    LOG_DEBUG_CAT("VulkanRenderer", "Descriptor pool and set created", std::source_location::current());

    // Ray tracing setup
    VulkanInitializer::createRayTracingPipeline(context_);
    VulkanInitializer::createAccelerationStructures(context_, vertices, indices);
    VulkanInitializer::createShaderBindingTable(context_);
    VulkanInitializer::createStorageImage(context_, width, height);
    LOG_DEBUG_CAT("VulkanRenderer", "Ray tracing resources initialized", std::source_location::current());
}

VulkanRenderer::~VulkanRenderer() {
    LOG_DEBUG_CAT("VulkanRenderer", "Destroying VulkanRenderer", std::source_location::current());
    if (context_.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(context_.device);
        LOG_DEBUG_CAT("VulkanRenderer", "Device idle, proceeding with cleanup", std::source_location::current());
    }

    // Clean up ray tracing resources
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

    bufferManager_.reset();
    pipelineManager_.reset();
    swapchainManager_.reset();

    if (context_.uniformBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(context_.device, context_.uniformBuffer, nullptr);
        context_.uniformBuffer = VK_NULL_HANDLE;
    }
    if (context_.uniformBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context_.device, context_.uniformBufferMemory, nullptr);
        context_.uniformBufferMemory = VK_NULL_HANDLE;
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

void VulkanRenderer::renderFrame(const AMOURANTH& amouranth) {
    LOG_DEBUG_CAT("VulkanRenderer", "renderFrame called with transform=[{:f}, {:f}, {:f}, {:f}]",
                  std::source_location::current(),
                  amouranth.getTransform()[0][0], amouranth.getTransform()[1][1],
                  amouranth.getTransform()[2][2], amouranth.getTransform()[3][3]);

    // Acquire next swapchain image
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(context_.device, swapchainManager_->swapchain(), UINT64_MAX,
                                            VK_NULL_HANDLE, VK_NULL_HANDLE, &imageIndex);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to acquire swapchain image: result={}",
                      std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to acquire swapchain image");
    }

    // Create command buffer
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = context_.commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VkCommandBuffer commandBuffer;
    result = vkAllocateCommandBuffers(context_.device, &allocInfo, &commandBuffer);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to allocate command buffer: result={}",
                      std::source_location::current(), vkResultToString(result));
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
                      std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to begin command buffer");
    }

    // Update uniform buffer with AMOURANTH transform
    UniformBufferObject ubo{};
    ubo.model = amouranth.getTransform();
    ubo.view = glm::lookAt(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    ubo.proj = glm::perspective(glm::radians(45.0f), swapchainManager_->swapchainExtent().width / (float)swapchainManager_->swapchainExtent().height, 0.1f, 100.0f);
    void* data;
    vkMapMemory(context_.device, context_.uniformBufferMemory, 0, sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(context_.device, context_.uniformBufferMemory);

    // Transition storage image to GENERAL layout for ray tracing
    VkImageMemoryBarrier imageBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
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
                         0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

    // Bind ray tracing pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, context_.rayTracingPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, context_.rayTracingPipelineLayout,
                            0, 1, &context_.rayTracingDescriptorSet, 0, nullptr);

    // Dispatch ray tracing
    VkStridedDeviceAddressRegionKHR raygenSBT{}, missSBT{}, hitSBT{}, callableSBT{};
    raygenSBT.deviceAddress = VulkanInitializer::getBufferDeviceAddress(context_.device, context_.shaderBindingTable);
    raygenSBT.stride = 32;
    raygenSBT.size = 32;
    missSBT.deviceAddress = raygenSBT.deviceAddress + 32;
    missSBT.stride = 32;
    missSBT.size = 32;
    hitSBT.deviceAddress = missSBT.deviceAddress + 32;
    hitSBT.stride = 32;
    hitSBT.size = 32;
    vkCmdTraceRaysKHR(commandBuffer, &raygenSBT, &missSBT, &hitSBT, &callableSBT,
                      swapchainManager_->swapchainExtent().width, swapchainManager_->swapchainExtent().height, 1);

    // Transition storage image to TRANSFER_SRC and swapchain image to TRANSFER_DST
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    imageBarrier.image = context_.storageImage;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

    VkImageMemoryBarrier swapchainBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
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
                         0, 0, nullptr, 0, nullptr, 1, &swapchainBarrier);

    // Copy storage image to swapchain image
    VkImageCopy copyRegion{
        .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .extent = {swapchainManager_->swapchainExtent().width, swapchainManager_->swapchainExtent().height, 1}
    };
    vkCmdCopyImage(commandBuffer, context_.storageImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   swapchainManager_->swapchainImages()[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    // Transition swapchain image to PRESENT_SRC
    swapchainBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    swapchainBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &swapchainBarrier);

    result = vkEndCommandBuffer(commandBuffer);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to end command buffer: result={}",
                      std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to end command buffer");
    }

    // Submit command buffer
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer
    };
    result = vkQueueSubmit(context_.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to submit command buffer: result={}",
                      std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to submit command buffer");
    }

    // Present
    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .swapchainCount = 1,
        .pSwapchains = &swapchainManager_->swapchain(),
        .pImageIndices = &imageIndex
    };
    result = vkQueuePresentKHR(context_.presentQueue, &presentInfo);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRenderer", "Failed to present: result={}",
                      std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to present");
    }

    vkQueueWaitIdle(context_.graphicsQueue);
    vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &commandBuffer);
}

VkDevice VulkanRenderer::getDevice() const { return context_.device; }
VkDeviceMemory VulkanRenderer::getVertexBufferMemory() const { return context_.vertexBufferMemory; }
VkPipeline VulkanRenderer::getGraphicsPipeline() const { return context_.pipeline; }

void VulkanRenderer::handleResize(int width, int height) {
    LOG_DEBUG_CAT("VulkanRenderer", "Handling resize to width={}, height={}",
                  std::source_location::current(), width, height);
    if (swapchainManager_) {
        swapchainManager_->handleResize(width, height);
        VulkanInitializer::createStorageImage(context_, width, height);
    }
}

std::string vkResultToString(VkResult result) {
    switch (result) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_NOT_READY: return "VK_NOT_READY";
        case VK_TIMEOUT: return "VK_TIMEOUT";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        default: return "Unknown VkResult (" + std::to_string(result) + ")";
    }
}