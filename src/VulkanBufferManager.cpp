// VulkanBufferManager.cpp
#include "engine/Vulkan_init.hpp"
#include <stdexcept>

VulkanBufferManager::VulkanBufferManager(VkDevice device, VkPhysicalDevice physicalDevice)
    : device_(device), physicalDevice_(physicalDevice), vertexBuffer_(VK_NULL_HANDLE), vertexBufferMemory_(VK_NULL_HANDLE),
      indexBuffer_(VK_NULL_HANDLE), indexBufferMemory_(VK_NULL_HANDLE) {}

VulkanBufferManager::~VulkanBufferManager() {
    if (vertexBuffer_ != VK_NULL_HANDLE) vkDestroyBuffer(device_, vertexBuffer_, nullptr);
    if (vertexBufferMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, vertexBufferMemory_, nullptr);
    if (indexBuffer_ != VK_NULL_HANDLE) vkDestroyBuffer(device_, indexBuffer_, nullptr);
    if (indexBufferMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, indexBufferMemory_, nullptr);
    for (auto buffer : uniformBuffers_) vkDestroyBuffer(device_, buffer, nullptr);
    for (auto memory : uniformBuffersMemory_) vkFreeMemory(device_, memory, nullptr);
}

void VulkanBufferManager::createVertexBuffer(std::span<const glm::vec3> vertices) {
    VkDeviceSize size = sizeof(glm::vec3) * vertices.size();
    VulkanInitializer::createBuffer(device_, physicalDevice_, size,
                                   VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   vertexBuffer_, vertexBufferMemory_);
    void* data;
    vkMapMemory(device_, vertexBufferMemory_, 0, size, 0, &data);
    memcpy(data, vertices.data(), size);
    vkUnmapMemory(device_, vertexBufferMemory_);
}

void VulkanBufferManager::createIndexBuffer(std::span<const uint32_t> indices) {
    indexCount_ = static_cast<uint32_t>(indices.size());
    VkDeviceSize size = sizeof(uint32_t) * indices.size();
    VulkanInitializer::createBuffer(device_, physicalDevice_, size,
                                   VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   indexBuffer_, indexBufferMemory_);
    void* data;
    vkMapMemory(device_, indexBufferMemory_, 0, size, 0, &data);
    memcpy(data, indices.data(), size);
    vkUnmapMemory(device_, indexBufferMemory_);
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

VkBuffer VulkanBufferManager::getVertexBuffer() const { return vertexBuffer_; }
VkBuffer VulkanBufferManager::getIndexBuffer() const { return indexBuffer_; }
VkBuffer VulkanBufferManager::getUniformBuffer(uint32_t index) const { return uniformBuffers_[index]; }
VkDeviceMemory VulkanBufferManager::getVertexBufferMemory() const { return vertexBufferMemory_; }
VkDeviceMemory VulkanBufferManager::getIndexBufferMemory() const { return indexBufferMemory_; }
VkDeviceMemory VulkanBufferManager::getUniformBufferMemory() const { return uniformBuffersMemory_[0]; }
uint32_t VulkanBufferManager::getIndexCount() const { return indexCount_; }