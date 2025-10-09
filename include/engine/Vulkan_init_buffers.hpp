#ifndef VULKAN_INIT_BUFFERS_HPP
#define VULKAN_INIT_BUFFERS_HPP

#include <glm/glm.hpp>
#include <span>
#include <vulkan/vulkan.h>
#include "engine/Vulkan_init.hpp"
#include "engine/logging.hpp"

class VulkanBufferManager {
public:
    explicit VulkanBufferManager(VulkanContext* context);
    ~VulkanBufferManager() = default;

    void initializeBuffers(std::span<const glm::vec3> vertices, std::span<const uint32_t> indices);
    void initializeQuadBuffers(std::span<const glm::vec3> quadVertices, std::span<const uint32_t> quadIndices);
    void initializeVoxelBuffers(std::span<const glm::vec3> voxelVertices, std::span<const uint32_t> voxelIndices);
    void cleanupBuffers();

private:
    VulkanContext* context_;
};

#endif // VULKAN_INIT_BUFFERS_HPP