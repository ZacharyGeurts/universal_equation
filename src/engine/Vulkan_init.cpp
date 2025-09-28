#include "Vulkan_init.hpp"
#include "Vulkan_func.hpp"
#include <stdexcept>
#include <iostream>

void VulkanInit::initializeVulkan(
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
    [[maybe_unused]] VkDescriptorSetLayout& descriptorSetLayoutOut, VkDescriptorPool& descriptorPool,
    [[maybe_unused]] VkDescriptorSet& descriptorSet, VkSampler& sampler,
    VkShaderModule& vertShaderModule, VkShaderModule& fragShaderModule,
    const std::vector<glm::vec3>& vertices, const std::vector<uint32_t>& indices, int width, int height) {
    if (instance == VK_NULL_HANDLE || surface == VK_NULL_HANDLE) {
        throw std::runtime_error("Vulkan instance or surface is null");
    }

    VulkanInitializer::createPhysicalDevice(
        instance, physicalDevice, graphicsFamily, presentFamily, surface, true,
        [](const std::string& msg) { std::cerr << "Vulkan: " << msg << "\n"; });
    VkSurfaceFormatKHR surfaceFormat = VulkanInitializer::selectSurfaceFormat(physicalDevice, surface);
    VulkanInitializer::createLogicalDevice(physicalDevice, device, graphicsQueue, presentQueue, graphicsFamily, presentFamily);
    VulkanInitializer::createSwapchain(physicalDevice, device, surface, swapchain, swapchainImages, swapchainImageViews,
                                      surfaceFormat.format, graphicsFamily, presentFamily, width, height);
    VulkanInitializer::createRenderPass(device, renderPass, surfaceFormat.format);
    VulkanInitializer::createDescriptorSetLayout(device, descriptorSetLayout);
    VulkanInitializer::createGraphicsPipeline(device, renderPass, pipeline, pipelineLayout, descriptorSetLayout,
                                             width, height, vertShaderModule, fragShaderModule);
    VulkanInitializer::createFramebuffers(device, renderPass, swapchainImageViews, swapchainFramebuffers, width, height);
    VulkanInitializer::createCommandPool(device, commandPool, graphicsFamily);
    VulkanInitializer::createCommandBuffers(device, commandPool, commandBuffers, swapchainFramebuffers);
    VulkanInitializer::createSyncObjects(device, imageAvailableSemaphores, renderFinishedSemaphores, inFlightFences,
                                        static_cast<uint32_t>(swapchainImages.size()));
    VulkanInitializer::createVertexBuffer(device, physicalDevice, commandPool, graphicsQueue, vertexBuffer,
                                         vertexBufferMemory, sphereStagingBuffer, sphereStagingBufferMemory, vertices);
    VulkanInitializer::createIndexBuffer(device, physicalDevice, commandPool, graphicsQueue, indexBuffer,
                                        indexBufferMemory, indexStagingBuffer, indexStagingBufferMemory, indices);
    VulkanInitializer::createSampler(device, physicalDevice, sampler);
    VulkanInitializer::createDescriptorPoolAndSet(device, descriptorSetLayout, descriptorPool, descriptorSet, sampler);
}

void VulkanInit::initializeQuadBuffers(
    VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
    VkBuffer& quadVertexBuffer, VkDeviceMemory& quadVertexBufferMemory,
    VkBuffer& quadIndexBuffer, VkDeviceMemory& quadIndexBufferMemory,
    VkBuffer& quadStagingBuffer, VkDeviceMemory& quadStagingBufferMemory,
    VkBuffer& quadIndexStagingBuffer, VkDeviceMemory& quadIndexStagingBufferMemory,
    const std::vector<glm::vec3>& quadVertices, const std::vector<uint32_t>& quadIndices) {
    VulkanInitializer::createVertexBuffer(device, physicalDevice, commandPool, graphicsQueue, quadVertexBuffer,
                                         quadVertexBufferMemory, quadStagingBuffer, quadStagingBufferMemory, quadVertices);
    VulkanInitializer::createIndexBuffer(device, physicalDevice, commandPool, graphicsQueue, quadIndexBuffer,
                                        quadIndexBufferMemory, quadIndexStagingBuffer, quadIndexStagingBufferMemory, quadIndices);
}

void VulkanInit::cleanupVulkan(
    VkDevice& device, VkSwapchainKHR& swapchain,
    std::vector<VkImageView>& swapchainImageViews, std::vector<VkFramebuffer>& swapchainFramebuffers,
    VkPipeline& pipeline, VkPipelineLayout& pipelineLayout, VkRenderPass& renderPass,
    VkCommandPool& commandPool, [[maybe_unused]] std::vector<VkCommandBuffer>& commandBuffers,
    std::vector<VkSemaphore>& imageAvailableSemaphores, std::vector<VkSemaphore>& renderFinishedSemaphores,
    std::vector<VkFence>& inFlightFences, VkBuffer& vertexBuffer, VkDeviceMemory& vertexBufferMemory,
    VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory,
    VkDescriptorSetLayout& descriptorSetLayout, VkDescriptorPool& descriptorPool,
    [[maybe_unused]] VkDescriptorSet& descriptorSet, VkSampler& sampler,
    VkBuffer& sphereStagingBuffer, VkDeviceMemory& sphereStagingBufferMemory,
    VkBuffer& indexStagingBuffer, VkDeviceMemory& indexStagingBufferMemory,
    VkShaderModule& vertShaderModule, VkShaderModule& fragShaderModule) {
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);
        for (auto& fence : inFlightFences) {
            if (fence != VK_NULL_HANDLE) {
                vkDestroyFence(device, fence, nullptr);
                fence = VK_NULL_HANDLE;
            }
        }
        for (auto& semaphore : imageAvailableSemaphores) {
            if (semaphore != VK_NULL_HANDLE) {
                vkDestroySemaphore(device, semaphore, nullptr);
                semaphore = VK_NULL_HANDLE;
            }
        }
        for (auto& semaphore : renderFinishedSemaphores) {
            if (semaphore != VK_NULL_HANDLE) {
                vkDestroySemaphore(device, semaphore, nullptr);
                semaphore = VK_NULL_HANDLE;
            }
        }
        if (commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, commandPool, nullptr);
            commandPool = VK_NULL_HANDLE;
        }
        for (auto& framebuffer : swapchainFramebuffers) {
            if (framebuffer != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(device, framebuffer, nullptr);
                framebuffer = VK_NULL_HANDLE;
            }
        }
        if (pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, pipeline, nullptr);
            pipeline = VK_NULL_HANDLE;
        }
        if (pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
            pipelineLayout = VK_NULL_HANDLE;
        }
        if (renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device, renderPass, nullptr);
            renderPass = VK_NULL_HANDLE;
        }
        for (auto& imageView : swapchainImageViews) {
            if (imageView != VK_NULL_HANDLE) {
                vkDestroyImageView(device, imageView, nullptr);
                imageView = VK_NULL_HANDLE;
            }
        }
        if (swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device, swapchain, nullptr);
            swapchain = VK_NULL_HANDLE;
        }
        if (sampler != VK_NULL_HANDLE) {
            vkDestroySampler(device, sampler, nullptr);
            sampler = VK_NULL_HANDLE;
        }
        if (descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, descriptorPool, nullptr);
            descriptorPool = VK_NULL_HANDLE;
        }
        if (descriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
            descriptorSetLayout = VK_NULL_HANDLE;
        }
        if (vertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, vertexBuffer, nullptr);
            vertexBuffer = VK_NULL_HANDLE;
        }
        if (vertexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device, vertexBufferMemory, nullptr);
            vertexBufferMemory = VK_NULL_HANDLE;
        }
        if (indexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, indexBuffer, nullptr);
            indexBuffer = VK_NULL_HANDLE;
        }
        if (indexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device, indexBufferMemory, nullptr);
            indexBufferMemory = VK_NULL_HANDLE;
        }
        if (sphereStagingBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, sphereStagingBuffer, nullptr);
            sphereStagingBuffer = VK_NULL_HANDLE;
        }
        if (sphereStagingBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device, sphereStagingBufferMemory, nullptr);
            sphereStagingBufferMemory = VK_NULL_HANDLE;
        }
        if (indexStagingBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, indexStagingBuffer, nullptr);
            indexStagingBuffer = VK_NULL_HANDLE;
        }
        if (indexStagingBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device, indexStagingBufferMemory, nullptr);
            indexStagingBufferMemory = VK_NULL_HANDLE;
        }
        if (vertShaderModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device, vertShaderModule, nullptr);
            vertShaderModule = VK_NULL_HANDLE;
        }
        if (fragShaderModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device, fragShaderModule, nullptr);
            fragShaderModule = VK_NULL_HANDLE;
        }
        vkDestroyDevice(device, nullptr);
        device = VK_NULL_HANDLE;
    }
}