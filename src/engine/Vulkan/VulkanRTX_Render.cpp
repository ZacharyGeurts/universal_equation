// VulkanRTX_Render.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
#include "engine/Vulkan/Vulkan_RTX.hpp"
#include <cstring>
#include <array>

namespace VulkanRTX {

// Get buffer device address
VkDeviceAddress VulkanRTX::getBufferDeviceAddress(VkBuffer buffer) {
    if (!buffer) {
        logger_.log(Logging::LogLevel::Error, "Null buffer provided for device address", std::source_location::current());
        throw VulkanRTXException("Null buffer provided for device address.");
    }
    VkBufferDeviceAddressInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = nullptr,
        .buffer = buffer
    };
    VkDeviceAddress address = vkGetBufferDeviceAddressFunc(device_, &bufferInfo);
    if (address == 0) {
        logger_.log(Logging::LogLevel::Error, "Invalid buffer device address (0)", std::source_location::current());
        throw VulkanRTXException("Invalid buffer device address (0).");
    }
    logger_.log(Logging::LogLevel::Debug, "Retrieved buffer device address: {}", std::source_location::current(), address);
    return address;
}

// Get acceleration structure device address
VkDeviceAddress VulkanRTX::getAccelerationStructureDeviceAddress(VkAccelerationStructureKHR as) {
    if (!as) {
        logger_.log(Logging::LogLevel::Error, "Null acceleration structure provided for device address", std::source_location::current());
        throw VulkanRTXException("Null acceleration structure provided for device address.");
    }
    VkAccelerationStructureDeviceAddressInfoKHR asInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .pNext = nullptr,
        .accelerationStructure = as
    };
    VkDeviceAddress address = vkGetASDeviceAddressFunc(device_, &asInfo);
    if (address == 0) {
        logger_.log(Logging::LogLevel::Error, "Invalid acceleration structure device address (0)", std::source_location::current());
        throw VulkanRTXException("Invalid acceleration structure device address (0).");
    }
    logger_.log(Logging::LogLevel::Debug, "Retrieved acceleration structure device address: {}", std::source_location::current(), address);
    return address;
}

// Update descriptor set for TLAS
void VulkanRTX::updateDescriptorSetForTLAS(VkAccelerationStructureKHR tlas) {
    if (!tlas) {
        logger_.log(Logging::LogLevel::Error, "Null TLAS provided for descriptor update", std::source_location::current());
        throw VulkanRTXException("Null TLAS provided for descriptor update.");
    }

    VkWriteDescriptorSetAccelerationStructureKHR asInfo = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .pNext = nullptr,
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &tlas
    };

    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = &asInfo,
        .dstSet = ds_.get(),
        .dstBinding = static_cast<uint32_t>(DescriptorBindings::TLAS),
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
        .pImageInfo = nullptr,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr
    };

    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
    logger_.log(Logging::LogLevel::Info, "Updated TLAS descriptor for acceleration structure", std::source_location::current());
}

// Record ray tracing commands
void VulkanRTX::recordRayTracingCommands(VkCommandBuffer cmdBuffer, VkExtent2D extent, VkImage outputImage,
                                        VkImageView outputImageView, const PushConstants& pc, VkAccelerationStructureKHR tlas) {
    logger_.log(Logging::LogLevel::Info, "Recording ray tracing commands for extent {}x{}", std::source_location::current(), extent.width, extent.height);

    // Update descriptor set for TLAS
    updateDescriptorSetForTLAS(tlas);

    // Update descriptor set for output image
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
    logger_.log(Logging::LogLevel::Debug, "Updated storage image descriptor for output image view", std::source_location::current());

    // Transition output image to GENERAL layout
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
    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
    logger_.log(Logging::LogLevel::Debug, "Transitioned output image to GENERAL layout", std::source_location::current());

    // Bind pipeline and descriptor sets
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline_.get());
    VkDescriptorSet descriptorSet = ds_.get();
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipelineLayout_.get(),
                            0, 1, &descriptorSet, 0, nullptr);
    logger_.log(Logging::LogLevel::Debug, "Bound ray tracing pipeline and descriptor sets", std::source_location::current());

    // Push constants
    vkCmdPushConstants(cmdBuffer, rtPipelineLayout_.get(),
                       VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                       VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR |
                       VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR,
                       0, sizeof(PushConstants), &pc);
    logger_.log(Logging::LogLevel::Debug, "Pushed constants with clearColor=({}, {}, {}, {}), lightIntensity={}, samplesPerPixel={}, maxDepth={}",
                std::source_location::current(), pc.clearColor.x, pc.clearColor.y, pc.clearColor.z, pc.clearColor.w,
                pc.lightIntensity, pc.samplesPerPixel, pc.maxDepth);

    // Trace rays
    vkCmdTraceRaysKHR(cmdBuffer, &sbt_.raygen, &sbt_.miss, &sbt_.hit, &sbt_.callable,
                      extent.width, extent.height, 1);
    logger_.log(Logging::LogLevel::Info, "Issued ray tracing command", std::source_location::current());
}

// Denoise image
void VulkanRTX::denoiseImage(VkCommandBuffer cmdBuffer, VkImage inputImage, VkImageView inputImageView,
                             VkImage outputImage, VkImageView outputImageView) {
    logger_.log(Logging::LogLevel::Info, "Starting image denoising", std::source_location::current());

    // Update descriptor set for denoising
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
    std::array<VkWriteDescriptorSet, 2> writes = {{
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = ds_.get(),
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::StorageImage),
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
            .dstSet = ds_.get(),
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::DenoiseImage),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &outputImageInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        }
    }};
    vkUpdateDescriptorSets(device_, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    logger_.log(Logging::LogLevel::Debug, "Updated {} denoising descriptors", std::source_location::current(), writes.size());

    // Transition images
    std::array<VkImageMemoryBarrier, 2> barriers = {{
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
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
    }};
    vkUpdateDescriptorSets(device_, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    logger_.log(Logging::LogLevel::Debug, "Transitioned {} images for denoising", std::source_location::current(), barriers.size());

    // Note: Actual denoising compute pipeline binding and dispatch would be implemented here
    // This is a placeholder as the compute pipeline setup is not provided
    logger_.log(Logging::LogLevel::Warning, "Denoising compute pipeline not implemented", std::source_location::current());
}

} // namespace VulkanRTX