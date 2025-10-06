// AMOURANTH RTX Engine, October 2025 - Vulkan core utilities implementation.
// Supports Windows/Linux; no mutexes; voxel geometry via glm::vec3 vertices.
// Dependencies: Vulkan 1.3+, GLM, C++20 standard library.
// Integrates with VulkanRTX for ray-traced voxel rendering, SwapchainManager for presentation,
// and Vulkan_func_pipe for graphics pipeline.
// Initializes Vulkan resources, buffers, and synchronization objects.
// Zachary Geurts 2025

#include "engine/Vulkan/Vulkan_func.hpp"
#include "engine/core.hpp"
#include <stdexcept>
#include <vector>
#include <set>
#include <cstring>
#include <limits>
#include <format>
#include <syncstream>
#include <iostream>
#include <ranges>
#include "engine/Vulkan/Vulkan_func_pipe.hpp"
#include "engine/Vulkan/Vulkan_func_swapchain.hpp"

// ANSI color codes
#define RESET "\033[0m"
#define MAGENTA "\033[1;35m" // Bold magenta for errors
#define CYAN "\033[1;36m"    // Bold cyan for debug
#define YELLOW "\033[1;33m"  // Bold yellow for warnings
#define GREEN "\033[1;32m"   // Bold green for info
#define BOLD "\033[1m"

namespace VulkanInitializer {

void initializeVulkan(
    VkInstance instance, VkPhysicalDevice& physicalDevice, VkDevice& device, VkSurfaceKHR surface,
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
    VkDescriptorSetLayout& descriptorSetLayout2, VkDescriptorPool& descriptorPool, VkDescriptorSet& descriptorSet,
    VkSampler& sampler, VkShaderModule& vertShaderModule, VkShaderModule& fragShaderModule,
    std::span<const glm::vec3> vertices, std::span<const uint32_t> indices, int width, int height) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Initializing Vulkan resources with width=" << width << ", height=" << height << RESET << std::endl;

    createPhysicalDevice(instance, physicalDevice, graphicsFamily, presentFamily, surface, true);

    createLogicalDevice(physicalDevice, device, graphicsQueue, presentQueue, graphicsFamily, presentFamily);
    if (device == VK_NULL_HANDLE) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to create logical device" << RESET << std::endl;
        throw std::runtime_error("Failed to create logical device");
    }
    std::osyncstream(std::cout) << CYAN << "[DEBUG] Logical device created successfully" << RESET << std::endl;

    VkSurfaceFormatKHR surfaceFormat = selectSurfaceFormat(physicalDevice, surface);
    std::osyncstream(std::cout) << CYAN << "[DEBUG] Selected surface format: " << surfaceFormat.format << RESET << std::endl;

    createSwapchain(physicalDevice, device, surface, swapchain, swapchainImages, swapchainImageViews,
                    surfaceFormat.format, graphicsFamily, presentFamily, width, height);

    createRenderPass(device, renderPass, surfaceFormat.format);

    createDescriptorSetLayout(device, descriptorSetLayout);
    descriptorSetLayout2 = descriptorSetLayout;
    std::osyncstream(std::cout) << CYAN << "[DEBUG] Descriptor set layout copied to descriptorSetLayout2" << RESET << std::endl;

    createGraphicsPipeline(device, renderPass, pipeline, pipelineLayout, descriptorSetLayout, width, height,
                          vertShaderModule, fragShaderModule);

    createFramebuffers(device, renderPass, swapchainImageViews, swapchainFramebuffers, width, height);

    createCommandPool(device, commandPool, graphicsFamily);
    createCommandBuffers(device, commandPool, commandBuffers, swapchainFramebuffers);
    std::osyncstream(std::cout) << CYAN << "[DEBUG] Created " << commandBuffers.size() << " command buffers" << RESET << std::endl;

    createSyncObjects(device, imageAvailableSemaphores, renderFinishedSemaphores, inFlightFences,
                      static_cast<uint32_t>(swapchainImages.size()));
    std::osyncstream(std::cout) << CYAN << "[DEBUG] Created sync objects for " << swapchainImages.size() << " frames" << RESET << std::endl;

    createVertexBuffer(device, physicalDevice, commandPool, graphicsQueue,
                       vertexBuffer, vertexBufferMemory, sphereStagingBuffer, sphereStagingBufferMemory, vertices);

    createIndexBuffer(device, physicalDevice, commandPool, graphicsQueue,
                      indexBuffer, indexBufferMemory, indexStagingBuffer, indexStagingBufferMemory, indices);

    createSampler(device, physicalDevice, sampler);
    createDescriptorPoolAndSet(device, descriptorSetLayout, descriptorPool, descriptorSet, sampler);

    std::osyncstream(std::cout) << GREEN << "[INFO] Vulkan initialization completed successfully" << RESET << std::endl;
}

void initializeQuadBuffers(
    VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
    VkBuffer& quadVertexBuffer, VkDeviceMemory& quadVertexBufferMemory,
    VkBuffer& quadIndexBuffer, VkDeviceMemory& quadIndexBufferMemory,
    VkBuffer& quadStagingBuffer, VkDeviceMemory& quadStagingBufferMemory,
    VkBuffer& quadIndexStagingBuffer, VkDeviceMemory& quadIndexStagingBufferMemory,
    std::span<const glm::vec3> quadVertices, std::span<const uint32_t> quadIndices) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Initializing quad buffers" << RESET << std::endl;

    createVertexBuffer(device, physicalDevice, commandPool, graphicsQueue,
                       quadVertexBuffer, quadVertexBufferMemory, quadStagingBuffer, quadStagingBufferMemory, quadVertices);
    createIndexBuffer(device, physicalDevice, commandPool, graphicsQueue,
                      quadIndexBuffer, quadIndexBufferMemory, quadIndexStagingBuffer, quadIndexStagingBufferMemory, quadIndices);

    std::osyncstream(std::cout) << GREEN << "[INFO] Quad buffers initialized successfully" << RESET << std::endl;
}

void initializeVoxelBuffers(
    VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
    VkBuffer& voxelVertexBuffer, VkDeviceMemory& voxelVertexBufferMemory,
    VkBuffer& voxelIndexBuffer, VkDeviceMemory& voxelIndexBufferMemory,
    VkBuffer& voxelStagingBuffer, VkDeviceMemory& voxelStagingBufferMemory,
    VkBuffer& voxelIndexStagingBuffer, VkDeviceMemory& voxelIndexStagingBufferMemory,
    std::span<const glm::vec3> voxelVertices, std::span<const uint32_t> voxelIndices) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Initializing voxel buffers" << RESET << std::endl;

    createVertexBuffer(device, physicalDevice, commandPool, graphicsQueue,
                       voxelVertexBuffer, voxelVertexBufferMemory, voxelStagingBuffer, voxelStagingBufferMemory, voxelVertices);
    createIndexBuffer(device, physicalDevice, commandPool, graphicsQueue,
                      voxelIndexBuffer, voxelIndexBufferMemory, voxelIndexStagingBuffer, voxelIndexStagingBufferMemory, voxelIndices);

    std::osyncstream(std::cout) << GREEN << "[INFO] Voxel buffers initialized successfully" << RESET << std::endl;
}

void cleanupVulkan(
    VkDevice device, VkSwapchainKHR& swapchain, std::vector<VkImageView>& swapchainImageViews,
    std::vector<VkFramebuffer>& swapchainFramebuffers, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout,
    VkRenderPass& renderPass, VkCommandPool& commandPool, std::vector<VkCommandBuffer>& commandBuffers,
    std::vector<VkSemaphore>& imageAvailableSemaphores, std::vector<VkSemaphore>& renderFinishedSemaphores,
    std::vector<VkFence>& inFlightFences, VkBuffer& vertexBuffer, VkDeviceMemory& vertexBufferMemory,
    VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory,
    VkDescriptorSetLayout& descriptorSetLayout, VkDescriptorPool& descriptorPool, VkDescriptorSet& descriptorSet,
    VkSampler& sampler, VkBuffer& sphereStagingBuffer, VkDeviceMemory& sphereStagingBufferMemory,
    VkBuffer& indexStagingBuffer, VkDeviceMemory& indexStagingBufferMemory,
    VkShaderModule& vertShaderModule, VkShaderModule& fragShaderModule) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Cleaning up Vulkan resources" << RESET << std::endl;

    if (device == VK_NULL_HANDLE) {
        std::osyncstream(std::cout) << YELLOW << "[WARNING] Device is null, skipping cleanup" << RESET << std::endl;
        return;
    }

    vkDeviceWaitIdle(device);
    std::osyncstream(std::cout) << CYAN << "[DEBUG] Device idle, proceeding with cleanup" << RESET << std::endl;

    for (size_t i : std::views::iota(0u, inFlightFences.size())) {
        if (inFlightFences[i] != VK_NULL_HANDLE) {
            vkDestroyFence(device, inFlightFences[i], nullptr);
            std::osyncstream(std::cout) << CYAN << "[DEBUG] Destroyed fence " << i << RESET << std::endl;
            inFlightFences[i] = VK_NULL_HANDLE;
        }
    }
    inFlightFences.clear();

    for (size_t i : std::views::iota(0u, imageAvailableSemaphores.size())) {
        if (imageAvailableSemaphores[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            std::osyncstream(std::cout) << CYAN << "[DEBUG] Destroyed image available semaphore " << i << RESET << std::endl;
            imageAvailableSemaphores[i] = VK_NULL_HANDLE;
        }
    }
    imageAvailableSemaphores.clear();

    for (size_t i : std::views::iota(0u, renderFinishedSemaphores.size())) {
        if (renderFinishedSemaphores[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
            std::osyncstream(std::cout) << CYAN << "[DEBUG] Destroyed render finished semaphore " << i << RESET << std::endl;
            renderFinishedSemaphores[i] = VK_NULL_HANDLE;
        }
    }
    renderFinishedSemaphores.clear();

    if (commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, commandPool, nullptr);
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Destroyed command pool" << RESET << std::endl;
        commandPool = VK_NULL_HANDLE;
    }
    commandBuffers.clear();

    for (size_t i : std::views::iota(0u, swapchainFramebuffers.size())) {
        if (swapchainFramebuffers[i] != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device, swapchainFramebuffers[i], nullptr);
            std::osyncstream(std::cout) << CYAN << "[DEBUG] Destroyed framebuffer " << i << RESET << std::endl;
            swapchainFramebuffers[i] = VK_NULL_HANDLE;
        }
    }
    swapchainFramebuffers.clear();

    if (pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, pipeline, nullptr);
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Destroyed pipeline" << RESET << std::endl;
        pipeline = VK_NULL_HANDLE;
    }

    if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Destroyed pipeline layout" << RESET << std::endl;
        pipelineLayout = VK_NULL_HANDLE;
    }

    if (renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, renderPass, nullptr);
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Destroyed render pass" << RESET << std::endl;
        renderPass = VK_NULL_HANDLE;
    }

    for (size_t i : std::views::iota(0u, swapchainImageViews.size())) {
        if (swapchainImageViews[i] != VK_NULL_HANDLE) {
            vkDestroyImageView(device, swapchainImageViews[i], nullptr);
            std::osyncstream(std::cout) << CYAN << "[DEBUG] Destroyed image view " << i << RESET << std::endl;
            swapchainImageViews[i] = VK_NULL_HANDLE;
        }
    }
    swapchainImageViews.clear();

    if (swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Destroyed swapchain" << RESET << std::endl;
        swapchain = VK_NULL_HANDLE;
    }

    if (descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Destroyed descriptor set layout" << RESET << std::endl;
        descriptorSetLayout = VK_NULL_HANDLE;
    }

    if (descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Destroyed descriptor pool" << RESET << std::endl;
        descriptorPool = VK_NULL_HANDLE;
    }
    descriptorSet = VK_NULL_HANDLE;

    if (sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, sampler, nullptr);
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Destroyed sampler" << RESET << std::endl;
        sampler = VK_NULL_HANDLE;
    }

    if (vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, vertexBuffer, nullptr);
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Destroyed vertex buffer" << RESET << std::endl;
        vertexBuffer = VK_NULL_HANDLE;
    }

    if (vertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, vertexBufferMemory, nullptr);
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Freed vertex buffer memory" << RESET << std::endl;
        vertexBufferMemory = VK_NULL_HANDLE;
    }

    if (indexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, indexBuffer, nullptr);
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Destroyed index buffer" << RESET << std::endl;
        indexBuffer = VK_NULL_HANDLE;
    }

    if (indexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, indexBufferMemory, nullptr);
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Freed index buffer memory" << RESET << std::endl;
        indexBufferMemory = VK_NULL_HANDLE;
    }

    if (sphereStagingBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, sphereStagingBuffer, nullptr);
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Destroyed sphere staging buffer" << RESET << std::endl;
        sphereStagingBuffer = VK_NULL_HANDLE;
    }

    if (sphereStagingBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, sphereStagingBufferMemory, nullptr);
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Freed sphere staging buffer memory" << RESET << std::endl;
        sphereStagingBufferMemory = VK_NULL_HANDLE;
    }

    if (indexStagingBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, indexStagingBuffer, nullptr);
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Destroyed index staging buffer" << RESET << std::endl;
        indexStagingBuffer = VK_NULL_HANDLE;
    }

    if (indexStagingBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, indexStagingBufferMemory, nullptr);
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Freed index staging buffer memory" << RESET << std::endl;
        indexStagingBufferMemory = VK_NULL_HANDLE;
    }

    if (vertShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Destroyed vertex shader module" << RESET << std::endl;
        vertShaderModule = VK_NULL_HANDLE;
    }

    if (fragShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Destroyed fragment shader module" << RESET << std::endl;
        fragShaderModule = VK_NULL_HANDLE;
    }

    vkDestroyDevice(device, nullptr);
    std::osyncstream(std::cout) << CYAN << "[DEBUG] Destroyed device" << RESET << std::endl;
    device = VK_NULL_HANDLE;

    std::osyncstream(std::cout) << GREEN << "[INFO] Vulkan cleanup completed successfully" << RESET << std::endl;
}

void createPhysicalDevice(VkInstance instance, VkPhysicalDevice& physicalDevice, uint32_t& graphicsFamily,
                         uint32_t& presentFamily, VkSurfaceKHR surface, bool preferNvidia) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Creating physical device with preferNvidia=" << preferNvidia << RESET << std::endl;

    uint32_t deviceCount = 0;
    VkResult result = vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (result != VK_SUCCESS || deviceCount == 0) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] No Vulkan-compatible devices found: " << result << RESET << std::endl;
        throw std::runtime_error(std::format("No Vulkan-compatible devices found: {}", result));
    }
    std::osyncstream(std::cout) << CYAN << "[DEBUG] Found " << deviceCount << " Vulkan devices" << RESET << std::endl;

    std::vector<VkPhysicalDevice> devices(deviceCount);
    result = vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
    if (result != VK_SUCCESS) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to enumerate physical devices: " << result << RESET << std::endl;
        throw std::runtime_error(std::format("Failed to enumerate physical devices: {}", result));
    }

    uint32_t instanceExtCount = 0;
    result = vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtCount, nullptr);
    if (result != VK_SUCCESS) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to enumerate instance extensions: " << result << RESET << std::endl;
        throw std::runtime_error(std::format("Failed to enumerate instance extensions: {}", result));
    }
    std::vector<VkExtensionProperties> instanceExtensions(instanceExtCount);
    result = vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtCount, instanceExtensions.data());
    if (result != VK_SUCCESS) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to retrieve instance extensions: " << result << RESET << std::endl;
        throw std::runtime_error(std::format("Failed to retrieve instance extensions: {}", result));
    }
    bool hasSurfaceExtension = false;
    for (const auto& ext : instanceExtensions) {
        if (strcmp(ext.extensionName, VK_KHR_SURFACE_EXTENSION_NAME) == 0) {
            hasSurfaceExtension = true;
            std::osyncstream(std::cout) << CYAN << "[DEBUG] VK_KHR_surface extension supported" << RESET << std::endl;
            break;
        }
    }
    if (!hasSurfaceExtension) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] VK_KHR_surface extension not supported" << RESET << std::endl;
        throw std::runtime_error("VK_KHR_surface extension not supported");
    }

    auto rateDevice = [preferNvidia, surface](VkPhysicalDevice dev, QueueFamilyIndices& indices, const DeviceRequirements& reqs) -> std::pair<int, std::string> {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev, &props);
        std::string deviceName = props.deviceName;
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Evaluating device: " << deviceName << RESET << std::endl;

        int score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 1000;
            if (preferNvidia && props.vendorID == 0x10DE) {
                score += 500;
                std::osyncstream(std::cout) << CYAN << "[DEBUG] Device " << deviceName << " is NVIDIA; score increased" << RESET << std::endl;
            }
        } else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
            score += 100;
            std::osyncstream(std::cout) << CYAN << "[DEBUG] Device " << deviceName << " is integrated GPU" << RESET << std::endl;
        } else {
            std::osyncstream(std::cout) << YELLOW << "[WARNING] Device " << deviceName << " is neither discrete nor integrated GPU" << RESET << std::endl;
            return {0, "Unsupported device type"};
        }

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, queueFamilies.data());

        indices = {};
        bool presentSupportFound = false;
        for (uint32_t i : std::views::iota(0u, queueFamilyCount)) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
                std::osyncstream(std::cout) << CYAN << "[DEBUG] Found graphics queue family " << i << RESET << std::endl;
            }
            VkBool32 presentSupport = VK_FALSE;
            VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &presentSupport);
            if (result != VK_SUCCESS) {
                std::osyncstream(std::cout) << YELLOW << "[WARNING] Device " << deviceName << " failed to query surface support: " << result << RESET << std::endl;
                return {0, std::format("Failed to query surface support: {}", result)};
            }
            if (presentSupport) {
                indices.presentFamily = i;
                presentSupportFound = true;
                std::osyncstream(std::cout) << CYAN << "[DEBUG] Found present queue family " << i << RESET << std::endl;
            }
        }
        if (!indices.isComplete()) {
            std::osyncstream(std::cout) << YELLOW << "[WARNING] Device " << deviceName << " lacks required queue families" << RESET << std::endl;
            return {0, "Lacks required queue families"};
        }
        if (!presentSupportFound) {
            std::osyncstream(std::cout) << YELLOW << "[WARNING] Device " << deviceName << " does not support presentation" << RESET << std::endl;
            return {0, "No presentation support"};
        }

        uint32_t extCount = 0;
        result = vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, nullptr);
        if (result != VK_SUCCESS) {
            std::osyncstream(std::cout) << YELLOW << "[WARNING] Device " << deviceName << " failed to enumerate extensions: " << result << RESET << std::endl;
            return {0, std::format("Failed to enumerate extensions: {}", result)};
        }
        std::vector<VkExtensionProperties> availableExts(extCount);
        result = vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, availableExts.data());
        if (result != VK_SUCCESS) {
            std::osyncstream(std::cout) << YELLOW << "[WARNING] Device " << deviceName << " failed to retrieve extensions: " << result << RESET << std::endl;
            return {0, std::format("Failed to retrieve extensions: {}", result)};
        }

        std::set<std::string> requiredExts(reqs.extensions.begin(), reqs.extensions.end());
        for (const auto& ext : availableExts) {
            requiredExts.erase(ext.extensionName);
        }
        if (!requiredExts.empty()) {
            std::string missing;
            for (const auto& ext : requiredExts) {
                missing += std::format("{}, ", ext);
            }
            std::osyncstream(std::cout) << YELLOW << "[WARNING] Device " << deviceName << " missing extensions: " << missing.substr(0, missing.size() - 2) << RESET << std::endl;
            return {0, std::format("Missing extensions: {}", missing.substr(0, missing.size() - 2))};
        }

        VkPhysicalDeviceFeatures2 features2{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2
        };
        VkPhysicalDeviceMaintenance4Features maint4 = reqs.maintenance4Features;
        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rt = reqs.rayTracingFeatures;
        VkPhysicalDeviceAccelerationStructureFeaturesKHR accel = reqs.accelerationStructureFeatures;
        VkPhysicalDeviceBufferDeviceAddressFeatures bufAddr = reqs.bufferDeviceAddressFeatures;
        maint4.pNext = &accel;
        accel.pNext = &rt;
        rt.pNext = &bufAddr;
        features2.pNext = &maint4;
        vkGetPhysicalDeviceFeatures2(dev, &features2);

        if (!features2.features.samplerAnisotropy || !maint4.maintenance4 || !rt.rayTracingPipeline ||
            !accel.accelerationStructure || !bufAddr.bufferDeviceAddress) {
            std::osyncstream(std::cout) << YELLOW << "[WARNING] Device " << deviceName << " lacks required features" << RESET << std::endl;
            return {0, "Lacks required features"};
        }

        std::osyncstream(std::cout) << CYAN << "[DEBUG] Device " << deviceName << " is suitable with score: " << score << RESET << std::endl;
        return {score, ""};
    };

    int maxScore = 0;
    physicalDevice = VK_NULL_HANDLE;
    QueueFamilyIndices selectedIndices;
    DeviceRequirements reqs;
    std::vector<std::pair<VkPhysicalDevice, QueueFamilyIndices>> fallbackDevices;
    std::vector<VkPhysicalDeviceProperties> fallbackProps;

    for (const auto& dev : devices) {
        QueueFamilyIndices indices;
        auto [score, reason] = rateDevice(dev, indices, reqs);
        if (score > maxScore) {
            maxScore = score;
            physicalDevice = dev;
            selectedIndices = indices;
            std::osyncstream(std::cout) << CYAN << "[DEBUG] Selected device with score: " << score << RESET << std::endl;
        }
        if (score > 0) {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(dev, &props);
            fallbackDevices.emplace_back(dev, indices);
            fallbackProps.push_back(props);
            std::osyncstream(std::cout) << CYAN << "[DEBUG] Stored fallback device: " << props.deviceName << RESET << std::endl;
        } else {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(dev, &props);
            std::osyncstream(std::cout) << YELLOW << "[WARNING] Rejected device: " << props.deviceName << " (reason: " << reason << ")" << RESET << std::endl;
        }
    }

    if (physicalDevice == VK_NULL_HANDLE && !fallbackDevices.empty()) {
        std::osyncstream(std::cout) << YELLOW << "[WARNING] No preferred device found; trying fallback devices" << RESET << std::endl;
        for (size_t i : std::views::iota(0u, fallbackDevices.size())) {
            physicalDevice = fallbackDevices[i].first;
            selectedIndices = fallbackDevices[i].second;
            std::osyncstream(std::cout) << CYAN << "[DEBUG] Falling back to device: " << fallbackProps[i].deviceName << RESET << std::endl;
            VkBool32 presentSupport = VK_FALSE;
            result = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, selectedIndices.presentFamily.value(), surface, &presentSupport);
            if (result == VK_SUCCESS && presentSupport) {
                std::osyncstream(std::cout) << CYAN << "[DEBUG] Fallback device supports surface; selected" << RESET << std::endl;
                break;
            } else {
                std::osyncstream(std::cout) << YELLOW << "[WARNING] Fallback device failed surface support: " << result << RESET << std::endl;
                physicalDevice = VK_NULL_HANDLE;
            }
        }
    }

    if (physicalDevice == VK_NULL_HANDLE) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] No suitable Vulkan device found" << RESET << std::endl;
        throw std::runtime_error("No suitable Vulkan device found");
    }

    graphicsFamily = selectedIndices.graphicsFamily.value();
    presentFamily = selectedIndices.presentFamily.value();

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physicalDevice, &props);
    std::osyncstream(std::cout) << GREEN << "[INFO] Selected device: " << props.deviceName << RESET << std::endl;
}

void createLogicalDevice(VkPhysicalDevice physicalDevice, VkDevice& device, VkQueue& graphicsQueue,
                         VkQueue& presentQueue, uint32_t graphicsFamily, uint32_t presentFamily) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Creating logical device with graphicsFamily=" << graphicsFamily << ", presentFamily=" << presentFamily << RESET << std::endl;

    DeviceRequirements reqs;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {graphicsFamily, presentFamily};

    const float queuePriority = 1.0f;
    for (const uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = queueFamily,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority
        };
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{
        .samplerAnisotropy = VK_TRUE
    };

    reqs.maintenance4Features.pNext = &reqs.accelerationStructureFeatures;
    reqs.accelerationStructureFeatures.pNext = &reqs.rayTracingFeatures;
    reqs.rayTracingFeatures.pNext = &reqs.bufferDeviceAddressFeatures;

    VkDeviceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &reqs.maintenance4Features,
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledExtensionCount = static_cast<uint32_t>(reqs.extensions.size()),
        .ppEnabledExtensionNames = reqs.extensions.data(),
        .pEnabledFeatures = &deviceFeatures
    };

    VkResult result = vkCreateDevice(physicalDevice, &createInfo, nullptr, &device);
    if (result != VK_SUCCESS) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to create logical device: " << result << RESET << std::endl;
        throw std::runtime_error(std::format("Failed to create logical device: {}", result));
    }

    vkGetDeviceQueue(device, graphicsFamily, 0, &graphicsQueue);
    vkGetDeviceQueue(device, presentFamily, 0, &presentQueue);
    std::osyncstream(std::cout) << GREEN << "[INFO] Logical device created successfully" << RESET << std::endl;
}

void createCommandPool(VkDevice device, VkCommandPool& commandPool, uint32_t graphicsFamily) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Creating command pool for graphicsFamily=" << graphicsFamily << RESET << std::endl;

    VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = graphicsFamily
    };

    VkResult result = vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool);
    if (result != VK_SUCCESS) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to create command pool: " << result << RESET << std::endl;
        throw std::runtime_error(std::format("Failed to create command pool: {}", result));
    }

    std::osyncstream(std::cout) << GREEN << "[INFO] Command pool created successfully" << RESET << std::endl;
}

void createCommandBuffers(VkDevice device, VkCommandPool commandPool, std::vector<VkCommandBuffer>& commandBuffers,
                          std::vector<VkFramebuffer>& swapchainFramebuffers) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Creating command buffers for " << swapchainFramebuffers.size() << " framebuffers" << RESET << std::endl;

    commandBuffers.resize(swapchainFramebuffers.size());
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<uint32_t>(commandBuffers.size())
    };

    VkResult result = vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data());
    if (result != VK_SUCCESS) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to allocate command buffers: " << result << RESET << std::endl;
        throw std::runtime_error(std::format("Failed to allocate command buffers: {}", result));
    }

    std::osyncstream(std::cout) << GREEN << "[INFO] Command buffers created successfully" << RESET << std::endl;
}

void createSyncObjects(VkDevice device, std::vector<VkSemaphore>& imageAvailableSemaphores,
                       std::vector<VkSemaphore>& renderFinishedSemaphores, std::vector<VkFence>& inFlightFences,
                       uint32_t maxFramesInFlight) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Creating sync objects for " << maxFramesInFlight << " frames" << RESET << std::endl;

    imageAvailableSemaphores.resize(maxFramesInFlight);
    renderFinishedSemaphores.resize(maxFramesInFlight);
    inFlightFences.resize(maxFramesInFlight);

    VkSemaphoreCreateInfo semaphoreInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    VkFenceCreateInfo fenceInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    for (uint32_t i : std::views::iota(0u, maxFramesInFlight)) {
        VkResult result;
        result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]);
        if (result != VK_SUCCESS) {
            std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to create image available semaphore " << i << ": " << result << RESET << std::endl;
            throw std::runtime_error(std::format("Failed to create image available semaphore: {}", result));
        }
        result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]);
        if (result != VK_SUCCESS) {
            std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to create render finished semaphore " << i << ": " << result << RESET << std::endl;
            throw std::runtime_error(std::format("Failed to create render finished semaphore: {}", result));
        }
        result = vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]);
        if (result != VK_SUCCESS) {
            std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to create in-flight fence " << i << ": " << result << RESET << std::endl;
            throw std::runtime_error(std::format("Failed to create in-flight fence: {}", result));
        }
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Created sync objects for frame " << i << RESET << std::endl;
    }

    std::osyncstream(std::cout) << GREEN << "[INFO] Sync objects created successfully" << RESET << std::endl;
}

void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
                  VkMemoryPropertyFlags props, VkBuffer& buffer, VkDeviceMemory& memory) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Creating buffer with size=" << size << RESET << std::endl;

    if (size == 0) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Cannot create buffer with zero size" << RESET << std::endl;
        throw std::runtime_error("Cannot create buffer with zero size");
    }

    VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);
    if (result != VK_SUCCESS) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to create buffer: " << result << RESET << std::endl;
        throw std::runtime_error(std::format("Failed to create buffer: {}", result));
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);
    std::osyncstream(std::cout) << CYAN << "[DEBUG] Buffer memory requirements: size=" << memRequirements.size << ", alignment=" << memRequirements.alignment << RESET << std::endl;

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    uint32_t memoryTypeIndex = std::numeric_limits<uint32_t>::max();
    for (uint32_t i : std::views::iota(0u, memProperties.memoryTypeCount)) {
        if ((memRequirements.memoryTypeBits & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & props) == props) {
            memoryTypeIndex = i;
            std::osyncstream(std::cout) << CYAN << "[DEBUG] Selected memory type index: " << i << RESET << std::endl;
            break;
        }
    }
    if (memoryTypeIndex == std::numeric_limits<uint32_t>::max()) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to find suitable memory type for buffer" << RESET << std::endl;
        throw std::runtime_error("Failed to find suitable memory type for buffer");
    }

    VkMemoryAllocateFlagsInfo allocFlagsInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .flags = static_cast<VkMemoryAllocateFlags>((usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) ? VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT : 0)
    };

    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) ? &allocFlagsInfo : nullptr,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = memoryTypeIndex
    };

    result = vkAllocateMemory(device, &allocInfo, nullptr, &memory);
    if (result != VK_SUCCESS) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to allocate buffer memory: " << result << RESET << std::endl;
        throw std::runtime_error(std::format("Failed to allocate buffer memory: {}", result));
    }

    result = vkBindBufferMemory(device, buffer, memory, 0);
    if (result != VK_SUCCESS) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to bind buffer memory: " << result << RESET << std::endl;
        throw std::runtime_error(std::format("Failed to bind buffer memory: {}", result));
    }

    std::osyncstream(std::cout) << GREEN << "[INFO] Buffer created successfully" << RESET << std::endl;
}

void copyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkBuffer src, VkBuffer dst,
                VkDeviceSize size) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Copying buffer with size=" << size << RESET << std::endl;

    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    VkCommandBuffer commandBuffer;
    VkResult result = vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);
    if (result != VK_SUCCESS) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to allocate command buffer for copy: " << result << RESET << std::endl;
        throw std::runtime_error(std::format("Failed to allocate command buffer for copy: {}", result));
    }

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (result != VK_SUCCESS) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to begin command buffer: " << result << RESET << std::endl;
        throw std::runtime_error(std::format("Failed to begin command buffer: {}", result));
    }

    VkBufferCopy copyRegion{
        .srcOffset = 0,
        .dstOffset = 0,
        .size = size
    };
    vkCmdCopyBuffer(commandBuffer, src, dst, 1, &copyRegion);
    std::osyncstream(std::cout) << CYAN << "[DEBUG] Recorded buffer copy command" << RESET << std::endl;

    result = vkEndCommandBuffer(commandBuffer);
    if (result != VK_SUCCESS) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to end command buffer: " << result << RESET << std::endl;
        throw std::runtime_error(std::format("Failed to end command buffer: {}", result));
    }

    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer
    };

    result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    if (result != VK_SUCCESS) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to submit buffer copy: " << result << RESET << std::endl;
        throw std::runtime_error(std::format("Failed to submit buffer copy: {}", result));
    }

    result = vkQueueWaitIdle(graphicsQueue);
    if (result != VK_SUCCESS) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to wait for queue: " << result << RESET << std::endl;
        throw std::runtime_error(std::format("Failed to wait for queue: {}", result));
    }

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    std::osyncstream(std::cout) << GREEN << "[INFO] Buffer copy completed successfully" << RESET << std::endl;
}

void createVertexBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                        VkBuffer& vertexBuffer, VkDeviceMemory& vertexBufferMemory, VkBuffer& stagingBuffer,
                        VkDeviceMemory& stagingBufferMemory, std::span<const glm::vec3> vertices) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Creating vertex buffer with " << vertices.size() << " vertices" << RESET << std::endl;

    if (vertices.empty()) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Cannot create vertex buffer: empty vertices span" << RESET << std::endl;
        throw std::runtime_error("Cannot create vertex buffer: empty vertices span");
    }

    VkDeviceSize bufferSize = sizeof(glm::vec3) * vertices.size();
    std::osyncstream(std::cout) << CYAN << "[DEBUG] Vertex buffer size: " << bufferSize << " bytes" << RESET << std::endl;

    createBuffer(device, physicalDevice, bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);

    void* data;
    VkResult result = vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    if (result != VK_SUCCESS) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to map staging buffer memory: " << result << RESET << std::endl;
        throw std::runtime_error(std::format("Failed to map staging buffer memory: {}", result));
    }
    std::memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(device, stagingBufferMemory);
    std::osyncstream(std::cout) << CYAN << "[DEBUG] Copied vertex data to staging buffer" << RESET << std::endl;

    createBuffer(device, physicalDevice, bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 vertexBuffer, vertexBufferMemory);

    copyBuffer(device, commandPool, graphicsQueue, stagingBuffer, vertexBuffer, bufferSize);
    std::osyncstream(std::cout) << GREEN << "[INFO] Vertex buffer created successfully" << RESET << std::endl;
}

void createIndexBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                       VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory, VkBuffer& indexBufferStaging,
                       VkDeviceMemory& indexBufferStagingMemory, std::span<const uint32_t> indices) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Creating index buffer with " << indices.size() << " indices" << RESET << std::endl;

    if (indices.empty()) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Cannot create index buffer: empty indices span" << RESET << std::endl;
        throw std::runtime_error("Cannot create index buffer: empty indices span");
    }

    VkDeviceSize bufferSize = sizeof(uint32_t) * indices.size();
    std::osyncstream(std::cout) << CYAN << "[DEBUG] Index buffer size: " << bufferSize << " bytes" << RESET << std::endl;

    createBuffer(device, physicalDevice, bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 indexBufferStaging, indexBufferStagingMemory);

    void* data;
    VkResult result = vkMapMemory(device, indexBufferStagingMemory, 0, bufferSize, 0, &data);
    if (result != VK_SUCCESS) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to map staging buffer memory: " << result << RESET << std::endl;
        throw std::runtime_error(std::format("Failed to map staging buffer memory: {}", result));
    }
    std::memcpy(data, indices.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(device, indexBufferStagingMemory);
    std::osyncstream(std::cout) << CYAN << "[DEBUG] Copied index data to staging buffer" << RESET << std::endl;

    createBuffer(device, physicalDevice, bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 indexBuffer, indexBufferMemory);

    copyBuffer(device, commandPool, graphicsQueue, indexBufferStaging, indexBuffer, bufferSize);
    std::osyncstream(std::cout) << GREEN << "[INFO] Index buffer created successfully" << RESET << std::endl;
}

} // namespace VulkanInitializer