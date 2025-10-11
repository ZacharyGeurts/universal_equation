// VulkanBufferManager.hpp
// AMOURANTH RTX Engine, October 2025 - Manages Vulkan buffer creation and memory allocation.
// Dependencies: Vulkan 1.3+, GLM, logging.hpp, VulkanCore.hpp.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#pragma once
#ifndef VULKAN_BUFFER_MANAGER_HPP
#define VULKAN_BUFFER_MANAGER_HPP

#include "engine/logging.hpp"
#include "VulkanCore.hpp"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <span>
#include <string>
#include <source_location>

// Forward declarations
class AMOURANTH;

class VulkanBufferManager {
public:
    VulkanBufferManager(VulkanContext& context);
    ~VulkanBufferManager();
    void initializeBuffers(std::span<const glm::vec3> vertices, std::span<const uint32_t> indices);
    void createUniformBuffers(uint32_t count);
    void createScratchBuffer(VkDeviceSize size);
    void cleanupBuffers();
    VkBuffer getVertexBuffer() const { return vertexBuffer_; }
    VkBuffer getIndexBuffer() const { return indexBuffer_; }
    VkDeviceMemory getVertexBufferMemory() const { return vertexBufferMemory_; }
    VkDeviceMemory getIndexBufferMemory() const { return indexBufferMemory_; }
    VkBuffer getUniformBuffer(uint32_t index) const { return uniformBuffers_[index]; }
    VkDeviceMemory getUniformBufferMemory(uint32_t index) const { return uniformBufferMemories_[index]; }
    uint32_t getVertexCount() const { return vertexCount_; }
    uint32_t getIndexCount() const { return indexCount_; }
    VkBuffer getScratchBuffer() const { return scratchBuffer_; }
    VkDeviceMemory getScratchBufferMemory() const { return scratchBufferMemory_; }
    VkDeviceAddress getVertexBufferAddress() const { return vertexBufferAddress_; }
    VkDeviceAddress getIndexBufferAddress() const { return indexBufferAddress_; }
    VkDeviceAddress getScratchBufferAddress() const { return scratchBufferAddress_; }

private:
    VulkanContext& context_;
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory_ = VK_NULL_HANDLE;
    VkDeviceAddress vertexBufferAddress_ = 0;
    VkBuffer indexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory_ = VK_NULL_HANDLE;
    VkDeviceAddress indexBufferAddress_ = 0;
    VkBuffer scratchBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory scratchBufferMemory_ = VK_NULL_HANDLE;
    VkDeviceAddress scratchBufferAddress_ = 0;
    std::vector<VkBuffer> uniformBuffers_;
    std::vector<VkDeviceMemory> uniformBufferMemories_;
    uint32_t vertexCount_ = 0;
    uint32_t indexCount_ = 0;
};

#endif // VULKAN_BUFFER_MANAGER_HPP