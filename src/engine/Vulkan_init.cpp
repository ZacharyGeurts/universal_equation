#include "engine/Vulkan_init.hpp"
#include <format>

VulkanRenderer::VulkanRenderer(
    VkInstance instance, VkSurfaceKHR surface,
    std::span<const glm::vec3> vertices, std::span<const uint32_t> indices,
    VkShaderModule vertShaderModule, VkShaderModule fragShaderModule,
    int width, int height,
    std::function<void(const std::string&)> logger)
    : instance_(instance), surface_(surface), logger_(logger) {
    if (instance_ == VK_NULL_HANDLE || surface_ == VK_NULL_HANDLE) {
        throw std::runtime_error("Vulkan instance or surface is null");
    }
    logger_(std::format("Initializing VulkanRenderer: width={}, height={}", width, height));
    initializeVulkan(vertices, indices, vertShaderModule, fragShaderModule, width, height);
}

VulkanRenderer::~VulkanRenderer() {
    cleanupVulkan();
}

void VulkanRenderer::initializeVulkan(
    std::span<const glm::vec3> vertices, std::span<const uint32_t> indices,
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

    // Create pipeline
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

    // Store shaders
    context_.vertShaderModule = vertShaderModule;
    context_.fragShaderModule = fragShaderModule;

    logger_(std::format("VulkanRenderer initialized successfully"));
}

void VulkanRenderer::initializeQuadBuffers(std::span<const glm::vec3> quadVertices, std::span<const uint32_t> quadIndices) {
    logger_(std::format("Initializing quad buffers: vertices={}, indices={}", quadVertices.size(), quadIndices.size()));

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

    logger_(std::format("Quad buffers initialized"));
}

void VulkanRenderer::initializeVoxelBuffers(std::span<const glm::vec3> voxelVertices, std::span<const uint32_t> voxelIndices) {
    logger_(std::format("Initializing voxel buffers: vertices={}, indices={}", voxelVertices.size(), voxelIndices.size()));

    // Temporary staging for voxel vertex
    VkBuffer voxelStagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory voxelStagingMemory = VK_NULL_HANDLE;
    VulkanInitializer::createVertexBuffer(context_.device, context_.physicalDevice, context_.commandPool, context_.graphicsQueue,
                                         context_.voxelVertexBuffer, context_.voxelVertexBufferMemory, voxelStagingBuffer, voxelStagingMemory, voxelVertices);
    vkDestroyBuffer(context_.device, voxelStagingBuffer, nullptr);
    vkFreeMemory(context_.device, voxelStagingMemory, nullptr);

    // Temporary staging for voxel index
    VkBuffer voxelIndexStagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory voxelIndexStagingMemory = VK_NULL_HANDLE;
    VulkanInitializer::createIndexBuffer(context_.device, context_.physicalDevice, context_.commandPool, context_.graphicsQueue,
                                        context_.voxelIndexBuffer, context_.voxelIndexBufferMemory, voxelIndexStagingBuffer, voxelIndexStagingMemory, voxelIndices);
    vkDestroyBuffer(context_.device, voxelIndexStagingBuffer, nullptr);
    vkFreeMemory(context_.device, voxelIndexStagingMemory, nullptr);

    logger_(std::format("Voxel buffers initialized"));
}

void VulkanRenderer::cleanupVulkan() {
    if (context_.device == VK_NULL_HANDLE) {
        logger_(std::format("No Vulkan device to clean up"));
        return;
    }

    logger_(std::format("Cleaning up Vulkan resources"));
    vkDeviceWaitIdle(context_.device);

    // Destroy quad buffers
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

    // Destroy voxel buffers
    if (context_.voxelVertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(context_.device, context_.voxelVertexBuffer, nullptr);
        context_.voxelVertexBuffer = VK_NULL_HANDLE;
    }
    if (context_.voxelVertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context_.device, context_.voxelVertexBufferMemory, nullptr);
        context_.voxelVertexBufferMemory = VK_NULL_HANDLE;
    }
    if (context_.voxelIndexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(context_.device, context_.voxelIndexBuffer, nullptr);
        context_.voxelIndexBuffer = VK_NULL_HANDLE;
    }
    if (context_.voxelIndexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context_.device, context_.voxelIndexBufferMemory, nullptr);
        context_.voxelIndexBufferMemory = VK_NULL_HANDLE;
    }

    // Destroy other resources
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

    logger_(std::format("Vulkan resources cleaned up"));
}