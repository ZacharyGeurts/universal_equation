#ifndef VULKAN_INIT_HPP
#define VULKAN_INIT_HPP

// AMOURANTH RTX engine September, 2025 - High-level Vulkan initialization and cleanup.
// Uses SDL3 for instance/surface; delegates to VulkanInitializer.
// Zachary Geurts 2025

#include <vulkan/vulkan.h>
#include <SDL3/SDL_vulkan.h>
#include <vector>
#include <glm/glm.hpp>
#include "Vulkan_func.hpp"

namespace VulkanInit {
    void initInstanceAndSurface(
        SDL_Window* window, VkInstance& instance, VkSurfaceKHR& surface, bool preferNvidia = true
    );

    void initializeVulkan(
        VkInstance& instance, VkPhysicalDevice& physicalDevice, VkDevice& device, VkSurfaceKHR& surface,
        VkQueue& graphicsQueue, VkQueue& presentQueue, uint32_t& graphicsFamily, uint32_t& presentFamily,
        VkSwapchainKHR& swapchain, std::vector<VkImage>& swapchainImages, std::vector<VkImageView>& swapchainImageViews,
        VkRenderPass& renderPass, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout, VkDescriptorSetLayout& descriptorSetLayout,
        std::vector<VkFramebuffer>& swapchainFramebuffers, VkCommandPool& commandPool,
        std::vector<VkCommandBuffer>& commandBuffers, std::vector<VkSemaphore>& imageAvailableSemaphores,
        std::vector<VkSemaphore>& renderFinishedSemaphores, std::vector<VkFence>& inFlightFences, VkBuffer& vertexBuffer,
        VkDeviceMemory& vertexBufferMemory, VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory,
        const std::vector<glm::vec3>& sphereVertices, const std::vector<uint32_t>& sphereIndices, int width, int height
    );

    void initializeQuadBuffers(
        VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
        VkBuffer& vertexBuffer, VkDeviceMemory& vertexBufferMemory, VkBuffer& indexBuffer,
        VkDeviceMemory& indexBufferMemory, const std::vector<glm::vec3>& vertices,
        const std::vector<uint32_t>& indices
    );

    void cleanupVulkan(
        VkInstance instance, VkDevice& device, VkSurfaceKHR surface, VkSwapchainKHR& swapchain,
        std::vector<VkImageView>& swapchainImageViews, std::vector<VkFramebuffer>& swapchainFramebuffers,
        VkPipeline& pipeline, VkPipelineLayout& pipelineLayout, VkRenderPass& renderPass, VkCommandPool& commandPool,
        std::vector<VkCommandBuffer>& commandBuffers, std::vector<VkSemaphore>& imageAvailableSemaphores,
        std::vector<VkSemaphore>& renderFinishedSemaphores, std::vector<VkFence>& inFlightFences, VkBuffer& vertexBuffer,
        VkDeviceMemory& vertexBufferMemory, VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory,
        VkBuffer& quadVertexBuffer, VkDeviceMemory& quadVertexBufferMemory, VkBuffer& quadIndexBuffer,
        VkDeviceMemory& quadIndexBufferMemory, VkDescriptorSetLayout& descriptorSetLayout
    );
}

#endif // VULKAN_INIT_HPP