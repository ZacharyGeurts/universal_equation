// AMOURANTH RTX Engine, October 2025 - Vulkan buffer initialization.
// Manages vertex, index, quad, and voxel buffer creation.
// Dependencies: Vulkan 1.3+, GLM, C++20 standard library.
// Zachary Geurts 2025

#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <span>
#include "engine/Vulkan_init.hpp"

class VulkanBufferManager {
public:
    VulkanBufferManager(VulkanContext& context);
    void initializeBuffers(std::span<const glm::vec3> vertices, std::span<const uint32_t> indices);
    void initializeQuadBuffers(std::span<const glm::vec3> quadVertices, std::span<const uint32_t> quadIndices);
    void initializeVoxelBuffers(std::span<const glm::vec3> voxelVertices, std::span<const uint32_t> voxelIndices);
    void cleanupBuffers();
private:
    VulkanContext& context_;
    Logging::Logger logger_;
};