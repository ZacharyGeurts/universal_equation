// VulkanBufferManager.cpp
// AMOURANTH RTX Engine, October 2025 - Manages Vulkan buffer creation and memory allocation.
// Dependencies: Vulkan 1.3+, GLM, VulkanCore.hpp, logging.hpp, ue_init.hpp.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#include "VulkanBufferManager.hpp"
#include "ue_init.hpp"
#include "engine/logging.hpp"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <cstring>
#include <source_location>

VulkanBufferManager::VulkanBufferManager(VulkanContext& context)
    : context_(context), vertexBuffer_(VK_NULL_HANDLE), vertexBufferMemory_(VK_NULL_HANDLE), vertexBufferAddress_(0),
      indexBuffer_(VK_NULL_HANDLE), indexBufferMemory_(VK_NULL_HANDLE), indexBufferAddress_(0),
      scratchBuffer_(VK_NULL_HANDLE), scratchBufferMemory_(VK_NULL_HANDLE), scratchBufferAddress_(0), indexCount_(0) {
    LOG_INFO("Initialized VulkanBufferManager", std::source_location::current());
}

VulkanBufferManager::~VulkanBufferManager() {
    cleanupBuffers();
}

void VulkanBufferManager::initializeBuffers(std::span<const glm::vec3> vertices, std::span<const uint32_t> indices) {
    VkDeviceSize vertexBufferSize = sizeof(glm::vec3) * vertices.size();
    VkDeviceSize indexBufferSize = sizeof(uint32_t) * indices.size();
    vertexCount_ = static_cast<uint32_t>(vertices.size());
    indexCount_ = static_cast<uint32_t>(indices.size());

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    VulkanInitializer::createBuffer(
        context_.device, context_.physicalDevice, vertexBufferSize + indexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingBufferMemory
    );
    LOG_INFO_CAT("Vulkan", "Created staging buffer: {:p}", std::source_location::current(), static_cast<void*>(stagingBuffer));

    void* data;
    vkMapMemory(context_.device, stagingBufferMemory, 0, vertexBufferSize + indexBufferSize, 0, &data);
    memcpy(data, vertices.data(), vertexBufferSize);
    memcpy(static_cast<char*>(data) + vertexBufferSize, indices.data(), indexBufferSize);
    vkUnmapMemory(context_.device, stagingBufferMemory);

    VulkanInitializer::createBuffer(
        context_.device, context_.physicalDevice, vertexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        vertexBuffer_, vertexBufferMemory_
    );
    vertexBufferAddress_ = VulkanInitializer::getBufferDeviceAddress(context_.device, vertexBuffer_);
    LOG_INFO_CAT("Vulkan", "Created vertex buffer: {:p}", std::source_location::current(), static_cast<void*>(vertexBuffer_));

    VulkanInitializer::createBuffer(
        context_.device, context_.physicalDevice, indexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        indexBuffer_, indexBufferMemory_
    );
    indexBufferAddress_ = VulkanInitializer::getBufferDeviceAddress(context_.device, indexBuffer_);
    LOG_INFO_CAT("Vulkan", "Created index buffer: {:p}", std::source_location::current(), static_cast<void*>(indexBuffer_));

    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = context_.commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VkCommandBuffer commandBuffer;
    if (vkAllocateCommandBuffers(context_.device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate command buffer", std::source_location::current());
        vkDestroyBuffer(context_.device, stagingBuffer, nullptr);
        vkFreeMemory(context_.device, stagingBufferMemory, nullptr);
        throw std::runtime_error("Failed to allocate command buffer");
    }
    LOG_INFO_CAT("Vulkan", "Allocated command buffer: {:p}", std::source_location::current(), static_cast<void*>(commandBuffer));

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        LOG_ERROR("Failed to begin command buffer", std::source_location::current());
        vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &commandBuffer);
        vkDestroyBuffer(context_.device, stagingBuffer, nullptr);
        vkFreeMemory(context_.device, stagingBufferMemory, nullptr);
        throw std::runtime_error("Failed to begin command buffer");
    }

    VkBufferCopy vertexCopyRegion{
        .srcOffset = 0,
        .dstOffset = 0,
        .size = vertexBufferSize
    };
    vkCmdCopyBuffer(commandBuffer, stagingBuffer, vertexBuffer_, 1, &vertexCopyRegion);

    VkBufferCopy indexCopyRegion{
        .srcOffset = vertexBufferSize,
        .dstOffset = 0,
        .size = indexBufferSize
    };
    vkCmdCopyBuffer(commandBuffer, stagingBuffer, indexBuffer_, 1, &indexCopyRegion);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        LOG_ERROR("Failed to end command buffer", std::source_location::current());
        vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &commandBuffer);
        vkDestroyBuffer(context_.device, stagingBuffer, nullptr);
        vkFreeMemory(context_.device, stagingBufferMemory, nullptr);
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
    if (vkQueueSubmit(context_.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        LOG_ERROR("Failed to submit command buffer", std::source_location::current());
        vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &commandBuffer);
        vkDestroyBuffer(context_.device, stagingBuffer, nullptr);
        vkFreeMemory(context_.device, stagingBufferMemory, nullptr);
        throw std::runtime_error("Failed to submit command buffer");
    }

    vkQueueWaitIdle(context_.graphicsQueue);
    vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &commandBuffer);
    vkDestroyBuffer(context_.device, stagingBuffer, nullptr);
    vkFreeMemory(context_.device, stagingBufferMemory, nullptr);
}

void VulkanBufferManager::createUniformBuffers(uint32_t count) {
    uniformBuffers_.resize(count);
    uniformBufferMemories_.resize(count);
    VkDeviceSize bufferSize = sizeof(UE::UniformBufferObject);
    for (uint32_t i = 0; i < count; ++i) {
        VulkanInitializer::createBuffer(
            context_.device, context_.physicalDevice, bufferSize,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            uniformBuffers_[i], uniformBufferMemories_[i]
        );
        LOG_INFO_CAT("Vulkan", "Created uniform buffer[{}]: {:p}", std::source_location::current(), i, static_cast<void*>(uniformBuffers_[i]));
    }
}

void VulkanBufferManager::createScratchBuffer(VkDeviceSize size) {
    VulkanInitializer::createBuffer(
        context_.device, context_.physicalDevice, size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        scratchBuffer_, scratchBufferMemory_
    );
    scratchBufferAddress_ = VulkanInitializer::getBufferDeviceAddress(context_.device, scratchBuffer_);
    LOG_INFO_CAT("Vulkan", "Created scratch buffer: {:p}", std::source_location::current(), static_cast<void*>(scratchBuffer_));
}

void VulkanBufferManager::cleanupBuffers() {
    for (auto buffer : uniformBuffers_) {
        if (buffer != VK_NULL_HANDLE) {
            LOG_INFO_CAT("Vulkan", "Destroying uniform buffer: {:p}", std::source_location::current(), static_cast<void*>(buffer));
            vkDestroyBuffer(context_.device, buffer, nullptr);
        }
    }
    for (auto memory : uniformBufferMemories_) {
        if (memory != VK_NULL_HANDLE) {
            LOG_INFO_CAT("Vulkan", "Freeing uniform buffer memory: {:p}", std::source_location::current(), static_cast<void*>(memory));
            vkFreeMemory(context_.device, memory, nullptr);
        }
    }
    if (vertexBuffer_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", "Destroying vertex buffer: {:p}", std::source_location::current(), static_cast<void*>(vertexBuffer_));
        vkDestroyBuffer(context_.device, vertexBuffer_, nullptr);
    }
    if (vertexBufferMemory_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", "Freeing vertex buffer memory: {:p}", std::source_location::current(), static_cast<void*>(vertexBufferMemory_));
        vkFreeMemory(context_.device, vertexBufferMemory_, nullptr);
    }
    if (indexBuffer_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", "Destroying index buffer: {:p}", std::source_location::current(), static_cast<void*>(indexBuffer_));
        vkDestroyBuffer(context_.device, indexBuffer_, nullptr);
    }
    if (indexBufferMemory_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", "Freeing index buffer memory: {:p}", std::source_location::current(), static_cast<void*>(indexBufferMemory_));
        vkFreeMemory(context_.device, indexBufferMemory_, nullptr);
    }
    if (scratchBuffer_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", "Destroying scratch buffer: {:p}", std::source_location::current(), static_cast<void*>(scratchBuffer_));
        vkDestroyBuffer(context_.device, scratchBuffer_, nullptr);
    }
    if (scratchBufferMemory_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", "Freeing scratch buffer memory: {:p}", std::source_location::current(), static_cast<void*>(scratchBufferMemory_));
        vkFreeMemory(context_.device, scratchBufferMemory_, nullptr);
    }
}