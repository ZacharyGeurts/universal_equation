// include/engine/Vulkan/Vulkan_func_pipe.hpp
#pragma once
#ifndef VULKAN_FUNC_PIPE_HPP
#define VULKAN_FUNC_PIPE_HPP

#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include "engine/Vulkan_types.hpp"

namespace VulkanInitializer {
    VkShaderModule createShaderModule(VkDevice device, const std::string& filename);
    void createRenderPass(VkDevice device, VkRenderPass& renderPass, VkFormat format);
    void createDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout& descriptorSetLayout);
    void createDescriptorPoolAndSet(VkDevice device, VkDescriptorSetLayout descriptorSetLayout,
                                   VkDescriptorPool& descriptorPool, VkDescriptorSet& descriptorSet,
                                   VkSampler sampler, VkBuffer uniformBuffer);
    void createSampler(VkDevice device, VkPhysicalDevice physicalDevice, VkSampler& sampler);
    void createGraphicsPipeline(VkDevice device, VkRenderPass renderPass, VkPipeline& pipeline,
                               VkPipelineLayout& pipelineLayout, VkDescriptorSetLayout& descriptorSetLayout,
                               int width, int height, VkShaderModule& vertShaderModule, VkShaderModule& fragShaderModule);
}

#endif // VULKAN_FUNC_PIPE_HPP