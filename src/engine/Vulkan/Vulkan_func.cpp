#include "engine/Vulkan/Vulkan_func.hpp"
#include "engine/core.hpp"
#include <stdexcept>
#include <vector>
#include <set>
#include <cstring>
#include <limits>
#include <format>
#include "engine/Vulkan/Vulkan_func_pipe.hpp"
#include "engine/Vulkan/Vulkan_func_swapchain.hpp"

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
    createPhysicalDevice(instance, physicalDevice, graphicsFamily, presentFamily, surface, true,
                        [](const std::string& msg) { std::cerr << std::format("Vulkan: {}\n", msg); });

    createLogicalDevice(physicalDevice, device, graphicsQueue, presentQueue, graphicsFamily, presentFamily);
    if (device == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to create logical device");
    }

    VkSurfaceFormatKHR surfaceFormat = selectSurfaceFormat(physicalDevice, surface);

    createSwapchain(physicalDevice, device, surface, swapchain, swapchainImages, swapchainImageViews,
                    surfaceFormat.format, graphicsFamily, presentFamily, width, height);

    createRenderPass(device, renderPass, surfaceFormat.format);

    createDescriptorSetLayout(device, descriptorSetLayout);
    descriptorSetLayout2 = descriptorSetLayout;

    createGraphicsPipeline(device, renderPass, pipeline, pipelineLayout, descriptorSetLayout, width, height,
                          vertShaderModule, fragShaderModule);

    createFramebuffers(device, renderPass, swapchainImageViews, swapchainFramebuffers, width, height);

    createCommandPool(device, commandPool, graphicsFamily);
    createCommandBuffers(device, commandPool, commandBuffers, swapchainFramebuffers);

    createSyncObjects(device, imageAvailableSemaphores, renderFinishedSemaphores, inFlightFences,
                      static_cast<uint32_t>(swapchainImages.size()));

    createVertexBuffer(device, physicalDevice, commandPool, graphicsQueue,
                       vertexBuffer, vertexBufferMemory, sphereStagingBuffer, sphereStagingBufferMemory, vertices);

    createIndexBuffer(device, physicalDevice, commandPool, graphicsQueue,
                      indexBuffer, indexBufferMemory, indexStagingBuffer, indexStagingBufferMemory, indices);

    createSampler(device, physicalDevice, sampler);
    createDescriptorPoolAndSet(device, descriptorSetLayout, descriptorPool, descriptorSet, sampler);
}

void initializeQuadBuffers(
    VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
    VkBuffer& quadVertexBuffer, VkDeviceMemory& quadVertexBufferMemory,
    VkBuffer& quadIndexBuffer, VkDeviceMemory& quadIndexBufferMemory,
    VkBuffer& quadStagingBuffer, VkDeviceMemory& quadStagingBufferMemory,
    VkBuffer& quadIndexStagingBuffer, VkDeviceMemory& quadIndexStagingBufferMemory,
    std::span<const glm::vec3> quadVertices, std::span<const uint32_t> quadIndices) {
    createVertexBuffer(device, physicalDevice, commandPool, graphicsQueue,
                       quadVertexBuffer, quadVertexBufferMemory, quadStagingBuffer, quadStagingBufferMemory, quadVertices);

    createIndexBuffer(device, physicalDevice, commandPool, graphicsQueue,
                      quadIndexBuffer, quadIndexBufferMemory, quadIndexStagingBuffer, quadIndexStagingBufferMemory, quadIndices);
}

void initializeVoxelBuffers(
    VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
    VkBuffer& voxelVertexBuffer, VkDeviceMemory& voxelVertexBufferMemory,
    VkBuffer& voxelIndexBuffer, VkDeviceMemory& voxelIndexBufferMemory,
    VkBuffer& voxelStagingBuffer, VkDeviceMemory& voxelStagingBufferMemory,
    VkBuffer& voxelIndexStagingBuffer, VkDeviceMemory& voxelIndexStagingBufferMemory,
    std::span<const glm::vec3> voxelVertices, std::span<const uint32_t> voxelIndices) {
    createVertexBuffer(device, physicalDevice, commandPool, graphicsQueue,
                       voxelVertexBuffer, voxelVertexBufferMemory, voxelStagingBuffer, voxelStagingBufferMemory, voxelVertices);

    createIndexBuffer(device, physicalDevice, commandPool, graphicsQueue,
                      voxelIndexBuffer, voxelIndexBufferMemory, voxelIndexStagingBuffer, voxelIndexStagingBufferMemory, voxelIndices);
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
    if (device == VK_NULL_HANDLE) return;

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
    swapchainFramebuffers.clear();
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
    swapchainImageViews.clear();
    if (swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }
    if (descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        descriptorSetLayout = VK_NULL_HANDLE;
    }
    if (descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        descriptorPool = VK_NULL_HANDLE;
    }
    descriptorSet = VK_NULL_HANDLE;
    if (sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, sampler, nullptr);
        sampler = VK_NULL_HANDLE;
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

    commandBuffers.clear();
    imageAvailableSemaphores.clear();
    renderFinishedSemaphores.clear();
    inFlightFences.clear();
}

void createPhysicalDevice(VkInstance instance, VkPhysicalDevice& physicalDevice, uint32_t& graphicsFamily,
                         uint32_t& presentFamily, VkSurfaceKHR surface, bool preferNvidia,
                         std::function<void(const std::string&)> logMessage) {
    uint32_t deviceCount = 0;
    VkResult result = vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (result != VK_SUCCESS || deviceCount == 0) {
        throw std::runtime_error(std::format("No Vulkan-compatible devices found: {}", result));
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    result = vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::format("Failed to enumerate physical devices: {}", result));
    }
    if (logMessage) logMessage(std::format("Found {} Vulkan devices", deviceCount));

    uint32_t instanceExtCount = 0;
    result = vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtCount, nullptr);
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::format("Failed to enumerate instance extensions: {}", result));
    }
    std::vector<VkExtensionProperties> instanceExtensions(instanceExtCount);
    result = vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtCount, instanceExtensions.data());
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::format("Failed to retrieve instance extensions: {}", result));
    }
    bool hasSurfaceExtension = false;
    for (const auto& ext : instanceExtensions) {
        if (strcmp(ext.extensionName, VK_KHR_SURFACE_EXTENSION_NAME) == 0) {
            hasSurfaceExtension = true;
            if (logMessage) logMessage("VK_KHR_surface extension supported");
            break;
        }
    }
    if (!hasSurfaceExtension) {
        throw std::runtime_error("VK_KHR_surface extension not supported");
    }

    auto rateDevice = [preferNvidia, surface, &logMessage](VkPhysicalDevice dev, QueueFamilyIndices& indices, const DeviceRequirements& reqs) -> std::pair<int, std::string> {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev, &props);
        std::string deviceName = props.deviceName;
        if (logMessage) logMessage(std::format("Evaluating device: {}", deviceName));

        int score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 1000;
            if (preferNvidia && props.vendorID == 0x10DE) {
                score += 500;
                if (logMessage) logMessage(std::format("Device {} is NVIDIA; score increased", deviceName));
            }
        } else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
            score += 100;
            if (logMessage) logMessage(std::format("Device {} is integrated GPU", deviceName));
        } else {
            if (logMessage) logMessage(std::format("Device {} is neither discrete nor integrated GPU", deviceName));
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
            }
            VkBool32 presentSupport = VK_FALSE;
            VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &presentSupport);
            if (result != VK_SUCCESS) {
                if (logMessage) logMessage(std::format("Device {} failed to query surface support: {}", deviceName, result));
                return {0, "Failed to query surface support"};
            }
            if (presentSupport) {
                indices.presentFamily = i;
                presentSupportFound = true;
            }
        }
        if (!indices.isComplete()) {
            if (logMessage) logMessage(std::format("Device {} lacks required queue families", deviceName));
            return {0, "Lacks required queue families"};
        }
        if (!presentSupportFound) {
            if (logMessage) logMessage(std::format("Device {} does not support presentation", deviceName));
            return {0, "No presentation support"};
        }

        uint32_t extCount = 0;
        result = vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, nullptr);
        if (result != VK_SUCCESS) {
            if (logMessage) logMessage(std::format("Device {} failed to enumerate extensions: {}", deviceName, result));
            return {0, "Failed to enumerate extensions"};
        }
        std::vector<VkExtensionProperties> availableExts(extCount);
        result = vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, availableExts.data());
        if (result != VK_SUCCESS) {
            if (logMessage) logMessage(std::format("Device {} failed to retrieve extensions: {}", deviceName, result));
            return {0, "Failed to retrieve extensions"};
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
            if (logMessage) logMessage(std::format("Device {} missing extensions: {}", deviceName, missing.substr(0, missing.size() - 2)));
            return {0, std::format("Missing extensions: {}", missing.substr(0, missing.size() - 2))};
        }

        VkPhysicalDeviceFeatures2 features2 = {
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
            if (logMessage) logMessage(std::format("Device {} lacks required features", deviceName));
            return {0, "Lacks required features"};
        }

        if (logMessage) logMessage(std::format("Device {} is suitable with score: {}", deviceName, score));
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
            if (logMessage) logMessage(std::format("Selected device with score: {}", score));
        }
        if (score > 0) {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(dev, &props);
            fallbackDevices.emplace_back(dev, indices);
            fallbackProps.push_back(props);
            if (logMessage) logMessage(std::format("Stored fallback device: {}", props.deviceName));
        } else {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(dev, &props);
            if (logMessage) logMessage(std::format("Rejected device: {} (reason: {})", props.deviceName, reason));
        }
    }

    if (physicalDevice == VK_NULL_HANDLE && !fallbackDevices.empty()) {
        if (logMessage) logMessage("No preferred device found; trying fallback devices");
        for (size_t i = 0; i < fallbackDevices.size(); ++i) {
            physicalDevice = fallbackDevices[i].first;
            selectedIndices = fallbackDevices[i].second;
            if (logMessage) logMessage(std::format("Falling back to device: {}", fallbackProps[i].deviceName));
            VkBool32 presentSupport = VK_FALSE;
            result = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, selectedIndices.presentFamily.value(), surface, &presentSupport);
            if (result == VK_SUCCESS && presentSupport) {
                if (logMessage) logMessage("Fallback device supports surface; selected");
                break;
            } else {
                if (logMessage) logMessage(std::format("Fallback device failed surface support: {}", result));
                physicalDevice = VK_NULL_HANDLE;
            }
        }
    }

    if (physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("No suitable Vulkan device found");
    }

    graphicsFamily = selectedIndices.graphicsFamily.value();
    presentFamily = selectedIndices.presentFamily.value();

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physicalDevice, &props);
    if (logMessage) logMessage(std::format("Selected device: {}", props.deviceName));
}

void createLogicalDevice(VkPhysicalDevice physicalDevice, VkDevice& device, VkQueue& graphicsQueue,
                         VkQueue& presentQueue, uint32_t graphicsFamily, uint32_t presentFamily) {
    DeviceRequirements reqs;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {graphicsFamily, presentFamily};

    const float queuePriority = 1.0f;
    for (const uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = queueFamily,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority
        };
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures = {};
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    reqs.maintenance4Features.pNext = &reqs.accelerationStructureFeatures;
    reqs.accelerationStructureFeatures.pNext = &reqs.rayTracingFeatures;
    reqs.rayTracingFeatures.pNext = &reqs.bufferDeviceAddressFeatures;

    VkDeviceCreateInfo createInfo = {
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

    VkResult result = vkCreateDevice(physicalDevice, &createInfo, nullptr, &device);
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::format("Failed to create logical device: {}", result));
    }

    vkGetDeviceQueue(device, graphicsFamily, 0, &graphicsQueue);
    vkGetDeviceQueue(device, presentFamily, 0, &presentQueue);
}

void createCommandPool(VkDevice device, VkCommandPool& commandPool, uint32_t graphicsFamily) {
    VkCommandPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = graphicsFamily
    };

    VkResult result = vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool);
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::format("Failed to create command pool: {}", result));
    }
}

void createCommandBuffers(VkDevice device, VkCommandPool commandPool, std::vector<VkCommandBuffer>& commandBuffers,
                          std::vector<VkFramebuffer>& swapchainFramebuffers) {
    commandBuffers.resize(swapchainFramebuffers.size());
    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<uint32_t>(commandBuffers.size())
    };

    VkResult result = vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data());
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::format("Failed to allocate command buffers: {}", result));
    }
}

void createSyncObjects(VkDevice device, std::vector<VkSemaphore>& imageAvailableSemaphores,
                       std::vector<VkSemaphore>& renderFinishedSemaphores, std::vector<VkFence>& inFlightFences,
                       uint32_t maxFramesInFlight) {
    imageAvailableSemaphores.resize(maxFramesInFlight);
    renderFinishedSemaphores.resize(maxFramesInFlight);
    inFlightFences.resize(maxFramesInFlight);

    VkSemaphoreCreateInfo semaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0
    };

    VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    for (uint32_t i = 0; i < maxFramesInFlight; ++i) {
        VkResult result;
        result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]);
        if (result != VK_SUCCESS) {
            throw std::runtime_error(std::format("Failed to create image available semaphore: {}", result));
        }
        result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]);
        if (result != VK_SUCCESS) {
            throw std::runtime_error(std::format("Failed to create render finished semaphore: {}", result));
        }
        result = vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]);
        if (result != VK_SUCCESS) {
            throw std::runtime_error(std::format("Failed to create in-flight fence: {}", result));
        }
    }
}

void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
                  VkMemoryPropertyFlags props, VkBuffer& buffer, VkDeviceMemory& memory) {
    if (size == 0) {
        throw std::runtime_error("Cannot create buffer with zero size");
    }

    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr
    };

    VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::format("Failed to create buffer: {}", result));
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    uint32_t memoryTypeIndex = std::numeric_limits<uint32_t>::max();
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((memRequirements.memoryTypeBits & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & props) == props) {
            memoryTypeIndex = i;
            break;
        }
    }
    if (memoryTypeIndex == std::numeric_limits<uint32_t>::max()) {
        throw std::runtime_error("Failed to find suitable memory type for buffer");
    }

    VkMemoryAllocateFlagsInfo allocFlagsInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .pNext = nullptr,
        .flags = static_cast<VkMemoryAllocateFlags>((usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) ? VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT : 0),
        .deviceMask = 0
    };

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) ? &allocFlagsInfo : nullptr,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = memoryTypeIndex
    };

    result = vkAllocateMemory(device, &allocInfo, nullptr, &memory);
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::format("Failed to allocate buffer memory: {}", result));
    }

    result = vkBindBufferMemory(device, buffer, memory, 0);
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::format("Failed to bind buffer memory: {}", result));
    }
}

void copyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkBuffer src, VkBuffer dst,
                VkDeviceSize size) {
    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    VkCommandBuffer commandBuffer;
    VkResult result = vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::format("Failed to allocate command buffer for copy: {}", result));
    }

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };

    result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::format("Failed to begin command buffer: {}", result));
    }

    VkBufferCopy copyRegion = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = size
    };
    vkCmdCopyBuffer(commandBuffer, src, dst, 1, &copyRegion);

    result = vkEndCommandBuffer(commandBuffer);
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::format("Failed to end command buffer: {}", result));
    }

    VkSubmitInfo submitInfo = {
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

    result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::format("Failed to submit buffer copy: {}", result));
    }

    result = vkQueueWaitIdle(graphicsQueue);
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::format("Failed to wait for queue: {}", result));
    }

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void createVertexBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                        VkBuffer& vertexBuffer, VkDeviceMemory& vertexBufferMemory, VkBuffer& stagingBuffer,
                        VkDeviceMemory& stagingBufferMemory, std::span<const glm::vec3> vertices) {
    if (vertices.empty()) {
        throw std::runtime_error("Cannot create vertex buffer: empty vertices span");
    }

    VkDeviceSize bufferSize = sizeof(glm::vec3) * vertices.size();

    createBuffer(device, physicalDevice, bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);

    void* data;
    VkResult result = vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::format("Failed to map staging buffer memory: {}", result));
    }
    std::memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(device, stagingBufferMemory);

    createBuffer(device, physicalDevice, bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 vertexBuffer, vertexBufferMemory);

    copyBuffer(device, commandPool, graphicsQueue, stagingBuffer, vertexBuffer, bufferSize);
}

void createIndexBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                       VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory, VkBuffer& indexBufferStaging,
                       VkDeviceMemory& indexBufferStagingMemory, std::span<const uint32_t> indices) {
    if (indices.empty()) {
        throw std::runtime_error("Cannot create index buffer: empty indices span");
    }

    VkDeviceSize bufferSize = sizeof(uint32_t) * indices.size();

    createBuffer(device, physicalDevice, bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 indexBufferStaging, indexBufferStagingMemory);

    void* data;
    VkResult result = vkMapMemory(device, indexBufferStagingMemory, 0, bufferSize, 0, &data);
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::format("Failed to map staging buffer memory: {}", result));
    }
    std::memcpy(data, indices.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(device, indexBufferStagingMemory);

    createBuffer(device, physicalDevice, bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 indexBuffer, indexBufferMemory);

    copyBuffer(device, commandPool, graphicsQueue, indexBufferStaging, indexBuffer, bufferSize);
}

} // namespace VulkanInitializer