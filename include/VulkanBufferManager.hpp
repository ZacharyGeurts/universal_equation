#pragma once
#ifndef VULKAN_BUFFER_MANAGER_HPP
#define VULKAN_BUFFER_MANAGER_HPP

#include <vulkan/vulkan.h>
#include <span>
#include <vector>
#include <glm/glm.hpp>
#include "engine/logging.hpp"
#include "engine/Vulkan_init.hpp" // Include Vulkan_init.hpp for VulkanContext

class VulkanBufferManager {
public:
    VulkanBufferManager(VulkanContext& context);
    ~VulkanBufferManager();
    void initializeBuffers(std::span<const glm::vec3> vertices, std::span<const uint32_t> indices);
    void createVertexBuffer(std::span<const glm::vec3> vertices);
    void createIndexBuffer(std::span<const uint32_t> indices);
    void createUniformBuffers(uint32_t swapchainImageCount);
    void cleanupBuffers();
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    VkBuffer getVertexBuffer() const { return vertexBuffer_; }
    VkBuffer getIndexBuffer() const { return indexBuffer_; }
    VkBuffer getUniformBuffer() const { return uniformBuffers_.empty() ? VK_NULL_HANDLE : uniformBuffers_[0]; }
    VkBuffer getUniformBuffer(uint32_t index) const;
    VkDeviceMemory getVertexBufferMemory() const { return vertexBufferMemory_; }
    VkDeviceMemory getIndexBufferMemory() const { return indexBufferMemory_; }
    VkDeviceMemory getUniformBufferMemory() const { return uniformBuffersMemory_.empty() ? VK_NULL_HANDLE : uniformBuffersMemory_[0]; }
    VkDeviceMemory getUniformBufferMemory(uint32_t index) const;
    uint32_t getIndexCount() const { return indexCount_; }

private:
    VulkanContext& context_;
    VkDevice device_;
    VkPhysicalDevice physicalDevice_;
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory_ = VK_NULL_HANDLE;
    VkBuffer indexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory_ = VK_NULL_HANDLE;
    std::vector<VkBuffer> uniformBuffers_;
    std::vector<VkDeviceMemory> uniformBuffersMemory_;
    uint32_t indexCount_ = 0;
};

#endif // VULKAN_BUFFER_MANAGER_HPP