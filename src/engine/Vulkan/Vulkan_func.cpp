// Vulkan_func.cpp
// AMOURANTH RTX Engine, October 2025 - Vulkan core utilities implementation.
// Supports Windows/Linux; no mutexes; voxel geometry via glm::vec3 vertices.
// Dependencies: Vulkan 1.3+, GLM, C++20 standard library.
// Integrates with VulkanRTX for ray-traced voxel rendering, SwapchainManager for presentation,
// and Vulkan_func_pipe for graphics pipeline.
// Initializes Vulkan resources, buffers, and synchronization objects using C++20 safe threading.
// Thread-safe with std::atomic and lock-free operations; uses engine/logging.hpp for logging.
// Zachary Geurts 2025

#include "engine/Vulkan/Vulkan_func.hpp"
#include "engine/Vulkan/Vulkan_func_pipe.hpp"
#include "engine/Vulkan/Vulkan_func_swapchain.hpp"
#include "engine/logging.hpp"
#include <stdexcept>
#include <vector>
#include <set>
#include <cstring>
#include <limits>
#include <ranges>
#include <atomic>
#include <thread>

// Define VK_CHECK for consistent error handling
#ifndef VK_CHECK
#define VK_CHECK(result, msg) do { if ((result) != VK_SUCCESS) { Logging::Logger::get().log(Logging::LogLevel::Error, "Vulkan", "{} (VkResult: {})", std::source_location::current(), (msg), static_cast<int>(result)); throw std::runtime_error((msg)); } } while (0)
#endif

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
    std::span<const glm::vec3> vertices, std::span<const uint32_t> indices, int width, int height,
    const Logging::Logger& logger) {
    logger.log(Logging::LogLevel::Info, "Vulkan", "Initializing Vulkan resources with width={}, height={}", std::source_location::current(), width, height);

    createPhysicalDevice(instance, physicalDevice, graphicsFamily, presentFamily, surface, true, logger);

    createLogicalDevice(physicalDevice, device, graphicsQueue, presentQueue, graphicsFamily, presentFamily, logger);
    if (device == VK_NULL_HANDLE) {
        logger.log(Logging::LogLevel::Error, "Vulkan", "Failed to create logical device", std::source_location::current());
        throw std::runtime_error("Failed to create logical device");
    }
    logger.log(Logging::LogLevel::Debug, "Vulkan", "Logical device created successfully", std::source_location::current());

    VkSurfaceFormatKHR surfaceFormat = selectSurfaceFormat(physicalDevice, surface, logger);
    logger.log(Logging::LogLevel::Debug, "Vulkan", "Selected surface format: {}", std::source_location::current(), surfaceFormat.format);

    createSwapchain(physicalDevice, device, surface, swapchain, swapchainImages, swapchainImageViews,
                    surfaceFormat.format, graphicsFamily, presentFamily, width, height, logger);

    createRenderPass(device, renderPass, surfaceFormat.format, logger);

    createDescriptorSetLayout(device, descriptorSetLayout, logger);
    descriptorSetLayout2 = descriptorSetLayout;
    logger.log(Logging::LogLevel::Debug, "Vulkan", "Descriptor set layout copied to descriptorSetLayout2", std::source_location::current());

    createGraphicsPipeline(device, renderPass, pipeline, pipelineLayout, descriptorSetLayout, width, height,
                          vertShaderModule, fragShaderModule, logger);

    createFramebuffers(device, renderPass, swapchainImageViews, swapchainFramebuffers, width, height, logger);

    createCommandPool(device, commandPool, graphicsFamily, logger);
    createCommandBuffers(device, commandPool, commandBuffers, swapchainFramebuffers, logger);
    logger.log(Logging::LogLevel::Debug, "Vulkan", "Created {} command buffers", std::source_location::current(), commandBuffers.size());

    createSyncObjects(device, imageAvailableSemaphores, renderFinishedSemaphores, inFlightFences,
                      static_cast<uint32_t>(swapchainImages.size()), logger);
    logger.log(Logging::LogLevel::Debug, "Vulkan", "Created sync objects for {} frames", std::source_location::current(), swapchainImages.size());

    createVertexBuffer(device, physicalDevice, commandPool, graphicsQueue,
                       vertexBuffer, vertexBufferMemory, sphereStagingBuffer, sphereStagingBufferMemory, vertices, logger);

    createIndexBuffer(device, physicalDevice, commandPool, graphicsQueue,
                      indexBuffer, indexBufferMemory, indexStagingBuffer, indexStagingBufferMemory, indices, logger);

    createSampler(device, physicalDevice, sampler, logger);
    createDescriptorPoolAndSet(device, descriptorSetLayout, descriptorPool, descriptorSet, sampler, logger);

    logger.log(Logging::LogLevel::Info, "Vulkan", "Vulkan initialization completed successfully", std::source_location::current());
}

void initializeQuadBuffers(
    VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
    VkBuffer& quadVertexBuffer, VkDeviceMemory& quadVertexBufferMemory,
    VkBuffer& quadIndexBuffer, VkDeviceMemory& quadIndexBufferMemory,
    VkBuffer& quadStagingBuffer, VkDeviceMemory& quadStagingBufferMemory,
    VkBuffer& quadIndexStagingBuffer, VkDeviceMemory& quadIndexStagingBufferMemory,
    std::span<const glm::vec3> quadVertices, std::span<const uint32_t> quadIndices) {
    Logging::Logger::get().log(Logging::LogLevel::Info, "Vulkan", "Initializing quad buffers", std::source_location::current());

    createVertexBuffer(device, physicalDevice, commandPool, graphicsQueue,
                       quadVertexBuffer, quadVertexBufferMemory, quadStagingBuffer, quadStagingBufferMemory, quadVertices,
                       Logging::Logger::get());
    createIndexBuffer(device, physicalDevice, commandPool, graphicsQueue,
                      quadIndexBuffer, quadIndexBufferMemory, quadIndexStagingBuffer, quadIndexStagingBufferMemory, quadIndices,
                      Logging::Logger::get());

    Logging::Logger::get().log(Logging::LogLevel::Info, "Vulkan", "Quad buffers initialized successfully", std::source_location::current());
}

void initializeVoxelBuffers(
    VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
    VkBuffer& voxelVertexBuffer, VkDeviceMemory& voxelVertexBufferMemory,
    VkBuffer& voxelIndexBuffer, VkDeviceMemory& voxelIndexBufferMemory,
    VkBuffer& voxelStagingBuffer, VkDeviceMemory& voxelStagingBufferMemory,
    VkBuffer& voxelIndexStagingBuffer, VkDeviceMemory& voxelIndexStagingBufferMemory,
    std::span<const glm::vec3> voxelVertices, std::span<const uint32_t> voxelIndices) {
    Logging::Logger::get().log(Logging::LogLevel::Info, "Vulkan", "Initializing voxel buffers", std::source_location::current());

    createVertexBuffer(device, physicalDevice, commandPool, graphicsQueue,
                       voxelVertexBuffer, voxelVertexBufferMemory, voxelStagingBuffer, voxelStagingBufferMemory, voxelVertices,
                       Logging::Logger::get());
    createIndexBuffer(device, physicalDevice, commandPool, graphicsQueue,
                      voxelIndexBuffer, voxelIndexBufferMemory, voxelIndexStagingBuffer, voxelIndexStagingBufferMemory, voxelIndices,
                      Logging::Logger::get());

    Logging::Logger::get().log(Logging::LogLevel::Info, "Vulkan", "Voxel buffers initialized successfully", std::source_location::current());
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
    VkShaderModule& vertShaderModule, VkShaderModule& fragShaderModule,
    const Logging::Logger& logger) {
    logger.log(Logging::LogLevel::Info, "Vulkan", "Cleaning up Vulkan resources", std::source_location::current());

    if (device == VK_NULL_HANDLE) {
        logger.log(Logging::LogLevel::Warning, "Vulkan", "Device is null, skipping cleanup", std::source_location::current());
        return;
    }

    VK_CHECK(vkDeviceWaitIdle(device), "Failed to wait for device idle");
    logger.log(Logging::LogLevel::Debug, "Vulkan", "Device idle, proceeding with cleanup", std::source_location::current());

    // Parallel cleanup using OpenMP tasks for thread-safe resource destruction
    #pragma omp parallel
    {
        #pragma omp single
        {
            for (size_t i = 0; i < inFlightFences.size(); ++i) {
                #pragma omp task
                if (inFlightFences[i] != VK_NULL_HANDLE) {
                    vkDestroyFence(device, inFlightFences[i], nullptr);
                    logger.log(Logging::LogLevel::Debug, "Vulkan", "Destroyed fence {}", std::source_location::current(), i);
                    inFlightFences[i] = VK_NULL_HANDLE;
                }
            }
            for (size_t i = 0; i < imageAvailableSemaphores.size(); ++i) {
                #pragma omp task
                if (imageAvailableSemaphores[i] != VK_NULL_HANDLE) {
                    vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
                    logger.log(Logging::LogLevel::Debug, "Vulkan", "Destroyed image available semaphore {}", std::source_location::current(), i);
                    imageAvailableSemaphores[i] = VK_NULL_HANDLE;
                }
            }
            for (size_t i = 0; i < renderFinishedSemaphores.size(); ++i) {
                #pragma omp task
                if (renderFinishedSemaphores[i] != VK_NULL_HANDLE) {
                    vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
                    logger.log(Logging::LogLevel::Debug, "Vulkan", "Destroyed render finished semaphore {}", std::source_location::current(), i);
                    renderFinishedSemaphores[i] = VK_NULL_HANDLE;
                }
            }
            for (size_t i = 0; i < swapchainFramebuffers.size(); ++i) {
                #pragma omp task
                if (swapchainFramebuffers[i] != VK_NULL_HANDLE) {
                    vkDestroyFramebuffer(device, swapchainFramebuffers[i], nullptr);
                    logger.log(Logging::LogLevel::Debug, "Vulkan", "Destroyed framebuffer {}", std::source_location::current(), i);
                    swapchainFramebuffers[i] = VK_NULL_HANDLE;
                }
            }
            for (size_t i = 0; i < swapchainImageViews.size(); ++i) {
                #pragma omp task
                if (swapchainImageViews[i] != VK_NULL_HANDLE) {
                    vkDestroyImageView(device, swapchainImageViews[i], nullptr);
                    logger.log(Logging::LogLevel::Debug, "Vulkan", "Destroyed image view {}", std::source_location::current(), i);
                    swapchainImageViews[i] = VK_NULL_HANDLE;
                }
            }
        }
    }

    inFlightFences.clear();
    imageAvailableSemaphores.clear();
    renderFinishedSemaphores.clear();
    swapchainFramebuffers.clear();
    swapchainImageViews.clear();
    commandBuffers.clear();

    if (commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, commandPool, nullptr);
        logger.log(Logging::LogLevel::Debug, "Vulkan", "Destroyed command pool", std::source_location::current());
        commandPool = VK_NULL_HANDLE;
    }

    if (pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, pipeline, nullptr);
        logger.log(Logging::LogLevel::Debug, "Vulkan", "Destroyed pipeline", std::source_location::current());
        pipeline = VK_NULL_HANDLE;
    }

    if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        logger.log(Logging::LogLevel::Debug, "Vulkan", "Destroyed pipeline layout", std::source_location::current());
        pipelineLayout = VK_NULL_HANDLE;
    }

    if (renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, renderPass, nullptr);
        logger.log(Logging::LogLevel::Debug, "Vulkan", "Destroyed render pass", std::source_location::current());
        renderPass = VK_NULL_HANDLE;
    }

    if (swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        logger.log(Logging::LogLevel::Debug, "Vulkan", "Destroyed swapchain", std::source_location::current());
        swapchain = VK_NULL_HANDLE;
    }

    if (descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        logger.log(Logging::LogLevel::Debug, "Vulkan", "Destroyed descriptor set layout", std::source_location::current());
        descriptorSetLayout = VK_NULL_HANDLE;
    }

    if (descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        logger.log(Logging::LogLevel::Debug, "Vulkan", "Destroyed descriptor pool", std::source_location::current());
        descriptorPool = VK_NULL_HANDLE;
    }
    descriptorSet = VK_NULL_HANDLE;

    if (sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, sampler, nullptr);
        logger.log(Logging::LogLevel::Debug, "Vulkan", "Destroyed sampler", std::source_location::current());
        sampler = VK_NULL_HANDLE;
    }

    if (vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, vertexBuffer, nullptr);
        logger.log(Logging::LogLevel::Debug, "Vulkan", "Destroyed vertex buffer", std::source_location::current());
        vertexBuffer = VK_NULL_HANDLE;
    }

    if (vertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, vertexBufferMemory, nullptr);
        logger.log(Logging::LogLevel::Debug, "Vulkan", "Freed vertex buffer memory", std::source_location::current());
        vertexBufferMemory = VK_NULL_HANDLE;
    }

    if (indexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, indexBuffer, nullptr);
        logger.log(Logging::LogLevel::Debug, "Vulkan", "Destroyed index buffer", std::source_location::current());
        indexBuffer = VK_NULL_HANDLE;
    }

    if (indexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, indexBufferMemory, nullptr);
        logger.log(Logging::LogLevel::Debug, "Vulkan", "Freed index buffer memory", std::source_location::current());
        indexBufferMemory = VK_NULL_HANDLE;
    }

    if (sphereStagingBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, sphereStagingBuffer, nullptr);
        logger.log(Logging::LogLevel::Debug, "Vulkan", "Destroyed sphere staging buffer", std::source_location::current());
        sphereStagingBuffer = VK_NULL_HANDLE;
    }

    if (sphereStagingBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, sphereStagingBufferMemory, nullptr);
        logger.log(Logging::LogLevel::Debug, "Vulkan", "Freed sphere staging buffer memory", std::source_location::current());
        sphereStagingBufferMemory = VK_NULL_HANDLE;
    }

    if (indexStagingBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, indexStagingBuffer, nullptr);
        logger.log(Logging::LogLevel::Debug, "Vulkan", "Destroyed index staging buffer", std::source_location::current());
        indexStagingBuffer = VK_NULL_HANDLE;
    }

    if (indexStagingBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, indexStagingBufferMemory, nullptr);
        logger.log(Logging::LogLevel::Debug, "Vulkan", "Freed index staging buffer memory", std::source_location::current());
        indexStagingBufferMemory = VK_NULL_HANDLE;
    }

    if (vertShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
        logger.log(Logging::LogLevel::Debug, "Vulkan", "Destroyed vertex shader module", std::source_location::current());
        vertShaderModule = VK_NULL_HANDLE;
    }

    if (fragShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        logger.log(Logging::LogLevel::Debug, "Vulkan", "Destroyed fragment shader module", std::source_location::current());
        fragShaderModule = VK_NULL_HANDLE;
    }

    vkDestroyDevice(device, nullptr);
    logger.log(Logging::LogLevel::Debug, "Vulkan", "Destroyed device", std::source_location::current());
    device = VK_NULL_HANDLE;

    logger.log(Logging::LogLevel::Info, "Vulkan", "Vulkan cleanup completed successfully", std::source_location::current());
}

void createPhysicalDevice(VkInstance instance, VkPhysicalDevice& physicalDevice, uint32_t& graphicsFamily,
                         uint32_t& presentFamily, VkSurfaceKHR surface, bool preferNvidia, const Logging::Logger& logger) {
    logger.log(Logging::LogLevel::Info, "Vulkan", "Creating physical device with preferNvidia={}", std::source_location::current(), preferNvidia);

    uint32_t deviceCount = 0;
    VkResult result = vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    VK_CHECK(result, "Failed to enumerate physical devices");
    if (deviceCount == 0) {
        logger.log(Logging::LogLevel::Error, "Vulkan", "No Vulkan-compatible devices found", std::source_location::current());
        throw std::runtime_error("No Vulkan-compatible devices found");
    }
    logger.log(Logging::LogLevel::Debug, "Vulkan", "Found {} Vulkan devices", std::source_location::current(), deviceCount);

    std::vector<VkPhysicalDevice> devices(deviceCount);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()), "Failed to retrieve physical devices");

    uint32_t instanceExtCount = 0;
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtCount, nullptr), "Failed to enumerate instance extensions");
    std::vector<VkExtensionProperties> instanceExtensions(instanceExtCount);
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtCount, instanceExtensions.data()), "Failed to retrieve instance extensions");
    bool hasSurfaceExtension = false;
    for (const auto& ext : instanceExtensions) {
        if (strcmp(ext.extensionName, VK_KHR_SURFACE_EXTENSION_NAME) == 0) {
            hasSurfaceExtension = true;
            logger.log(Logging::LogLevel::Debug, "Vulkan", "VK_KHR_surface extension supported", std::source_location::current());
            break;
        }
    }
    if (!hasSurfaceExtension) {
        logger.log(Logging::LogLevel::Error, "Vulkan", "VK_KHR_surface extension not supported", std::source_location::current());
        throw std::runtime_error("VK_KHR_surface extension not supported");
    }

    auto rateDevice = [preferNvidia, surface, &logger](VkPhysicalDevice dev, QueueFamilyIndices& indices, const DeviceRequirements& reqs) -> std::pair<int, std::string> {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev, &props);
        std::string deviceName = props.deviceName;
        logger.log(Logging::LogLevel::Debug, "Vulkan", "Evaluating device: {}", std::source_location::current(), deviceName);

        int score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 1000;
            if (preferNvidia && props.vendorID == 0x10DE) {
                score += 500;
                logger.log(Logging::LogLevel::Debug, "Vulkan", "Device {} is NVIDIA; score increased", std::source_location::current(), deviceName);
            }
        } else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
            score += 100;
            logger.log(Logging::LogLevel::Debug, "Vulkan", "Device {} is integrated GPU", std::source_location::current(), deviceName);
        } else {
            logger.log(Logging::LogLevel::Warning, "Vulkan", "Device {} is neither discrete nor integrated GPU", std::source_location::current(), deviceName);
            return {0, "Unsupported device type"};
        }

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, queueFamilies.data());

        indices = {};
        bool presentSupportFound = false;
        for (uint32_t i = 0; i < queueFamilyCount; ++i) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
                logger.log(Logging::LogLevel::Debug, "Vulkan", "Found graphics queue family {}", std::source_location::current(), i);
            }
            VkBool32 presentSupport = VK_FALSE;
            VkResult localResult = vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &presentSupport);
            if (localResult != VK_SUCCESS) {
                logger.log(Logging::LogLevel::Warning, "Vulkan", "Device {} failed to query surface support: VkResult {}", std::source_location::current(), deviceName, static_cast<int>(localResult));
                return {0, std::string("Failed to query surface support: ") + std::to_string(static_cast<int>(localResult))};
            }
            if (presentSupport) {
                indices.presentFamily = i;
                presentSupportFound = true;
                logger.log(Logging::LogLevel::Debug, "Vulkan", "Found present queue family {}", std::source_location::current(), i);
            }
        }
        if (!indices.isComplete()) {
            logger.log(Logging::LogLevel::Warning, "Vulkan", "Device {} lacks required queue families", std::source_location::current(), deviceName);
            return {0, "Lacks required queue families"};
        }
        if (!presentSupportFound) {
            logger.log(Logging::LogLevel::Warning, "Vulkan", "Device {} does not support presentation", std::source_location::current(), deviceName);
            return {0, "No presentation support"};
        }

        uint32_t extCount = 0;
        VK_CHECK(vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, nullptr), "Failed to enumerate device extensions");
        std::vector<VkExtensionProperties> availableExts(extCount);
        VK_CHECK(vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, availableExts.data()), "Failed to retrieve device extensions");

        std::set<std::string> requiredExts(reqs.extensions.begin(), reqs.extensions.end());
        for (const auto& ext : availableExts) {
            requiredExts.erase(ext.extensionName);
        }
        if (!requiredExts.empty()) {
            std::string missing;
            for (const auto& ext : requiredExts) {
                missing += std::string(ext) + ", ";
            }
            logger.log(Logging::LogLevel::Warning, "Vulkan", "Device {} missing extensions: {}", std::source_location::current(), deviceName, missing.substr(0, missing.size() - 2));
            return {0, "Missing extensions: " + missing.substr(0, missing.size() - 2)};
        }

        VkPhysicalDeviceFeatures2 features2{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = nullptr,
            .features = {}
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
            logger.log(Logging::LogLevel::Warning, "Vulkan", "Device {} lacks required features", std::source_location::current(), deviceName);
            return {0, "Lacks required features"};
        }

        logger.log(Logging::LogLevel::Debug, "Vulkan", "Device {} is suitable with score: {}", std::source_location::current(), deviceName, score);
        return {score, ""};
    };

    std::atomic<int> maxScore{0};
    std::atomic<VkPhysicalDevice> selectedDevice{VK_NULL_HANDLE};
    QueueFamilyIndices selectedIndices;
    DeviceRequirements reqs;
    std::vector<std::pair<VkPhysicalDevice, QueueFamilyIndices>> fallbackDevices;
    std::vector<VkPhysicalDeviceProperties> fallbackProps;

    // Parallel device evaluation using OpenMP
    #pragma omp parallel
    {
        #pragma omp single
        {
            for (const auto& dev : devices) {
                #pragma omp task
                {
                    QueueFamilyIndices indices;
                    auto [score, reason] = rateDevice(dev, indices, reqs);
                    if (score > maxScore.load(std::memory_order_relaxed)) {
                        #pragma omp critical
                        {
                            if (score > maxScore.load(std::memory_order_relaxed)) {
                                maxScore.store(score, std::memory_order_release);
                                selectedDevice.store(dev, std::memory_order_release);
                                selectedIndices = indices;
                                logger.log(Logging::LogLevel::Debug, "Vulkan", "Selected device with score: {}", std::source_location::current(), score);
                            }
                        }
                    }
                    if (score > 0) {
                        VkPhysicalDeviceProperties props;
                        vkGetPhysicalDeviceProperties(dev, &props);
                        #pragma omp critical
                        {
                            fallbackDevices.emplace_back(dev, indices);
                            fallbackProps.push_back(props);
                            logger.log(Logging::LogLevel::Debug, "Vulkan", "Stored fallback device: {}", std::source_location::current(), props.deviceName);
                        }
                    } else {
                        VkPhysicalDeviceProperties props;
                        vkGetPhysicalDeviceProperties(dev, &props);
                        logger.log(Logging::LogLevel::Warning, "Vulkan", "Rejected device: {} (reason: {})", std::source_location::current(), props.deviceName, reason);
                    }
                }
            }
        }
    }

    physicalDevice = selectedDevice.load(std::memory_order_acquire);
    if (physicalDevice == VK_NULL_HANDLE && !fallbackDevices.empty()) {
        logger.log(Logging::LogLevel::Warning, "Vulkan", "No preferred device found; trying fallback devices", std::source_location::current());
        for (size_t i = 0; i < fallbackDevices.size(); ++i) {
            physicalDevice = fallbackDevices[i].first;
            selectedIndices = fallbackDevices[i].second;
            logger.log(Logging::LogLevel::Debug, "Vulkan", "Falling back to device: {}", std::source_location::current(), fallbackProps[i].deviceName);
            VkBool32 presentSupport = VK_FALSE;
            VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, selectedIndices.presentFamily.value(), surface, &presentSupport);
            if (result == VK_SUCCESS && presentSupport) {
                logger.log(Logging::LogLevel::Debug, "Vulkan", "Fallback device supports surface; selected", std::source_location::current());
                break;
            } else {
                logger.log(Logging::LogLevel::Warning, "Vulkan", "Fallback device failed surface support: VkResult {}", std::source_location::current(), static_cast<int>(result));
                physicalDevice = VK_NULL_HANDLE;
            }
        }
    }

    if (physicalDevice == VK_NULL_HANDLE) {
        logger.log(Logging::LogLevel::Error, "Vulkan", "No suitable Vulkan device found", std::source_location::current());
        throw std::runtime_error("No suitable Vulkan device found");
    }

    graphicsFamily = selectedIndices.graphicsFamily.value();
    presentFamily = selectedIndices.presentFamily.value();

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physicalDevice, &props);
    logger.log(Logging::LogLevel::Info, "Vulkan", "Selected device: {}", std::source_location::current(), props.deviceName);
}

void createLogicalDevice(VkPhysicalDevice physicalDevice, VkDevice& device, VkQueue& graphicsQueue,
                         VkQueue& presentQueue, uint32_t graphicsFamily, uint32_t presentFamily,
                         const Logging::Logger& logger) {
    logger.log(Logging::LogLevel::Info, "Vulkan", "Creating logical device with graphicsFamily={}, presentFamily={}", std::source_location::current(), graphicsFamily, presentFamily);

    DeviceRequirements reqs;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {graphicsFamily, presentFamily};

    const float queuePriority = 1.0f;
    for (const uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = queueFamily,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority
        };
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{
        .robustBufferAccess = VK_FALSE,
        .fullDrawIndexUint32 = VK_FALSE,
        .imageCubeArray = VK_FALSE,
        .independentBlend = VK_FALSE,
        .geometryShader = VK_FALSE,
        .tessellationShader = VK_FALSE,
        .sampleRateShading = VK_FALSE,
        .dualSrcBlend = VK_FALSE,
        .logicOp = VK_FALSE,
        .multiDrawIndirect = VK_FALSE,
        .drawIndirectFirstInstance = VK_FALSE,
        .depthClamp = VK_FALSE,
        .depthBiasClamp = VK_FALSE,
        .fillModeNonSolid = VK_FALSE,
        .depthBounds = VK_FALSE,
        .wideLines = VK_FALSE,
        .largePoints = VK_FALSE,
        .alphaToOne = VK_FALSE,
        .multiViewport = VK_FALSE,
        .samplerAnisotropy = VK_TRUE,
        .textureCompressionETC2 = VK_FALSE,
        .textureCompressionASTC_LDR = VK_FALSE,
        .textureCompressionBC = VK_FALSE,
        .occlusionQueryPrecise = VK_FALSE,
        .pipelineStatisticsQuery = VK_FALSE,
        .vertexPipelineStoresAndAtomics = VK_FALSE,
        .fragmentStoresAndAtomics = VK_FALSE,
        .shaderTessellationAndGeometryPointSize = VK_FALSE,
        .shaderImageGatherExtended = VK_FALSE,
        .shaderStorageImageExtendedFormats = VK_FALSE,
        .shaderStorageImageMultisample = VK_FALSE,
        .shaderStorageImageReadWithoutFormat = VK_FALSE,
        .shaderStorageImageWriteWithoutFormat = VK_FALSE,
        .shaderUniformBufferArrayDynamicIndexing = VK_FALSE,
        .shaderSampledImageArrayDynamicIndexing = VK_FALSE,
        .shaderStorageBufferArrayDynamicIndexing = VK_FALSE,
        .shaderStorageImageArrayDynamicIndexing = VK_FALSE,
        .shaderClipDistance = VK_FALSE,
        .shaderCullDistance = VK_FALSE,
        .shaderFloat64 = VK_FALSE,
        .shaderInt64 = VK_FALSE,
        .shaderInt16 = VK_FALSE,
        .shaderResourceResidency = VK_FALSE,
        .shaderResourceMinLod = VK_FALSE,
        .sparseBinding = VK_FALSE,
        .sparseResidencyBuffer = VK_FALSE,
        .sparseResidencyImage2D = VK_FALSE,
        .sparseResidencyImage3D = VK_FALSE,
        .sparseResidency2Samples = VK_FALSE,
        .sparseResidency4Samples = VK_FALSE,
        .sparseResidency8Samples = VK_FALSE,
        .sparseResidency16Samples = VK_FALSE,
        .sparseResidencyAliased = VK_FALSE,
        .variableMultisampleRate = VK_FALSE,
        .inheritedQueries = VK_FALSE
    };

    reqs.maintenance4Features.pNext = &reqs.accelerationStructureFeatures;
    reqs.accelerationStructureFeatures.pNext = &reqs.rayTracingFeatures;
    reqs.rayTracingFeatures.pNext = &reqs.bufferDeviceAddressFeatures;

    VkDeviceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &reqs.maintenance4Features,
        .flags = 0,
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = static_cast<uint32_t>(reqs.extensions.size()),
        .ppEnabledExtensionNames = reqs.extensions.data(),
        .pEnabledFeatures = &deviceFeatures
    };

    VK_CHECK(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device), "Failed to create logical device");

    vkGetDeviceQueue(device, graphicsFamily, 0, &graphicsQueue);
    vkGetDeviceQueue(device, presentFamily, 0, &presentQueue);
    logger.log(Logging::LogLevel::Info, "Vulkan", "Logical device created successfully", std::source_location::current());
}

void createCommandPool(VkDevice device, VkCommandPool& commandPool, uint32_t graphicsFamily, const Logging::Logger& logger) {
    logger.log(Logging::LogLevel::Info, "Vulkan", "Creating command pool for graphicsFamily={}", std::source_location::current(), graphicsFamily);

    VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = graphicsFamily
    };

    VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool), "Failed to create command pool");

    logger.log(Logging::LogLevel::Info, "Vulkan", "Command pool created successfully", std::source_location::current());
}

void createCommandBuffers(VkDevice device, VkCommandPool commandPool, std::vector<VkCommandBuffer>& commandBuffers,
                          std::vector<VkFramebuffer>& swapchainFramebuffers, const Logging::Logger& logger) {
    logger.log(Logging::LogLevel::Info, "Vulkan", "Creating command buffers for {} framebuffers", std::source_location::current(), swapchainFramebuffers.size());

    commandBuffers.resize(swapchainFramebuffers.size());
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<uint32_t>(commandBuffers.size())
    };

    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()), "Failed to allocate command buffers");

    logger.log(Logging::LogLevel::Info, "Vulkan", "Command buffers created successfully", std::source_location::current());
}

void createSyncObjects(VkDevice device, std::vector<VkSemaphore>& imageAvailableSemaphores,
                       std::vector<VkSemaphore>& renderFinishedSemaphores, std::vector<VkFence>& inFlightFences,
                       uint32_t maxFramesInFlight, const Logging::Logger& logger) {
    logger.log(Logging::LogLevel::Info, "Vulkan", "Creating sync objects for {} frames", std::source_location::current(), maxFramesInFlight);

    imageAvailableSemaphores.resize(maxFramesInFlight);
    renderFinishedSemaphores.resize(maxFramesInFlight);
    inFlightFences.resize(maxFramesInFlight);

    VkSemaphoreCreateInfo semaphoreInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0
    };

    VkFenceCreateInfo fenceInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    // Parallel sync object creation using OpenMP tasks
    #pragma omp parallel
    {
        #pragma omp single
        {
            for (uint32_t i = 0; i < maxFramesInFlight; ++i) {
                #pragma omp task
                {
                    VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]), "Failed to create image available semaphore");
                    VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]), "Failed to create render finished semaphore");
                    VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]), "Failed to create in-flight fence");
                    logger.log(Logging::LogLevel::Debug, "Vulkan", "Created sync objects for frame {}", std::source_location::current(), i);
                }
            }
        }
    }

    logger.log(Logging::LogLevel::Info, "Vulkan", "Sync objects created successfully", std::source_location::current());
}

void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
                  VkMemoryPropertyFlags props, VkBuffer& buffer, VkDeviceMemory& memory, const Logging::Logger& logger) {
    logger.log(Logging::LogLevel::Info, "Vulkan", "Creating buffer with size={}", std::source_location::current(), size);

    if (size == 0) {
        logger.log(Logging::LogLevel::Error, "Vulkan", "Cannot create buffer with zero size", std::source_location::current());
        throw std::runtime_error("Cannot create buffer with zero size");
    }

    VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr
    };

    VK_CHECK(vkCreateBuffer(device, &bufferInfo, nullptr, &buffer), "Failed to create buffer");

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);
    logger.log(Logging::LogLevel::Debug, "Vulkan", "Buffer memory requirements: size={}, alignment={}", std::source_location::current(), memRequirements.size, memRequirements.alignment);

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    std::atomic<uint32_t> memoryTypeIndex{std::numeric_limits<uint32_t>::max()};
    #pragma omp parallel for
    for (uint32_t i = 0; i < static_cast<uint32_t>(memProperties.memoryTypeCount); ++i) {
        if ((memRequirements.memoryTypeBits & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & props) == props) {
            uint32_t current = memoryTypeIndex.load(std::memory_order_relaxed);
            if (current == std::numeric_limits<uint32_t>::max()) {
                memoryTypeIndex.compare_exchange_strong(current, i, std::memory_order_release, std::memory_order_relaxed);
                logger.log(Logging::LogLevel::Debug, "Vulkan", "Selected memory type index: {}", std::source_location::current(), i);
            }
        }
    }

    if (memoryTypeIndex.load(std::memory_order_acquire) == std::numeric_limits<uint32_t>::max()) {
        logger.log(Logging::LogLevel::Error, "Vulkan", "Failed to find suitable memory type for buffer", std::source_location::current());
        throw std::runtime_error("Failed to find suitable memory type for buffer");
    }

    VkMemoryAllocateFlagsInfo allocFlagsInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .pNext = nullptr,
        .flags = static_cast<VkMemoryAllocateFlags>((usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) ? VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT : 0),
        .deviceMask = 0
    };

    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) ? &allocFlagsInfo : nullptr,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = memoryTypeIndex.load(std::memory_order_acquire)
    };

    VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &memory), "Failed to allocate buffer memory");

    VK_CHECK(vkBindBufferMemory(device, buffer, memory, 0), "Failed to bind buffer memory");

    logger.log(Logging::LogLevel::Info, "Vulkan", "Buffer created successfully", std::source_location::current());
}

void copyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkBuffer src, VkBuffer dst,
                VkDeviceSize size, const Logging::Logger& logger) {
    logger.log(Logging::LogLevel::Info, "Vulkan", "Copying buffer with size={}", std::source_location::current(), size);

    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    VkCommandBuffer commandBuffer;
    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer), "Failed to allocate command buffer for copy");

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };

    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo), "Failed to begin command buffer");

    VkBufferCopy copyRegion{
        .srcOffset = 0,
        .dstOffset = 0,
        .size = size
    };
    vkCmdCopyBuffer(commandBuffer, src, dst, 1, &copyRegion);
    logger.log(Logging::LogLevel::Debug, "Vulkan", "Recorded buffer copy command", std::source_location::current());

    VK_CHECK(vkEndCommandBuffer(commandBuffer), "Failed to end command buffer");

    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .pWaitDstStageMask = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = nullptr
    };

    VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE), "Failed to submit buffer copy");

    VK_CHECK(vkQueueWaitIdle(graphicsQueue), "Failed to wait for queue");

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    logger.log(Logging::LogLevel::Info, "Vulkan", "Buffer copy completed successfully", std::source_location::current());
}

void createVertexBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                        VkBuffer& vertexBuffer, VkDeviceMemory& vertexBufferMemory, VkBuffer& stagingBuffer,
                        VkDeviceMemory& stagingBufferMemory, std::span<const glm::vec3> vertices, const Logging::Logger& logger) {
    logger.log(Logging::LogLevel::Info, "Vulkan", "Creating vertex buffer with {} vertices", std::source_location::current(), vertices.size());

    if (vertices.empty()) {
        logger.log(Logging::LogLevel::Error, "Vulkan", "Cannot create vertex buffer: empty vertices span", std::source_location::current());
        throw std::runtime_error("Cannot create vertex buffer: empty vertices span");
    }

    VkDeviceSize bufferSize = sizeof(glm::vec3) * vertices.size();
    logger.log(Logging::LogLevel::Debug, "Vulkan", "Vertex buffer size: {} bytes", std::source_location::current(), bufferSize);

    createBuffer(device, physicalDevice, bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory, logger);

    void* data;
    VK_CHECK(vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data), "Failed to map staging buffer memory");
    std::memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(device, stagingBufferMemory);
    logger.log(Logging::LogLevel::Debug, "Vulkan", "Copied vertex data to staging buffer", std::source_location::current());

    createBuffer(device, physicalDevice, bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 vertexBuffer, vertexBufferMemory, logger);

    copyBuffer(device, commandPool, graphicsQueue, stagingBuffer, vertexBuffer, bufferSize, logger);
    logger.log(Logging::LogLevel::Info, "Vulkan", "Vertex buffer created successfully", std::source_location::current());
}

void createIndexBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                       VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory, VkBuffer& indexBufferStaging,
                       VkDeviceMemory& indexBufferStagingMemory, std::span<const uint32_t> indices, const Logging::Logger& logger) {
    logger.log(Logging::LogLevel::Info, "Vulkan", "Creating index buffer with {} indices", std::source_location::current(), indices.size());

    if (indices.empty()) {
        logger.log(Logging::LogLevel::Error, "Vulkan", "Cannot create index buffer: empty indices span", std::source_location::current());
        throw std::runtime_error("Cannot create index buffer: empty indices span");
    }

    VkDeviceSize bufferSize = sizeof(uint32_t) * indices.size();
    logger.log(Logging::LogLevel::Debug, "Vulkan", "Index buffer size: {} bytes", std::source_location::current(), bufferSize);

    createBuffer(device, physicalDevice, bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 indexBufferStaging, indexBufferStagingMemory, logger);

    void* data;
    VK_CHECK(vkMapMemory(device, indexBufferStagingMemory, 0, bufferSize, 0, &data), "Failed to map staging buffer memory");
    std::memcpy(data, indices.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(device, indexBufferStagingMemory);
    logger.log(Logging::LogLevel::Debug, "Vulkan", "Copied index data to staging buffer", std::source_location::current());

    createBuffer(device, physicalDevice, bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 indexBuffer, indexBufferMemory, logger);

    copyBuffer(device, commandPool, graphicsQueue, indexBufferStaging, indexBuffer, bufferSize, logger);
    logger.log(Logging::LogLevel::Info, "Vulkan", "Index buffer created successfully", std::source_location::current());
}

} // namespace VulkanInitializer