// include/engine/Vulkan_init.hpp
#pragma once
#ifndef VULKAN_INIT_HPP
#define VULKAN_INIT_HPP

#include "engine/Vulkan_types.hpp"
#include "engine/Vulkan/Vulkan_func_swapchain.hpp"
#include "engine/Vulkan/Vulkan_func_pipe.hpp"
#include "engine/Vulkan_init_pipeline.hpp"
#include "engine/Vulkan_init_buffers.hpp"
#include "universal_equation.hpp"
#include <vulkan/vulkan.h>
#include <span>
#include <memory>
#include <glm/glm.hpp>

namespace VulkanInitializer {
    void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size,
                      VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                      VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void createDescriptorPoolAndSet(VkDevice device, VkDescriptorSetLayout descriptorSetLayout,
                                    VkDescriptorPool& descriptorPool, VkDescriptorSet& descriptorSet,
                                    VkSampler& sampler, VkBuffer uniformBuffer);
    void createRayTracingPipeline(VulkanContext& context);
    void createAccelerationStructures(VulkanContext& context, std::span<const glm::vec3> vertices, std::span<const uint32_t> indices);
    void createShaderBindingTable(VulkanContext& context);
    void createStorageImage(VulkanContext& context, uint32_t width, uint32_t height);
}

class VulkanRenderer {
public:
    VulkanRenderer(VkInstance instance, VkSurfaceKHR surface,
                  std::span<const glm::vec3> vertices, std::span<const uint32_t> indices,
                  int width, int height);
    ~VulkanRenderer();
    void renderFrame(const AMOURANTH& amouranth);
    VkDevice getDevice() const;
    VkDeviceMemory getVertexBufferMemory() const;
    VkPipeline getGraphicsPipeline() const;
    void handleResize(int width, int height);

private:
    VulkanContext context_;
    std::unique_ptr<VulkanSwapchainManager> swapchainManager_;
    std::unique_ptr<VulkanPipelineManager> pipelineManager_;
    std::unique_ptr<VulkanBufferManager> bufferManager_;
};

#endif // VULKAN_INIT_HPP