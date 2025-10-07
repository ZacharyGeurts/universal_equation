// AMOURANTH RTX Engine, October 2025 - Vulkan core initialization.
// Initializes physical device, logical device, queues, command pool, and synchronization objects.
// Dependencies: Vulkan 1.3+, GLM, C++20 standard library.
// Zachary Geurts 2025

#include "engine/Vulkan_init.hpp"
#include "engine/Vulkan_init_buffers.hpp"
#include "engine/Vulkan_init_swapchain.hpp"
#include "engine/Vulkan_init_pipeline.hpp"
#include "engine/logging.hpp"
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <format>

VulkanRenderer::VulkanRenderer(VkInstance instance, VkSurfaceKHR surface,
                              std::span<const glm::vec3> vertices, std::span<const uint32_t> indices,
                              VkShaderModule vertShaderModule, VkShaderModule fragShaderModule,
                              int width, int height) 
    : instance_(instance), surface_(surface) {
    vertShaderModule_ = vertShaderModule;
    fragShaderModule_ = fragShaderModule;
    initializeVulkan(vertices, indices, vertShaderModule, fragShaderModule, width, height);
}

VulkanRenderer::~VulkanRenderer() {
    cleanupVulkan();
}

void VulkanRenderer::initializeVulkan(std::span<const glm::vec3> vertices, std::span<const uint32_t> indices,
                                     VkShaderModule vertShaderModule, VkShaderModule fragShaderModule,
                                     int width, int height) {
    Logging::Logger logger;
    logger.log(Logging::LogLevel::Info, "Initializing Vulkan renderer", std::source_location::current());

    // Select physical device
    logger.log(Logging::LogLevel::Debug, "Selecting physical device", std::source_location::current());
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
    if (deviceCount == 0) {
        std::string error = "No Vulkan-capable devices found";
        logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());
    context_.physicalDevice = devices[0]; // Simplified selection
    logger.log(Logging::LogLevel::Info, "Selected physical device", std::source_location::current());

    // Queue families
    logger.log(Logging::LogLevel::Debug, "Querying queue families", std::source_location::current());
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(context_.physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(context_.physicalDevice, &queueFamilyCount, queueFamilies.data());
    bool foundGraphics = false, foundPresent = false;
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            context_.graphicsFamily = i;
            foundGraphics = true;
        }
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(context_.physicalDevice, i, surface_, &presentSupport);
        if (presentSupport) {
            context_.presentFamily = i;
            foundPresent = true;
        }
        if (foundGraphics && foundPresent) break;
    }
    if (!foundGraphics || !foundPresent) {
        std::string error = "Failed to find suitable queue families";
        logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }

    // Check device extension support
    logger.log(Logging::LogLevel::Debug, "Checking device extensions", std::source_location::current());
    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(context_.physicalDevice, nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extCount);
    vkEnumerateDeviceExtensionProperties(context_.physicalDevice, nullptr, &extCount, availableExtensions.data());
    bool hasSwapchain = false;
    for (const auto& ext : availableExtensions) {
        if (std::strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
            hasSwapchain = true;
            logger.log(Logging::LogLevel::Info, "VK_KHR_swapchain extension found", std::source_location::current());
            break;
        }
    }
    if (!hasSwapchain) {
        std::string error = "Physical device does not support VK_KHR_swapchain";
        logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }

    // Create logical device
    logger.log(Logging::LogLevel::Debug, "Creating logical device", std::source_location::current());
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    float queuePriority = 1.0f;
    std::vector<uint32_t> uniqueFamilies = {context_.graphicsFamily};
    if (context_.graphicsFamily != context_.presentFamily) {
        uniqueFamilies.push_back(context_.presentFamily);
    }
    for (uint32_t family : uniqueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = family,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority,
        };
        queueCreateInfos.push_back(queueCreateInfo);
    }
    VkPhysicalDeviceFeatures deviceFeatures = {};
    const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo deviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = deviceExtensions,
        .pEnabledFeatures = &deviceFeatures,
    };
    if (vkCreateDevice(context_.physicalDevice, &deviceCreateInfo, nullptr, &context_.device) != VK_SUCCESS) {
        std::string error = "Failed to create logical device";
        logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }
    logger.log(Logging::LogLevel::Info, "Logical device created", std::source_location::current());

    vkGetDeviceQueue(context_.device, context_.graphicsFamily, 0, &context_.graphicsQueue);
    vkGetDeviceQueue(context_.device, context_.presentFamily, 0, &context_.presentQueue);

    // Initialize swapchain
    VulkanSwapchainManager swapchainManager(context_, surface_);
    swapchainManager.initializeSwapchain(width, height);

    // Initialize pipeline
    VulkanPipelineManager pipelineManager(context_);
    pipelineManager.initializePipeline(vertShaderModule, fragShaderModule, width, height);

    // Initialize buffers
    VulkanBufferManager bufferManager(context_);
    bufferManager.initializeBuffers(vertices, indices);

    // Create command pool
    VkCommandPoolCreateInfo commandPoolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queueFamilyIndex = context_.graphicsFamily,
    };
    if (vkCreateCommandPool(context_.device, &commandPoolInfo, nullptr, &context_.commandPool) != VK_SUCCESS) {
        std::string error = "Failed to create command pool";
        logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }
    logger.log(Logging::LogLevel::Info, "Command pool created", std::source_location::current());

    // Create command buffers
    context_.commandBuffers.resize(context_.swapchainFramebuffers.size());
    VkCommandBufferAllocateInfo commandBufferAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = context_.commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<uint32_t>(context_.commandBuffers.size()),
    };
    if (vkAllocateCommandBuffers(context_.device, &commandBufferAllocInfo, context_.commandBuffers.data()) != VK_SUCCESS) {
        std::string error = "Failed to allocate command buffers";
        logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }
    logger.log(Logging::LogLevel::Info, "Allocated {} command buffers", std::source_location::current(), context_.commandBuffers.size());

    // Create semaphores and fences
    context_.imageAvailableSemaphores.resize(context_.swapchainImages.size());
    context_.renderFinishedSemaphores.resize(context_.swapchainImages.size());
    context_.inFlightFences.resize(context_.swapchainImages.size());
    VkSemaphoreCreateInfo semaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
    };
    VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    for (size_t i = 0; i < context_.swapchainImages.size(); ++i) {
        if (vkCreateSemaphore(context_.device, &semaphoreInfo, nullptr, &context_.imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(context_.device, &semaphoreInfo, nullptr, &context_.renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(context_.device, &fenceInfo, nullptr, &context_.inFlightFences[i]) != VK_SUCCESS) {
            std::string error = "Failed to create synchronization objects";
            logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
            throw std::runtime_error(error);
        }
    }
    logger.log(Logging::LogLevel::Info, "Created synchronization objects", std::source_location::current());
}

void VulkanRenderer::beginFrame() {
    Logging::Logger logger;
    logger.log(Logging::LogLevel::Debug, "Beginning frame {}", std::source_location::current(), currentFrame_);

    vkWaitForFences(context_.device, 1, &context_.inFlightFences[currentFrame_], VK_TRUE, UINT64_MAX);
    vkResetFences(context_.device, 1, &context_.inFlightFences[currentFrame_]);

    VkResult result = vkAcquireNextImageKHR(context_.device, context_.swapchain, UINT64_MAX,
                                            context_.imageAvailableSemaphores[currentFrame_], VK_NULL_HANDLE, &currentFrame_);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        logger.log(Logging::LogLevel::Warning, "Swapchain out of date, needs resize", std::source_location::current());
        return;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        std::string error = "Failed to acquire swapchain image";
        logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }

    vkResetCommandBuffer(context_.commandBuffers[currentFrame_], 0);
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };
    if (vkBeginCommandBuffer(context_.commandBuffers[currentFrame_], &beginInfo) != VK_SUCCESS) {
        std::string error = "Failed to begin command buffer";
        logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = context_.renderPass,
        .framebuffer = context_.swapchainFramebuffers[currentFrame_],
        .renderArea = {{0, 0}, context_.swapchainExtent},
        .clearValueCount = 1,
        .pClearValues = &clearColor,
    };
    vkCmdBeginRenderPass(context_.commandBuffers[currentFrame_], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    logger.log(Logging::LogLevel::Debug, "Render pass begun for frame {}", std::source_location::current(), currentFrame_);
}

void VulkanRenderer::endFrame() {
    Logging::Logger logger;
    logger.log(Logging::LogLevel::Debug, "Ending frame {}", std::source_location::current(), currentFrame_);

    vkCmdEndRenderPass(context_.commandBuffers[currentFrame_]);
    if (vkEndCommandBuffer(context_.commandBuffers[currentFrame_]) != VK_SUCCESS) {
        std::string error = "Failed to record command buffer";
        logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }

    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &context_.imageAvailableSemaphores[currentFrame_],
        .pWaitDstStageMask = waitStages,
        .commandBufferCount = 1,
        .pCommandBuffers = &context_.commandBuffers[currentFrame_],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &context_.renderFinishedSemaphores[currentFrame_],
    };
    if (vkQueueSubmit(context_.graphicsQueue, 1, &submitInfo, context_.inFlightFences[currentFrame_]) != VK_SUCCESS) {
        std::string error = "Failed to submit draw command buffer";
        logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }

    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &context_.renderFinishedSemaphores[currentFrame_],
        .swapchainCount = 1,
        .pSwapchains = &context_.swapchain,
        .pImageIndices = &currentFrame_,
        .pResults = nullptr,
    };
    VkResult result = vkQueuePresentKHR(context_.presentQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        logger.log(Logging::LogLevel::Warning, "Swapchain out of date or suboptimal during present", std::source_location::current());
    } else if (result != VK_SUCCESS) {
        std::string error = "Failed to present queue";
        logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }
    logger.log(Logging::LogLevel::Debug, "Frame {} presented", std::source_location::current(), currentFrame_);
}

void VulkanRenderer::handleResize(int width, int height) {
    VulkanSwapchainManager swapchainManager(context_, surface_);
    swapchainManager.handleResize(width, height);
}

void VulkanRenderer::cleanupVulkan() {
    Logging::Logger logger;
    logger.log(Logging::LogLevel::Info, "Cleaning up Vulkan resources", std::source_location::current());

    vkDeviceWaitIdle(context_.device);
    for (auto semaphore : context_.imageAvailableSemaphores) {
        vkDestroySemaphore(context_.device, semaphore, nullptr);
    }
    for (auto semaphore : context_.renderFinishedSemaphores) {
        vkDestroySemaphore(context_.device, semaphore, nullptr);
    }
    for (auto fence : context_.inFlightFences) {
        vkDestroyFence(context_.device, fence, nullptr);
    }
    vkDestroyCommandPool(context_.device, context_.commandPool, nullptr);
    for (auto framebuffer : context_.swapchainFramebuffers) {
        vkDestroyFramebuffer(context_.device, framebuffer, nullptr);
    }
    for (auto imageView : context_.swapchainImageViews) {
        vkDestroyImageView(context_.device, imageView, nullptr);
    }
    vkDestroySwapchainKHR(context_.device, context_.swapchain, nullptr);
    VulkanPipelineManager pipelineManager(context_);
    pipelineManager.cleanupPipeline();
    VulkanBufferManager bufferManager(context_);
    bufferManager.cleanupBuffers();
    if (vertShaderModule_ != VK_NULL_HANDLE) {
        vkDestroyShaderModule(context_.device, vertShaderModule_, nullptr);
    }
    if (fragShaderModule_ != VK_NULL_HANDLE) {
        vkDestroyShaderModule(context_.device, fragShaderModule_, nullptr);
    }
    vkDestroyDevice(context_.device, nullptr);
    logger.log(Logging::LogLevel::Info, "Vulkan resources cleaned up", std::source_location::current());
}

VkShaderModule VulkanRenderer::createShaderModule(const std::string& filename) const {
    Logging::Logger logger;
    logger.log(Logging::LogLevel::Debug, "Creating shader module from {}", std::source_location::current(), filename);

    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::string error = std::format("Failed to open shader file: {}", filename);
        logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> buffer(size);
    if (!file.read(buffer.data(), size)) {
        std::string error = std::format("Failed to read shader file: {}", filename);
        logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }

    VkShaderModuleCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .codeSize = static_cast<size_t>(size),
        .pCode = reinterpret_cast<const uint32_t*>(buffer.data()),
    };

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(context_.device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        std::string error = std::format("Failed to create shader module: {}", filename);
        logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }
    logger.log(Logging::LogLevel::Info, "Shader module created: {}", std::source_location::current(), filename);
    return shaderModule;
}

void VulkanRenderer::setShaderModules(VkShaderModule vertShaderModule, VkShaderModule fragShaderModule) {
    Logging::Logger logger;
    logger.log(Logging::LogLevel::Info, "Setting shader modules", std::source_location::current());
    vertShaderModule_ = vertShaderModule;
    fragShaderModule_ = fragShaderModule;
}

uint32_t VulkanRenderer::getCurrentImageIndex() const {
    return currentFrame_;
}

VkBuffer VulkanRenderer::getVertexBuffer() const {
    return context_.vertexBuffer;
}

VkBuffer VulkanRenderer::getIndexBuffer() const {
    return context_.indexBuffer;
}

VkCommandBuffer VulkanRenderer::getCommandBuffer() const {
    return context_.commandBuffers[currentFrame_];
}

VkPipelineLayout VulkanRenderer::getPipelineLayout() const {
    return context_.pipelineLayout;
}

VkDescriptorSet VulkanRenderer::getDescriptorSet() const {
    return context_.descriptorSet;
}

const VulkanContext& VulkanRenderer::getContext() const {
    return context_;
}