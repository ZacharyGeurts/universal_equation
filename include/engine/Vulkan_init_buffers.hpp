// include/engine/Vulkan_init_buffers.hpp
#pragma once
#ifndef VULKAN_INIT_BUFFERS_HPP
#define VULKAN_INIT_BUFFERS_HPP

#include "engine/Vulkan_types.hpp"
#include <span>
#include <glm/glm.hpp>

class VulkanBufferManager {
public:
    VulkanBufferManager(VulkanContext& context);
    ~VulkanBufferManager();

    void initializeBuffers(std::span<const glm::vec3> vertices, std::span<const uint32_t> indices);
    void updateVertexBuffers(std::span<const glm::vec3> vertices, std::span<const uint32_t> indices);
    void cleanupBuffers();

private:
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    VulkanContext& context_;
};

#endif // VULKAN_INIT_BUFFERS_HPP