#include "engine/Vulkan_init.hpp"
#include <stdexcept>

VulkanBufferManager::VulkanBufferManager(VulkanContext& context) : context_(context) {
    LOG_INFO_CAT("Vulkan", "Constructing VulkanBufferManager", std::source_location::current());
}

VulkanBufferManager::~VulkanBufferManager() {
    cleanupBuffers();
}

void VulkanBufferManager::initializeBuffers(std::span<const glm::vec3> vertices, std::span<const uint32_t> indices) {
    LOG_DEBUG_CAT("Vulkan", "Initializing buffers with {} vertices, {} indices",
                  std::source_location::current(), vertices.size(), indices.size());

    VkDeviceSize vertexBufferSize = vertices.size() * sizeof(glm::vec3);
    VkDeviceSize indexBufferSize = indices.size() * sizeof(uint32_t);

    VkBuffer stagingVertexBuffer, stagingIndexBuffer;
    VkDeviceMemory stagingVertexMemory, stagingIndexMemory;

    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, vertexBufferSize,
                                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                    stagingVertexBuffer, stagingVertexMemory);
    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, indexBufferSize,
                                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                    stagingIndexBuffer, stagingIndexMemory);

    void* data;
    VkResult result = vkMapMemory(context_.device, stagingVertexMemory, 0, vertexBufferSize, 0, &data);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to map vertex buffer memory: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to map vertex buffer memory");
    }
    memcpy(data, vertices.data(), vertexBufferSize);
    vkUnmapMemory(context_.device, stagingVertexMemory);

    result = vkMapMemory(context_.device, stagingIndexMemory, 0, indexBufferSize, 0, &data);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to map index buffer memory: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to map index buffer memory");
    }
    memcpy(data, indices.data(), indexBufferSize);
    vkUnmapMemory(context_.device, stagingIndexMemory);

    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, vertexBufferSize,
                                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer_, vertexBufferMemory_);
    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, indexBufferSize,
                                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer_, indexBufferMemory_);

    copyBuffer(stagingVertexBuffer, vertexBuffer_, vertexBufferSize);
    copyBuffer(stagingIndexBuffer, indexBuffer_, indexBufferSize);

    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, sizeof(UniversalEquation::UniformBufferObject),
                                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                    uniformBuffer_, uniformBufferMemory_);

    vkDestroyBuffer(context_.device, stagingVertexBuffer, nullptr);
    vkFreeMemory(context_.device, stagingVertexMemory, nullptr);
    vkDestroyBuffer(context_.device, stagingIndexBuffer, nullptr);
    vkFreeMemory(context_.device, stagingIndexMemory, nullptr);
}

void VulkanBufferManager::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer;
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = context_.commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VkResult result = vkAllocateCommandBuffers(context_.device, &allocInfo, &commandBuffer);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to allocate command buffer: result={}", std::source_location::current(), result);
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
        LOG_ERROR_CAT("Vulkan", "Failed to begin command buffer: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to begin command buffer");
    }

    VkBufferCopy copyRegion{
        .srcOffset = 0,
        .dstOffset = 0,
        .size = size
    };
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    result = vkEndCommandBuffer(commandBuffer);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to end command buffer: result={}", std::source_location::current(), result);
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
        LOG_ERROR_CAT("Vulkan", "Failed to submit command buffer: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to submit command buffer");
    }

    vkQueueWaitIdle(context_.graphicsQueue);
    vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &commandBuffer);
}

void VulkanBufferManager::cleanupBuffers() {
    if (context_.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(context_.device);
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