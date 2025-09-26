#ifndef VULKAN_INIT_HPP
#define VULKAN_INIT_HPP

// Vulkan AMOURANTH RTX engine September, 2025 - Header for Vulkan initialization.
// Provides static methods for high-level initialization and cleanup of Vulkan resources.
// Delegates to VulkanInitializer utility functions in Vulkan_func.hpp.
// Zachary Geurts 2025

#include <vulkan/vulkan.h>
#include <SDL3/SDL_vulkan.h>
#include <vector>
#include <glm/glm.hpp>
#include "Vulkan_func.hpp"

namespace VulkanInit {
    // Initializes the Vulkan instance and surface using SDL3.
    // Inputs: SDL window, preferNvidia flag for GPU selection.
    // Outputs: Vulkan instance and surface.
    void initInstanceAndSurface(
        SDL_Window* window, VkInstance& instance, VkSurfaceKHR& surface, bool preferNvidia = true
    );

    // Initializes all core Vulkan resources: device, queues, swapchain, render pass, pipeline, framebuffers,
    // command pool/buffers, sync objects, and geometry buffers for a sphere.
    // Inputs: instance, surface, geometry data (vertices/indices), window dimensions.
    // Outputs: All other parameters are filled with created resources.
    void initializeVulkan(
        VkInstance& instance, VkPhysicalDevice& physicalDevice, VkDevice& device, VkSurfaceKHR& surface,
        VkQueue& graphicsQueue, VkQueue& presentQueue, uint32_t& graphicsFamily, uint32_t& presentFamily,
        VkSwapchainKHR& swapchain, std::vector<VkImage>& swapchainImages, std::vector<VkImageView>& swapchainImageViews,
        VkRenderPass& renderPass, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout, VkDescriptorSetLayout& descriptorSetLayout,
        std::vector<VkFramebuffer>& swapchainFramebuffers, VkCommandPool& commandPool,
        std::vector<VkCommandBuffer>& commandBuffers, VkSemaphore& imageAvailableSemaphore,
        VkSemaphore& renderFinishedSemaphore, VkFence& inFlightFence, VkBuffer& vertexBuffer,
        VkDeviceMemory& vertexBufferMemory, VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory,
        const std::vector<glm::vec3>& sphereVertices, const std::vector<uint32_t>& sphereIndices, int width, int height
    );

    // Initializes vertex and index buffers for quad geometry (e.g., UI or post-processing).
    // Reuses existing device, physical device, command pool, and queue.
    void initializeQuadBuffers(
        VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
        VkBuffer& vertexBuffer, VkDeviceMemory& vertexBufferMemory, VkBuffer& indexBuffer,
        VkDeviceMemory& indexBufferMemory, const std::vector<glm::vec3>& vertices,
        const std::vector<uint32_t>& indices
    );

    // Cleans up all resources created by initializeVulkan and initializeQuadBuffers.
    // Safe to call even if some resources are null. Instance and surface are not destroyed (handled externally).
    void cleanupVulkan(
        VkInstance instance, VkDevice& device, VkSurfaceKHR surface, VkSwapchainKHR& swapchain,
        std::vector<VkImageView>& swapchainImageViews, std::vector<VkFramebuffer>& swapchainFramebuffers,
        VkPipeline& pipeline, VkPipelineLayout& pipelineLayout, VkRenderPass& renderPass, VkCommandPool& commandPool,
        std::vector<VkCommandBuffer>& commandBuffers, VkSemaphore& imageAvailableSemaphore,
        VkSemaphore& renderFinishedSemaphore, VkFence& inFlightFence, VkBuffer& vertexBuffer,
        VkDeviceMemory& vertexBufferMemory, VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory,
        VkBuffer& quadVertexBuffer, VkDeviceMemory& quadVertexBufferMemory, VkBuffer& quadIndexBuffer,
        VkDeviceMemory& quadIndexBufferMemory, VkDescriptorSetLayout& descriptorSetLayout
    );
}

#endif // VULKAN_INIT_HPP