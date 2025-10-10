#ifndef VULKAN_BUFFER_MANAGER_HPP
#define VULKAN_BUFFER_MANAGER_HPP

#include "engine/Vulkan_init.hpp"
#include <vector>
#include <glm/vec3.hpp>
#include <vulkan/vulkan.h>

class VulkanBufferManager {
public:
    VulkanBufferManager(VulkanContext& context);
    ~VulkanBufferManager();

    void initializeBuffers(std::span<const glm::vec3> vertices, std::span<const uint32_t> indices);
    void createUniformBuffers(uint32_t swapchainImageCount);
    void createScratchBuffer(VkDeviceSize size);

    VkBuffer getVertexBuffer() const { return vertexBuffer_; }
    VkBuffer getIndexBuffer() const { return indexBuffer_; }
    VkBuffer getUniformBuffer(uint32_t index) const { return uniformBuffers_[index]; }
    VkBuffer getScratchBuffer() const { return scratchBuffer_; }
    VkDeviceMemory getVertexBufferMemory() const { return vertexBufferMemory_; }
    VkDeviceMemory getIndexBufferMemory() const { return indexBufferMemory_; }
    VkDeviceMemory getUniformBufferMemory(uint32_t index) const { return uniformBufferMemories_[index]; }
    VkDeviceMemory getScratchBufferMemory() const { return scratchBufferMemory_; }
    uint32_t getIndexCount() const { return indexCount_; }
    VkDeviceAddress getVertexBufferDeviceAddress() const { return vertexBufferAddress_; }
    VkDeviceAddress getIndexBufferDeviceAddress() const { return indexBufferAddress_; }
    VkDeviceAddress getScratchBufferDeviceAddress() const { return scratchBufferAddress_; }

private:
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                     VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
    VkDeviceAddress getBufferDeviceAddress(VkBuffer buffer) const;
    void cleanupBuffers();

    VulkanContext& context_;
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory_ = VK_NULL_HANDLE;
    VkDeviceAddress vertexBufferAddress_ = 0;
    VkBuffer indexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory_ = VK_NULL_HANDLE;
    VkDeviceAddress indexBufferAddress_ = 0;
    std::vector<VkBuffer> uniformBuffers_;
    std::vector<VkDeviceMemory> uniformBufferMemories_;
    VkBuffer scratchBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory scratchBufferMemory_ = VK_NULL_HANDLE;
    VkDeviceAddress scratchBufferAddress_ = 0;
    uint32_t indexCount_ = 0;
};

#endif // VULKAN_BUFFER_MANAGER_HPP