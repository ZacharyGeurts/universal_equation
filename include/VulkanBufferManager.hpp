#pragma once
#ifndef VULKAN_BUFFER_MANAGER_HPP
#define VULKAN_BUFFER_MANAGER_HPP

#include <vulkan/vulkan.h>
#include <span>
#include <glm/glm.hpp>
#include "engine/logging.hpp"
#include "ue_init.hpp"

class VulkanContext; // Forward declaration

class VulkanBufferManager {
public:
    VulkanBufferManager(VulkanContext& context);
    ~VulkanBufferManager();
    void initializeBuffers(std::span<const glm::vec3> vertices, std::span<const uint32_t> indices);
    void cleanupBuffers();
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    VkBuffer getVertexBuffer() const { return vertexBuffer_; }
    VkBuffer getIndexBuffer() const { return indexBuffer_; }
    VkBuffer getUniformBuffer() const { return uniformBuffer_; }
    VkDeviceMemory getUniformBufferMemory() const { return uniformBufferMemory_; }
    VkDeviceMemory getVertexBufferMemory() const { return vertexBufferMemory_; }
    uint32_t getIndexCount() const { return indexCount_; }

private:
    VulkanContext& context_;
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory_ = VK_NULL_HANDLE;
    VkBuffer indexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory_ = VK_NULL_HANDLE;
    VkBuffer uniformBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory uniformBufferMemory_ = VK_NULL_HANDLE;
    uint32_t indexCount_ = 0;
};

#endif // VULKAN_BUFFER_MANAGER_HPP