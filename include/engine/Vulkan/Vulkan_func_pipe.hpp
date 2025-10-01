#ifndef VULKAN_FUNC_PIPE_HPP
#define VULKAN_FUNC_PIPE_HPP
// AMOURANTH RTX September 2025
// Zachary Geurts 2025
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "engine/core.hpp" // For PushConstants

namespace VulkanInitializer {

    // Create a shader module from a SPIR-V file
    VkShaderModule createShaderModule(VkDevice device, const std::string& filename);

    // Create a render pass for the graphics pipeline
    void createRenderPass(VkDevice device, VkRenderPass& renderPass, VkFormat format);

    // Create a descriptor set layout for shader resources
    void createDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout& descriptorSetLayout);

    // Create a descriptor pool and allocate a descriptor set
    void createDescriptorPoolAndSet(VkDevice device, VkDescriptorSetLayout descriptorSetLayout,
                                   VkDescriptorPool& descriptorPool, VkDescriptorSet& descriptorSet, VkSampler sampler);

    // Create a texture sampler
    void createSampler(VkDevice device, VkPhysicalDevice physicalDevice, VkSampler& sampler);

    // Create a graphics pipeline with provided shaders
    void createGraphicsPipeline(VkDevice device, VkRenderPass renderPass, VkPipeline& pipeline,
                               VkPipelineLayout& pipelineLayout, VkDescriptorSetLayout& descriptorSetLayout,
                               int width, int height, VkShaderModule& vertShaderModule, VkShaderModule& fragShaderModule);

}

#endif // VULKAN_FUNC_PIPE_HPP
