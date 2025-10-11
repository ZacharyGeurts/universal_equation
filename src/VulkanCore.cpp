// VulkanCore.cpp
// AMOURANTH RTX Engine, October 2025 - Implementation of core Vulkan utilities.
// Dependencies: Vulkan 1.3+, GLM, logging.hpp, ue_init.hpp.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#include "VulkanCore.hpp"
#include "engine/logging.hpp"
#include "ue_init.hpp"
#include <stdexcept>

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
            LOG_ERROR("Failed to create buffer size: {}", size);
            throw std::runtime_error("Failed to create buffer");
        }
        LOG_INFO("Created buffer");
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
            LOG_ERROR("Failed to allocate buffer memory size: {}", memRequirements.size);
            throw std::runtime_error("Failed to allocate buffer memory");
        }
        LOG_INFO("Allocated buffer memory");
        vkBindBufferMemory(device, buffer, bufferMemory, 0);
    }

    VkDeviceAddress getBufferDeviceAddress(VkDevice device, VkBuffer buffer) {
        auto vkGetBufferDeviceAddress = reinterpret_cast<PFN_vkGetBufferDeviceAddress>(vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddress"));
        if (!vkGetBufferDeviceAddress) {
            LOG_ERROR("Failed to load vkGetBufferDeviceAddress");
            throw std::runtime_error("Failed to load vkGetBufferDeviceAddress");
        }
        VkBufferDeviceAddressInfo addressInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .pNext = nullptr,
            .buffer = buffer
        };
        return vkGetBufferDeviceAddress(device, &addressInfo);
    }
}