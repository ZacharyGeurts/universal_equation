// include/engine/Vulkan_init_pipeline.hpp
#pragma once
#ifndef VULKAN_INIT_PIPELINE_HPP
#define VULKAN_INIT_PIPELINE_HPP

#include "engine/Vulkan_types.hpp"
#include "engine/logging.hpp"
#include <vector>
#include <string>

class VulkanPipelineManager {
public:
    VulkanPipelineManager(VulkanContext& context);
    ~VulkanPipelineManager() noexcept;

private:
    void createRenderPass();
    void createPipelineLayout();
    void createGraphicsPipeline();
    void cleanupPipeline();
    std::vector<char> readFile(const std::string& filename);
    VkShaderModule createShaderModule(const std::vector<char>& code);

    VulkanContext& context_;
};

// Utility functions for logging Vulkan types
std::string vkFormatToString(VkFormat format);
std::string vkResultToString(VkResult result);

#endif // VULKAN_INIT_PIPELINE_HPP