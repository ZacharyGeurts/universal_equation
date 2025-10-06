// VulkanRTX_Utility.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
#include "engine/Vulkan/Vulkan_RTX.hpp"
#include <stdexcept>
#include <format>
#include <syncstream>
#include <iostream>

// Define color macros for logging
#define GREEN "\033[32m"
#define MAGENTA "\033[35m"
#define RESET "\033[0m"

namespace VulkanRTX {

uint32_t VulkanRTX::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to find suitable memory type" << RESET << std::endl;
    throw VulkanRTXException("Failed to find suitable memory type.");
}

void VulkanRTX::createBuffer(VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
                             VkMemoryPropertyFlags props, VulkanResource<VkBuffer, PFN_vkDestroyBuffer>& buffer,
                             VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>& memory) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Creating buffer of size " << size << RESET << std::endl;

    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr
    };

    VkBuffer tempBuffer;
    VK_CHECK(vkCreateBuffer(device_, &bufferInfo, nullptr, &tempBuffer), "Buffer creation failed");

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device_, tempBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, props)
    };

    VkDeviceMemory tempMemory;
    VK_CHECK(vkAllocateMemory(device_, &allocInfo, nullptr, &tempMemory), "Memory allocation failed");

    VK_CHECK(vkBindBufferMemory(device_, tempBuffer, tempMemory, 0), "Buffer memory binding failed");

    buffer = VulkanResource<VkBuffer, PFN_vkDestroyBuffer>(device_, tempBuffer, vkDestroyBuffer);
    memory = VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>(device_, tempMemory, vkFreeMemory);

    std::osyncstream(std::cout) << GREEN << "[INFO] Created buffer successfully" << RESET << std::endl;
}

VkCommandBuffer VulkanRTX::allocateTransientCommandBuffer(VkCommandPool commandPool) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Allocating transient command buffer" << RESET << std::endl;

    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    VkCommandBuffer commandBuffer;
    VK_CHECK(vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer), "Command buffer allocation failed");
    return commandBuffer;
}

void VulkanRTX::submitAndWaitTransient(VkCommandBuffer cmdBuffer, VkQueue queue, VkCommandPool commandPool) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Submitting and waiting for transient command buffer" << RESET << std::endl;

    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .pWaitDstStageMask = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmdBuffer,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = nullptr
    };

    VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE), "Queue submission failed");
    VK_CHECK(vkQueueWaitIdle(queue), "Queue wait idle failed");

    vkFreeCommandBuffers(device_, commandPool, 1, &cmdBuffer);
    std::osyncstream(std::cout) << GREEN << "[INFO] Transient command buffer submitted and freed" << RESET << std::endl;
}

} // namespace VulkanRTX