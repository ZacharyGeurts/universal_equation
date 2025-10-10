// include/engine/Vulkan_init.hpp
#pragma once
#ifndef VULKAN_INIT_HPP
#define VULKAN_INIT_HPP

#include "engine/Vulkan_types.hpp"
#include <vulkan/vulkan.h>
#include <span>
#include <memory>
#include <glm/glm.hpp>
#include "universal_equation.hpp" // For AMOURANTH

class VulkanSwapchainManager;
class VulkanPipelineManager;
class VulkanBufferManager;

namespace VulkanInitializer {
    void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size,
                      VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                      VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void createDescriptorPoolAndSet(VkDevice device, VkDescriptorSetLayout descriptorSetLayout,
                                    VkDescriptorPool& descriptorPool, VkDescriptorSet& descriptorSet,
                                    VkSampler& sampler, VkBuffer uniformBuffer);
    // Other declarations as needed
}

class VulkanRenderer {
public:
    VulkanRenderer(VkInstance instance, VkSurfaceKHR surface,
                  std::span<const glm::vec3> vertices, std::span<const uint32_t> indices,
                  int width, int height);
    ~VulkanRenderer();
    void renderFrame(const AMOURANTH& amouranth);
    VkDevice getDevice() const { return context_.device; }
    VkDeviceMemory getVertexBufferMemory() const { return context_.vertexBufferMemory; }
    VkPipeline getGraphicsPipeline() const { return context_.pipeline; }
    void handleResize(int width, int height);

private:
    VulkanContext context_;
    std::unique_ptr<VulkanSwapchainManager> swapchainManager_;
    std::unique_ptr<VulkanPipelineManager> pipelineManager_;
    std::unique_ptr<VulkanBufferManager> bufferManager_;
};

#endif // VULKAN_INIT_HPP