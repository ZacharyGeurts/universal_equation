#include "Vulkan_init.hpp"

// Vulkan AMOURANTH RTX engine September, 2025 - Implementation for Vulkan initialization.
// Provides high-level initialization and cleanup functions for Vulkan resources.
// Delegates resource creation to VulkanInitializer in Vulkan_func.cpp.
// Zachary Geurts 2025

#include <stdexcept>
#include <iostream>
#include <cstring>

namespace VulkanInit {
    void initInstanceAndSurface(SDL_Window* window, VkInstance& instance, VkSurfaceKHR& surface, [[maybe_unused]] bool preferNvidia) {
        // Query available instance extensions
        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

        // Required extensions
        std::vector<const char*> requiredExtensions = {
            VK_KHR_SURFACE_EXTENSION_NAME
        };

        // Add platform-specific surface extension based on SDL's backend
        #ifdef VK_USE_PLATFORM_XLIB_KHR
        requiredExtensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
        #elif VK_USE_PLATFORM_XCB_KHR
        requiredExtensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
        #endif

        // Get SDL Vulkan extensions
        uint32_t sdlExtensionCount = 0;
        if (!SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount)) {
            throw std::runtime_error("Failed to get SDL Vulkan extension count: " + std::string(SDL_GetError()));
        }
        std::vector<const char*> sdlExtensions(sdlExtensionCount);
        if (!SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount)) {
            throw std::runtime_error("Failed to get SDL Vulkan extensions: " + std::string(SDL_GetError()));
        }

        // Combine SDL and required extensions
        requiredExtensions.insert(requiredExtensions.end(), sdlExtensions.begin(), sdlExtensions.end());

        // Verify all required extensions are available
        for (const auto& reqExt : requiredExtensions) {
            bool found = false;
            for (const auto& ext : availableExtensions) {
                if (std::strcmp(reqExt, ext.extensionName) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                throw std::runtime_error(std::string("Required Vulkan extension not supported: ") + reqExt);
            }
        }

        // Create Vulkan instance
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Dimensional Navigator";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "AMOURANTH RTX";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
        createInfo.ppEnabledExtensionNames = requiredExtensions.data();
        // Add validation layers for debugging (optional)
        const char* validationLayers[] = {"VK_LAYER_KHRONOS_validation"};
        createInfo.enabledLayerCount = 0; // Set to 1 for debugging
        createInfo.ppEnabledLayerNames = validationLayers;

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan instance");
        }

        // Create Vulkan surface
        if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
            vkDestroyInstance(instance, nullptr);
            throw std::runtime_error("Failed to create Vulkan surface: " + std::string(SDL_GetError()));
        }
    }

    void initializeVulkan(
        VkInstance& instance, VkPhysicalDevice& physicalDevice, VkDevice& device, VkSurfaceKHR& surface,
        VkQueue& graphicsQueue, VkQueue& presentQueue, uint32_t& graphicsFamily, uint32_t& presentFamily,
        VkSwapchainKHR& swapchain, std::vector<VkImage>& swapchainImages, std::vector<VkImageView>& swapchainImageViews,
        VkRenderPass& renderPass, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout, VkDescriptorSetLayout& descriptorSetLayout,
        std::vector<VkFramebuffer>& swapchainFramebuffers, VkCommandPool& commandPool,
        std::vector<VkCommandBuffer>& commandBuffers, VkSemaphore& imageAvailableSemaphore,
        VkSemaphore& renderFinishedSemaphore, VkFence& inFlightFence, VkBuffer& vertexBuffer,
        VkDeviceMemory& vertexBufferMemory, VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory,
        const std::vector<glm::vec3>& sphereVertices, const std::vector<uint32_t>& sphereIndices, int width, int height) {
        // Local variables for internal resources not exposed as outputs
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;

        // Step 1: Select physical device and queue families
        VulkanInitializer::createPhysicalDevice(instance, physicalDevice, graphicsFamily, presentFamily, surface, true);

        // Step 2: Create logical device with queues
        VulkanInitializer::createLogicalDevice(physicalDevice, device, graphicsQueue, presentQueue, graphicsFamily, presentFamily);

        // Step 3: Set up swapchain
        VkFormat swapchainFormat;
        VulkanInitializer::createSwapchain(physicalDevice, device, surface, swapchain, swapchainImages, swapchainImageViews, swapchainFormat, graphicsFamily, presentFamily, width, height);

        // Step 4: Create render pass
        VulkanInitializer::createRenderPass(device, renderPass, swapchainFormat);

        // Step 5: Create descriptor set layout
        VulkanInitializer::createDescriptorSetLayout(device, descriptorSetLayout);

        // Step 6: Create sampler
        VulkanInitializer::createSampler(device, physicalDevice, sampler);

        // Step 7: Create graphics pipeline
        VulkanInitializer::createGraphicsPipeline(device, renderPass, pipeline, pipelineLayout, descriptorSetLayout, width, height);

        // Step 8: Create descriptor pool and set
        VulkanInitializer::createDescriptorPoolAndSet(device, descriptorSetLayout, descriptorPool, descriptorSet, sampler);

        // Step 9: Create framebuffers
        VulkanInitializer::createFramebuffers(device, renderPass, swapchainImageViews, swapchainFramebuffers, width, height);

        // Step 10: Create command pool
        VulkanInitializer::createCommandPool(device, commandPool, graphicsFamily);

        // Step 11: Allocate command buffers
        VulkanInitializer::createCommandBuffers(device, commandPool, commandBuffers, swapchainFramebuffers);

        // Step 12: Create synchronization objects
        VulkanInitializer::createSyncObjects(device, imageAvailableSemaphore, renderFinishedSemaphore, inFlightFence);

        // Step 13: Create vertex and index buffers for sphere
        VulkanInitializer::createVertexBuffer(device, physicalDevice, commandPool, graphicsQueue, vertexBuffer, vertexBufferMemory, sphereVertices);
        VulkanInitializer::createIndexBuffer(device, physicalDevice, commandPool, graphicsQueue, indexBuffer, indexBufferMemory, sphereIndices);

        // Cleanup internal resources
        if (descriptorSet != VK_NULL_HANDLE) {
            vkFreeDescriptorSets(device, descriptorPool, 1, &descriptorSet);
        }
        if (descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        }
        if (sampler != VK_NULL_HANDLE) {
            vkDestroySampler(device, sampler, nullptr);
        }
    }

    void initializeQuadBuffers(
        VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
        VkBuffer& vertexBuffer, VkDeviceMemory& vertexBufferMemory, VkBuffer& indexBuffer,
        VkDeviceMemory& indexBufferMemory, const std::vector<glm::vec3>& vertices,
        const std::vector<uint32_t>& indices) {
        // Delegate to utility functions for buffer creation
        VulkanInitializer::createVertexBuffer(device, physicalDevice, commandPool, graphicsQueue, vertexBuffer, vertexBufferMemory, vertices);
        VulkanInitializer::createIndexBuffer(device, physicalDevice, commandPool, graphicsQueue, indexBuffer, indexBufferMemory, indices);
    }

    void cleanupVulkan(
        VkInstance /*instance*/, VkDevice& device, VkSurfaceKHR /*surface*/, VkSwapchainKHR& swapchain,
        std::vector<VkImageView>& swapchainImageViews, std::vector<VkFramebuffer>& swapchainFramebuffers,
        VkPipeline& pipeline, VkPipelineLayout& pipelineLayout, VkRenderPass& renderPass, VkCommandPool& commandPool,
        std::vector<VkCommandBuffer>& commandBuffers, VkSemaphore& imageAvailableSemaphore,
        VkSemaphore& renderFinishedSemaphore, VkFence& inFlightFence, VkBuffer& vertexBuffer,
        VkDeviceMemory& vertexBufferMemory, VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory,
        VkBuffer& quadVertexBuffer, VkDeviceMemory& quadVertexBufferMemory, VkBuffer& quadIndexBuffer,
        VkDeviceMemory& quadIndexBufferMemory, VkDescriptorSetLayout& descriptorSetLayout) {
        if (device == VK_NULL_HANDLE) return;

        // Wait for device to finish operations
        VkResult idleResult = vkDeviceWaitIdle(device);
        if (idleResult != VK_SUCCESS) {
            std::cerr << "Warning: vkDeviceWaitIdle failed during cleanup: " << idleResult << std::endl;
        }

        // Lambda for safe buffer destruction
        auto destroyBuffer = [&](VkBuffer& buffer, VkDeviceMemory& memory) {
            if (buffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(device, buffer, nullptr);
                buffer = VK_NULL_HANDLE;
            }
            if (memory != VK_NULL_HANDLE) {
                vkFreeMemory(device, memory, nullptr);
                memory = VK_NULL_HANDLE;
            }
        };

        // Destroy geometry buffers
        destroyBuffer(quadVertexBuffer, quadVertexBufferMemory);
        destroyBuffer(quadIndexBuffer, quadIndexBufferMemory);
        destroyBuffer(vertexBuffer, vertexBufferMemory);
        destroyBuffer(indexBuffer, indexBufferMemory);

        // Destroy synchronization objects
        if (imageAvailableSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
            imageAvailableSemaphore = VK_NULL_HANDLE;
        }
        if (renderFinishedSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
            renderFinishedSemaphore = VK_NULL_HANDLE;
        }
        if (inFlightFence != VK_NULL_HANDLE) {
            vkDestroyFence(device, inFlightFence, nullptr);
            inFlightFence = VK_NULL_HANDLE;
        }

        // Free command buffers and destroy pool
        if (!commandBuffers.empty()) {
            vkFreeCommandBuffers(device, commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
            commandBuffers.clear();
        }
        if (commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, commandPool, nullptr);
            commandPool = VK_NULL_HANDLE;
        }

        // Destroy framebuffers
        for (auto& fb : swapchainFramebuffers) {
            if (fb != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(device, fb, nullptr);
                fb = VK_NULL_HANDLE;
            }
        }
        swapchainFramebuffers.clear();

        // Destroy pipeline and layout
        if (pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, pipeline, nullptr);
            pipeline = VK_NULL_HANDLE;
        }
        if (pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
            pipelineLayout = VK_NULL_HANDLE;
        }

        // Destroy render pass
        if (renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device, renderPass, nullptr);
            renderPass = VK_NULL_HANDLE;
        }

        // Destroy image views
        for (auto& iv : swapchainImageViews) {
            if (iv != VK_NULL_HANDLE) {
                vkDestroyImageView(device, iv, nullptr);
                iv = VK_NULL_HANDLE;
            }
        }
        swapchainImageViews.clear();

        // Destroy swapchain
        if (swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device, swapchain, nullptr);
            swapchain = VK_NULL_HANDLE;
        }

        // Destroy descriptor set layout
        if (descriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
            descriptorSetLayout = VK_NULL_HANDLE;
        }

        // Destroy logical device
        if (device != VK_NULL_HANDLE) {
            vkDestroyDevice(device, nullptr);
            device = VK_NULL_HANDLE;
        }
    }
}