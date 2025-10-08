// AMOURANTH RTX Engine, October 2025 - Vulkan core initialization.
// Initializes physical device, logical device, queues, command pool, and synchronization objects.
// Dependencies: Vulkan 1.3+, GLM, C++20 standard library.
// Zachary Geurts 2025

#include "engine/Vulkan_init.hpp"
#include "engine/Vulkan_init_buffers.hpp"
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <format>

VulkanRenderer::VulkanRenderer(VkInstance instance, VkSurfaceKHR surface,
                              std::span<const glm::vec3> vertices, std::span<const uint32_t> indices,
                              int width, int height, const Logging::Logger& logger)
    : instance_(instance), 
      surface_(surface), 
      swapchainManager_(context_, surface),
      pipelineManager_(context_),
      logger_(logger), 
      lastFrameTime_(std::chrono::steady_clock::now()) {
    logger_.log(Logging::LogLevel::Info, "Constructing VulkanRenderer with instance={:p}, surface={:p}, width={}, height={}, vertices.size={}, indices.size={}",
                std::source_location::current(), static_cast<void*>(instance), static_cast<void*>(surface), width, height, vertices.size(), indices.size());
    if (!instance || !surface) {
        logger_.log(Logging::LogLevel::Error, "Invalid Vulkan instance or surface: instance={:p}, surface={:p}",
                    std::source_location::current(), static_cast<void*>(instance), static_cast<void*>(surface));
        throw std::invalid_argument("Vulkan instance or surface is null");
    }
    if (vertices.empty()) {
        logger_.log(Logging::LogLevel::Error, "Vertex buffer is empty", std::source_location::current());
        throw std::invalid_argument("Vertex buffer cannot be empty");
    }
    if (width <= 0 || height <= 0) {
        logger_.log(Logging::LogLevel::Error, "Invalid window dimensions: width={}, height={}",
                    std::source_location::current(), width, height);
        throw std::invalid_argument("Invalid window dimensions");
    }
    initializeVulkan(vertices, indices, width, height);
    logger_.log(Logging::LogLevel::Debug, "VulkanRenderer constructed successfully", std::source_location::current());
}

VulkanRenderer::~VulkanRenderer() {
    logger_.log(Logging::LogLevel::Info, "Destroying VulkanRenderer", std::source_location::current());
    cleanupVulkan();
    logger_.log(Logging::LogLevel::Debug, "VulkanRenderer destroyed", std::source_location::current());
}

void VulkanRenderer::initializeVulkan(std::span<const glm::vec3> vertices, std::span<const uint32_t> indices,
                                     int width, int height) {
    logger_.log(Logging::LogLevel::Info, "Initializing Vulkan renderer with vertices.size={}, indices.size={}",
                std::source_location::current(), vertices.size(), indices.size());

    // Select physical device
    logger_.log(Logging::LogLevel::Debug, "Selecting physical device", std::source_location::current());
    uint32_t deviceCount = 0;
    VkResult result = vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
    if (result != VK_SUCCESS || deviceCount == 0) {
        logger_.log(Logging::LogLevel::Error, "No Vulkan-capable devices found: VkResult={}", 
                    std::source_location::current(), static_cast<int>(result));
        throw std::runtime_error("No Vulkan-capable devices found");
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());
    context_.physicalDevice = devices[0]; // Simplified selection
    VkPhysicalDeviceProperties deviceProps;
    vkGetPhysicalDeviceProperties(context_.physicalDevice, &deviceProps);
    logger_.log(Logging::LogLevel::Info, "Selected physical device: {}", 
                std::source_location::current(), deviceProps.deviceName);

    // Queue families
    logger_.log(Logging::LogLevel::Debug, "Querying queue families", std::source_location::current());
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(context_.physicalDevice, &queueFamilyCount, nullptr);
    if (queueFamilyCount == 0) {
        logger_.log(Logging::LogLevel::Error, "No queue families found", std::source_location::current());
        throw std::runtime_error("No queue families found");
    }
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(context_.physicalDevice, &queueFamilyCount, queueFamilies.data());
    bool foundGraphics = false, foundPresent = false;
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            context_.graphicsFamily = i;
            foundGraphics = true;
            logger_.log(Logging::LogLevel::Debug, "Found graphics queue family: index={}", 
                        std::source_location::current(), i);
        }
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(context_.physicalDevice, i, surface_, &presentSupport);
        if (presentSupport) {
            context_.presentFamily = i;
            foundPresent = true;
            logger_.log(Logging::LogLevel::Debug, "Found present queue family: index={}", 
                        std::source_location::current(), i);
        }
        if (foundGraphics && foundPresent) break;
    }
    if (!foundGraphics || !foundPresent) {
        logger_.log(Logging::LogLevel::Error, "Failed to find suitable queue families: graphics={}, present={}",
                    std::source_location::current(), foundGraphics, foundPresent);
        throw std::runtime_error("Failed to find suitable queue families");
    }

    // Verify surface compatibility
    logger_.log(Logging::LogLevel::Debug, "Verifying surface capabilities", std::source_location::current());
    VkSurfaceCapabilitiesKHR surfaceCaps;
    result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context_.physicalDevice, surface_, &surfaceCaps);
    if (result != VK_SUCCESS) {
        logger_.log(Logging::LogLevel::Error, "Failed to get surface capabilities: VkResult={}", 
                    std::source_location::current(), static_cast<int>(result));
        throw std::runtime_error("Failed to get surface capabilities");
    }
    logger_.log(Logging::LogLevel::Debug, "Surface capabilities: minImageCount={}, maxImageCount={}, currentExtent={{{}, {}}}",
                std::source_location::current(), surfaceCaps.minImageCount, surfaceCaps.maxImageCount,
                surfaceCaps.currentExtent.width, surfaceCaps.currentExtent.height);

    // Check device extension support
    logger_.log(Logging::LogLevel::Debug, "Checking device extensions", std::source_location::current());
    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(context_.physicalDevice, nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extCount);
    vkEnumerateDeviceExtensionProperties(context_.physicalDevice, nullptr, &extCount, availableExtensions.data());
    bool hasSwapchain = false;
    for (const auto& ext : availableExtensions) {
        if (std::strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
            hasSwapchain = true;
            logger_.log(Logging::LogLevel::Info, "VK_KHR_swapchain extension found", std::source_location::current());
            break;
        }
    }
    if (!hasSwapchain) {
        logger_.log(Logging::LogLevel::Error, "Physical device does not support VK_KHR_swapchain", 
                    std::source_location::current());
        throw std::runtime_error("Physical device does not support VK_KHR_swapchain");
    }

    // Create logical device
    logger_.log(Logging::LogLevel::Debug, "Creating logical device", std::source_location::current());
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
    const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
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
        logger_.log(Logging::LogLevel::Error, "Failed to create logical device", std::source_location::current());
        throw std::runtime_error("Failed to create logical device");
    }
    logger_.log(Logging::LogLevel::Info, "Logical device created", std::source_location::current());

    vkGetDeviceQueue(context_.device, context_.graphicsFamily, 0, &context_.graphicsQueue);
    vkGetDeviceQueue(context_.device, context_.presentFamily, 0, &context_.presentQueue);
    if (!context_.graphicsQueue || !context_.presentQueue) {
        logger_.log(Logging::LogLevel::Error, "Failed to retrieve device queues", std::source_location::current());
        throw std::runtime_error("Failed to retrieve device queues");
    }
    logger_.log(Logging::LogLevel::Debug, "Device queues retrieved: graphicsQueue={:p}, presentQueue={:p}",
                std::source_location::current(), static_cast<void*>(context_.graphicsQueue), static_cast<void*>(context_.presentQueue));

    // Initialize swapchain
    logger_.log(Logging::LogLevel::Debug, "Initializing swapchain", std::source_location::current());
    swapchainManager_.initializeSwapchain(width, height);
    if (!context_.swapchain || context_.swapchainImages.empty()) {
        logger_.log(Logging::LogLevel::Error, "Failed to initialize swapchain", std::source_location::current());
        throw std::runtime_error("Failed to initialize swapchain");
    }
    logger_.log(Logging::LogLevel::Debug, "Swapchain initialized with {} images", 
                std::source_location::current(), context_.swapchainImages.size());

    // Initialize pipeline
    logger_.log(Logging::LogLevel::Debug, "Initializing pipeline", std::source_location::current());
    pipelineManager_.initializePipeline(width, height);
    if (!context_.pipeline || !context_.pipelineLayout || !context_.renderPass) {
        logger_.log(Logging::LogLevel::Error, "Failed to initialize pipeline", std::source_location::current());
        throw std::runtime_error("Failed to initialize pipeline");
    }
    logger_.log(Logging::LogLevel::Debug, "Pipeline initialized successfully", std::source_location::current());

    // Initialize buffers
    logger_.log(Logging::LogLevel::Debug, "Initializing buffers", std::source_location::current());
    VulkanBufferManager bufferManager(context_);
    bufferManager.initializeBuffers(vertices, indices);
    if (!context_.vertexBuffer || (context_.indexBuffer == VK_NULL_HANDLE && !indices.empty())) {
        logger_.log(Logging::LogLevel::Error, "Failed to initialize vertex or index buffers", 
                    std::source_location::current());
        throw std::runtime_error("Failed to initialize vertex or index buffers");
    }
    logger_.log(Logging::LogLevel::Debug, "Buffers initialized: vertexBuffer={:p}, indexBuffer={:p}",
                std::source_location::current(), static_cast<void*>(context_.vertexBuffer), 
                static_cast<void*>(context_.indexBuffer));

    // Create command pool
    logger_.log(Logging::LogLevel::Debug, "Creating command pool", std::source_location::current());
    VkCommandPoolCreateInfo commandPoolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = context_.graphicsFamily,
    };
    if (vkCreateCommandPool(context_.device, &commandPoolInfo, nullptr, &context_.commandPool) != VK_SUCCESS) {
        logger_.log(Logging::LogLevel::Error, "Failed to create command pool", std::source_location::current());
        throw std::runtime_error("Failed to create command pool");
    }
    logger_.log(Logging::LogLevel::Info, "Command pool created", std::source_location::current());

    // Create command buffers
    logger_.log(Logging::LogLevel::Debug, "Creating command buffers", std::source_location::current());
    if (context_.swapchainFramebuffers.empty()) {
        logger_.log(Logging::LogLevel::Error, "No swapchain framebuffers available for command buffer allocation", 
                    std::source_location::current());
        throw std::runtime_error("No swapchain framebuffers available");
    }
    context_.commandBuffers.resize(context_.swapchainFramebuffers.size());
    VkCommandBufferAllocateInfo commandBufferAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = context_.commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<uint32_t>(context_.commandBuffers.size()),
    };
    if (vkAllocateCommandBuffers(context_.device, &commandBufferAllocInfo, context_.commandBuffers.data()) != VK_SUCCESS) {
        logger_.log(Logging::LogLevel::Error, "Failed to allocate command buffers", std::source_location::current());
        throw std::runtime_error("Failed to allocate command buffers");
    }
    logger_.log(Logging::LogLevel::Info, "Allocated {} command buffers", 
                std::source_location::current(), context_.commandBuffers.size());

    // Create semaphores and fences
    logger_.log(Logging::LogLevel::Debug, "Creating synchronization objects", std::source_location::current());
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
            logger_.log(Logging::LogLevel::Error, "Failed to create synchronization objects for index={}", 
                        std::source_location::current(), i);
            throw std::runtime_error("Failed to create synchronization objects");
        }
    }
    logger_.log(Logging::LogLevel::Info, "Created {} synchronization objects", 
                std::source_location::current(), context_.swapchainImages.size());
}

void VulkanRenderer::beginFrame() {
    logger_.log(Logging::LogLevel::Debug, "Beginning frame {}", std::source_location::current(), currentFrame_);
    if (context_.inFlightFences.empty() || context_.inFlightFences[currentFrame_] == VK_NULL_HANDLE) {
        logger_.log(Logging::LogLevel::Error, "Invalid in-flight fence for frame {}", 
                    std::source_location::current(), currentFrame_);
        throw std::runtime_error("Invalid in-flight fence");
    }
    VkResult result = vkWaitForFences(context_.device, 1, &context_.inFlightFences[currentFrame_], VK_TRUE, UINT64_MAX);
    if (result != VK_SUCCESS) {
        logger_.log(Logging::LogLevel::Error, "Failed to wait for fence for frame {}: VkResult={}",
                    std::source_location::current(), currentFrame_, static_cast<int>(result));
        throw std::runtime_error("Failed to wait for fence");
    }
    vkResetFences(context_.device, 1, &context_.inFlightFences[currentFrame_]);

    result = vkAcquireNextImageKHR(context_.device, context_.swapchain, UINT64_MAX,
                                   context_.imageAvailableSemaphores[currentFrame_], VK_NULL_HANDLE, &currentImageIndex_);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        logger_.log(Logging::LogLevel::Warning, "Swapchain out of date, resizing", std::source_location::current());
        swapchainManager_.handleResize(context_.swapchainExtent.width, context_.swapchainExtent.height);
        return;
    }
    if (result != VK_SUCCESS) {
        logger_.log(Logging::LogLevel::Error, "Failed to acquire swapchain image: VkResult={}",
                    std::source_location::current(), static_cast<int>(result));
        throw std::runtime_error("Failed to acquire swapchain image");
    }

    vkResetCommandBuffer(context_.commandBuffers[currentFrame_], 0);
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };
    if (vkBeginCommandBuffer(context_.commandBuffers[currentFrame_], &beginInfo) != VK_SUCCESS) {
        logger_.log(Logging::LogLevel::Error, "Failed to begin command buffer for frame {}",
                    std::source_location::current(), currentFrame_);
        throw std::runtime_error("Failed to begin command buffer");
    }
}

void VulkanRenderer::endFrame() {
    logger_.log(Logging::LogLevel::Debug, "Ending frame {}", std::source_location::current(), currentFrame_);
    if (vkEndCommandBuffer(context_.commandBuffers[currentFrame_]) != VK_SUCCESS) {
        logger_.log(Logging::LogLevel::Error, "Failed to record command buffer for frame {}",
                    std::source_location::current(), currentFrame_);
        throw std::runtime_error("Failed to record command buffer");
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
        logger_.log(Logging::LogLevel::Error, "Failed to submit draw command buffer for frame {}",
                    std::source_location::current(), currentFrame_);
        throw std::runtime_error("Failed to submit draw command buffer");
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
        logger_.log(Logging::LogLevel::Warning, "Swapchain out of date during present for frame {}",
                    std::source_location::current(), currentFrame_);
        swapchainManager_.handleResize(context_.swapchainExtent.width, context_.swapchainExtent.height);
    } else if (result != VK_SUCCESS) {
        logger_.log(Logging::LogLevel::Error, "Failed to present queue for frame {}: VkResult={}",
                    std::source_location::current(), currentFrame_, static_cast<int>(result));
        throw std::runtime_error("Failed to present queue");
    }

    currentFrame_ = (currentFrame_ + 1) % context_.swapchainFramebuffers.size();
}

void VulkanRenderer::renderFrame(AMOURANTH* amouranth) {
    logger_.log(Logging::LogLevel::Debug, "Starting renderFrame for frame {}", 
                std::source_location::current(), currentFrame_);
    if (!amouranth) {
        logger_.log(Logging::LogLevel::Error, "AMOURANTH pointer is null", std::source_location::current());
        throw std::invalid_argument("AMOURANTH pointer cannot be null");
    }
    auto currentTime = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(currentTime - lastFrameTime_).count();
    lastFrameTime_ = currentTime;

    VkSurfaceCapabilitiesKHR surfaceCaps;
    VkResult surfaceResult = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context_.physicalDevice, surface_, &surfaceCaps);
    if (surfaceResult != VK_SUCCESS) {
        logger_.log(Logging::LogLevel::Warning, "Failed to get surface capabilities: VkResult={}",
                    std::source_location::current(), static_cast<int>(surfaceResult));
    }

    beginFrame();
    amouranth->render(
        currentImageIndex_,
        context_.vertexBuffer,
        context_.commandBuffers[currentFrame_],
        context_.indexBuffer,
        context_.pipelineLayout,
        context_.descriptorSet,
        context_.renderPass,
        context_.swapchainFramebuffers[currentImageIndex_],
        deltaTime
    );
    endFrame();
}

void VulkanRenderer::handleResize(int width, int height) {
    logger_.log(Logging::LogLevel::Info, "Handling resize to width={}, height={}", 
                std::source_location::current(), width, height);
    if (width <= 0 || height <= 0) {
        logger_.log(Logging::LogLevel::Error, "Invalid resize dimensions: width={}, height={}",
                    std::source_location::current(), width, height);
        throw std::invalid_argument("Invalid resize dimensions");
    }
    if (context_.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(context_.device);
    }
    swapchainManager_.handleResize(width, height);
}

void VulkanRenderer::cleanupVulkan() {
    logger_.log(Logging::LogLevel::Info, "Cleaning up Vulkan resources", std::source_location::current());
    if (context_.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(context_.device);
    }
    swapchainManager_.cleanupSwapchain();
    for (auto& semaphore : context_.imageAvailableSemaphores) {
        if (semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(context_.device, semaphore, nullptr);
        }
    }
    for (auto& semaphore : context_.renderFinishedSemaphores) {
        if (semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(context_.device, semaphore, nullptr);
        }
    }
    for (auto& fence : context_.inFlightFences) {
        if (fence != VK_NULL_HANDLE) {
            vkDestroyFence(context_.device, fence, nullptr);
        }
    }
    if (context_.commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(context_.device, context_.commandPool, nullptr);
    }
    pipelineManager_.cleanupPipeline();
    VulkanBufferManager bufferManager(context_);
    bufferManager.cleanupBuffers();
    if (context_.device != VK_NULL_HANDLE) {
        vkDestroyDevice(context_.device, nullptr);
    }
}