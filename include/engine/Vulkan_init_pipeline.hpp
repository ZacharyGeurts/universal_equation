// AMOURANTH RTX Engine, October 2025 - Vulkan pipeline initialization.
// Manages render pass, descriptor sets, and graphics pipeline creation.
// Dependencies: Vulkan 1.3+, GLM, C++20 standard library.
// Zachary Geurts 2025

#ifndef VULKAN_PIPELINE_HPP
#define VULKAN_PIPELINE_HPP

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include "Vulkan_init.hpp" // Include full VulkanContext definition

class VulkanPipelineManager {
public:
    VulkanPipelineManager(VulkanContext& context);
    void initializePipeline(VkShaderModule vertShaderModule, VkShaderModule fragShaderModule, int width, int height);
    void cleanupPipeline();

private:
    VulkanContext& context_;
};

#endif // VULKAN_PIPELINE_HPP