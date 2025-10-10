// VulkanBufferManager.cpp
#include "engine/logging.hpp"
#include "VulkanBufferManager.hpp"
#include "engine/Vulkan_init.hpp" // Include full Vulkan_init.hpp for VulkanContext and VulkanInitializer
#include <cstring>

VulkanBufferManager::VulkanBufferManager(VulkanContext& context) : context_(context) {
    LOG_INFO_CAT("VulkanBuffer", "Constructing VulkanBufferManager with context.device={:p}",
                 std::source_location::current(), reinterpret_cast<void*>(context_.device));
}

VulkanBufferManager::~VulkanBufferManager() {
    LOG_INFO_CAT("VulkanBuffer", "Destroying VulkanBufferManager", std::source_location::current());
    cleanupBuffers();
}

void VulkanBufferManager::initializeBuffers(std::span<const glm::vec3> vertices, std::span<const uint32_t> indices) {
    LOG_DEBUG_CAT("VulkanBuffer", "Initializing buffers with vertex count={}, index count={}",
                  std::source_location::current(), vertices.size(), indices.size());

    indexCount_ = static_cast<uint32_t>(indices.size());

    VkDeviceSize vertexBufferSize = sizeof(glm::vec3) * vertices.size();
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, vertexBufferSize,
                                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(context_.device, stagingBufferMemory, 0, vertexBufferSize, 0, &data);
    memcpy(data, vertices.data(), vertexBufferSize);
    vkUnmapMemory(context_.device, stagingBufferMemory);

    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, vertexBufferSize,
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                   vertexBuffer_, vertexBufferMemory_);

    copyBuffer(stagingBuffer, vertexBuffer_, vertexBufferSize);

    vkDestroyBuffer(context_.device, stagingBuffer, nullptr);
    vkFreeMemory(context_.device, stagingBufferMemory, nullptr);

    VkDeviceSize indexBufferSize = sizeof(uint32_t) * indices.size();
    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, indexBufferSize,
                                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   stagingBuffer, stagingBufferMemory);

    vkMapMemory(context_.device, stagingBufferMemory, 0, indexBufferSize, 0, &data);
    memcpy(data, indices.data(), indexBufferSize);
    vkUnmapMemory(context_.device, stagingBufferMemory);

    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, indexBufferSize,
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                   indexBuffer_, indexBufferMemory_);

    copyBuffer(stagingBuffer, indexBuffer_, indexBufferSize);

    vkDestroyBuffer(context_.device, stagingBuffer, nullptr);
    vkFreeMemory(context_.device, stagingBufferMemory, nullptr);

    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, sizeof(UniversalEquation::UniformBufferObject),
                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   uniformBuffer_, uniformBufferMemory_);
}

void VulkanBufferManager::cleanupBuffers() {
    LOG_DEBUG_CAT("VulkanBuffer", "Cleaning up buffers", std::source_location::current());
    if (context_.device == VK_NULL_HANDLE) {
        LOG_WARNING_CAT("VulkanBuffer", "Device is null, skipping cleanup", std::source_location::current());
        return;
    }

    if (vertexBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(context_.device, vertexBuffer_, nullptr);
        vertexBuffer_ = VK_NULL_HANDLE;
    }
    if (vertexBufferMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(context_.device, vertexBufferMemory_, nullptr);
        vertexBufferMemory_ = VK_NULL_HANDLE;
    }
    if (indexBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(context_.device, indexBuffer_, nullptr);
        indexBuffer_ = VK_NULL_HANDLE;
    }
    if (indexBufferMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(context_.device, indexBufferMemory_, nullptr);
        indexBufferMemory_ = VK_NULL_HANDLE;
    }
    if (uniformBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(context_.device, uniformBuffer_, nullptr);
        uniformBuffer_ = VK_NULL_HANDLE;
    }
    if (uniformBufferMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(context_.device, uniformBufferMemory_, nullptr);
        uniformBufferMemory_ = VK_NULL_HANDLE;
    }
}

void VulkanBufferManager::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
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
        LOG_ERROR_CAT("VulkanBuffer", "Failed to allocate command buffer: result={}", std::source_location::current(), result);
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
        LOG_ERROR_CAT("VulkanBuffer", "Failed to begin command buffer: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to begin command buffer");
    }

    VkBufferCopy copyRegion{.srcOffset = 0, .dstOffset = 0, .size = size};
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    result = vkEndCommandBuffer(commandBuffer);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanBuffer", "Failed to end command buffer: result={}", std::source_location::current(), result);
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
    result = vkQueueSubmit(context_.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanBuffer", "Failed to submit copy command: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to submit copy command");
    }

    vkQueueWaitIdle(context_.graphicsQueue);
    vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &commandBuffer);
}