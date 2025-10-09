// Vulkan_func.hpp
// Utility functions for Vulkan swapchain and pipeline in AMOURANTH RTX Engine
// AMOURANTH RTX Engine, October 2025
// Zachary Geurts 2025

#ifndef VULKAN_FUNC_HPP
#define VULKAN_FUNC_HPP

#include <vulkan/vulkan.h>
#include <vector>
#include <string_view>
#include <glm/glm.hpp>
#include "engine/logging.hpp"

struct PushConstants {
    alignas(16) glm::mat4 model;       // 64 bytes
    alignas(16) glm::mat4 view_proj;   // 64 bytes
    alignas(16) glm::vec4 extra[8];    // 128 bytes
};
static_assert(sizeof(PushConstants) == 256, "PushConstants must be 256 bytes");

namespace VulkanInitializer {
// Declarations for Vulkan_func_swapchain.cpp
VkSurfaceFormatKHR selectSurfaceFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);

void createSwapchain(
    VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, VkSwapchainKHR& swapchain,
    std::vector<VkImage>& swapchainImages, std::vector<VkImageView>& swapchainImageViews,
    VkFormat& swapchainFormat, uint32_t graphicsFamily, uint32_t presentFamily, int width, int height);

void createFramebuffers(
    VkDevice device, VkRenderPass renderPass, std::vector<VkImageView>& swapchainImageViews,
    std::vector<VkFramebuffer>& swapchainFramebuffers, int width, int height);

// Declarations for Vulkan_func_pipe.cpp
VkShaderModule createShaderModule(VkDevice device, const std::string& filename);

void createRenderPass(VkDevice device, VkRenderPass& renderPass, VkFormat format);

void createDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout& descriptorSetLayout);

void createDescriptorPoolAndSet(
    VkDevice device, VkDescriptorSetLayout descriptorSetLayout, VkDescriptorPool& descriptorPool,
    VkDescriptorSet& descriptorSet, VkSampler sampler);

void createSampler(VkDevice device, VkPhysicalDevice physicalDevice, VkSampler& sampler);

void createGraphicsPipeline(
    VkDevice device, VkRenderPass renderPass, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout,
    VkDescriptorSetLayout& descriptorSetLayout, int width, int height,
    VkShaderModule& vertShaderModule, VkShaderModule& fragShaderModule);
} // namespace VulkanInitializer

#endif // VULKAN_FUNC_HPP