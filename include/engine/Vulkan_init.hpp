#ifndef VULKAN_INIT_HPP
#define VULKAN_INIT_HPP

#include <vulkan/vulkan.h>
#include <vector>
#include <glm/glm.hpp>

class VulkanInit {
public:
    static void initializeVulkan(
        VkInstance& instance, VkPhysicalDevice& physicalDevice, VkDevice& device, VkSurfaceKHR& surface,
        VkQueue& graphicsQueue, VkQueue& presentQueue, uint32_t& graphicsFamily, uint32_t& presentFamily,
        VkSwapchainKHR& swapchain, std::vector<VkImage>& swapchainImages, std::vector<VkImageView>& swapchainImageViews,
        VkRenderPass& renderPass, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout,
        VkDescriptorSetLayout& descriptorSetLayout, std::vector<VkFramebuffer>& swapchainFramebuffers,
        VkCommandPool& commandPool, std::vector<VkCommandBuffer>& commandBuffers,
        std::vector<VkSemaphore>& imageAvailableSemaphores, std::vector<VkSemaphore>& renderFinishedSemaphores,
        std::vector<VkFence>& inFlightFences, VkBuffer& vertexBuffer, VkDeviceMemory& vertexBufferMemory,
        VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory,
        VkBuffer& sphereStagingBuffer, VkDeviceMemory& sphereStagingBufferMemory,
        VkBuffer& indexStagingBuffer, VkDeviceMemory& indexStagingBufferMemory,
        VkDescriptorSetLayout& descriptorSetLayoutOut, VkDescriptorPool& descriptorPool,
        VkDescriptorSet& descriptorSet, VkSampler& sampler,
        VkShaderModule& vertShaderModule, VkShaderModule& fragShaderModule,
        const std::vector<glm::vec3>& vertices, const std::vector<uint32_t>& indices, int width, int height);

    static void initializeQuadBuffers(
        VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
        VkBuffer& quadVertexBuffer, VkDeviceMemory& quadVertexBufferMemory,
        VkBuffer& quadIndexBuffer, VkDeviceMemory& quadIndexBufferMemory,
        VkBuffer& quadStagingBuffer, VkDeviceMemory& quadStagingBufferMemory,
        VkBuffer& quadIndexStagingBuffer, VkDeviceMemory& quadIndexStagingBufferMemory,
        const std::vector<glm::vec3>& quadVertices, const std::vector<uint32_t>& quadIndices);

    static void cleanupVulkan(
        VkDevice& device, VkSwapchainKHR& swapchain,
        std::vector<VkImageView>& swapchainImageViews, std::vector<VkFramebuffer>& swapchainFramebuffers,
        VkPipeline& pipeline, VkPipelineLayout& pipelineLayout, VkRenderPass& renderPass,
        VkCommandPool& commandPool, std::vector<VkCommandBuffer>& commandBuffers,
        std::vector<VkSemaphore>& imageAvailableSemaphores, std::vector<VkSemaphore>& renderFinishedSemaphores,
        std::vector<VkFence>& inFlightFences, VkBuffer& vertexBuffer, VkDeviceMemory& vertexBufferMemory,
        VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory,
        VkDescriptorSetLayout& descriptorSetLayout, VkDescriptorPool& descriptorPool,
        VkDescriptorSet& descriptorSet, VkSampler& sampler,
        VkBuffer& sphereStagingBuffer, VkDeviceMemory& sphereStagingBufferMemory,
        VkBuffer& indexStagingBuffer, VkDeviceMemory& indexStagingBufferMemory,
        VkShaderModule& vertShaderModule, VkShaderModule& fragShaderModule);
};

#endif // VULKAN_INIT_HPP