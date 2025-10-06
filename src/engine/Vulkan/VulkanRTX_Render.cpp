// VulkanRTX_Render.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
#include "engine/Vulkan/Vulkan_RTX.hpp"
#include <stdexcept>
#include <format>
#include <syncstream>
#include <vector>
#include <iostream>
#include <cstring>
#include <fstream>
#include <glm/glm.hpp>

// Define color macros for logging
#define GREEN "\033[32m"
#define MAGENTA "\033[35m"
#define RESET "\033[0m"

namespace VulkanRTX {

void VulkanRTX::createBottomLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                                    const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries) {
    if (geometries.empty()) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] No geometries provided for BLAS creation" << RESET << std::endl;
        throw VulkanRTXException("No geometries provided for BLAS creation.");
    }

    std::vector<VkAccelerationStructureGeometryKHR> asGeometries;
    std::vector<uint32_t> maxPrimitives;
    asGeometries.reserve(geometries.size());
    maxPrimitives.reserve(geometries.size());

    for (const auto& [vertexBuffer, indexBuffer, vertexCount, indexCount, vertexStride] : geometries) {
        VkAccelerationStructureGeometryKHR geometry = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .pNext = nullptr,
            .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
            .geometry = {
                .triangles = {
                    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
                    .pNext = nullptr,
                    .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
                    .vertexData = { .deviceAddress = getBufferDeviceAddress(vertexBuffer) },
                    .vertexStride = vertexStride,
                    .maxVertex = vertexCount,
                    .indexType = VK_INDEX_TYPE_UINT32,
                    .indexData = { .deviceAddress = indexBuffer ? getBufferDeviceAddress(indexBuffer) : 0 },
                    .transformData = { .deviceAddress = 0 }
                }
            },
            .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
        };
        asGeometries.push_back(geometry);
        maxPrimitives.push_back(indexCount / 3);
    }

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .pNext = nullptr,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .srcAccelerationStructure = VK_NULL_HANDLE,
        .dstAccelerationStructure = VK_NULL_HANDLE,
        .geometryCount = static_cast<uint32_t>(asGeometries.size()),
        .pGeometries = asGeometries.data(),
        .ppGeometries = nullptr,
        .scratchData = { .deviceAddress = 0 }
    };

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
        .pNext = nullptr,
        .accelerationStructureSize = 0,
        .updateScratchSize = 0,
        .buildScratchSize = 0
    };
    vkGetASBuildSizesFunc(device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, maxPrimitives.data(), &sizeInfo);

    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> blasBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> blasMemory(device_, VK_NULL_HANDLE, vkFreeMemory);
    createBuffer(physicalDevice, sizeInfo.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, blasBuffer, blasMemory);

    VkAccelerationStructureCreateInfoKHR createInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .createFlags = 0,
        .buffer = blasBuffer.get(),
        .offset = 0,
        .size = sizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .deviceAddress = 0
    };
    VkAccelerationStructureKHR asHandle;
    VK_CHECK(vkCreateASFunc(device_, &createInfo, nullptr, &asHandle), "Failed to create BLAS.");
    blas_ = VulkanResource<VkAccelerationStructureKHR, PFN_vkDestroyAccelerationStructureKHR>(device_, asHandle, vkDestroyASFunc);

    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> scratchBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> scratchMemory(device_, VK_NULL_HANDLE, vkFreeMemory);
    createBuffer(physicalDevice, sizeInfo.buildScratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer, scratchMemory);

    buildInfo.dstAccelerationStructure = blas_.get();
    buildInfo.scratchData.deviceAddress = getBufferDeviceAddress(scratchBuffer.get());

    std::vector<VkAccelerationStructureBuildRangeInfoKHR> buildRanges;
    for (const auto& [vertexBuffer, indexBuffer, vertexCount, indexCount, vertexStride] : geometries) {
        buildRanges.push_back({
            .primitiveCount = indexCount / 3,
            .primitiveOffset = 0,
            .firstVertex = 0,
            .transformOffset = 0
        });
    }
    std::vector<VkAccelerationStructureBuildRangeInfoKHR*> buildRangePtrs(buildRanges.size());
    for (size_t i = 0; i < buildRanges.size(); ++i) {
        buildRangePtrs[i] = &buildRanges[i];
    }

    VkCommandBuffer cmdBuffer = allocateTransientCommandBuffer(commandPool);
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo), "Failed to begin command buffer.");

    vkCmdBuildASFunc(cmdBuffer, 1, &buildInfo, buildRangePtrs.data());

    VK_CHECK(vkEndCommandBuffer(cmdBuffer), "Failed to end command buffer.");
    submitAndWaitTransient(cmdBuffer, queue, commandPool);

    primitiveCounts_ = std::move(buildRanges);
    std::osyncstream(std::cout) << GREEN << std::format("[INFO] Created bottom-level acceleration structure with {} geometries", geometries.size()) << RESET << std::endl;
}

void VulkanRTX::createTopLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                                 const std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4>>& instances) {
    if (instances.empty()) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] No instances provided for TLAS creation" << RESET << std::endl;
        throw VulkanRTXException("No instances provided for TLAS creation.");
    }

    VkDeviceSize instanceSize = instances.size() * sizeof(VkAccelerationStructureInstanceKHR);
    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> instanceBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> instanceMemory(device_, VK_NULL_HANDLE, vkFreeMemory);
    createBuffer(physicalDevice, instanceSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 instanceBuffer, instanceMemory);

    void* data;
    VK_CHECK(vkMapMemory(device_, instanceMemory.get(), 0, instanceSize, 0, &data), "Instance buffer map failed.");
    auto* instanceData = static_cast<VkAccelerationStructureInstanceKHR*>(data);
    for (uint32_t i = 0; i < instances.size(); ++i) {
        const auto& [blas, transform] = instances[i];
        VkAccelerationStructureDeviceAddressInfoKHR addressInfo = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .pNext = nullptr,
            .accelerationStructure = blas
        };
        instanceData[i].accelerationStructureReference = vkGetASDeviceAddressFunc(device_, &addressInfo);
        instanceData[i].transform.matrix[0][0] = transform[0][0];
        instanceData[i].transform.matrix[0][1] = transform[0][1];
        instanceData[i].transform.matrix[0][2] = transform[0][2];
        instanceData[i].transform.matrix[1][0] = transform[1][0];
        instanceData[i].transform.matrix[1][1] = transform[1][1];
        instanceData[i].transform.matrix[1][2] = transform[1][2];
        instanceData[i].transform.matrix[2][0] = transform[2][0];
        instanceData[i].transform.matrix[2][1] = transform[2][1];
        instanceData[i].transform.matrix[2][2] = transform[2][2];
        instanceData[i].transform.matrix[0][3] = transform[3][0];
        instanceData[i].transform.matrix[1][3] = transform[3][1];
        instanceData[i].transform.matrix[2][3] = transform[3][2];
        instanceData[i].instanceCustomIndex = i;
        instanceData[i].mask = 0xFF;
        instanceData[i].instanceShaderBindingTableRecordOffset = 0;
        instanceData[i].flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instanceData[i].accelerationStructureReference = vkGetASDeviceAddressFunc(device_, &addressInfo);
    }
    vkUnmapMemory(device_, instanceMemory.get());

    VkAccelerationStructureGeometryKHR asGeometry = {
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

    uint32_t primitiveCount = static_cast<uint32_t>(instances.size());
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .pNext = nullptr,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .srcAccelerationStructure = VK_NULL_HANDLE,
        .dstAccelerationStructure = VK_NULL_HANDLE,
        .geometryCount = 1,
        .pGeometries = &asGeometry,
        .ppGeometries = nullptr,
        .scratchData = { .deviceAddress = 0 }
    };

    VkAccelerationStructureBuildSizesInfoKHR buildSizes = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
        .pNext = nullptr,
        .accelerationStructureSize = 0,
        .updateScratchSize = 0,
        .buildScratchSize = 0
    };
    vkGetASBuildSizesFunc(device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &primitiveCount, &buildSizes);

    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> tlasBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> tlasMemory(device_, VK_NULL_HANDLE, vkFreeMemory);
    createBuffer(physicalDevice, buildSizes.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, tlasBuffer, tlasMemory);

    VkAccelerationStructureCreateInfoKHR createInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .createFlags = 0,
        .buffer = tlasBuffer.get(),
        .offset = 0,
        .size = buildSizes.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .deviceAddress = 0
    };
    VkAccelerationStructureKHR tempAS;
    VK_CHECK(vkCreateASFunc(device_, &createInfo, nullptr, &tempAS), "TLAS creation failed.");
    tlas_ = VulkanResource<VkAccelerationStructureKHR, PFN_vkDestroyAccelerationStructureKHR>(device_, tempAS, vkDestroyASFunc);

    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> scratchBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> scratchMemory(device_, VK_NULL_HANDLE, vkFreeMemory);
    createBuffer(physicalDevice, buildSizes.buildScratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer, scratchMemory);

    buildInfo.dstAccelerationStructure = tlas_.get();
    buildInfo.scratchData.deviceAddress = getBufferDeviceAddress(scratchBuffer.get());

    VkAccelerationStructureBuildRangeInfoKHR buildRanges = {
        .primitiveCount = primitiveCount,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0
    };
    VkAccelerationStructureBuildRangeInfoKHR* buildRangePtr = &buildRanges;

    VkCommandBuffer cmdBuffer = allocateTransientCommandBuffer(commandPool);
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo), "Failed to begin command buffer.");

    vkCmdBuildASFunc(cmdBuffer, 1, &buildInfo, &buildRangePtr);

    VK_CHECK(vkEndCommandBuffer(cmdBuffer), "Failed to end command buffer.");
    submitAndWaitTransient(cmdBuffer, queue, commandPool);

    std::osyncstream(std::cout) << GREEN << std::format("[INFO] Created top-level acceleration structure with {} instances", instances.size()) << RESET << std::endl;
}

void VulkanRTX::createStorageImage(VkPhysicalDevice physicalDevice, VkExtent2D extent, VkFormat format,
                                   VulkanResource<VkImage, PFN_vkDestroyImage>& image,
                                   VulkanResource<VkImageView, PFN_vkDestroyImageView>& imageView,
                                   VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>& memory) {
    VkImageCreateInfo imageInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = { extent.width, extent.height, 1 },
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

    VkImage tempImage;
    VK_CHECK(vkCreateImage(device_, &imageInfo, nullptr, &tempImage), std::format("Storage image creation failed for extent={}x{}", extent.width, extent.height));
    image = VulkanResource<VkImage, PFN_vkDestroyImage>(device_, tempImage, vkDestroyImage);

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device_, image.get(), &memReqs);

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = findMemoryType(physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    VkDeviceMemory tempMemory;
    VK_CHECK(vkAllocateMemory(device_, &allocInfo, nullptr, &tempMemory), "Storage image memory allocation failed.");
    memory = VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>(device_, tempMemory, vkFreeMemory);

    VK_CHECK(vkBindImageMemory(device_, image.get(), memory.get(), 0), "Storage image memory binding failed.");

    VkImageViewCreateInfo viewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = image.get(),
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    VkImageView tempImageView;
    VK_CHECK(vkCreateImageView(device_, &viewInfo, nullptr, &tempImageView), "Storage image view creation failed.");
    imageView = VulkanResource<VkImageView, PFN_vkDestroyImageView>(device_, tempImageView, vkDestroyImageView);

    std::osyncstream(std::cout) << GREEN << std::format("[INFO] Created storage image with extent {}x{}", extent.width, extent.height) << RESET << std::endl;
}

void VulkanRTX::recordRayTracingCommands(VkCommandBuffer cmdBuffer, VkExtent2D extent, VkImage outputImage,
                                         VkImageView outputImageView, const PushConstants& pc, VkAccelerationStructureKHR /*tlas*/) {
    VkDescriptorImageInfo imageInfo = {
        .sampler = VK_NULL_HANDLE,
        .imageView = outputImageView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };

    VkWriteDescriptorSet imageWrite = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = ds_.get(),
        .dstBinding = static_cast<uint32_t>(DescriptorBindings::StorageImage),
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .pImageInfo = &imageInfo,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr
    };

    vkUpdateDescriptorSets(device_, 1, &imageWrite, 0, nullptr);

    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = outputImage,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline_.get());

    VkDescriptorSet dsHandle = ds_.get();
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipelineLayout_.get(),
                            0, 1, &dsHandle, 0, nullptr);

    vkCmdPushConstants(cmdBuffer, rtPipelineLayout_.get(),
                       VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
                       0, sizeof(PushConstants), &pc);

    VkStridedDeviceAddressRegionKHR raygenSBT = sbt_.raygen;
    VkStridedDeviceAddressRegionKHR missSBT = sbt_.miss;
    VkStridedDeviceAddressRegionKHR hitSBT = sbt_.hit;
    VkStridedDeviceAddressRegionKHR callableSBT = sbt_.callable;

    vkCmdTraceRaysKHR(cmdBuffer, &raygenSBT, &missSBT, &hitSBT, &callableSBT, extent.width, extent.height, 1);

    std::osyncstream(std::cout) << GREEN << "[INFO] Recorded ray tracing commands" << RESET << std::endl;
}

void VulkanRTX::initializeRTX(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                              const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                              uint32_t maxRayRecursionDepth, const std::vector<DimensionData>& dimensionCache) {
    createDescriptorSetLayout();
    createDescriptorPoolAndSet();
    createRayTracingPipeline(maxRayRecursionDepth);
    createShaderBindingTable(physicalDevice);

    extent_ = { 1920, 1080 }; // Example default extent; adjust as needed
    createBottomLevelAS(physicalDevice, commandPool, graphicsQueue, geometries);

    std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4>> instances = {
        { blas_.get(), glm::mat4(1.0f) } // Example instance; adjust as needed
    };
    createTopLevelAS(physicalDevice, commandPool, graphicsQueue, instances);

    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> dimensionBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> dimensionMemory(device_, VK_NULL_HANDLE, vkFreeMemory);
    if (!dimensionCache.empty()) {
        VkDeviceSize dimSize = dimensionCache.size() * sizeof(DimensionData);
        VulkanResource<VkBuffer, PFN_vkDestroyBuffer> stagingBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
        VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> stagingMemory(device_, VK_NULL_HANDLE, vkFreeMemory);
        createBuffer(physicalDevice, dimSize,
                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingBuffer, stagingMemory);

        void* data;
        VK_CHECK(vkMapMemory(device_, stagingMemory.get(), 0, dimSize, 0, &data), "Dimension staging map failed.");
        memcpy(data, dimensionCache.data(), dimSize);
        vkUnmapMemory(device_, stagingMemory.get());

        createBuffer(physicalDevice, dimSize,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, dimensionBuffer, dimensionMemory);

        VkCommandBuffer cmdBuffer = allocateTransientCommandBuffer(commandPool);
        VkCommandBufferBeginInfo beginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr
        };
        VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo), "Failed to begin command buffer.");

        VkBufferCopy copyRegion = {
            .srcOffset = 0,
            .dstOffset = 0,
            .size = dimSize
        };
        vkCmdCopyBuffer(cmdBuffer, stagingBuffer.get(), dimensionBuffer.get(), 1, &copyRegion);

        VK_CHECK(vkEndCommandBuffer(cmdBuffer), "Failed to end command buffer.");
        submitAndWaitTransient(cmdBuffer, graphicsQueue, commandPool);
    }

    updateDescriptors(VK_NULL_HANDLE, VK_NULL_HANDLE, dimensionBuffer.get());
    updateDescriptorSetForTLAS(tlas_.get());

    previousDimensionCache_ = dimensionCache;
    std::osyncstream(std::cout) << GREEN << std::format("[INFO] Initialized RTX with {} geometries", geometries.size()) << RESET << std::endl;
}

void VulkanRTX::updateRTX(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                          const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                          const std::vector<DimensionData>& dimensionCache) {
    if (primitiveCounts_ != previousPrimitiveCounts_) {
        createBottomLevelAS(physicalDevice, commandPool, graphicsQueue, geometries);
        std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4>> instances = {
            { blas_.get(), glm::mat4(1.0f) } // Example instance; adjust as needed
        };
        createTopLevelAS(physicalDevice, commandPool, graphicsQueue, instances);
    }

    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> dimensionBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> dimensionMemory(device_, VK_NULL_HANDLE, vkFreeMemory);
    if (!dimensionCache.empty() && dimensionCache != previousDimensionCache_) {
        VkDeviceSize dimSize = dimensionCache.size() * sizeof(DimensionData);
        VulkanResource<VkBuffer, PFN_vkDestroyBuffer> stagingBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
        VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> stagingMemory(device_, VK_NULL_HANDLE, vkFreeMemory);
        createBuffer(physicalDevice, dimSize,
                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingBuffer, stagingMemory);

        void* data;
        VK_CHECK(vkMapMemory(device_, stagingMemory.get(), 0, dimSize, 0, &data), "Dimension staging map failed.");
        memcpy(data, dimensionCache.data(), dimSize);
        vkUnmapMemory(device_, stagingMemory.get());

        createBuffer(physicalDevice, dimSize,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, dimensionBuffer, dimensionMemory);

        VkCommandBuffer cmdBuffer = allocateTransientCommandBuffer(commandPool);
        VkCommandBufferBeginInfo beginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr
        };
        VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo), "Failed to begin command buffer.");

        VkBufferCopy copyRegion = {
            .srcOffset = 0,
            .dstOffset = 0,
            .size = dimSize
        };
        vkCmdCopyBuffer(cmdBuffer, stagingBuffer.get(), dimensionBuffer.get(), 1, &copyRegion);

        VK_CHECK(vkEndCommandBuffer(cmdBuffer), "Failed to end command buffer.");
        submitAndWaitTransient(cmdBuffer, graphicsQueue, commandPool);

        previousDimensionCache_ = dimensionCache;
    }

    updateDescriptors(VK_NULL_HANDLE, VK_NULL_HANDLE, dimensionBuffer.get());
    updateDescriptorSetForTLAS(tlas_.get());

    previousPrimitiveCounts_ = primitiveCounts_;
    std::osyncstream(std::cout) << GREEN << std::format("[INFO] Updated RTX with {} geometries", geometries.size()) << RESET << std::endl;
}

void VulkanRTX::denoiseImage(VkCommandBuffer cmdBuffer, VkImage inputImage, VkImageView inputImageView,
                             VkImage outputImage, VkImageView outputImageView) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Starting image denoising" << RESET << std::endl;

    VkDescriptorSetLayoutBinding bindings[] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = nullptr
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = nullptr
        }
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = 2,
        .pBindings = bindings
    };
    VkDescriptorSetLayout denoiseLayout;
    VK_CHECK(vkCreateDescriptorSetLayoutFunc(device_, &layoutInfo, nullptr, &denoiseLayout), "Denoise layout creation failed.");
    VulkanResource<VkDescriptorSetLayout, PFN_vkDestroyDescriptorSetLayout> denoiseLayoutResource(device_, denoiseLayout, vkDestroyDescriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = dsPool_.get(),
        .descriptorSetCount = 1,
        .pSetLayouts = &denoiseLayout
    };
    VkDescriptorSet dsHandle;
    VK_CHECK(vkAllocateDescriptorSetsFunc(device_, &allocInfo, &dsHandle), "Denoise descriptor set allocation failed.");
    VulkanDescriptorSet denoiseDS(device_, dsPool_.get(), dsHandle, vkFreeDescriptorSets);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = &denoiseLayout,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr
    };
    VkPipelineLayout denoiseLayoutPipeline;
    VK_CHECK(vkCreatePipelineLayout(device_, &pipelineLayoutInfo, nullptr, &denoiseLayoutPipeline), "Denoise pipeline layout creation failed.");
    VulkanResource<VkPipelineLayout, PFN_vkDestroyPipelineLayout> denoiseLayoutPipelineResource(device_, denoiseLayoutPipeline, vkDestroyPipelineLayout);

    // Placeholder for denoise shader; replace with actual shader path
    std::string denoiseShaderPath = shaderPaths_.empty() ? "" : shaderPaths_[0]; // Adjust as needed
    if (denoiseShaderPath.empty() || !shaderFileExists(denoiseShaderPath)) {
        std::osyncstream(std::cerr) << MAGENTA << std::format("[ERROR] Denoise shader not found: {}", denoiseShaderPath) << RESET << std::endl;
        throw VulkanRTXException(std::format("Denoise shader not found: {}.", denoiseShaderPath));
    }
    VkShaderModule denoiseModule = createShaderModule(denoiseShaderPath);

    VkPipelineShaderStageCreateInfo stageInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = denoiseModule,
        .pName = "main",
        .pSpecializationInfo = nullptr
    };

    VkComputePipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = stageInfo,
        .layout = denoiseLayoutPipeline,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };
    VkPipeline denoisePipeline;
    VK_CHECK(vkCreateComputePipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &denoisePipeline), "Denoise pipeline creation failed.");
    VulkanResource<VkPipeline, PFN_vkDestroyPipeline> denoisePipelineResource(device_, denoisePipeline, vkDestroyPipeline);

    VkDescriptorImageInfo inputImageInfo = {
        .sampler = VK_NULL_HANDLE,
        .imageView = inputImageView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };
    VkDescriptorImageInfo outputImageInfo = {
        .sampler = VK_NULL_HANDLE,
        .imageView = outputImageView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };

    std::vector<VkWriteDescriptorSet> writes = {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = dsHandle,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &inputImageInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = dsHandle,
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &outputImageInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        }
    };
    vkUpdateDescriptorSets(device_, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

    VkImageMemoryBarrier barriers[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = inputImage,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        },
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = outputImage,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        }
    };

    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 2, barriers);

    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, denoisePipeline);

    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, denoiseLayoutPipeline, 0, 1, &dsHandle, 0, nullptr);

    // Example dispatch size; adjust based on image dimensions
    uint32_t groupCountX = (extent_.width + 15) / 16;
    uint32_t groupCountY = (extent_.height + 15) / 16;
    vkCmdDispatch(cmdBuffer, groupCountX, groupCountY, 1);

    vkDestroyShaderModule(device_, denoiseModule, nullptr);
    // Resources are automatically cleaned up by VulkanResource and VulkanDescriptorSet destructors
    std::osyncstream(std::cout) << GREEN << "[INFO] Denoised image" << RESET << std::endl;
}

} // namespace VulkanRTX