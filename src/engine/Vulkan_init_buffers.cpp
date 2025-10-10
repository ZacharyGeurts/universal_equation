// src/engine/Vulkan_init_buffers.cpp
#include "engine/Vulkan_init_buffers.hpp"
#include "engine/Vulkan_init.hpp"
#include "engine/logging.hpp"
#include <stdexcept>
#include <source_location>
#include <glm/glm.hpp>

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

VulkanBufferManager::VulkanBufferManager(VulkanContext& context)
    : context_(context) {
    LOG_INFO_CAT("VulkanBuffer", "Constructing VulkanBufferManager with context.device={:p}",
                 std::source_location::current(), static_cast<void*>(context_.device));
}

VulkanBufferManager::~VulkanBufferManager() {
    LOG_INFO_CAT("VulkanBuffer", "Destroying VulkanBufferManager", std::source_location::current());
    cleanupBuffers();
}

void VulkanBufferManager::initializeBuffers(std::span<const glm::vec3> vertices, std::span<const uint32_t> indices) {
    LOG_INFO_CAT("VulkanBuffer", "Initializing buffers with {} vertices and {} indices",
                 std::source_location::current(), vertices.size(), indices.size());

    if (vertices.empty()) {
        LOG_ERROR_CAT("VulkanBuffer", "Empty vertex data provided", std::source_location::current());
        throw std::runtime_error("Empty vertex data provided");
    }

    VkDeviceSize vertexBufferSize = vertices.size() * sizeof(glm::vec3);
    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, vertexBufferSize,
                                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   context_.sphereStagingBuffer, context_.sphereStagingBufferMemory);

    void* vertexData;
    VkResult result = vkMapMemory(context_.device, context_.sphereStagingBufferMemory, 0, vertexBufferSize, 0, &vertexData);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanBuffer", "Failed to map vertex staging buffer memory: result={}",
                      std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to map vertex staging buffer memory");
    }
    memcpy(vertexData, vertices.data(), vertexBufferSize);
    vkUnmapMemory(context_.device, context_.sphereStagingBufferMemory);
    LOG_DEBUG_CAT("VulkanBuffer", "Copied {} bytes to vertex staging buffer", std::source_location::current(), vertexBufferSize);

    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, vertexBufferSize,
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                   context_.vertexBuffer, context_.vertexBufferMemory);
    LOG_DEBUG_CAT("VulkanBuffer", "Created vertex buffer: vertexBuffer={:p}", std::source_location::current(), static_cast<void*>(context_.vertexBuffer));

    copyBuffer(context_.sphereStagingBuffer, context_.vertexBuffer, vertexBufferSize);
    LOG_DEBUG_CAT("VulkanBuffer", "Vertex data copied to device-local buffer", std::source_location::current());

    if (!indices.empty()) {
        VkDeviceSize indexBufferSize = indices.size() * sizeof(uint32_t);
        VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, indexBufferSize,
                                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                       context_.indexStagingBuffer, context_.indexStagingBufferMemory);

        void* indexData;
        result = vkMapMemory(context_.device, context_.indexStagingBufferMemory, 0, indexBufferSize, 0, &indexData);
        if (result != VK_SUCCESS) {
            LOG_ERROR_CAT("VulkanBuffer", "Failed to map index staging buffer memory: result={}",
                          std::source_location::current(), vkResultToString(result));
            throw std::runtime_error("Failed to map index staging buffer memory");
        }
        memcpy(indexData, indices.data(), indexBufferSize);
        vkUnmapMemory(context_.device, context_.indexStagingBufferMemory);
        LOG_DEBUG_CAT("VulkanBuffer", "Copied {} bytes to index staging buffer", std::source_location::current(), indexBufferSize);

        VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, indexBufferSize,
                                       VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                       context_.indexBuffer, context_.indexBufferMemory);
        LOG_DEBUG_CAT("VulkanBuffer", "Created index buffer: indexBuffer={:p}", std::source_location::current(), static_cast<void*>(context_.indexBuffer));

        copyBuffer(context_.indexStagingBuffer, context_.indexBuffer, indexBufferSize);
        LOG_DEBUG_CAT("VulkanBuffer", "Index data copied to device-local buffer", std::source_location::current());
    }

    LOG_INFO_CAT("VulkanBuffer", "Buffers initialized successfully", std::source_location::current());
}

void VulkanBufferManager::updateVertexBuffers(std::span<const glm::vec3> vertices, std::span<const uint32_t> indices) {
    LOG_INFO_CAT("VulkanBuffer", "Updating vertex buffers with {} vertices and {} indices",
                 std::source_location::current(), vertices.size(), indices.size());

    cleanupBuffers();
    initializeBuffers(vertices, indices);
}

void VulkanBufferManager::cleanupBuffers() {
    LOG_DEBUG_CAT("VulkanBuffer", "Cleaning up buffers", std::source_location::current());
    if (context_.device == VK_NULL_HANDLE) {
        LOG_WARNING_CAT("VulkanBuffer", "Device is null, skipping cleanup", std::source_location::current());
        return;
    }

    vkDeviceWaitIdle(context_.device);
    LOG_DEBUG_CAT("VulkanBuffer", "Device idle, proceeding with cleanup", std::source_location::current());

    if (context_.vertexBuffer != VK_NULL_HANDLE) {
        LOG_DEBUG_CAT("VulkanBuffer", "Destroying vertexBuffer={:p}", std::source_location::current(), static_cast<void*>(context_.vertexBuffer));
        vkDestroyBuffer(context_.device, context_.vertexBuffer, nullptr);
        context_.vertexBuffer = VK_NULL_HANDLE;
    }
    if (context_.vertexBufferMemory != VK_NULL_HANDLE) {
        LOG_DEBUG_CAT("VulkanBuffer", "Freeing vertexBufferMemory={:p}", std::source_location::current(), static_cast<void*>(context_.vertexBufferMemory));
        vkFreeMemory(context_.device, context_.vertexBufferMemory, nullptr);
        context_.vertexBufferMemory = VK_NULL_HANDLE;
    }
    if (context_.indexBuffer != VK_NULL_HANDLE) {
        LOG_DEBUG_CAT("VulkanBuffer", "Destroying indexBuffer={:p}", std::source_location::current(), static_cast<void*>(context_.indexBuffer));
        vkDestroyBuffer(context_.device, context_.indexBuffer, nullptr);
        context_.indexBuffer = VK_NULL_HANDLE;
    }
    if (context_.indexBufferMemory != VK_NULL_HANDLE) {
        LOG_DEBUG_CAT("VulkanBuffer", "Freeing indexBufferMemory={:p}", std::source_location::current(), static_cast<void*>(context_.indexBufferMemory));
        vkFreeMemory(context_.device, context_.indexBufferMemory, nullptr);
        context_.indexBufferMemory = VK_NULL_HANDLE;
    }
    if (context_.sphereStagingBuffer != VK_NULL_HANDLE) {
        LOG_DEBUG_CAT("VulkanBuffer", "Destroying sphereStagingBuffer={:p}", std::source_location::current(), static_cast<void*>(context_.sphereStagingBuffer));
        vkDestroyBuffer(context_.device, context_.sphereStagingBuffer, nullptr);
        context_.sphereStagingBuffer = VK_NULL_HANDLE;
    }
    if (context_.sphereStagingBufferMemory != VK_NULL_HANDLE) {
        LOG_DEBUG_CAT("VulkanBuffer", "Freeing sphereStagingBufferMemory={:p}", std::source_location::current(), static_cast<void*>(context_.sphereStagingBufferMemory));
        vkFreeMemory(context_.device, context_.sphereStagingBufferMemory, nullptr);
        context_.sphereStagingBufferMemory = VK_NULL_HANDLE;
    }
    if (context_.indexStagingBuffer != VK_NULL_HANDLE) {
        LOG_DEBUG_CAT("VulkanBuffer", "Destroying indexStagingBuffer={:p}", std::source_location::current(), static_cast<void*>(context_.indexStagingBuffer));
        vkDestroyBuffer(context_.device, context_.indexStagingBuffer, nullptr);
        context_.indexStagingBuffer = VK_NULL_HANDLE;
    }
    if (context_.indexStagingBufferMemory != VK_NULL_HANDLE) {
        LOG_DEBUG_CAT("VulkanBuffer", "Freeing indexStagingBufferMemory={:p}", std::source_location::current(), static_cast<void*>(context_.indexStagingBufferMemory));
        vkFreeMemory(context_.device, context_.indexStagingBufferMemory, nullptr);
        context_.indexStagingBufferMemory = VK_NULL_HANDLE;
    }
    LOG_DEBUG_CAT("VulkanBuffer", "Buffers cleaned up", std::source_location::current());
}

void VulkanBufferManager::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    LOG_DEBUG_CAT("VulkanBuffer", "Copying buffer from srcBuffer={:p} to dstBuffer={:p}, size={}",
                  std::source_location::current(), static_cast<void*>(srcBuffer), static_cast<void*>(dstBuffer), size);

    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = context_.commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    VkCommandBuffer commandBuffer;
    VkResult result = vkAllocateCommandBuffers(context_.device, &allocInfo, &commandBuffer);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanBuffer", "Failed to allocate command buffer for copy: result={}",
                      std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to allocate command buffer for copy");
    }
    LOG_DEBUG_CAT("VulkanBuffer", "Allocated command buffer for copy: commandBuffer={:p}", std::source_location::current(), static_cast<void*>(commandBuffer));

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };

    result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanBuffer", "Failed to begin command buffer for copy: result={}",
                      std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to begin command buffer for copy");
    }

    VkBufferCopy copyRegion{
        .srcOffset = 0,
        .dstOffset = 0,
        .size = size
    };
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
    LOG_DEBUG_CAT("VulkanBuffer", "Issued copy command for {} bytes", std::source_location::current(), size);

    result = vkEndCommandBuffer(commandBuffer);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanBuffer", "Failed to end command buffer for copy: result={}",
                      std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to end command buffer for copy");
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

    result = vkQueueSubmit(context_.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanBuffer", "Failed to submit copy command buffer: result={}",
                      std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to submit copy command buffer");
    }

    vkQueueWaitIdle(context_.graphicsQueue);
    LOG_DEBUG_CAT("VulkanBuffer", "Copy operation completed", std::source_location::current());

    vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &commandBuffer);
    LOG_DEBUG_CAT("VulkanBuffer", "Freed command buffer for copy", std::source_location::current());
}