// include/engine/Vulkan/Vulkan_func_pipe.hpp
#pragma once
#ifndef VULKAN_FUNC_PIPE_HPP
#define VULKAN_FUNC_PIPE_HPP

#include "engine/Vulkan_types.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

class VulkanPipelineManager {
public:
    VulkanPipelineManager(VulkanContext& context);
    ~VulkanPipelineManager();

    void createRenderPass();
    void createPipelineLayout();
    void createGraphicsPipeline();
    void cleanupPipeline();
    std::vector<char> readFile(const std::string& filename);
    VkShaderModule createShaderModule(const std::vector<char>& code);

private:
    VulkanContext& context_;
};

namespace VulkanInitializer {
    void createRenderPass(VkDevice device, VkRenderPass& renderPass, VkFormat format);
    void createDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout& descriptorSetLayout);
    void createDescriptorPoolAndSet(VkDevice device, VkDescriptorSetLayout descriptorSetLayout,
                                   VkDescriptorPool& descriptorPool, VkDescriptorSet& descriptorSet,
                                   VkSampler& sampler, VkBuffer uniformBuffer);
    void createSampler(VkDevice device, VkPhysicalDevice physicalDevice, VkSampler& sampler);
    void createGraphicsPipeline(VkDevice device, VkRenderPass renderPass, VkPipeline& pipeline,
                               VkPipelineLayout& pipelineLayout, VkDescriptorSetLayout& descriptorSetLayout,
                               int width, int height, VkShaderModule& vertShaderModule, VkShaderModule& fragShaderModule);
    VkShaderModule createShaderModule(VkDevice device, const std::string& filename);
}

#endif // VULKAN_FUNC_PIPE_HPP