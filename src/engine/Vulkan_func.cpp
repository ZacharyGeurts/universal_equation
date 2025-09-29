#include "Vulkan_func.hpp"
#include "core.hpp"
#include <stdexcept>
#include <fstream>
#include <algorithm>
#include <set>
#include <cstring>
#include <limits>
#include <iostream>
#include <glm/glm.hpp>
// AMOURANTH RTX September 2025
// Zachary Geurts 2025
namespace VulkanInitializer {
    // Initialize all Vulkan resources for rendering
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
        const std::vector<glm::vec3>& vertices, const std::vector<uint32_t>& indices, int width, int height) {
        // Select physical device with validation enabled
        createPhysicalDevice(instance, physicalDevice, graphicsFamily, presentFamily, surface, true,
                            [](const std::string& msg) { std::cerr << "Vulkan: " << msg << "\n"; });

        // Create logical device and queues
        createLogicalDevice(physicalDevice, device, graphicsQueue, presentQueue, graphicsFamily, presentFamily);
        if (device == VK_NULL_HANDLE) {
            throw std::runtime_error("Failed to create logical device");
        }

        // Select surface format
        VkSurfaceFormatKHR surfaceFormat = selectSurfaceFormat(physicalDevice, surface);

        // Create swapchain and images
        createSwapchain(physicalDevice, device, surface, swapchain, swapchainImages, swapchainImageViews,
                        surfaceFormat.format, graphicsFamily, presentFamily, width, height);

        // Create render pass
        createRenderPass(device, renderPass, surfaceFormat.format);

        // Create descriptor set layout
        createDescriptorSetLayout(device, descriptorSetLayout);
        descriptorSetLayout2 = descriptorSetLayout; // Handle duplicate parameter (if needed)

        // Create graphics pipeline with provided shaders
        createGraphicsPipeline(device, renderPass, pipeline, pipelineLayout, descriptorSetLayout, width, height,
                              vertShaderModule, fragShaderModule);

        // Create framebuffers
        createFramebuffers(device, renderPass, swapchainImageViews, swapchainFramebuffers, width, height);

        // Create command pool and buffers
        createCommandPool(device, commandPool, graphicsFamily);
        createCommandBuffers(device, commandPool, commandBuffers, swapchainFramebuffers);

        // Create synchronization objects
        createSyncObjects(device, imageAvailableSemaphores, renderFinishedSemaphores, inFlightFences,
                          static_cast<uint32_t>(swapchainImages.size()));

        // Create vertex buffer with temporary staging
        createVertexBuffer(device, physicalDevice, commandPool, graphicsQueue,
                           vertexBuffer, vertexBufferMemory, sphereStagingBuffer, sphereStagingBufferMemory, vertices);

        // Create index buffer with temporary staging
        createIndexBuffer(device, physicalDevice, commandPool, graphicsQueue,
                          indexBuffer, indexBufferMemory, indexStagingBuffer, indexStagingBufferMemory, indices);

        // Create sampler and descriptor set
        createSampler(device, physicalDevice, sampler);
        createDescriptorPoolAndSet(device, descriptorSetLayout, descriptorPool, descriptorSet, sampler);
    }

    // Initialize quad vertex and index buffers
    void initializeQuadBuffers(
        VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
        VkBuffer& quadVertexBuffer, VkDeviceMemory& quadVertexBufferMemory,
        VkBuffer& quadIndexBuffer, VkDeviceMemory& quadIndexBufferMemory,
        VkBuffer& quadStagingBuffer, VkDeviceMemory& quadStagingBufferMemory,
        VkBuffer& quadIndexStagingBuffer, VkDeviceMemory& quadIndexStagingBufferMemory,
        const std::vector<glm::vec3>& quadVertices, const std::vector<uint32_t>& quadIndices) {
        // Create quad vertex buffer
        createVertexBuffer(device, physicalDevice, commandPool, graphicsQueue,
                           quadVertexBuffer, quadVertexBufferMemory, quadStagingBuffer, quadStagingBufferMemory, quadVertices);

        // Create quad index buffer
        createIndexBuffer(device, physicalDevice, commandPool, graphicsQueue,
                          quadIndexBuffer, quadIndexBufferMemory, quadIndexStagingBuffer, quadIndexStagingBufferMemory, quadIndices);
    }

    // Clean up Vulkan resources
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
        if (device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(device);

            // Destroy synchronization objects
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

            // Destroy command pool
            if (commandPool != VK_NULL_HANDLE) {
                vkDestroyCommandPool(device, commandPool, nullptr);
                commandPool = VK_NULL_HANDLE;
            }

            // Destroy framebuffers
            for (auto& framebuffer : swapchainFramebuffers) {
                if (framebuffer != VK_NULL_HANDLE) {
                    vkDestroyFramebuffer(device, framebuffer, nullptr);
                    framebuffer = VK_NULL_HANDLE;
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

            // Destroy swapchain image views
            for (auto& imageView : swapchainImageViews) {
                if (imageView != VK_NULL_HANDLE) {
                    vkDestroyImageView(device, imageView, nullptr);
                    imageView = VK_NULL_HANDLE;
                }
            }
            swapchainImageViews.clear();

            // Destroy swapchain
            if (swapchain != VK_NULL_HANDLE) {
                vkDestroySwapchainKHR(device, swapchain, nullptr);
                swapchain = VK_NULL_HANDLE;
            }

            // Destroy descriptor set layout, pool, and set
            if (descriptorSetLayout != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
                descriptorSetLayout = VK_NULL_HANDLE;
            }
            if (descriptorPool != VK_NULL_HANDLE) {
                vkDestroyDescriptorPool(device, descriptorPool, nullptr);
                descriptorPool = VK_NULL_HANDLE;
            }
            descriptorSet = VK_NULL_HANDLE; // Descriptor set is freed with pool

            // Destroy sampler
            if (sampler != VK_NULL_HANDLE) {
                vkDestroySampler(device, sampler, nullptr);
                sampler = VK_NULL_HANDLE;
            }

            // Destroy vertex and index buffers
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

            // Destroy staging buffers
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

            // Destroy shader modules
            if (vertShaderModule != VK_NULL_HANDLE) {
                vkDestroyShaderModule(device, vertShaderModule, nullptr);
                vertShaderModule = VK_NULL_HANDLE;
            }
            if (fragShaderModule != VK_NULL_HANDLE) {
                vkDestroyShaderModule(device, fragShaderModule, nullptr);
                fragShaderModule = VK_NULL_HANDLE;
            }

            // Destroy logical device
            vkDestroyDevice(device, nullptr);
            device = VK_NULL_HANDLE;
        }

        // Clear vectors
        commandBuffers.clear();
        imageAvailableSemaphores.clear();
        renderFinishedSemaphores.clear();
        inFlightFences.clear();
    }

    // Select a suitable surface format for the swapchain
    VkSurfaceFormatKHR selectSurfaceFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
        uint32_t formatCount = 0;
        VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
        if (result != VK_SUCCESS || formatCount == 0) {
            throw std::runtime_error("Failed to get surface formats: " + std::to_string(result));
        }
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to retrieve surface formats: " + std::to_string(result));
        }
        for (const auto& format : formats) {
            if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return format;
            }
        }
        return formats[0]; // Fallback to first available format
    }

    // Create a physical device with graphics and presentation support
    void createPhysicalDevice(VkInstance instance, VkPhysicalDevice& physicalDevice, uint32_t& graphicsFamily,
                             uint32_t& presentFamily, VkSurfaceKHR surface, bool preferNvidia,
                             std::function<void(const std::string&)> logMessage) {
        uint32_t deviceCount = 0;
        VkResult result = vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (result != VK_SUCCESS || deviceCount == 0) {
            const std::string msg = "No Vulkan-compatible devices found: " + std::to_string(result);
            if (logMessage) logMessage(msg);
            throw std::runtime_error(msg);
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        result = vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
        if (result != VK_SUCCESS) {
            const std::string msg = "Failed to enumerate physical devices: " + std::to_string(result);
            if (logMessage) logMessage(msg);
            throw std::runtime_error(msg);
        }
        if (logMessage) {
            logMessage("Found " + std::to_string(deviceCount) + " Vulkan devices.");
        }

        // Check for VK_KHR_surface extension
        uint32_t instanceExtCount = 0;
        result = vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtCount, nullptr);
        if (result != VK_SUCCESS) {
            const std::string msg = "Failed to enumerate instance extensions: " + std::to_string(result);
            if (logMessage) logMessage(msg);
            throw std::runtime_error(msg);
        }
        std::vector<VkExtensionProperties> instanceExtensions(instanceExtCount);
        result = vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtCount, instanceExtensions.data());
        if (result != VK_SUCCESS) {
            const std::string msg = "Failed to retrieve instance extensions: " + std::to_string(result);
            if (logMessage) logMessage(msg);
            throw std::runtime_error(msg);
        }
        bool hasSurfaceExtension = false;
        for (const auto& ext : instanceExtensions) {
            if (strcmp(ext.extensionName, VK_KHR_SURFACE_EXTENSION_NAME) == 0) {
                hasSurfaceExtension = true;
                if (logMessage) logMessage("VK_KHR_surface extension supported.");
                break;
            }
        }
        if (!hasSurfaceExtension) {
            const std::string msg = "VK_KHR_surface extension not supported.";
            if (logMessage) logMessage(msg);
            throw std::runtime_error(msg);
        }

        // Device rating function
        auto rateDevice = [preferNvidia, surface, &logMessage, &result](VkPhysicalDevice dev, QueueFamilyIndices& indices, const DeviceRequirements& reqs) -> std::pair<int, std::string> {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(dev, &props);
            const std::string deviceName = props.deviceName;
            if (logMessage) {
                logMessage("Evaluating device: " + deviceName);
            }

            int score = 0;
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                score += 1000;
                if (preferNvidia && props.vendorID == 0x10DE) {
                    score += 500;
                    if (logMessage) logMessage("Device " + deviceName + " is NVIDIA; score increased.");
                }
            } else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
                score += 100;
                if (logMessage) logMessage("Device " + deviceName + " is integrated GPU.");
            } else {
                if (logMessage) logMessage("Device " + deviceName + " is neither discrete nor integrated GPU.");
                return {0, "Unsupported device type"};
            }

            // Check queue families
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
                result = vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &presentSupport);
                if (result != VK_SUCCESS) {
                    if (logMessage) logMessage("Device " + deviceName + " failed to query surface support: " + std::to_string(result));
                    return {0, "Failed to query surface support"};
                }
                if (presentSupport) {
                    indices.presentFamily = i;
                    presentSupportFound = true;
                }
            }
            if (!indices.isComplete()) {
                if (logMessage) logMessage("Device " + deviceName + " lacks required queue families.");
                return {0, "Lacks required queue families"};
            }
            if (!presentSupportFound) {
                if (logMessage) logMessage("Device " + deviceName + " does not support presentation.");
                return {0, "No presentation support"};
            }

            // Check device extensions
            uint32_t extCount = 0;
            result = vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, nullptr);
            if (result != VK_SUCCESS) {
                if (logMessage) logMessage("Device " + deviceName + " failed to enumerate extensions: " + std::to_string(result));
                return {0, "Failed to enumerate extensions"};
            }
            std::vector<VkExtensionProperties> availableExts(extCount);
            result = vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, availableExts.data());
            if (result != VK_SUCCESS) {
                if (logMessage) logMessage("Device " + deviceName + " failed to retrieve extensions: " + std::to_string(result));
                return {0, "Failed to retrieve extensions"};
            }

            std::set<std::string> requiredExts(reqs.extensions.begin(), reqs.extensions.end());
            for (const auto& ext : availableExts) {
                requiredExts.erase(ext.extensionName);
            }
            if (!requiredExts.empty()) {
                std::string missing;
                for (const auto& ext : requiredExts) {
                    missing += ext + ", ";
                }
                if (logMessage) logMessage("Device " + deviceName + " missing extensions: " + missing);
                return {0, "Missing extensions: " + missing.substr(0, missing.size() - 2)};
            }

            // Check device features
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
                if (logMessage) logMessage("Device " + deviceName + " lacks required features.");
                return {0, "Lacks required features"};
            }

            if (logMessage) logMessage("Device " + deviceName + " is suitable with score: " + std::to_string(score));
            return {score, ""};
        };

        // Select the best device
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
                if (logMessage) logMessage("Selected device with score: " + std::to_string(score));
            }
            if (score > 0) {
                VkPhysicalDeviceProperties props;
                vkGetPhysicalDeviceProperties(dev, &props);
                fallbackDevices.emplace_back(dev, indices);
                fallbackProps.push_back(props);
                if (logMessage) logMessage("Stored fallback device: " + std::string(props.deviceName));
            } else {
                VkPhysicalDeviceProperties props;
                vkGetPhysicalDeviceProperties(dev, &props);
                if (logMessage) logMessage("Rejected device: " + std::string(props.deviceName) + " (reason: " + reason + ")");
            }
        }

        // Fallback to other devices if no preferred device is found
        if (physicalDevice == VK_NULL_HANDLE && !fallbackDevices.empty()) {
            if (logMessage) logMessage("No preferred device found; trying fallback devices");
            for (size_t i = 0; i < fallbackDevices.size(); ++i) {
                physicalDevice = fallbackDevices[i].first;
                selectedIndices = fallbackDevices[i].second;
                if (logMessage) logMessage("Falling back to device: " + std::string(fallbackProps[i].deviceName));
                VkBool32 presentSupport = VK_FALSE;
                result = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, selectedIndices.presentFamily.value(), surface, &presentSupport);
                if (result == VK_SUCCESS && presentSupport) {
                    if (logMessage) logMessage("Fallback device supports surface; selected");
                    break;
                } else {
                    if (logMessage) logMessage("Fallback device failed surface support: " + std::to_string(result));
                    physicalDevice = VK_NULL_HANDLE;
                }
            }
        }

        if (physicalDevice == VK_NULL_HANDLE) {
            const std::string msg = "No suitable Vulkan device found.";
            if (logMessage) logMessage(msg);
            throw std::runtime_error(msg);
        }

        graphicsFamily = selectedIndices.graphicsFamily.value();
        presentFamily = selectedIndices.presentFamily.value();

        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(physicalDevice, &props);
        if (logMessage) logMessage("Selected device: " + std::string(props.deviceName));
    }

    // Create a logical device and retrieve graphics/present queues
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
            throw std::runtime_error("Failed to create logical device: " + std::to_string(result));
        }

        vkGetDeviceQueue(device, graphicsFamily, 0, &graphicsQueue);
        vkGetDeviceQueue(device, presentFamily, 0, &presentQueue);
    }

    // Create a swapchain and its associated images/views
    void createSwapchain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, VkSwapchainKHR& swapchain,
                         std::vector<VkImage>& swapchainImages, std::vector<VkImageView>& swapchainImageViews,
                         VkFormat& swapchainFormat, uint32_t graphicsFamily, uint32_t presentFamily, int width, int height) {
        VkSurfaceCapabilitiesKHR capabilities;
        VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to get surface capabilities: " + std::to_string(result));
        }

        uint32_t formatCount = 0;
        result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
        if (result != VK_SUCCESS || formatCount == 0) {
            throw std::runtime_error("Failed to get surface formats: " + std::to_string(result));
        }

        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to retrieve surface formats: " + std::to_string(result));
        }

        VkSurfaceFormatKHR selectedFormat = formats[0];
        for (const auto& availableFormat : formats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
                availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                selectedFormat = availableFormat;
                break;
            }
        }
        swapchainFormat = selectedFormat.format;

        VkImageFormatProperties formatProps;
        result = vkGetPhysicalDeviceImageFormatProperties(
            physicalDevice, selectedFormat.format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 0, &formatProps);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Selected swapchain format not supported: " + std::to_string(result));
        }

        uint32_t presentModeCount = 0;
        result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
        if (result != VK_SUCCESS || presentModeCount == 0) {
            throw std::runtime_error("Failed to get present modes: " + std::to_string(result));
        }

        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data());
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to retrieve present modes: " + std::to_string(result));
        }

        VkPresentModeKHR selectedPresentMode = VK_PRESENT_MODE_FIFO_KHR;
        for (const auto& availablePresentMode : presentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                selectedPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
                break;
            }
        }

        VkExtent2D extent;
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            extent = capabilities.currentExtent;
        } else {
            extent = {
                std::clamp(static_cast<uint32_t>(width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
                std::clamp(static_cast<uint32_t>(height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
            };
        }

        uint32_t imageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
            imageCount = capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo = {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .surface = surface,
            .minImageCount = imageCount,
            .imageFormat = selectedFormat.format,
            .imageColorSpace = selectedFormat.colorSpace,
            .imageExtent = extent,
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .imageSharingMode = (graphicsFamily != presentFamily) ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = (graphicsFamily != presentFamily) ? 2u : 0u,
            .pQueueFamilyIndices = (graphicsFamily != presentFamily) ? std::array<uint32_t, 2>{graphicsFamily, presentFamily}.data() : nullptr,
            .preTransform = capabilities.currentTransform,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode = selectedPresentMode,
            .clipped = VK_TRUE,
            .oldSwapchain = VK_NULL_HANDLE
        };

        result = vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create swapchain: " + std::to_string(result));
        }

        result = vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to get swapchain image count: " + std::to_string(result));
        }
        swapchainImages.resize(imageCount);
        result = vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to retrieve swapchain images: " + std::to_string(result));
        }

        swapchainImageViews.resize(imageCount);
        for (size_t i = 0; i < swapchainImages.size(); ++i) {
            VkImageViewCreateInfo viewInfo = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .image = swapchainImages[i],
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = selectedFormat.format,
                .components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
                .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                }
            };

            result = vkCreateImageView(device, &viewInfo, nullptr, &swapchainImageViews[i]);
            if (result != VK_SUCCESS) {
                throw std::runtime_error("Failed to create swapchain image view " + std::to_string(i) + ": " + std::to_string(result));
            }
        }
    }

    // Create a render pass for the graphics pipeline
    void createRenderPass(VkDevice device, VkRenderPass& renderPass, VkFormat format) {
        VkAttachmentDescription colorAttachment = {
            .flags = 0,
            .format = format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        };

        VkAttachmentReference colorAttachmentRef = {
            .attachment = 0,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        };

        VkSubpassDescription subpass = {
            .flags = 0,
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .inputAttachmentCount = 0,
            .pInputAttachments = nullptr,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachmentRef,
            .pResolveAttachments = nullptr,
            .pDepthStencilAttachment = nullptr,
            .preserveAttachmentCount = 0,
            .pPreserveAttachments = nullptr
        };

        VkSubpassDependency dependency = {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = 0
        };

        VkRenderPassCreateInfo renderPassInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .attachmentCount = 1,
            .pAttachments = &colorAttachment,
            .subpassCount = 1,
            .pSubpasses = &subpass,
            .dependencyCount = 1,
            .pDependencies = &dependency
        };

        VkResult result = vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create render pass: " + std::to_string(result));
        }
    }

    // Create a shader module from a SPIR-V file
    VkShaderModule createShaderModule(VkDevice device, const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open shader file: " + filename);
        }

        size_t fileSize = static_cast<size_t>(file.tellg());
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();

        VkShaderModuleCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .codeSize = buffer.size(),
            .pCode = reinterpret_cast<const uint32_t*>(buffer.data())
        };

        VkShaderModule shaderModule;
        VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shader module from " + filename + ": " + std::to_string(result));
        }

        return shaderModule;
    }

    // Create a descriptor set layout for shader resources
    void createDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout& descriptorSetLayout) {
        VkDescriptorSetLayoutBinding samplerBinding = {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr
        };

        VkDescriptorSetLayoutCreateInfo layoutInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .bindingCount = 1,
            .pBindings = &samplerBinding
        };

        VkResult result = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor set layout: " + std::to_string(result));
        }
    }

    // Create a descriptor pool and allocate a descriptor set
    void createDescriptorPoolAndSet(VkDevice device, VkDescriptorSetLayout descriptorSetLayout,
                                   VkDescriptorPool& descriptorPool, VkDescriptorSet& descriptorSet, VkSampler sampler) {
        VkDescriptorPoolSize poolSize = {
            .type = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = 1
        };

        VkDescriptorPoolCreateInfo poolInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
            .maxSets = 1,
            .poolSizeCount = 1,
            .pPoolSizes = &poolSize
        };

        VkResult result = vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor pool: " + std::to_string(result));
        }

        VkDescriptorSetAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = nullptr,
            .descriptorPool = descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &descriptorSetLayout
        };

        result = vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate descriptor set: " + std::to_string(result));
        }

        VkDescriptorImageInfo imageInfo = {
            .sampler = sampler,
            .imageView = VK_NULL_HANDLE,
            .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };

        VkWriteDescriptorSet descriptorWrite = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = descriptorSet,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .pImageInfo = &imageInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        };

        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
    }

    // Create a texture sampler
    void createSampler(VkDevice device, VkPhysicalDevice physicalDevice, VkSampler& sampler) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);

        VkSamplerCreateInfo samplerInfo = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .mipLodBias = 0.0f,
            .anisotropyEnable = VK_TRUE,
            .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
            .compareEnable = VK_FALSE,
            .compareOp = VK_COMPARE_OP_ALWAYS,
            .minLod = 0.0f,
            .maxLod = 0.0f,
            .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
            .unnormalizedCoordinates = VK_FALSE
        };

        VkResult result = vkCreateSampler(device, &samplerInfo, nullptr, &sampler);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create sampler: " + std::to_string(result));
        }
    }

    // Create a graphics pipeline with provided shaders
    void createGraphicsPipeline(VkDevice device, VkRenderPass renderPass, VkPipeline& pipeline,
                               VkPipelineLayout& pipelineLayout, VkDescriptorSetLayout& descriptorSetLayout,
                               int width, int height, VkShaderModule& vertShaderModule, VkShaderModule& fragShaderModule) {
        vertShaderModule = createShaderModule(device, "assets/shaders/vertex.spv");
        fragShaderModule = createShaderModule(device, "assets/shaders/fragment.spv");

        VkPipelineShaderStageCreateInfo vertShaderStageInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertShaderModule,
            .pName = "main",
            .pSpecializationInfo = nullptr
        };

        VkPipelineShaderStageCreateInfo fragShaderStageInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragShaderModule,
            .pName = "main",
            .pSpecializationInfo = nullptr
        };

        VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

        // Vertex input for glm::vec3 (position only)
        VkVertexInputBindingDescription bindingDescription = {
            .binding = 0,
            .stride = sizeof(glm::vec3),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
        };

        VkVertexInputAttributeDescription attributeDescription = {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = 0
        };

        VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &bindingDescription,
            .vertexAttributeDescriptionCount = 1,
            .pVertexAttributeDescriptions = &attributeDescription
        };

        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE
        };

        VkViewport viewport = {
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(width),
            .height = static_cast<float>(height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        VkRect2D scissor = {
            .offset = {0, 0},
            .extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}
        };

        VkPipelineViewportStateCreateInfo viewportState = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .viewportCount = 1,
            .pViewports = &viewport,
            .scissorCount = 1,
            .pScissors = &scissor
        };

        VkPipelineRasterizationStateCreateInfo rasterizer = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .depthBiasEnable = VK_FALSE,
            .depthBiasConstantFactor = 0.0f,
            .depthBiasClamp = 0.0f,
            .depthBiasSlopeFactor = 0.0f,
            .lineWidth = 1.0f
        };

        VkPipelineMultisampleStateCreateInfo multisampling = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable = VK_FALSE,
            .minSampleShading = 1.0f,
            .pSampleMask = nullptr,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable = VK_FALSE
        };

        VkPipelineDepthStencilStateCreateInfo depthStencil = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .depthTestEnable = VK_FALSE,
            .depthWriteEnable = VK_FALSE,
            .depthCompareOp = VK_COMPARE_OP_LESS,
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable = VK_FALSE,
            .front = {},
            .back = {},
            .minDepthBounds = 0.0f,
            .maxDepthBounds = 1.0f
        };

        VkPipelineColorBlendAttachmentState colorBlendAttachment = {
            .blendEnable = VK_TRUE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .colorBlendOp = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
            .alphaBlendOp = VK_BLEND_OP_ADD,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
        };

        VkPipelineColorBlendStateCreateInfo colorBlending = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .logicOpEnable = VK_FALSE,
            .logicOp = VK_LOGIC_OP_COPY,
            .attachmentCount = 1,
            .pAttachments = &colorBlendAttachment,
            .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f}
        };

        VkPushConstantRange pushConstantRange = {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = sizeof(PushConstants)
        };

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .setLayoutCount = 1,
            .pSetLayouts = &descriptorSetLayout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &pushConstantRange
        };

        VkResult result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pipeline layout: " + std::to_string(result));
        }

        VkGraphicsPipelineCreateInfo pipelineInfo = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stageCount = 2,
            .pStages = shaderStages,
            .pVertexInputState = &vertexInputInfo,
            .pInputAssemblyState = &inputAssembly,
            .pTessellationState = nullptr,
            .pViewportState = &viewportState,
            .pRasterizationState = &rasterizer,
            .pMultisampleState = &multisampling,
            .pDepthStencilState = &depthStencil,
            .pColorBlendState = &colorBlending,
            .pDynamicState = nullptr,
            .layout = pipelineLayout,
            .renderPass = renderPass,
            .subpass = 0,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = -1
        };

        result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create graphics pipeline: " + std::to_string(result));
        }
    }

    // Create framebuffers for the swapchain images
    void createFramebuffers(VkDevice device, VkRenderPass renderPass, std::vector<VkImageView>& swapchainImageViews,
                            std::vector<VkFramebuffer>& swapchainFramebuffers, int width, int height) {
        swapchainFramebuffers.resize(swapchainImageViews.size());
        for (size_t i = 0; i < swapchainImageViews.size(); ++i) {
            VkImageView attachments[] = {swapchainImageViews[i]};
            VkFramebufferCreateInfo framebufferInfo = {
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .renderPass = renderPass,
                .attachmentCount = 1,
                .pAttachments = attachments,
                .width = static_cast<uint32_t>(width),
                .height = static_cast<uint32_t>(height),
                .layers = 1
            };

            VkResult result = vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapchainFramebuffers[i]);
            if (result != VK_SUCCESS) {
                throw std::runtime_error("Failed to create framebuffer " + std::to_string(i) + ": " + std::to_string(result));
            }
        }
    }

    // Create a command pool for allocating command buffers
    void createCommandPool(VkDevice device, VkCommandPool& commandPool, uint32_t graphicsFamily) {
        VkCommandPoolCreateInfo poolInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = graphicsFamily
        };

        VkResult result = vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create command pool: " + std::to_string(result));
        }
    }

    // Allocate command buffers for rendering
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
            throw std::runtime_error("Failed to allocate command buffers: " + std::to_string(result));
        }
    }

    // Create synchronization objects (semaphores and fences)
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
                throw std::runtime_error("Failed to create image available semaphore: " + std::to_string(result));
            }
            result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]);
            if (result != VK_SUCCESS) {
                throw std::runtime_error("Failed to create render finished semaphore: " + std::to_string(result));
            }
            result = vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]);
            if (result != VK_SUCCESS) {
                throw std::runtime_error("Failed to create in-flight fence: " + std::to_string(result));
            }
        }
    }

    // Create a buffer with specified usage and memory properties
    void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags props, VkBuffer& buffer, VkDeviceMemory& memory) {
        if (size == 0) {
            throw std::runtime_error("Cannot create buffer with zero size.");
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
            throw std::runtime_error("Failed to create buffer: " + std::to_string(result));
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
            throw std::runtime_error("Failed to find suitable memory type for buffer.");
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
            throw std::runtime_error("Failed to allocate buffer memory: " + std::to_string(result));
        }

        result = vkBindBufferMemory(device, buffer, memory, 0);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to bind buffer memory: " + std::to_string(result));
        }
    }

    // Copy data between buffers
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
            throw std::runtime_error("Failed to allocate command buffer for copy: " + std::to_string(result));
        }

        VkCommandBufferBeginInfo beginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr
        };

        result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin command buffer: " + std::to_string(result));
        }

        VkBufferCopy copyRegion = {
            .srcOffset = 0,
            .dstOffset = 0,
            .size = size
        };
        vkCmdCopyBuffer(commandBuffer, src, dst, 1, &copyRegion);

        result = vkEndCommandBuffer(commandBuffer);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to end command buffer: " + std::to_string(result));
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
            throw std::runtime_error("Failed to submit buffer copy: " + std::to_string(result));
        }

        result = vkQueueWaitIdle(graphicsQueue);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to wait for queue: " + std::to_string(result));
        }

        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    }

    // Create a vertex buffer with staging
    void createVertexBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                            VkBuffer& vertexBuffer, VkDeviceMemory& vertexBufferMemory, VkBuffer& stagingBuffer,
                            VkDeviceMemory& stagingBufferMemory, const std::vector<glm::vec3>& vertices) {
        if (vertices.empty()) {
            throw std::runtime_error("Cannot create vertex buffer: empty vertices vector.");
        }

        const VkDeviceSize bufferSize = sizeof(glm::vec3) * vertices.size();

        // Create staging buffer
        createBuffer(device, physicalDevice, bufferSize,
                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingBuffer, stagingBufferMemory);

        // Copy vertex data to staging buffer
        void* data;
        VkResult result = vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to map staging buffer memory: " + std::to_string(result));
        }
        std::memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
        vkUnmapMemory(device, stagingBufferMemory);

        // Create vertex buffer
        createBuffer(device, physicalDevice, bufferSize,
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                     VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     vertexBuffer, vertexBufferMemory);

        // Copy data from staging to vertex buffer
        copyBuffer(device, commandPool, graphicsQueue, stagingBuffer, vertexBuffer, bufferSize);
    }

    // Create an index buffer with staging
    void createIndexBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                           VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory, VkBuffer& stagingBuffer,
                           VkDeviceMemory& stagingBufferMemory, const std::vector<uint32_t>& indices) {
        if (indices.empty()) {
            throw std::runtime_error("Cannot create index buffer: empty indices vector.");
        }

        const VkDeviceSize bufferSize = sizeof(uint32_t) * indices.size();

        // Create staging buffer
        createBuffer(device, physicalDevice, bufferSize,
                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingBuffer, stagingBufferMemory);

        // Copy index data to staging buffer
        void* data;
        VkResult result = vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to map staging buffer memory: " + std::to_string(result));
        }
        std::memcpy(data, indices.data(), static_cast<size_t>(bufferSize));
        vkUnmapMemory(device, stagingBufferMemory);

        // Create index buffer
        createBuffer(device, physicalDevice, bufferSize,
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                     VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     indexBuffer, indexBufferMemory);

        // Copy data from staging to index buffer
        copyBuffer(device, commandPool, graphicsQueue, stagingBuffer, indexBuffer, bufferSize);
    }
}