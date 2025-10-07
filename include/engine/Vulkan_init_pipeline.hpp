// AMOURANTH RTX Engine, October 2025 - Vulkan pipeline initialization.
// Manages render pass, descriptor sets, graphics pipeline creation, and shader modules.
// Dependencies: Vulkan 1.3+, GLM, C++20 standard library.
// Zachary Geurts 2025

#ifndef VULKAN_PIPELINE_HPP
#define VULKAN_PIPELINE_HPP

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include "logging.hpp"

// Forward declaration
struct VulkanContext;

class VulkanPipelineManager {
public:
    VulkanPipelineManager(VulkanContext& context);
    ~VulkanPipelineManager();

    void initializePipeline(int width, int height);
    void cleanupPipeline();
    VkShaderModule createShaderModule(const std::string& filename);

private:
    VulkanContext& context_;
    VkShaderModule vertShaderModule_;
    VkShaderModule fragShaderModule_;
    Logging::Logger logger_; // Logger instance for VulkanPipelineManager
};

#endif // VULKAN_PIPELINE_HPP