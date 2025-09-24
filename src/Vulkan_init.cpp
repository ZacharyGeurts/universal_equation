// File: Vulkan_init.cpp
#include "Vulkan_init.hpp"  // Include the header

// Vulkan AMOURANTH RTX engine - Base Vulkan initialization for rasterization.
// Note: Ray tracing (RTX) setup is handled in a separate file. This module focuses on core Vulkan setup,
// including device selection, swapchain, render pass, graphics pipeline, and buffers for basic rendering.
// Optimizations: Prefer discrete GPUs, use optimal memory types, minimize allocations, ensure memory safety with proper cleanup.
// Memory safety: All resources are tracked and destroyed in cleanupVulkan. Use RAII where possible, but Vulkan requires manual management.

#include <vulkan/vulkan.h>
#include <SDL3/SDL_vulkan.h>
#include <vector>
#include <stdexcept>
#include <fstream>
#include <glm/glm.hpp>
#include <algorithm>
#include <cstring>
#include <set>
#include <optional>

// Structure to hold queue family indices for clarity and safety.
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

// Structure to hold device extension and feature requirements.
struct DeviceRequirements {
    std::vector<const char*> extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
    };
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR, nullptr, VK_TRUE,
        VK_FALSE, VK_FALSE, VK_FALSE, VK_FALSE  // Explicitly initialize all fields
    };
    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR, nullptr, VK_TRUE,
        VK_FALSE, VK_FALSE, VK_FALSE, VK_FALSE  // Explicitly initialize all fields
    };
};

// Initializes all core Vulkan resources in a single call for convenience.
// Parameters include outputs for all created resources and inputs for window dimensions and geometry data.
// Throws std::runtime_error on failure.
void VulkanInitializer::initializeVulkan(
    VkInstance& instance, VkPhysicalDevice& physicalDevice, VkDevice& device, VkSurfaceKHR& surface,
    VkQueue& graphicsQueue, VkQueue& presentQueue, uint32_t& graphicsFamily, uint32_t& presentFamily,
    VkSwapchainKHR& swapchain, std::vector<VkImage>& swapchainImages, std::vector<VkImageView>& swapchainImageViews,
    VkRenderPass& renderPass, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout,
    std::vector<VkFramebuffer>& swapchainFramebuffers, VkCommandPool& commandPool,
    std::vector<VkCommandBuffer>& commandBuffers, VkSemaphore& imageAvailableSemaphore,
    VkSemaphore& renderFinishedSemaphore, VkFence& inFlightFence, VkBuffer& vertexBuffer,
    VkDeviceMemory& vertexBufferMemory, VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory,
    const std::vector<glm::vec3>& sphereVertices, const std::vector<uint32_t>& sphereIndices, int width, int height
) {
    VkFormat swapchainFormat;
    createPhysicalDevice(instance, physicalDevice, graphicsFamily, presentFamily, surface);
    createLogicalDevice(physicalDevice, device, graphicsQueue, presentQueue, graphicsFamily, presentFamily);
    createSwapchain(physicalDevice, device, surface, swapchain, swapchainImages, swapchainImageViews, swapchainFormat, width, height);
    createRenderPass(device, renderPass, swapchainFormat);
    createGraphicsPipeline(device, renderPass, pipeline, pipelineLayout, width, height);
    createFramebuffers(device, renderPass, swapchainImageViews, swapchainFramebuffers, width, height);
    createCommandPool(device, commandPool, graphicsFamily);
    createCommandBuffers(device, commandPool, commandBuffers, swapchainFramebuffers);
    createSyncObjects(device, imageAvailableSemaphore, renderFinishedSemaphore, inFlightFence);
    createVertexBuffer(device, physicalDevice, commandPool, graphicsQueue, vertexBuffer, vertexBufferMemory, sphereVertices);
    createIndexBuffer(device, physicalDevice, commandPool, graphicsQueue, indexBuffer, indexBufferMemory, sphereIndices);
}

// Initializes vertex and index buffers for a quad geometry.
// Reuses buffer creation utilities for consistency.
void VulkanInitializer::initializeQuadBuffers(
    VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
    VkBuffer& vertexBuffer, VkDeviceMemory& vertexBufferMemory, VkBuffer& indexBuffer,
    VkDeviceMemory& indexBufferMemory, const std::vector<glm::vec3>& vertices,
    const std::vector<uint32_t>& indices
) {
    createVertexBuffer(device, physicalDevice, commandPool, graphicsQueue, vertexBuffer, vertexBufferMemory, vertices);
    createIndexBuffer(device, physicalDevice, commandPool, graphicsQueue, indexBuffer, indexBufferMemory, indices);
}

// Cleans up all Vulkan resources to prevent memory leaks.
// Waits for device idle before destruction for safety.
// Handles optional quad buffers for flexibility.
void VulkanInitializer::cleanupVulkan(
    [[maybe_unused]] VkInstance instance, VkDevice& device, [[maybe_unused]] VkSurfaceKHR surface, VkSwapchainKHR& swapchain,
    std::vector<VkImageView>& swapchainImageViews, std::vector<VkFramebuffer>& swapchainFramebuffers,
    VkPipeline& pipeline, VkPipelineLayout& pipelineLayout, VkRenderPass& renderPass, VkCommandPool& commandPool,
    std::vector<VkCommandBuffer>& commandBuffers, VkSemaphore& imageAvailableSemaphore,
    VkSemaphore& renderFinishedSemaphore, VkFence& inFlightFence, VkBuffer& vertexBuffer,
    VkDeviceMemory& vertexBufferMemory, VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory,
    VkBuffer& quadVertexBuffer, VkDeviceMemory& quadVertexBufferMemory, VkBuffer& quadIndexBuffer,
    VkDeviceMemory& quadIndexBufferMemory
) {
    if (device == VK_NULL_HANDLE) return;
    vkDeviceWaitIdle(device);  // Ensure no pending operations

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

    destroyBuffer(quadVertexBuffer, quadVertexBufferMemory);
    destroyBuffer(quadIndexBuffer, quadIndexBufferMemory);
    destroyBuffer(vertexBuffer, vertexBufferMemory);
    destroyBuffer(indexBuffer, indexBufferMemory);

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

    if (!commandBuffers.empty()) {
        vkFreeCommandBuffers(device, commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
        commandBuffers.clear();
    }
    if (commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, commandPool, nullptr);
        commandPool = VK_NULL_HANDLE;
    }

    for (auto& fb : swapchainFramebuffers) {
        if (fb != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device, fb, nullptr);
            fb = VK_NULL_HANDLE;
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

    for (auto& iv : swapchainImageViews) {
        if (iv != VK_NULL_HANDLE) {
            vkDestroyImageView(device, iv, nullptr);
            iv = VK_NULL_HANDLE;
        }
    }
    swapchainImageViews.clear();

    if (swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }

    vkDestroyDevice(device, nullptr);
    device = VK_NULL_HANDLE;
}

// Selects the best physical device supporting required queues and extensions.
// Optimization: Scores devices, preferring discrete GPUs over integrated.
void VulkanInitializer::createPhysicalDevice(VkInstance instance, VkPhysicalDevice& physicalDevice, uint32_t& graphicsFamily, uint32_t& presentFamily, VkSurfaceKHR surface) {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) throw std::runtime_error("No Vulkan devices found");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    // Score function: Higher score for discrete GPUs, check required extensions
    auto rateDevice = [](VkPhysicalDevice dev, VkSurfaceKHR surf, QueueFamilyIndices& indices, const DeviceRequirements& reqs) -> int {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev, &props);

        int score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 1000;
        else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 100;

        // Check queue families
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, queueFamilies.data());

        for (uint32_t i = 0; i < queueFamilyCount; ++i) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) indices.graphicsFamily = i;

            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surf, &presentSupport);
            if (presentSupport) indices.presentFamily = i;
        }
        if (!indices.isComplete()) return 0;

        // Check extensions
        uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> availableExts(extCount);
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, availableExts.data());

        std::set<std::string> requiredExts(reqs.extensions.begin(), reqs.extensions.end());
        for (const auto& ext : availableExts) {
            requiredExts.erase(ext.extensionName);
        }
        if (!requiredExts.empty()) return 0;

        return score;
    };

    int maxScore = 0;
    physicalDevice = VK_NULL_HANDLE;
    QueueFamilyIndices selectedIndices;
    DeviceRequirements reqs;

    for (const auto& dev : devices) {
        QueueFamilyIndices indices;
        int score = rateDevice(dev, surface, indices, reqs);
        if (score > maxScore) {
            maxScore = score;
            physicalDevice = dev;
            selectedIndices = indices;
        }
    }

    if (physicalDevice == VK_NULL_HANDLE) throw std::runtime_error("No suitable Vulkan device found");

    graphicsFamily = selectedIndices.graphicsFamily.value();
    presentFamily = selectedIndices.presentFamily.value();
}

// Creates the logical device with required queues, extensions, and features (including ray tracing).
// Optimization: Use unique queues if graphics and present differ, enable only necessary features.
void VulkanInitializer::createLogicalDevice(VkPhysicalDevice physicalDevice, VkDevice& device, VkQueue& graphicsQueue, VkQueue& presentQueue, uint32_t graphicsFamily, uint32_t presentFamily) {
    DeviceRequirements reqs;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {graphicsFamily, presentFamily};

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};

    // Chain features for ray tracing
    reqs.accelerationStructureFeatures.pNext = &reqs.rayTracingFeatures;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &reqs.accelerationStructureFeatures;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(reqs.extensions.size());
    createInfo.ppEnabledExtensionNames = reqs.extensions.data();

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device");
    }

    vkGetDeviceQueue(device, graphicsFamily, 0, &graphicsQueue);
    vkGetDeviceQueue(device, presentFamily, 0, &presentQueue);
}

// Creates the swapchain with optimal format, present mode, and extent.
// Optimization: Prefer SRGB format, mailbox present mode for low latency, clamp extent to capabilities.
void VulkanInitializer::createSwapchain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, VkSwapchainKHR& swapchain,
                                        std::vector<VkImage>& swapchainImages, std::vector<VkImageView>& swapchainImageViews, VkFormat& swapchainFormat, int width, int height) {
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());

    VkSurfaceFormatKHR selectedFormat = formats[0];
    for (const auto& availableFormat : formats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            selectedFormat = availableFormat;
            break;
        }
    }
    swapchainFormat = selectedFormat.format;

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data());

    VkPresentModeKHR selectedPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (const auto& availablePresentMode : presentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            selectedPresentMode = availablePresentMode;
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

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = selectedFormat.format;
    createInfo.imageColorSpace = selectedFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = selectedPresentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swapchain");
    }

    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
    swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());

    swapchainImageViews.resize(imageCount);
    for (size_t i = 0; i < swapchainImages.size(); ++i) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = selectedFormat.format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &viewInfo, nullptr, &swapchainImageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image view");
        }
    }
}

// Creates a simple render pass for color attachment.
// Optimization: Single subpass for efficiency.
void VulkanInitializer::createRenderPass(VkDevice device, VkRenderPass& renderPass, VkFormat format) {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass");
    }
}

// Utility to load and create a shader module from SPIR-V file.
// Memory safe: File is closed automatically via ifstream.
VkShaderModule VulkanInitializer::createShaderModule(VkDevice device, const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file: " + filename);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = buffer.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module for " + filename);
    }

    return shaderModule;
}

// Creates the graphics pipeline for rasterization.
// Optimization: Fixed viewport, no depth testing (as per original), alpha blending enabled.
// Push constants for dynamic uniforms without descriptor sets for speed.
void VulkanInitializer::createGraphicsPipeline(VkDevice device, VkRenderPass renderPass, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout, int width, int height) {
    VkShaderModule vertShaderModule = createShaderModule(device, "assets/shaders/vertex.spv");
    VkShaderModule fragShaderModule = createShaderModule(device, "assets/shaders/fragment.spv");

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(glm::vec3);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributeDescription{};
    attributeDescription.binding = 0;
    attributeDescription.location = 0;
    attributeDescription.format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescription.offset = 0;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = 1;
    vertexInputInfo.pVertexAttributeDescriptions = &attributeDescription;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(width);
    viewport.height = static_cast<float>(height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // Push constants for model/view/proj and custom parameters
    struct PushConstants {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
        glm::vec3 baseColor;
        float value;
        float dimension;
        float wavePhase;
        float cycleProgress;
        float darkMatter;
        float darkEnergy;
    };

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline");
    }

    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
}

// Creates framebuffers for each swapchain image.
// Optimization: One per image for triple buffering if applicable.
void VulkanInitializer::createFramebuffers(VkDevice device, VkRenderPass renderPass, std::vector<VkImageView>& swapchainImageViews, std::vector<VkFramebuffer>& swapchainFramebuffers, int width, int height) {
    swapchainFramebuffers.resize(swapchainImageViews.size());

    for (size_t i = 0; i < swapchainImageViews.size(); ++i) {
        VkImageView attachments[] = {swapchainImageViews[i]};

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = static_cast<uint32_t>(width);
        framebufferInfo.height = static_cast<uint32_t>(height);
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapchainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer");
        }
    }
}

// Creates a command pool for graphics commands.
// Flags allow resetting individual buffers for flexibility.
void VulkanInitializer::createCommandPool(VkDevice device, VkCommandPool& commandPool, uint32_t graphicsFamily) {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = graphicsFamily;

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool");
    }
}

// Allocates primary command buffers, one per framebuffer.
void VulkanInitializer::createCommandBuffers(VkDevice device, VkCommandPool commandPool, std::vector<VkCommandBuffer>& commandBuffers, std::vector<VkFramebuffer>& swapchainFramebuffers) {
    commandBuffers.resize(swapchainFramebuffers.size());

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers");
    }
}

// Creates semaphores and fence for synchronization.
// Fence starts signaled for initial wait.
void VulkanInitializer::createSyncObjects(VkDevice device, VkSemaphore& imageAvailableSemaphore, VkSemaphore& renderFinishedSemaphore, VkFence& inFlightFence) {
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
        vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS ||
        vkCreateFence(device, &fenceInfo, nullptr, &inFlightFence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create synchronization objects");
    }
}

// Utility to create a buffer with specified usage and memory properties.
// Optimization: Find optimal memory type, prefer device local for performance.
void VulkanInitializer::createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer& buffer, VkDeviceMemory& memory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer");
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
        throw std::runtime_error("Failed to find suitable memory type");
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory");
    }

    vkBindBufferMemory(device, buffer, memory, 0);
}

// Utility to copy data from one buffer to another using a transient command buffer.
// Optimization: One-time submit for efficiency, wait idle to ensure completion.
void VulkanInitializer::copyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, src, dst, 1, &copyRegion);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

// Creates and populates a vertex buffer using staging buffer for optimal transfer.
// Memory safe: Staging resources destroyed after use.
void VulkanInitializer::createVertexBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                                           VkBuffer& vertexBuffer, VkDeviceMemory& vertexBufferMemory, const std::vector<glm::vec3>& vertices) {
    if (vertices.empty()) throw std::runtime_error("Vertices vector is empty");

    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(device, physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(device, stagingBufferMemory);

    createBuffer(device, physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

    copyBuffer(device, commandPool, graphicsQueue, stagingBuffer, vertexBuffer, bufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

// Creates and populates an index buffer using staging buffer for optimal transfer.
// Memory safe: Staging resources destroyed after use.
void VulkanInitializer::createIndexBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                                          VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory, const std::vector<uint32_t>& indices) {
    if (indices.empty()) throw std::runtime_error("Indices vector is empty");

    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(device, physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, indices.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(device, stagingBufferMemory);

    createBuffer(device, physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

    copyBuffer(device, commandPool, graphicsQueue, stagingBuffer, indexBuffer, bufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}