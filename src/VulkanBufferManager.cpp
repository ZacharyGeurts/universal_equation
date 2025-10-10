#include "VulkanBufferManager.hpp"
#include <stdexcept>
#include <cstring>

VulkanBufferManager::VulkanBufferManager(VulkanContext& context)
    : context_(context), device_(context.device), physicalDevice_(context.physicalDevice),
      vertexBuffer_(VK_NULL_HANDLE), vertexBufferMemory_(VK_NULL_HANDLE),
      indexBuffer_(VK_NULL_HANDLE), indexBufferMemory_(VK_NULL_HANDLE),
      uniformBuffers_(), uniformBuffersMemory_(), indexCount_(0) {
    LOG_INFO("Initialized VulkanBufferManager");
}

VulkanBufferManager::~VulkanBufferManager() {
    cleanupBuffers();
}

void VulkanBufferManager::initializeBuffers(std::span<const glm::vec3> vertices, std::span<const uint32_t> indices) {
    createVertexBuffer(vertices);
    createIndexBuffer(indices);
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
                                   VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                   vertexBuffer_, vertexBufferMemory_);
    copyBuffer(stagingBuffer, vertexBuffer_, size);
    vkDestroyBuffer(device_, stagingBuffer, nullptr);
    vkFreeMemory(device_, stagingBufferMemory, nullptr);
    LOG_INFO_CAT("Vulkan", vertexBuffer_, "vertexBuffer", std::source_location::current());
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
                                   VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                   indexBuffer_, indexBufferMemory_);
    copyBuffer(stagingBuffer, indexBuffer_, size);
    vkDestroyBuffer(device_, stagingBuffer, nullptr);
    vkFreeMemory(device_, stagingBufferMemory, nullptr);
    LOG_INFO_CAT("Vulkan", indexBuffer_, "indexBuffer", std::source_location::current());
}

void VulkanBufferManager::createUniformBuffers(uint32_t swapchainImageCount) {
    VkDeviceSize bufferSize = sizeof(UE::UniformBufferObject);
    uniformBuffers_.resize(swapchainImageCount);
    uniformBuffersMemory_.resize(swapchainImageCount);
    for (uint32_t i = 0; i < swapchainImageCount; ++i) {
        VulkanInitializer::createBuffer(device_, physicalDevice_, bufferSize,
                                       VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                       uniformBuffers_[i], uniformBuffersMemory_[i]);
        LOG_INFO_CAT("Vulkan", uniformBuffers_[i], "uniformBuffer", std::source_location::current());
    }
}

void VulkanBufferManager::cleanupBuffers() {
    if (vertexBuffer_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", vertexBuffer_, "vertexBuffer", std::source_location::current());
        vkDestroyBuffer(device_, vertexBuffer_, nullptr);
        vertexBuffer_ = VK_NULL_HANDLE;
    }
    if (vertexBufferMemory_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", vertexBufferMemory_, "vertexBufferMemory", std::source_location::current());
        vkFreeMemory(device_, vertexBufferMemory_, nullptr);
        vertexBufferMemory_ = VK_NULL_HANDLE;
    }
    if (indexBuffer_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", indexBuffer_, "indexBuffer", std::source_location::current());
        vkDestroyBuffer(device_, indexBuffer_, nullptr);
        indexBuffer_ = VK_NULL_HANDLE;
    }
    if (indexBufferMemory_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", indexBufferMemory_, "indexBufferMemory", std::source_location::current());
        vkFreeMemory(device_, indexBufferMemory_, nullptr);
        indexBufferMemory_ = VK_NULL_HANDLE;
    }
    for (size_t i = 0; i < uniformBuffers_.size(); ++i) {
        if (uniformBuffers_[i] != VK_NULL_HANDLE) {
            LOG_INFO_CAT("Vulkan", uniformBuffers_[i], "uniformBuffer", std::source_location::current());
            vkDestroyBuffer(device_, uniformBuffers_[i], nullptr);
        }
        if (uniformBuffersMemory_[i] != VK_NULL_HANDLE) {
            LOG_INFO_CAT("Vulkan", uniformBuffersMemory_[i], "uniformBufferMemory", std::source_location::current());
            vkFreeMemory(device_, uniformBuffersMemory_[i], nullptr);
        }
    }
    uniformBuffers_.clear();
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
        LOG_ERROR_CAT("Vulkan", "Failed to allocate command buffer", std::source_location::current());
        throw std::runtime_error("Failed to allocate command buffer");
    }
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to begin command buffer", std::source_location::current());
        throw std::runtime_error("Failed to begin command buffer");
    }
    VkBufferCopy copyRegion{
        .srcOffset = 0,
        .dstOffset = 0,
        .size = size
    };
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to end command buffer", std::source_location::current());
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
        LOG_ERROR_CAT("Vulkan", "Failed to submit copy command", std::source_location::current());
        throw std::runtime_error("Failed to submit copy command");
    }
    vkQueueWaitIdle(context_.graphicsQueue);
    vkFreeCommandBuffers(device_, context_.commandPool, 1, &commandBuffer);
}

VkBuffer VulkanBufferManager::getUniformBuffer(uint32_t index) const {
    if (index >= uniformBuffers_.size()) {
        LOG_ERROR_CAT("Vulkan", "Invalid uniform buffer index: {}", std::source_location::current(), index);
        return VK_NULL_HANDLE;
    }
    return uniformBuffers_[index];
}

VkDeviceMemory VulkanBufferManager::getUniformBufferMemory(uint32_t index) const {
    if (index >= uniformBuffersMemory_.size()) {
        LOG_ERROR_CAT("Vulkan", "Invalid uniform buffer memory index: {}", std::source_location::current(), index);
        return VK_NULL_HANDLE;
    }
    return uniformBuffersMemory_[index];
}