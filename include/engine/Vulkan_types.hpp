// include/engine/Vulkan_types.hpp
#pragma once
#ifndef VULKAN_TYPES_HPP
#define VULKAN_TYPES_HPP

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

struct PushConstants {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

struct VulkanContext {
    VkInstance instance{VK_NULL_HANDLE};
    VkSurfaceKHR surface{VK_NULL_HANDLE};
    VkPhysicalDevice physicalDevice{VK_NULL_HANDLE};
    VkDevice device{VK_NULL_HANDLE};
    VkQueue graphicsQueue{VK_NULL_HANDLE};
    VkQueue presentQueue{VK_NULL_HANDLE};
    VkCommandPool commandPool{VK_NULL_HANDLE};
    VkRenderPass renderPass{VK_NULL_HANDLE};
    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};
    VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorPool descriptorPool{VK_NULL_HANDLE};
    VkDescriptorSet descriptorSet{VK_NULL_HANDLE};
    VkSampler sampler{VK_NULL_HANDLE};
    VkBuffer uniformBuffer{VK_NULL_HANDLE};
    VkDeviceMemory uniformBufferMemory{VK_NULL_HANDLE};
    VkBuffer vertexBuffer{VK_NULL_HANDLE};
    VkDeviceMemory vertexBufferMemory{VK_NULL_HANDLE};
    VkBuffer indexBuffer{VK_NULL_HANDLE};
    VkDeviceMemory indexBufferMemory{VK_NULL_HANDLE};
    VkBuffer sphereStagingBuffer{VK_NULL_HANDLE};
    VkDeviceMemory sphereStagingBufferMemory{VK_NULL_HANDLE};
    VkBuffer indexStagingBuffer{VK_NULL_HANDLE};
    VkDeviceMemory indexStagingBufferMemory{VK_NULL_HANDLE};
    // Ray tracing members
    VkPipeline rayTracingPipeline{VK_NULL_HANDLE};
    VkPipelineLayout rayTracingPipelineLayout{VK_NULL_HANDLE};
    VkDescriptorSetLayout rayTracingDescriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorSet rayTracingDescriptorSet{VK_NULL_HANDLE};
    VkAccelerationStructureKHR bottomLevelAS{VK_NULL_HANDLE};
    VkBuffer bottomLevelASBuffer{VK_NULL_HANDLE};
    VkDeviceMemory bottomLevelASBufferMemory{VK_NULL_HANDLE};
    VkAccelerationStructureKHR topLevelAS{VK_NULL_HANDLE};
    VkBuffer topLevelASBuffer{VK_NULL_HANDLE};
    VkDeviceMemory topLevelASBufferMemory{VK_NULL_HANDLE};
    VkBuffer shaderBindingTable{VK_NULL_HANDLE};
    VkDeviceMemory shaderBindingTableMemory{VK_NULL_HANDLE};
    VkImage storageImage{VK_NULL_HANDLE};
    VkDeviceMemory storageImageMemory{VK_NULL_HANDLE};
    VkImageView storageImageView{VK_NULL_HANDLE};
};

#endif // VULKAN_TYPES_HPP