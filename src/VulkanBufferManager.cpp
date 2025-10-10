#include "engine/Vulkan_init.hpp"
#include <stdexcept>
#include <cstring>

VulkanBufferManager::VulkanBufferManager(VkDevice device, VkPhysicalDevice physicalDevice, VulkanContext& context)
    : device_(device), physicalDevice_(physicalDevice), vertexBuffer_(VK_NULL_HANDLE),
      vertexBufferMemory_(VK_NULL_HANDLE), indexBuffer_(VK_NULL_HANDLE), indexBufferMemory_(VK_NULL_HANDLE),
      uniformBuffers_(), uniformBuffersMemory_(), indexCount_(0), context_(context) {}

VulkanBufferManager::~VulkanBufferManager() {
    cleanupBuffers();
}

void VulkanBufferManager::initializeBuffers(std::span<const glm::vec3> vertices, std::span<const uint32_t> indices) {
    // Create vertex buffer
    VkDeviceSize vertexSize = sizeof(glm::vec3) * vertices.size();
    VkBuffer stagingVertexBuffer;
    VkDeviceMemory stagingVertexBufferMemory;
    VulkanInitializer::createBuffer(device_, physicalDevice_, vertexSize,
                                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   stagingVertexBuffer, stagingVertexBufferMemory);
    void* data;
    vkMapMemory(device_, stagingVertexBufferMemory, 0, vertexSize, 0, &data);
    memcpy(data, vertices.data(), vertexSize);
    vkUnmapMemory(device_, stagingVertexBufferMemory);
    VulkanInitializer::createBuffer(device_, physicalDevice_, vertexSize,
                                   VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                   vertexBuffer_, vertexBufferMemory_);
    copyBuffer(stagingVertexBuffer, vertexBuffer_, vertexSize);
    vkDestroyBuffer(device_, stagingVertexBuffer, nullptr);
    vkFreeMemory(device_, stagingVertexBufferMemory, nullptr);

    // Create index buffer
    indexCount_ = static_cast<uint32_t>(indices.size());
    VkDeviceSize indexSize = sizeof(uint32_t) * indices.size();
    VkBuffer stagingIndexBuffer;
    VkDeviceMemory stagingIndexBufferMemory;
    VulkanInitializer::createBuffer(device_, physicalDevice_, indexSize,
                                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   stagingIndexBuffer, stagingIndexBufferMemory);
    vkMapMemory(device_, stagingIndexBufferMemory, 0, indexSize, 0, &data);
    memcpy(data, indices.data(), indexSize);
    vkUnmapMemory(device_, stagingIndexBufferMemory);
    VulkanInitializer::createBuffer(device_, physicalDevice_, indexSize,
                                   VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                   indexBuffer_, indexBufferMemory_);
    copyBuffer(stagingIndexBuffer, indexBuffer_, indexSize);
    vkDestroyBuffer(device_, stagingIndexBuffer, nullptr);
    vkFreeMemory(device_, stagingIndexBufferMemory, nullptr);
}

void VulkanBufferManager::createVertexBuffer(std::span<const glm::vec3> vertices) {
    VkDeviceSize size = sizeof(glm::vec3) * vertices.size();
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    VulkanInitializer::createBuffer(device_, physicalDevice_, size,
                                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   stagingBuffer, stagingBufferMemory);
    void* data;
    vkMapMemory(device_, stagingBufferMemory, 0, size, 0, &data);
    memcpy(data, vertices.data(), size);
    vkUnmapMemory(device_, stagingBufferMemory);
    VulkanInitializer::createBuffer(device_, physicalDevice_, size,
                                   VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                   vertexBuffer_, vertexBufferMemory_);
    copyBuffer(stagingBuffer, vertexBuffer_, size);
    vkDestroyBuffer(device_, stagingBuffer, nullptr);
    vkFreeMemory(device_, stagingBufferMemory, nullptr);
}

void VulkanBufferManager::createIndexBuffer(std::span<const uint32_t> indices) {
    indexCount_ = static_cast<uint32_t>(indices.size());
    VkDeviceSize size = sizeof(uint32_t) * indices.size();
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    VulkanInitializer::createBuffer(device_, physicalDevice_, size,
                                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   stagingBuffer, stagingBufferMemory);
    void* data;
    vkMapMemory(device_, stagingBufferMemory, 0, size, 0, &data);
    memcpy(data, indices.data(), size);
    vkUnmapMemory(device_, stagingBufferMemory);
    VulkanInitializer::createBuffer(device_, physicalDevice_, size,
                                   VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                   indexBuffer_, indexBufferMemory_);
    copyBuffer(stagingBuffer, indexBuffer_, size);
    vkDestroyBuffer(device_, stagingBuffer, nullptr);
    vkFreeMemory(device_, stagingBufferMemory, nullptr);
}

void VulkanBufferManager::createUniformBuffers(uint32_t swapchainImageCount) {
    VkDeviceSize size = sizeof(UE::UniformBufferObject);
    uniformBuffers_.resize(swapchainImageCount);
    uniformBuffersMemory_.resize(swapchainImageCount);
    for (uint32_t i = 0; i < swapchainImageCount; ++i) {
        VulkanInitializer::createBuffer(device_, physicalDevice_, size,
                                       VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                       uniformBuffers_[i], uniformBuffersMemory_[i]);
    }
}

void VulkanBufferManager::cleanupBuffers() {
    if (vertexBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, vertexBuffer_, nullptr);
        vertexBuffer_ = VK_NULL_HANDLE;
    }
    if (vertexBufferMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, vertexBufferMemory_, nullptr);
        vertexBufferMemory_ = VK_NULL_HANDLE;
    }
    if (indexBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, indexBuffer_, nullptr);
        indexBuffer_ = VK_NULL_HANDLE;
    }
    if (indexBufferMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, indexBufferMemory_, nullptr);
        indexBufferMemory_ = VK_NULL_HANDLE;
    }
    for (auto buffer : uniformBuffers_) {
        if (buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device_, buffer, nullptr);
        }
    }
    uniformBuffers_.clear();
    for (auto memory : uniformBuffersMemory_) {
        if (memory != VK_NULL_HANDLE) {
            vkFreeMemory(device_, memory, nullptr);
        }
    }
    uniformBuffersMemory_.clear();
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
    if (vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffer");
    }
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin command buffer");
    }
    VkBufferCopy copyRegion{
        .srcOffset = 0,
        .dstOffset = 0,
        .size = size
    };
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
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
        throw std::runtime_error("Failed to submit copy command");
    }
    vkQueueWaitIdle(context_.graphicsQueue);
    vkFreeCommandBuffers(device_, context_.commandPool, 1, &commandBuffer);
}

VkBuffer VulkanBufferManager::getVertexBuffer() const { return vertexBuffer_; }
VkBuffer VulkanBufferManager::getIndexBuffer() const { return indexBuffer_; }
VkBuffer VulkanBufferManager::getUniformBuffer(uint32_t index) const { return uniformBuffers_[index]; }
VkDeviceMemory VulkanBufferManager::getVertexBufferMemory() const { return vertexBufferMemory_; }
VkDeviceMemory VulkanBufferManager::getIndexBufferMemory() const { return indexBufferMemory_; }
VkDeviceMemory VulkanBufferManager::getUniformBufferMemory() const { return uniformBuffersMemory_[0]; }
uint32_t VulkanBufferManager::getIndexCount() const { return indexCount_; }