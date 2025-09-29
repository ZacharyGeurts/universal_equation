#include "Vulkan_init.hpp"  // This header
#include "Vulkan_func.hpp"  // Assumes VulkanInitializer is here
// AMOURANTH RTX September 2025
// Zachary Geurts 2025
VulkanRenderer::VulkanRenderer(
    VkInstance instance, VkSurfaceKHR surface,
    const std::vector<glm::vec3>& vertices, const std::vector<uint32_t>& indices,
    VkShaderModule vertShaderModule, VkShaderModule fragShaderModule,
    int width, int height,
    std::function<void(const std::string&)> logger)
    : instance_(instance), surface_(surface), logger_(logger) {
    if (instance_ == VK_NULL_HANDLE || surface_ == VK_NULL_HANDLE) {
        throw std::runtime_error("Vulkan instance or surface is null");
    }
    initializeVulkan(vertices, indices, vertShaderModule, fragShaderModule, width, height);
}

VulkanRenderer::~VulkanRenderer() {
    cleanupVulkan();
}

void VulkanRenderer::initializeVulkan(
    const std::vector<glm::vec3>& vertices, const std::vector<uint32_t>& indices,
    VkShaderModule vertShaderModule, VkShaderModule fragShaderModule,
    int width, int height) {
    // Pick physical device with validation enabled
    VulkanInitializer::createPhysicalDevice(
        instance_, context_.physicalDevice, context_.graphicsFamily, context_.presentFamily, surface_, true, logger_);

    // Select surface format
    VkSurfaceFormatKHR surfaceFormat = VulkanInitializer::selectSurfaceFormat(context_.physicalDevice, surface_);

    // Create logical device and queues
    VulkanInitializer::createLogicalDevice(context_.physicalDevice, context_.device, context_.graphicsQueue, context_.presentQueue,
                                           context_.graphicsFamily, context_.presentFamily);

    if (context_.device == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to create logical device");
    }

    // Create swapchain and images
    VulkanInitializer::createSwapchain(context_.physicalDevice, context_.device, surface_, context_.swapchain, context_.swapchainImages,
                                       context_.swapchainImageViews, surfaceFormat.format, context_.graphicsFamily, context_.presentFamily, width, height);

    // Create render pass
    VulkanInitializer::createRenderPass(context_.device, context_.renderPass, surfaceFormat.format);

    // Create descriptor layout
    VulkanInitializer::createDescriptorSetLayout(context_.device, context_.descriptorSetLayout);

    // Create pipeline (uses provided shaders)
    VulkanInitializer::createGraphicsPipeline(context_.device, context_.renderPass, context_.pipeline, context_.pipelineLayout,
                                               context_.descriptorSetLayout, width, height, vertShaderModule, fragShaderModule);

    // Create framebuffers
    VulkanInitializer::createFramebuffers(context_.device, context_.renderPass, context_.swapchainImageViews, context_.swapchainFramebuffers, width, height);

    // Create command pool and buffers
    VulkanInitializer::createCommandPool(context_.device, context_.commandPool, context_.graphicsFamily);
    VulkanInitializer::createCommandBuffers(context_.device, context_.commandPool, context_.commandBuffers, context_.swapchainFramebuffers);

    // Create sync objects
    VulkanInitializer::createSyncObjects(context_.device, context_.imageAvailableSemaphores, context_.renderFinishedSemaphores,
                                         context_.inFlightFences, static_cast<uint32_t>(context_.swapchainImages.size()));

    // Create vertex buffer with temporary staging
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    VulkanInitializer::createVertexBuffer(context_.device, context_.physicalDevice, context_.commandPool, context_.graphicsQueue,
                                          context_.vertexBuffer, context_.vertexBufferMemory, stagingBuffer, stagingMemory, vertices);
    vkDestroyBuffer(context_.device, stagingBuffer, nullptr);
    vkFreeMemory(context_.device, stagingMemory, nullptr);

    // Create index buffer with temporary staging
    VkBuffer indexStagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexStagingMemory = VK_NULL_HANDLE;
    VulkanInitializer::createIndexBuffer(context_.device, context_.physicalDevice, context_.commandPool, context_.graphicsQueue,
                                         context_.indexBuffer, context_.indexBufferMemory, indexStagingBuffer, indexStagingMemory, indices);
    vkDestroyBuffer(context_.device, indexStagingBuffer, nullptr);
    vkFreeMemory(context_.device, indexStagingMemory, nullptr);

    // Create sampler and descriptors
    VulkanInitializer::createSampler(context_.device, context_.physicalDevice, context_.sampler);
    VulkanInitializer::createDescriptorPoolAndSet(context_.device, context_.descriptorSetLayout, context_.descriptorPool, context_.descriptorSet, context_.sampler);

    // Store shaders (provided by caller)
    context_.vertShaderModule = vertShaderModule;
    context_.fragShaderModule = fragShaderModule;
}

void VulkanRenderer::initializeQuadBuffers(
    const std::vector<glm::vec3>& quadVertices, const std::vector<uint32_t>& quadIndices) {
    // Temporary staging for quad vertex
    VkBuffer quadStagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory quadStagingMemory = VK_NULL_HANDLE;
    VulkanInitializer::createVertexBuffer(context_.device, context_.physicalDevice, context_.commandPool, context_.graphicsQueue,
                                          context_.quadVertexBuffer, context_.quadVertexBufferMemory, quadStagingBuffer, quadStagingMemory, quadVertices);
    vkDestroyBuffer(context_.device, quadStagingBuffer, nullptr);
    vkFreeMemory(context_.device, quadStagingMemory, nullptr);

    // Temporary staging for quad index
    VkBuffer quadIndexStagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory quadIndexStagingMemory = VK_NULL_HANDLE;
    VulkanInitializer::createIndexBuffer(context_.device, context_.physicalDevice, context_.commandPool, context_.graphicsQueue,
                                         context_.quadIndexBuffer, context_.quadIndexBufferMemory, quadIndexStagingBuffer, quadIndexStagingMemory, quadIndices);
    vkDestroyBuffer(context_.device, quadIndexStagingBuffer, nullptr);
    vkFreeMemory(context_.device, quadIndexStagingMemory, nullptr);
}

void VulkanRenderer::cleanupVulkan() {
    if (context_.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(context_.device);

        // Destroy quad buffers if initialized
        if (context_.quadVertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(context_.device, context_.quadVertexBuffer, nullptr);
            context_.quadVertexBuffer = VK_NULL_HANDLE;
        }
        if (context_.quadVertexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(context_.device, context_.quadVertexBufferMemory, nullptr);
            context_.quadVertexBufferMemory = VK_NULL_HANDLE;
        }
        if (context_.quadIndexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(context_.device, context_.quadIndexBuffer, nullptr);
            context_.quadIndexBuffer = VK_NULL_HANDLE;
        }
        if (context_.quadIndexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(context_.device, context_.quadIndexBufferMemory, nullptr);
            context_.quadIndexBufferMemory = VK_NULL_HANDLE;
        }

        for (auto& fence : context_.inFlightFences) {
            if (fence != VK_NULL_HANDLE) vkDestroyFence(context_.device, fence, nullptr);
        }
        for (auto& semaphore : context_.imageAvailableSemaphores) {
            if (semaphore != VK_NULL_HANDLE) vkDestroySemaphore(context_.device, semaphore, nullptr);
        }
        for (auto& semaphore : context_.renderFinishedSemaphores) {
            if (semaphore != VK_NULL_HANDLE) vkDestroySemaphore(context_.device, semaphore, nullptr);
        }
        if (context_.commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(context_.device, context_.commandPool, nullptr);
            context_.commandPool = VK_NULL_HANDLE;
        }
        for (auto& framebuffer : context_.swapchainFramebuffers) {
            if (framebuffer != VK_NULL_HANDLE) vkDestroyFramebuffer(context_.device, framebuffer, nullptr);
        }
        if (context_.pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(context_.device, context_.pipeline, nullptr);
            context_.pipeline = VK_NULL_HANDLE;
        }
        if (context_.pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(context_.device, context_.pipelineLayout, nullptr);
            context_.pipelineLayout = VK_NULL_HANDLE;
        }
        if (context_.renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(context_.device, context_.renderPass, nullptr);
            context_.renderPass = VK_NULL_HANDLE;
        }
        for (auto& imageView : context_.swapchainImageViews) {
            if (imageView != VK_NULL_HANDLE) vkDestroyImageView(context_.device, imageView, nullptr);
        }
        if (context_.swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(context_.device, context_.swapchain, nullptr);
            context_.swapchain = VK_NULL_HANDLE;
        }
        if (context_.sampler != VK_NULL_HANDLE) {
            vkDestroySampler(context_.device, context_.sampler, nullptr);
            context_.sampler = VK_NULL_HANDLE;
        }
        if (context_.descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(context_.device, context_.descriptorPool, nullptr);
            context_.descriptorPool = VK_NULL_HANDLE;
        }
        if (context_.descriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(context_.device, context_.descriptorSetLayout, nullptr);
            context_.descriptorSetLayout = VK_NULL_HANDLE;
        }
        if (context_.vertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(context_.device, context_.vertexBuffer, nullptr);
            context_.vertexBuffer = VK_NULL_HANDLE;
        }
        if (context_.vertexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(context_.device, context_.vertexBufferMemory, nullptr);
            context_.vertexBufferMemory = VK_NULL_HANDLE;
        }
        if (context_.indexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(context_.device, context_.indexBuffer, nullptr);
            context_.indexBuffer = VK_NULL_HANDLE;
        }
        if (context_.indexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(context_.device, context_.indexBufferMemory, nullptr);
            context_.indexBufferMemory = VK_NULL_HANDLE;
        }
        if (context_.vertShaderModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(context_.device, context_.vertShaderModule, nullptr);
            context_.vertShaderModule = VK_NULL_HANDLE;
        }
        if (context_.fragShaderModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(context_.device, context_.fragShaderModule, nullptr);
            context_.fragShaderModule = VK_NULL_HANDLE;
        }
        vkDestroyDevice(context_.device, nullptr);
        context_.device = VK_NULL_HANDLE;
    }
    // Note: Instance and surface not destroyed here; caller owns them
}