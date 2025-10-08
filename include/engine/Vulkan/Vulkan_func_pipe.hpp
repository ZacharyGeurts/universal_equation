// engine/Vulkan/Vulkan_func_pipe.hpp
// AMOURANTH RTX Engine, October 2025 - Vulkan pipeline utilities for render pass and pipeline creation.
// Supports Windows/Linux; no mutexes; voxel geometry rendering.
// Dependencies: Vulkan 1.3+, GLM, C++20 standard library.
// Usage: Manages graphics pipeline, render pass, descriptor sets, and samplers for VulkanRenderer.
// Thread-safety: Uses Logging::Logger for thread-safe logging; no mutexes required.
// Zachary Geurts 2025

#ifndef VULKAN_FUNC_PIPE_HPP
#define VULKAN_FUNC_PIPE_HPP

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "engine/logging.hpp" // For Logging::Logger
#include "engine/Vulkan/Vulkan_func.hpp" // For PushConstants
#include "engine/Vulkan/Vulkan_func_swapchain.hpp" // For vkResultToString, vkFormatToString

namespace VulkanInitializer {

    // Create a shader module from a SPIR-V file
    VkShaderModule createShaderModule(VkDevice device, const std::string& filename, const Logging::Logger& logger);

    // Create a render pass for the graphics pipeline
    void createRenderPass(VkDevice device, VkRenderPass& renderPass, VkFormat format, const Logging::Logger& logger);

    // Create a descriptor set layout for shader resources
    void createDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout& descriptorSetLayout, const Logging::Logger& logger);

    // Create a descriptor pool and allocate a descriptor set
    void createDescriptorPoolAndSet(VkDevice device, VkDescriptorSetLayout descriptorSetLayout,
                                   VkDescriptorPool& descriptorPool, VkDescriptorSet& descriptorSet, VkSampler sampler,
                                   const Logging::Logger& logger);

    // Create a texture sampler
    void createSampler(VkDevice device, VkPhysicalDevice physicalDevice, VkSampler& sampler, const Logging::Logger& logger);

    // Create a graphics pipeline with provided shaders
    void createGraphicsPipeline(VkDevice device, VkRenderPass renderPass, VkPipeline& pipeline,
                               VkPipelineLayout& pipelineLayout, VkDescriptorSetLayout& descriptorSetLayout,
                               int width, int height, VkShaderModule& vertShaderModule, VkShaderModule& fragShaderModule,
                               const Logging::Logger& logger);

} // namespace VulkanInitializer

#endif // VULKAN_FUNC_PIPE_HPP