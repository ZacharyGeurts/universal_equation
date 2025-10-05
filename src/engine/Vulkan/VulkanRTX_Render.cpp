// AMOURANTH RTX © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
#include "engine/Vulkan/Vulkan_RTX.hpp"
#include <array>
#include <vector>
#include <tuple>
#include <cstring>
#include <fmt/format.h>

void VulkanRTX::createStorageImage(
    VkPhysicalDevice physicalDevice, VkExtent2D extent, VkFormat format,
    VulkanResource<VkImage, PFN_vkDestroyImage>& image,
    VulkanResource<VkImageView, PFN_vkDestroyImageView>& imageView,
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>& memory) {
    if (!physicalDevice || extent.width == 0 || extent.height == 0) {
        throw VulkanRTXException("Invalid image params: null device or zero extent.");
    }
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent = {extent.width, extent.height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage tempImage = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImage(device_, &imageInfo, nullptr, &tempImage), "Storage image creation failed.");
    image = VulkanResource<VkImage, PFN_vkDestroyImage>(device_, tempImage, vkDestroyImage);

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device_, image.get(), &memReqs);
    uint32_t memType = findMemoryType(physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = memType;

    VkDeviceMemory tempMemory = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(device_, &allocInfo, nullptr, &tempMemory), "Storage image memory allocation failed.");
    memory = VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>(device_, tempMemory, vkFreeMemory);
    VK_CHECK(vkBindImageMemory(device_, image.get(), memory.get(), 0), "Storage image memory binding failed.");

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image.get();
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    VkImageView tempImageView = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImageView(device_, &viewInfo, nullptr, &tempImageView), "Storage image view creation failed.");
    imageView = VulkanResource<VkImageView, PFN_vkDestroyImageView>(device_, tempImageView, vkDestroyImageView);
}

void VulkanRTX::recordRayTracingCommands(
    VkCommandBuffer cmdBuffer, VkExtent2D extent, VkImage outputImage, VkImageView outputImageView,
    const PushConstants& pc, VkAccelerationStructureKHR tlas) {
    if (!cmdBuffer || !outputImage || !outputImageView) {
        throw VulkanRTXException("Null cmd buffer/image/view for ray tracing.");
    }
    if (tlas == VK_NULL_HANDLE) tlas = tlas_.get();
    if (tlas != tlas_.get()) updateDescriptorSetForTLAS(tlas);

    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageView = outputImageView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet imageWrite = {};
    imageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    imageWrite.dstSet = ds_.get();
    imageWrite.dstBinding = static_cast<uint32_t>(DescriptorBindings::StorageImage);
    imageWrite.dstArrayElement = 0;
    imageWrite.descriptorCount = 1;
    imageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    imageWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device_, 1, &imageWrite, 0, nullptr);

    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = outputImage;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

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
}

void VulkanRTX::initializeRTX(
    VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
    const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
    uint32_t maxRayRecursionDepth,
    const std::vector<DimensionData>& dimensionCache) {
    if (!physicalDevice || !commandPool || !graphicsQueue || geometries.empty()) {
        throw VulkanRTXException("Invalid init params: null device/pool/queue or empty geometries.");
    }
    for (const auto& [vertexBuffer, indexBuffer, vertexCount, indexCount, vertexStride] : geometries) {
        if (!vertexBuffer || !indexBuffer || vertexCount == 0 || indexCount == 0 || vertexStride == 0) {
            throw VulkanRTXException("Invalid geometry: null buffer or zero count/stride.");
        }
    }

    createDescriptorSetLayout();
    createDescriptorPoolAndSet();
    createRayTracingPipeline(maxRayRecursionDepth);
    createShaderBindingTable(physicalDevice);

    primitiveCounts_.clear();
    primitiveCounts_.reserve(geometries.size());
    for (const auto& [_, __, ___, indexCount, ____] : geometries) {
        VkAccelerationStructureBuildRangeInfoKHR rangeInfo = {};
        rangeInfo.primitiveCount = indexCount / 3;
        rangeInfo.primitiveOffset = 0;
        rangeInfo.firstVertex = 0;
        rangeInfo.transformOffset = 0;
        primitiveCounts_.push_back(rangeInfo);
    }
    createBottomLevelAS(physicalDevice, commandPool, graphicsQueue, geometries);

    std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4>> instances = {
        {blas_.get(), glm::mat4(1.0f)}
    };
    createTopLevelAS(physicalDevice, commandPool, graphicsQueue, instances);

    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> dimensionBufferResource(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> dimensionMemoryResource(device_, VK_NULL_HANDLE, vkFreeMemory);
    if (!dimensionCache.empty()) {
        VkDeviceSize dimSize = static_cast<VkDeviceSize>(dimensionCache.size()) * sizeof(DimensionData);
        VulkanResource<VkBuffer, PFN_vkDestroyBuffer> dimBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
        VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> dimMemory(device_, VK_NULL_HANDLE, vkFreeMemory);
        createBuffer(physicalDevice, dimSize,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, dimBuffer, dimMemory);

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
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo), "Failed to begin command buffer");
        VkBufferCopy copyRegion = {0, 0, dimSize};
        vkCmdCopyBuffer(cmdBuffer, stagingBuffer.get(), dimBuffer.get(), 1, &copyRegion);
        VK_CHECK(vkEndCommandBuffer(cmdBuffer), "Failed to end command buffer");
        submitAndWaitTransient(cmdBuffer, graphicsQueue, commandPool);

        dimensionBufferResource = std::move(dimBuffer);
        dimensionMemoryResource = std::move(dimMemory);
    }

    VkBuffer cameraBuffer = VK_NULL_HANDLE, materialBuffer = VK_NULL_HANDLE;
    updateDescriptors(cameraBuffer, materialBuffer, dimensionBufferResource.get());
}

void VulkanRTX::updateRTX(
    VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
    const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
    const std::vector<DimensionData>& dimensionCache) {
    if (!physicalDevice || !commandPool || !graphicsQueue || geometries.empty()) {
        throw VulkanRTXException("Invalid update params.");
    }

    primitiveCounts_.clear();
    primitiveCounts_.reserve(geometries.size());
    bool geometriesChanged = false;
    for (size_t i = 0; i < geometries.size(); ++i) {
        const auto& [_, __, ___, indexCount, ____] = geometries[i];
        uint32_t newCount = indexCount / 3;
        VkAccelerationStructureBuildRangeInfoKHR rangeInfo = {};
        rangeInfo.primitiveCount = newCount;
        rangeInfo.primitiveOffset = 0;
        rangeInfo.firstVertex = 0;
        rangeInfo.transformOffset = 0;
        primitiveCounts_.push_back(rangeInfo);
        if (i < previousPrimitiveCounts_.size() && newCount != previousPrimitiveCounts_[i].primitiveCount) {
            geometriesChanged = true;
        }
    }
    previousPrimitiveCounts_ = primitiveCounts_;

    if (geometriesChanged) {
        createBottomLevelAS(physicalDevice, commandPool, graphicsQueue, geometries);
        std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4>> instances = {
            {blas_.get(), glm::mat4(1.0f)}
        };
        createTopLevelAS(physicalDevice, commandPool, graphicsQueue, instances);
    }

    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> dimensionBufferResource(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> dimensionMemoryResource(device_, VK_NULL_HANDLE, vkFreeMemory);
    if (!dimensionCache.empty()) {
        if (dimensionCache != previousDimensionCache_) {
            VkDeviceSize dimSize = static_cast<VkDeviceSize>(dimensionCache.size()) * sizeof(DimensionData);
            VulkanResource<VkBuffer, PFN_vkDestroyBuffer> dimBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
            VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> dimMemory(device_, VK_NULL_HANDLE, vkFreeMemory);
            createBuffer(physicalDevice, dimSize,
                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, dimBuffer, dimMemory);

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
            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo), "Failed to begin command buffer");
            VkBufferCopy copyRegion = {0, 0, dimSize};
            vkCmdCopyBuffer(cmdBuffer, stagingBuffer.get(), dimBuffer.get(), 1, &copyRegion);
            VK_CHECK(vkEndCommandBuffer(cmdBuffer), "Failed to end command buffer");
            submitAndWaitTransient(cmdBuffer, graphicsQueue, commandPool);

            dimensionBufferResource = std::move(dimBuffer);
            dimensionMemoryResource = std::move(dimMemory);
            previousDimensionCache_ = dimensionCache;
        }
    }

    VkBuffer cameraBuffer = VK_NULL_HANDLE, materialBuffer = VK_NULL_HANDLE;
    updateDescriptors(cameraBuffer, materialBuffer, dimensionBufferResource.get());
}

void VulkanRTX::denoiseImage(VkCommandBuffer cmdBuffer, VkImage inputImage, VkImageView inputImageView, VkImage outputImage, VkImageView outputImageView) {
    if (!cmdBuffer || !inputImageView || !outputImageView) {
        throw VulkanRTXException("Null denoise params.");
    }

    VkShaderModule denoiseModule = createShaderModule("assets/shaders/denoise.spv");
    VkPipelineShaderStageCreateInfo stage = {};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = denoiseModule;
    stage.pName = "main";

    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = dsLayout_.getPtr();
    VkPipelineLayout denoiseLayout;
    VK_CHECK(vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &denoiseLayout), "Denoise layout failed.");

    VkComputePipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = stage;
    pipelineInfo.layout = denoiseLayout;
    VkPipeline denoisePipeline;
    VK_CHECK(vkCreateComputePipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &denoisePipeline), "Denoise pipeline failed.");

    std::array<VkDescriptorImageInfo, 2> imageInfos = {{
        {VK_NULL_HANDLE, inputImageView, VK_IMAGE_LAYOUT_GENERAL},
        {VK_NULL_HANDLE, outputImageView, VK_IMAGE_LAYOUT_GENERAL}
    }};
    std::array<VkWriteDescriptorSet, 2> writes = {{
        {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, ds_.get(),
            static_cast<uint32_t>(DescriptorBindings::StorageImage), 0, 1,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imageInfos[0], nullptr, nullptr
        },
        {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, ds_.get(),
            static_cast<uint32_t>(DescriptorBindings::DenoiseImage), 0, 1,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imageInfos[1], nullptr, nullptr
        }
    }};
    vkUpdateDescriptorSets(device_, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

    std::array<VkImageMemoryBarrier, 2> barriers = {{
        {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, 0, VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
            inputImage, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        },
        {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, 0, VK_ACCESS_SHADER_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
            outputImage, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        }
    }};
    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 2, barriers.data());

    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, denoisePipeline);
    VkDescriptorSet dsHandle = ds_.get();
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, denoiseLayout, 0, 1, &dsHandle, 0, nullptr);
    constexpr uint32_t WORKGROUP_SIZE = 16;
    vkCmdDispatch(cmdBuffer, (1920 + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE, (1080 + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE, 1);

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
}