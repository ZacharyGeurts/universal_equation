// AMOURANTH RTX Engine, October 2025 - Vulkan core initialization.
// Initializes physical device, logical device, queues, command pool, and synchronization objects.
// Dependencies: Vulkan 1.3+, GLM, C++20 standard library.
// Zachary Geurts 2025

#include "engine/Vulkan_init.hpp"
#include "engine/Vulkan_init_buffers.hpp"
#include "engine/Vulkan_init_swapchain.hpp"
#include "engine/Vulkan_init_pipeline.hpp"
#include "engine/core.hpp" // For AMOURANTH definition
#include "engine/logging.hpp"
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <format>

VulkanRenderer::VulkanRenderer(VkInstance instance, VkSurfaceKHR surface,
                              std::span<const glm::vec3> vertices, std::span<const uint32_t> indices,
                              int width, int height) 
    : instance_(instance), surface_(surface), swapchainManager_(context_, surface_), pipelineManager_(context_) {
    Logging::Logger logger;
    logger.log(Logging::LogLevel::Info, "Constructing VulkanRenderer with instance={:p}, surface={:p}, width={}, height={}",
               std::source_location::current(), static_cast<void*>(instance), static_cast<void*>(surface), width, height);
    initializeVulkan(vertices, indices, width, height);
}

VulkanRenderer::~VulkanRenderer() {
    Logging::Logger logger;
    logger.log(Logging::LogLevel::Info, "Destroying VulkanRenderer", std::source_location::current());
    cleanupVulkan();
}

void VulkanRenderer::initializeVulkan(std::span<const glm::vec3> vertices, std::span<const uint32_t> indices,
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
    swapchainManager_.initializeSwapchain(width, height);

    // Initialize pipeline
    pipelineManager_.initializePipeline(width, height);

    // Initialize buffers
    VulkanBufferManager bufferManager(context_);
    bufferManager.initializeBuffers(vertices, indices);

    // Create command pool
    VkCommandPoolCreateInfo commandPoolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
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
                                            context_.imageAvailableSemaphores[currentFrame_], VK_NULL_HANDLE, &currentImageIndex_);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        logger.log(Logging::LogLevel::Warning, "Swapchain out of date, resizing", std::source_location::current());
        swapchainManager_.handleResize(context_.swapchainExtent.width, context_.swapchainExtent.height);
        return;
    }
    if (result != VK_SUCCESS) {
        std::string error = std::format("Failed to acquire swapchain image: VkResult={}", static_cast<int>(result));
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
        .framebuffer = context_.swapchainFramebuffers[currentImageIndex_],
        .renderArea = {{0, 0}, context_.swapchainExtent},
        .clearValueCount = 1,
        .pClearValues = &clearColor,
    };
    vkCmdBeginRenderPass(context_.commandBuffers[currentFrame_], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    logger.log(Logging::LogLevel::Debug, "Render pass begun for frame {}, image index {}", std::source_location::current(), currentFrame_, currentImageIndex_);
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
        .pImageIndices = &currentImageIndex_,
        .pResults = nullptr,
    };
    VkResult result = vkQueuePresentKHR(context_.presentQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        logger.log(Logging::LogLevel::Warning, "Swapchain out of date or suboptimal during present", std::source_location::current());
        swapchainManager_.handleResize(context_.swapchainExtent.width, context_.swapchainExtent.height);
    } else if (result != VK_SUCCESS) {
        std::string error = std::format("Failed to present queue: VkResult={}", static_cast<int>(result));
        logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }
    logger.log(Logging::LogLevel::Debug, "Frame {} presented with image index {}", std::source_location::current(), currentFrame_, currentImageIndex_);
    currentFrame_ = (currentFrame_ + 1) % context_.swapchainFramebuffers.size();
}

void VulkanRenderer::renderFrame(AMOURANTH* amouranth) {
    Logging::Logger logger;
    logger.log(Logging::LogLevel::Debug, "Rendering frame with AMOURANTH", std::source_location::current());
    beginFrame();
    amouranth->render(
        currentImageIndex_,
        context_.vertexBuffer,
        context_.commandBuffers[currentFrame_],
        context_.indexBuffer,
        context_.pipelineLayout,
        context_.descriptorSet
    );
    endFrame();
}

void VulkanRenderer::handleResize(int width, int height) {
    Logging::Logger logger;
    logger.log(Logging::LogLevel::Info, "Handling resize to width={}, height={}", std::source_location::current(), width, height);
    if (context_.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(context_.device);
    }
    swapchainManager_.handleResize(width, height);
}

void VulkanRenderer::cleanupVulkan() {
    Logging::Logger logger;
    logger.log(Logging::LogLevel::Info, "Cleaning up Vulkan resources", std::source_location::current());

    if (context_.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(context_.device);
    }

    for (auto semaphore : context_.imageAvailableSemaphores) {
        if (semaphore != VK_NULL_HANDLE) vkDestroySemaphore(context_.device, semaphore, nullptr);
    }
    for (auto semaphore : context_.renderFinishedSemaphores) {
        if (semaphore != VK_NULL_HANDLE) vkDestroySemaphore(context_.device, semaphore, nullptr);
    }
    for (auto fence : context_.inFlightFences) {
        if (fence != VK_NULL_HANDLE) vkDestroyFence(context_.device, fence, nullptr);
    }
    if (context_.commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(context_.device, context_.commandPool, nullptr);
        context_.commandPool = VK_NULL_HANDLE;
    }
    swapchainManager_.cleanupSwapchain();
    pipelineManager_.cleanupPipeline();
    VulkanBufferManager bufferManager(context_);
    bufferManager.cleanupBuffers();
    if (context_.device != VK_NULL_HANDLE) {
        vkDestroyDevice(context_.device, nullptr);
        context_.device = VK_NULL_HANDLE;
    }
    logger.log(Logging::LogLevel::Info, "Vulkan resources cleaned up", std::source_location::current());
}