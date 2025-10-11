// VulkanCore.hpp
// AMOURANTH RTX Engine, October 2025 - Core Vulkan utilities.
// Dependencies: Vulkan 1.3+, GLM.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#pragma once
#ifndef VULKAN_CORE_HPP
#define VULKAN_CORE_HPP

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <span>
#include <glm/glm.hpp>

struct VulkanContext {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat swapchainImageFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent_ = {0, 0};
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkPhysicalDeviceMemoryProperties memoryProperties;
    uint32_t graphicsQueueFamilyIndex = 0;
    uint32_t presentQueueFamilyIndex = 0;
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer indexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
    VkImage storageImage = VK_NULL_HANDLE;
    VkDeviceMemory storageImageMemory = VK_NULL_HANDLE;
    VkImageView storageImageView = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VkAccelerationStructureKHR topLevelAS = VK_NULL_HANDLE;
    VkBuffer topLevelASBuffer = VK_NULL_HANDLE;
    VkDeviceMemory topLevelASBufferMemory = VK_NULL_HANDLE;
    VkAccelerationStructureKHR bottomLevelAS = VK_NULL_HANDLE;
    VkBuffer bottomLevelASBuffer = VK_NULL_HANDLE;
    VkDeviceMemory bottomLevelASBufferMemory = VK_NULL_HANDLE;
    VkPipeline rayTracingPipeline = VK_NULL_HANDLE;
    VkPipelineLayout rayTracingPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout rayTracingDescriptorSetLayout = VK_NULL_HANDLE;
    VkBuffer shaderBindingTable = VK_NULL_HANDLE;
    VkDeviceMemory shaderBindingTableMemory = VK_NULL_HANDLE;
};

namespace VulkanInitializer {
    // Functions defined in VulkanCore.cpp
    void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size,
                     VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                     VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    VkDeviceAddress getBufferDeviceAddress(VkDevice device, VkBuffer buffer);
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void createAccelerationStructures(VulkanContext& context, std::span<const glm::vec3> vertices, std::span<const uint32_t> indices);
    void createRayTracingPipeline(VulkanContext& context);
    void createShaderBindingTable(VulkanContext& context);

    // Functions defined in Vulkan_init.cpp
    void initializeVulkan(VulkanContext& context, int width, int height);
    void createSwapchain(VulkanContext& context, int width, int height);
    void createImageViews(VulkanContext& context);
    void createDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout& descriptorSetLayout);
    void createRenderPass(VkDevice device, VkRenderPass& renderPass, VkFormat format);
    VkShaderModule loadShader(VkDevice device, const std::string& filepath);
    void createGraphicsPipeline(VkDevice device, VkRenderPass renderPass, VkPipeline& pipeline,
                               VkPipelineLayout& pipelineLayout, VkDescriptorSetLayout& descriptorSetLayout,
                               int width, int height, VkShaderModule& vertexShaderModule, VkShaderModule& fragmentShaderModule);
    void createStorageImage(VkDevice device, VkPhysicalDevice physicalDevice, VkImage& storageImage,
                            VkDeviceMemory& storageImageMemory, VkImageView& storageImageView,
                            uint32_t width, uint32_t height);
    void createDescriptorPoolAndSet(VkDevice device, VkDescriptorSetLayout descriptorSetLayout,
                                   VkDescriptorPool& descriptorPool, VkDescriptorSet& descriptorSet,
                                   VkSampler& sampler, VkBuffer uniformBuffer, VkImageView storageImageView,
                                   VkAccelerationStructureKHR accelerationStructure);
}

#endif // VULKAN_CORE_HPP