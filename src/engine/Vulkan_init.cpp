// Vulkan_init.cpp
#include "engine/Vulkan_init.hpp"
#include "engine/logging.hpp"
#include <fstream>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

VulkanSwapchainManager::VulkanSwapchainManager(VulkanContext& context, VkSurfaceKHR surface)
    : context_(context), swapchain_(VK_NULL_HANDLE), imageCount_(0), swapchainImageFormat_(VK_FORMAT_UNDEFINED), swapchainExtent_{0, 0} {
    context_.surface = surface;
    LOG_INFO_CAT("Vulkan", context_.surface, "surface", std::source_location::current());
    LOG_INFO("Initialized VulkanSwapchainManager");
}

VulkanSwapchainManager::~VulkanSwapchainManager() {
    cleanupSwapchain();
}

void VulkanSwapchainManager::initializeSwapchain(int width, int height) {
    VkSurfaceCapabilitiesKHR capabilities;
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context_.physicalDevice, context_.surface, &capabilities) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to get surface capabilities", std::source_location::current());
        return;
    }
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(context_.physicalDevice, context_.surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(context_.physicalDevice, context_.surface, &formatCount, formats.data());
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(context_.physicalDevice, context_.surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(context_.physicalDevice, context_.surface, &presentModeCount, presentModes.data());
    VkSurfaceFormatKHR surfaceFormat = formats[0];
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = format;
            break;
        }
    }
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (const auto& mode : presentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            presentMode = mode;
            break;
        }
    }
    VkExtent2D extent = capabilities.currentExtent.width == UINT32_MAX
        ? VkExtent2D{std::clamp(static_cast<uint32_t>(width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
                     std::clamp(static_cast<uint32_t>(height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height)}
        : capabilities.currentExtent;
    imageCount_ = std::min(capabilities.minImageCount + 1, capabilities.maxImageCount > 0 ? capabilities.maxImageCount : UINT32_MAX);
    VkSwapchainCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .surface = context_.surface,
        .minImageCount = imageCount_,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = swapchain_
    };
    if (vkCreateSwapchainKHR(context_.device, &createInfo, nullptr, &swapchain_) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create swapchain", std::source_location::current());
        return;
    }
    LOG_INFO_CAT("Vulkan", swapchain_, "swapchain", std::source_location::current());
    LOG_INFO_CAT("Vulkan", "Created swapchain format: {}, extent: {}x{}", std::source_location::current(), surfaceFormat.format, extent.width, extent.height);
    vkGetSwapchainImagesKHR(context_.device, swapchain_, &imageCount_, nullptr);
    swapchainImages_.resize(imageCount_);
    vkGetSwapchainImagesKHR(context_.device, swapchain_, &imageCount_, swapchainImages_.data());
    swapchainImageViews_.resize(imageCount_);
    for (uint32_t i = 0; i < imageCount_; ++i) {
        VkImageViewCreateInfo viewInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = swapchainImages_[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = surfaceFormat.format,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };
        if (vkCreateImageView(context_.device, &viewInfo, nullptr, &swapchainImageViews_[i]) != VK_SUCCESS) {
            LOG_ERROR_CAT("Vulkan", "Failed to create image view for image {}", std::source_location::current(), i);
            return;
        }
        LOG_INFO_CAT("Vulkan", swapchainImageViews_[i], "imageView", std::source_location::current());
        LOG_INFO_CAT("Vulkan", "Created image view for image {}", std::source_location::current(), i);
    }
    swapchainImageFormat_ = surfaceFormat.format;
    swapchainExtent_ = extent;
}

void VulkanSwapchainManager::cleanupSwapchain() {
    for (auto imageView : swapchainImageViews_) {
        LOG_INFO_CAT("Vulkan", imageView, "imageView", std::source_location::current());
        vkDestroyImageView(context_.device, imageView, nullptr);
    }
    if (swapchain_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", swapchain_, "swapchain", std::source_location::current());
        vkDestroySwapchainKHR(context_.device, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
    swapchainImages_.clear();
    swapchainImageViews_.clear();
}

void VulkanSwapchainManager::handleResize(int width, int height) {
    cleanupSwapchain();
    initializeSwapchain(width, height);
    LOG_INFO_CAT("Vulkan", "Resized swapchain to {}x{}", std::source_location::current(), width, height);
}

VulkanPipelineManager::VulkanPipelineManager(VulkanContext& context, int width, int height)
    : context_(context), width_(width), height_(height), renderPass_(VK_NULL_HANDLE), graphicsPipeline_(VK_NULL_HANDLE),
      pipelineLayout_(VK_NULL_HANDLE), descriptorSetLayout_(VK_NULL_HANDLE) {
    createRenderPass();
    createPipelineLayout();
    createGraphicsPipeline();
    LOG_INFO_CAT("Vulkan", "Initialized VulkanPipelineManager {}x{}", std::source_location::current(), width, height);
}

VulkanPipelineManager::~VulkanPipelineManager() {
    cleanupPipeline();
}

void VulkanPipelineManager::createRenderPass() {
    VulkanInitializer::createRenderPass(context_.device, renderPass_, context_.swapchainImageFormat_);
    LOG_INFO_CAT("Vulkan", renderPass_, "renderPass", std::source_location::current());
}

void VulkanPipelineManager::createPipelineLayout() {
    VulkanInitializer::createDescriptorSetLayout(context_.device, descriptorSetLayout_);
    VkPipelineLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = &descriptorSetLayout_,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr
    };
    if (vkCreatePipelineLayout(context_.device, &layoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create pipeline layout", std::source_location::current());
        return;
    }
    LOG_INFO_CAT("Vulkan", pipelineLayout_, "pipelineLayout", std::source_location::current());
}

void VulkanPipelineManager::createGraphicsPipeline() {
    vertexShaderModule_ = VulkanInitializer::loadShader(context_.device, "shaders/vert.spv");
    fragmentShaderModule_ = VulkanInitializer::loadShader(context_.device, "shaders/frag.spv");
    VulkanInitializer::createGraphicsPipeline(context_.device, renderPass_, graphicsPipeline_, pipelineLayout_,
                                             descriptorSetLayout_, width_, height_, vertexShaderModule_, fragmentShaderModule_);
    LOG_INFO_CAT("Vulkan", graphicsPipeline_, "graphicsPipeline", std::source_location::current());
}

void VulkanPipelineManager::cleanupPipeline() {
    if (graphicsPipeline_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", graphicsPipeline_, "graphicsPipeline", std::source_location::current());
        vkDestroyPipeline(context_.device, graphicsPipeline_, nullptr);
    }
    if (pipelineLayout_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", pipelineLayout_, "pipelineLayout", std::source_location::current());
        vkDestroyPipelineLayout(context_.device, pipelineLayout_, nullptr);
    }
    if (renderPass_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", renderPass_, "renderPass", std::source_location::current());
        vkDestroyRenderPass(context_.device, renderPass_, nullptr);
    }
    if (descriptorSetLayout_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", descriptorSetLayout_, "descriptorSetLayout", std::source_location::current());
        vkDestroyDescriptorSetLayout(context_.device, descriptorSetLayout_, nullptr);
    }
    if (vertexShaderModule_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", vertexShaderModule_, "vertexShaderModule", std::source_location::current());
        vkDestroyShaderModule(context_.device, vertexShaderModule_, nullptr);
    }
    if (fragmentShaderModule_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", fragmentShaderModule_, "fragmentShaderModule", std::source_location::current());
        vkDestroyShaderModule(context_.device, fragmentShaderModule_, nullptr);
    }
}

VulkanRenderer::VulkanRenderer(VkInstance instance, VkSurfaceKHR surface, std::span<const glm::vec3> vertices,
                              std::span<const uint32_t> indices, int width, int height)
    : width_(width), height_(height) {
    context_.instance = instance;
    context_.surface = surface;
    LOG_INFO_CAT("Vulkan", instance, "instance", std::source_location::current());
    LOG_INFO_CAT("Vulkan", surface, "surface", std::source_location::current());
    VulkanInitializer::initializeVulkan(context_, width, height);
    swapchainManager_ = std::make_unique<VulkanSwapchainManager>(context_, surface);
    swapchainManager_->initializeSwapchain(width, height);
    bufferManager_ = std::make_unique<VulkanBufferManager>(context_);
    bufferManager_->initializeBuffers(vertices, indices);
    context_.vertexBuffer_ = bufferManager_->getVertexBuffer();
    context_.indexBuffer_ = bufferManager_->getIndexBuffer();
    bufferManager_->createUniformBuffers(swapchainManager_->getSwapchainImageViews().size());
    pipelineManager_ = std::make_unique<VulkanPipelineManager>(context_, width, height);
    VulkanInitializer::createStorageImage(context_.device, context_.physicalDevice, context_.storageImage,
                                         context_.storageImageMemory, context_.storageImageView, width, height);
    VulkanInitializer::createAccelerationStructures(context_, vertices, indices);
    VulkanInitializer::createRayTracingPipeline(context_);
    VulkanInitializer::createShaderBindingTable(context_);
    VulkanInitializer::createDescriptorPoolAndSet(context_.device, pipelineManager_->getDescriptorSetLayout(),
                                                context_.descriptorPool, context_.descriptorSet, context_.sampler,
                                                bufferManager_->getUniformBuffer(0), context_.storageImageView, context_.topLevelAS);
    createFramebuffers();
    createCommandBuffers();
    createSyncObjects();
    LOG_INFO_CAT("Vulkan", "VulkanRenderer initialized {}x{}", std::source_location::current(), width, height);
}

VulkanRenderer::~VulkanRenderer() {
    vkDeviceWaitIdle(context_.device);
    LOG_INFO("Destroying VulkanRenderer");
    if (imageAvailableSemaphore_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", imageAvailableSemaphore_, "imageAvailableSemaphore", std::source_location::current());
        vkDestroySemaphore(context_.device, imageAvailableSemaphore_, nullptr);
    }
    if (renderFinishedSemaphore_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", renderFinishedSemaphore_, "renderFinishedSemaphore", std::source_location::current());
        vkDestroySemaphore(context_.device, renderFinishedSemaphore_, nullptr);
    }
    if (inFlightFence_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", inFlightFence_, "inFlightFence", std::source_location::current());
        vkDestroyFence(context_.device, inFlightFence_, nullptr);
    }
    for (auto framebuffer : framebuffers_) {
        LOG_INFO_CAT("Vulkan", framebuffer, "framebuffer", std::source_location::current());
        vkDestroyFramebuffer(context_.device, framebuffer, nullptr);
    }
    if (context_.commandPool != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", context_.commandPool, "commandPool", std::source_location::current());
        vkDestroyCommandPool(context_.device, context_.commandPool, nullptr);
    }
    if (context_.storageImageView != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", context_.storageImageView, "storageImageView", std::source_location::current());
        vkDestroyImageView(context_.device, context_.storageImageView, nullptr);
    }
    if (context_.storageImage != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", context_.storageImage, "storageImage", std::source_location::current());
        vkDestroyImage(context_.device, context_.storageImage, nullptr);
    }
    if (context_.storageImageMemory != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", context_.storageImageMemory, "storageImageMemory", std::source_location::current());
        vkFreeMemory(context_.device, context_.storageImageMemory, nullptr);
    }
    if (context_.sampler != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", context_.sampler, "sampler", std::source_location::current());
        vkDestroySampler(context_.device, context_.sampler, nullptr);
    }
    if (context_.descriptorPool != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", context_.descriptorPool, "descriptorPool", std::source_location::current());
        vkDestroyDescriptorPool(context_.device, context_.descriptorPool, nullptr);
    }
    auto vkDestroyAccelerationStructureKHR = (PFN_vkDestroyAccelerationStructureKHR)vkGetDeviceProcAddr(context_.device, "vkDestroyAccelerationStructureKHR");
    if (context_.topLevelAS != VK_NULL_HANDLE && vkDestroyAccelerationStructureKHR) {
        LOG_INFO_CAT("Vulkan", context_.topLevelAS, "topLevelAS", std::source_location::current());
        vkDestroyAccelerationStructureKHR(context_.device, context_.topLevelAS, nullptr);
    }
    if (context_.topLevelASBuffer != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", context_.topLevelASBuffer, "topLevelASBuffer", std::source_location::current());
        vkDestroyBuffer(context_.device, context_.topLevelASBuffer, nullptr);
    }
    if (context_.topLevelASBufferMemory != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", context_.topLevelASBufferMemory, "topLevelASBufferMemory", std::source_location::current());
        vkFreeMemory(context_.device, context_.topLevelASBufferMemory, nullptr);
    }
    if (context_.bottomLevelAS != VK_NULL_HANDLE && vkDestroyAccelerationStructureKHR) {
        LOG_INFO_CAT("Vulkan", context_.bottomLevelAS, "bottomLevelAS", std::source_location::current());
        vkDestroyAccelerationStructureKHR(context_.device, context_.bottomLevelAS, nullptr);
    }
    if (context_.bottomLevelASBuffer != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", context_.bottomLevelASBuffer, "bottomLevelASBuffer", std::source_location::current());
        vkDestroyBuffer(context_.device, context_.bottomLevelASBuffer, nullptr);
    }
    if (context_.bottomLevelASBufferMemory != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", context_.bottomLevelASBufferMemory, "bottomLevelASBufferMemory", std::source_location::current());
        vkFreeMemory(context_.device, context_.bottomLevelASBufferMemory, nullptr);
    }
    if (context_.rayTracingPipeline != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", context_.rayTracingPipeline, "rayTracingPipeline", std::source_location::current());
        vkDestroyPipeline(context_.device, context_.rayTracingPipeline, nullptr);
    }
    if (context_.rayTracingPipelineLayout != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", context_.rayTracingPipelineLayout, "rayTracingPipelineLayout", std::source_location::current());
        vkDestroyPipelineLayout(context_.device, context_.rayTracingPipelineLayout, nullptr);
    }
    if (context_.rayTracingDescriptorSetLayout != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", context_.rayTracingDescriptorSetLayout, "rayTracingDescriptorSetLayout", std::source_location::current());
        vkDestroyDescriptorSetLayout(context_.device, context_.rayTracingDescriptorSetLayout, nullptr);
    }
    if (context_.shaderBindingTable != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", context_.shaderBindingTable, "shaderBindingTable", std::source_location::current());
        vkDestroyBuffer(context_.device, context_.shaderBindingTable, nullptr);
    }
    if (context_.shaderBindingTableMemory != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", context_.shaderBindingTableMemory, "shaderBindingTableMemory", std::source_location::current());
        vkFreeMemory(context_.device, context_.shaderBindingTableMemory, nullptr);
    }
    if (context_.device != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", context_.device, "device", std::source_location::current());
        vkDestroyDevice(context_.device, nullptr);
    }
    if (context_.surface != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", context_.surface, "surface", std::source_location::current());
        vkDestroySurfaceKHR(context_.instance, context_.surface, nullptr);
    }
    if (context_.instance != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", context_.instance, "instance", std::source_location::current());
        vkDestroyInstance(context_.instance, nullptr);
    }
}

void VulkanRenderer::createFramebuffers() {
    framebuffers_.resize(swapchainManager_->getSwapchainImageViews().size());
    for (size_t i = 0; i < framebuffers_.size(); ++i) {
        VkFramebufferCreateInfo framebufferInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderPass = pipelineManager_->getRenderPass(),
            .attachmentCount = 1,
            .pAttachments = &swapchainManager_->getSwapchainImageViews()[i],
            .width = swapchainManager_->getSwapchainExtent().width,
            .height = swapchainManager_->getSwapchainExtent().height,
            .layers = 1
        };
        if (vkCreateFramebuffer(context_.device, &framebufferInfo, nullptr, &framebuffers_[i]) != VK_SUCCESS) {
            LOG_ERROR_CAT("Vulkan", "Failed to create framebuffer {}", std::source_location::current(), i);
            return;
        }
        LOG_INFO_CAT("Vulkan", framebuffers_[i], "framebuffer", std::source_location::current());
        LOG_INFO_CAT("Vulkan", "Created framebuffer for image {}", std::source_location::current(), i);
    }
}

void VulkanRenderer::createCommandBuffers() {
    commandBuffers_.resize(swapchainManager_->getSwapchainImageViews().size());
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = context_.commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<uint32_t>(commandBuffers_.size())
    };
    if (vkAllocateCommandBuffers(context_.device, &allocInfo, commandBuffers_.data()) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to allocate command buffers", std::source_location::current());
        return;
    }
    LOG_INFO_CAT("Vulkan", "Allocated {} command buffers", std::source_location::current(), commandBuffers_.size());
}

void VulkanRenderer::createSyncObjects() {
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
    if (vkCreateSemaphore(context_.device, &semaphoreInfo, nullptr, &imageAvailableSemaphore_) != VK_SUCCESS ||
        vkCreateSemaphore(context_.device, &semaphoreInfo, nullptr, &renderFinishedSemaphore_) != VK_SUCCESS ||
        vkCreateFence(context_.device, &fenceInfo, nullptr, &inFlightFence_) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create sync objects", std::source_location::current());
        return;
    }
    LOG_INFO_CAT("Vulkan", imageAvailableSemaphore_, "imageAvailableSemaphore", std::source_location::current());
    LOG_INFO_CAT("Vulkan", renderFinishedSemaphore_, "renderFinishedSemaphore", std::source_location::current());
    LOG_INFO_CAT("Vulkan", inFlightFence_, "inFlightFence", std::source_location::current());
}

void VulkanRenderer::renderFrame(const AMOURANTH& camera) {
    vkWaitForFences(context_.device, 1, &inFlightFence_, VK_TRUE, UINT64_MAX);
    vkResetFences(context_.device, 1, &inFlightFence_);
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(context_.device, swapchainManager_->getSwapchain(), UINT64_MAX,
                                            imageAvailableSemaphore_, VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        LOG_INFO_CAT("Vulkan", "Swapchain out of date, resizing", std::source_location::current());
        swapchainManager_->handleResize(width_, height_);
        return;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        LOG_ERROR_CAT("Vulkan", "Failed to acquire swapchain image: {}", std::source_location::current(), result);
        return;
    }
    UE::UniformBufferObject ubo{
        .model = glm::mat4(1.0f),
        .view = camera.getViewMatrix(),
        .proj = camera.getProjectionMatrix()
    };
    void* data;
    vkMapMemory(context_.device, bufferManager_->getUniformBufferMemory(), 0, sizeof(UE::UniformBufferObject), 0, &data);
    memcpy(data, &ubo, sizeof(UE::UniformBufferObject));
    vkUnmapMemory(context_.device, bufferManager_->getUniformBufferMemory());
    LOG_DEBUG_CAT("Vulkan", ubo.view, "Updated uniform buffer", std::source_location::current());
    VkCommandBuffer commandBuffer = commandBuffers_[imageIndex];
    vkResetCommandBuffer(commandBuffer, 0);
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pInheritanceInfo = nullptr
    };
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", commandBuffer, "commandBuffer", std::source_location::current());
        return;
    }
    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = pipelineManager_->getRenderPass(),
        .framebuffer = framebuffers_[imageIndex],
        .renderArea = {
            .offset = {0, 0},
            .extent = {static_cast<uint32_t>(width_), static_cast<uint32_t>(height_)}
        },
        .clearValueCount = 1,
        .pClearValues = &clearColor
    };
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineManager_->getGraphicsPipeline());
    VkBuffer vertexBuffers[] = {bufferManager_->getVertexBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, bufferManager_->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineManager_->getPipelineLayout(),
                            0, 1, &context_.descriptorSet, 0, nullptr);
    vkCmdDrawIndexed(commandBuffer, bufferManager_->getIndexCount(), 1, 0, 0, 0);
    vkCmdEndRenderPass(commandBuffer);
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", commandBuffer, "commandBuffer", std::source_location::current());
        return;
    }
    VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &imageAvailableSemaphore_,
        .pWaitDstStageMask = &waitStages,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &renderFinishedSemaphore_
    };
    if (vkQueueSubmit(context_.graphicsQueue, 1, &submitInfo, inFlightFence_) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", context_.graphicsQueue, "graphicsQueue", std::source_location::current());
        return;
    }
    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &renderFinishedSemaphore_,
        .swapchainCount = 1,
        .pSwapchains = &context_.swapchain,
        .pImageIndices = &imageIndex,
        .pResults = nullptr
    };
    result = vkQueuePresentKHR(context_.presentQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        LOG_INFO_CAT("Vulkan", "Presenting swapchain suboptimal, resizing", std::source_location::current());
        swapchainManager_->handleResize(width_, height_);
    } else if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to present swapchain image: {}", std::source_location::current(), result);
        return;
    }
}

void VulkanRenderer::handleResize(int width, int height) {
    width_ = width;
    height_ = height;
    swapchainManager_->handleResize(width, height);
    for (auto framebuffer : framebuffers_) {
        LOG_INFO_CAT("Vulkan", framebuffer, "framebuffer", std::source_location::current());
        vkDestroyFramebuffer(context_.device, framebuffer, nullptr);
    }
    createFramebuffers();
    createCommandBuffers();
    LOG_INFO_CAT("Vulkan", "Handled resize to {}x{}", std::source_location::current(), width, height);
}

void VulkanInitializer::initializeVulkan(VulkanContext& context, int /*width*/, int /*height*/) {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(context.instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        LOG_ERROR_CAT("Vulkan", "No physical devices found", std::source_location::current());
        return;
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(context.instance, &deviceCount, devices.data());
    context.physicalDevice = devices[0];
    LOG_INFO_CAT("Vulkan", context.physicalDevice, "physicalDevice", std::source_location::current());
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(context.physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(context.physicalDevice, &queueFamilyCount, queueFamilies.data());
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) context.graphicsQueueFamilyIndex = i;
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(context.physicalDevice, i, context.surface, &presentSupport);
        if (presentSupport) context.presentQueueFamilyIndex = i;
    }
    LOG_INFO_CAT("Vulkan", "Graphics queue family: {}, present: {}", std::source_location::current(),
                 context.graphicsQueueFamilyIndex, context.presentQueueFamilyIndex);
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfos[] = {
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = context.graphicsQueueFamilyIndex,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority
        },
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = context.presentQueueFamilyIndex,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority
        }
    };
    VkPhysicalDeviceFeatures deviceFeatures{};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
        .pNext = nullptr,
        .accelerationStructure = VK_TRUE,
        .accelerationStructureCaptureReplay = VK_FALSE,
        .accelerationStructureIndirectBuild = VK_FALSE,
        .accelerationStructureHostCommands = VK_FALSE,
        .descriptorBindingAccelerationStructureUpdateAfterBind = VK_FALSE
    };
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
        .pNext = &accelFeatures,
        .rayTracingPipeline = VK_TRUE,
        .rayTracingPipelineShaderGroupHandleCaptureReplay = VK_FALSE,
        .rayTracingPipelineShaderGroupHandleCaptureReplayMixed = VK_FALSE,
        .rayTracingPipelineTraceRaysIndirect = VK_FALSE,
        .rayTraversalPrimitiveCulling = VK_FALSE
    };
    const char* deviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
    };
    VkDeviceCreateInfo deviceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &rtPipelineFeatures,
        .flags = 0,
        .queueCreateInfoCount = static_cast<uint32_t>(context.graphicsQueueFamilyIndex == context.presentQueueFamilyIndex ? 1 : 2),
        .pQueueCreateInfos = queueCreateInfos,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = 4,
        .ppEnabledExtensionNames = deviceExtensions,
        .pEnabledFeatures = &deviceFeatures
    };
    if (vkCreateDevice(context.physicalDevice, &deviceCreateInfo, nullptr, &context.device) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create logical device", std::source_location::current());
        return;
    }
    LOG_INFO_CAT("Vulkan", context.device, "device", std::source_location::current());
    vkGetDeviceQueue(context.device, context.graphicsQueueFamilyIndex, 0, &context.graphicsQueue);
    vkGetDeviceQueue(context.device, context.presentQueueFamilyIndex, 0, &context.presentQueue);
    LOG_INFO_CAT("Vulkan", context.graphicsQueue, "graphicsQueue", std::source_location::current());
    LOG_INFO_CAT("Vulkan", context.presentQueue, "presentQueue", std::source_location::current());
    VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = context.graphicsQueueFamilyIndex
    };
    if (vkCreateCommandPool(context.device, &poolInfo, nullptr, &context.commandPool) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create command pool", std::source_location::current());
        return;
    }
    LOG_INFO_CAT("Vulkan", context.commandPool, "commandPool", std::source_location::current());
    vkGetPhysicalDeviceMemoryProperties(context.physicalDevice, &context.memoryProperties);
}

void VulkanInitializer::createSwapchain(VulkanContext& context) {
    VkSurfaceCapabilitiesKHR capabilities;
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context.physicalDevice, context.surface, &capabilities) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to get surface capabilities", std::source_location::current());
        return;
    }
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(context.physicalDevice, context.surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(context.physicalDevice, context.surface, &formatCount, formats.data());
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(context.physicalDevice, context.surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(context.physicalDevice, context.surface, &presentModeCount, presentModes.data());
    VkSurfaceFormatKHR surfaceFormat = formats[0];
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = format;
            break;
        }
    }
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (const auto& mode : presentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            presentMode = mode;
            break;
        }
    }
    VkExtent2D extent = capabilities.currentExtent.width == UINT32_MAX
        ? VkExtent2D{std::clamp(800u, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
                     std::clamp(600u, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)}
        : capabilities.currentExtent;
    uint32_t imageCount = std::min(capabilities.minImageCount + 1, capabilities.maxImageCount > 0 ? capabilities.maxImageCount : UINT32_MAX);
    VkSwapchainCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .surface = context.surface,
        .minImageCount = imageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE
    };
    if (vkCreateSwapchainKHR(context.device, &createInfo, nullptr, &context.swapchain) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create swapchain", std::source_location::current());
        return;
    }
    LOG_INFO_CAT("Vulkan", context.swapchain, "swapchain", std::source_location::current());
    LOG_INFO_CAT("Vulkan", "Created swapchain format: {}, extent: {}x{}", std::source_location::current(),
                 surfaceFormat.format, extent.width, extent.height);
    context.swapchainImageFormat_ = surfaceFormat.format;
    context.swapchainExtent_ = extent;
}

void VulkanInitializer::createImageViews(VulkanContext& context) {
    uint32_t imageCount;
    vkGetSwapchainImagesKHR(context.device, context.swapchain, &imageCount, nullptr);
    context.swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(context.device, context.swapchain, &imageCount, context.swapchainImages.data());
    context.swapchainImageViews.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; ++i) {
        VkImageViewCreateInfo viewInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = context.swapchainImages[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = context.swapchainImageFormat_,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };
        if (vkCreateImageView(context.device, &viewInfo, nullptr, &context.swapchainImageViews[i]) != VK_SUCCESS) {
            LOG_ERROR_CAT("Vulkan", "Failed to create image view for image {}", std::source_location::current(), i);
            return;
        }
        LOG_INFO_CAT("Vulkan", context.swapchainImageViews[i], "imageView", std::source_location::current());
        LOG_INFO_CAT("Vulkan", "Created image view for image {}", std::source_location::current(), i);
    }
}

VkDeviceAddress VulkanInitializer::getBufferDeviceAddress(VkDevice device, VkBuffer buffer) {
    auto vkGetBufferDeviceAddressKHR = (PFN_vkGetBufferDeviceAddressKHR)vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddressKHR");
    if (!vkGetBufferDeviceAddressKHR) {
        LOG_ERROR_CAT("Vulkan", "Failed to get vkGetBufferDeviceAddressKHR", std::source_location::current());
        return 0;
    }
    VkBufferDeviceAddressInfo addressInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = nullptr,
        .buffer = buffer
    };
    VkDeviceAddress address = vkGetBufferDeviceAddressKHR(device, &addressInfo);
    LOG_DEBUG_CAT("Vulkan", buffer, "buffer", std::source_location::current());
    return address;
}

uint32_t VulkanInitializer::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            LOG_DEBUG_CAT("Vulkan", "Memory type index: {}", std::source_location::current(), i);
            return i;
        }
    }
    LOG_ERROR_CAT("Vulkan", "Failed to find memory type filter: {}, properties: {}", std::source_location::current(), typeFilter, properties);
    return UINT32_MAX;
}

void VulkanInitializer::createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size,
                                    VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                                    VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
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
    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create buffer size: {}", std::source_location::current(), size);
        return;
    }
    LOG_INFO_CAT("Vulkan", buffer, "buffer", std::source_location::current());
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);
    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties)
    };
    if (allocInfo.memoryTypeIndex == UINT32_MAX || vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to allocate buffer memory size: {}", std::source_location::current(), memRequirements.size);
        return;
    }
    LOG_INFO_CAT("Vulkan", bufferMemory, "bufferMemory", std::source_location::current());
    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

VkShaderModule VulkanInitializer::loadShader(VkDevice device, const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file) {
        LOG_ERROR_CAT("Vulkan", "Failed to open shader: {}", std::source_location::current(), filepath);
        return VK_NULL_HANDLE;
    }
    size_t size = file.tellg();
    std::vector<char> buffer(size);
    file.seekg(0);
    file.read(buffer.data(), size);
    file.close();
    VkShaderModuleCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .codeSize = size,
        .pCode = reinterpret_cast<const uint32_t*>(buffer.data())
    };
    VkShaderModule module;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &module) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create shader module: {}", std::source_location::current(), filepath);
        return VK_NULL_HANDLE;
    }
    LOG_INFO_CAT("Vulkan", module, "shaderModule", std::source_location::current());
    return module;
}

void VulkanInitializer::createRenderPass(VkDevice device, VkRenderPass& renderPass, VkFormat format) {
    VkAttachmentDescription colorAttachment{
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
    VkAttachmentReference colorAttachmentRef{
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };
    VkSubpassDescription subpass{
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
    VkRenderPassCreateInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .attachmentCount = 1,
        .pAttachments = &colorAttachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 0,
        .pDependencies = nullptr
    };
    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create render pass", std::source_location::current());
        return;
    }
    LOG_INFO_CAT("Vulkan", renderPass, "renderPass", std::source_location::current());
}

void VulkanInitializer::createDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout& descriptorSetLayout) {
    VkDescriptorSetLayoutBinding bindings[] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR,
            .pImmutableSamplers = nullptr
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR,
            .pImmutableSamplers = nullptr
        },
        {
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
            .pImmutableSamplers = nullptr
        }
    };
    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = 3,
        .pBindings = bindings
    };
    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create descriptor set layout", std::source_location::current());
        return;
    }
    LOG_INFO_CAT("Vulkan", descriptorSetLayout, "descriptorSetLayout", std::source_location::current());
}

void VulkanInitializer::createGraphicsPipeline(VkDevice device, VkRenderPass renderPass, VkPipeline& pipeline,
                                              VkPipelineLayout& pipelineLayout, VkDescriptorSetLayout& descriptorSetLayout,
                                              int width, int height, VkShaderModule& vertexShaderModule,
                                              VkShaderModule& fragmentShaderModule) {
    VkPipelineShaderStageCreateInfo shaderStages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertexShaderModule,
            .pName = "main",
            .pSpecializationInfo = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragmentShaderModule,
            .pName = "main",
            .pSpecializationInfo = nullptr
        }
    };
    VkVertexInputBindingDescription bindingDescription{
        .binding = 0,
        .stride = sizeof(glm::vec3),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };
    VkVertexInputAttributeDescription attributeDescription{
        .location = 0,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = 0
    };
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindingDescription,
        .vertexAttributeDescriptionCount = 1,
        .pVertexAttributeDescriptions = &attributeDescription
    };
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };
    VkViewport viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(width),
        .height = static_cast<float>(height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    VkRect2D scissor{
        .offset = {0, 0},
        .extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}
    };
    VkPipelineViewportStateCreateInfo viewportState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor
    };
    VkPipelineRasterizationStateCreateInfo rasterizer{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasClamp = 0.0f,
        .depthBiasSlopeFactor = 0.0f,
        .lineWidth = 1.0f
    };
    VkPipelineMultisampleStateCreateInfo multisampling{
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
    VkPipelineColorBlendAttachmentState colorBlendAttachment{
        .blendEnable = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };
    VkPipelineColorBlendStateCreateInfo colorBlending{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment,
        .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f}
    };
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = &descriptorSetLayout,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr
    };
    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create pipeline layout", std::source_location::current());
        return;
    }
    LOG_INFO_CAT("Vulkan", pipelineLayout, "pipelineLayout", std::source_location::current());
    VkGraphicsPipelineCreateInfo pipelineInfo{
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
        .pDepthStencilState = nullptr,
        .pColorBlendState = &colorBlending,
        .pDynamicState = nullptr,
        .layout = pipelineLayout,
        .renderPass = renderPass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create graphics pipeline", std::source_location::current());
        return;
    }
    LOG_INFO_CAT("Vulkan", pipeline, "pipeline", std::source_location::current());
}

void VulkanInitializer::createDescriptorPoolAndSet(VkDevice device, VkDescriptorSetLayout descriptorSetLayout,
                                                  VkDescriptorPool& descriptorPool, VkDescriptorSet& descriptorSet,
                                                  VkSampler& sampler, VkBuffer uniformBuffer, VkImageView storageImageView,
                                                  VkAccelerationStructureKHR topLevelAS) {
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
        {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1}
    };
    VkDescriptorPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .maxSets = 1,
        .poolSizeCount = 3,
        .pPoolSizes = poolSizes
    };
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create descriptor pool", std::source_location::current());
        return;
    }
    LOG_INFO_CAT("Vulkan", descriptorPool, "descriptorPool", std::source_location::current());
    VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &descriptorSetLayout
    };
    if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to allocate descriptor set", std::source_location::current());
        return;
    }
    LOG_INFO_CAT("Vulkan", descriptorSet, "descriptorSet", std::source_location::current());
    VkSamplerCreateInfo samplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0f,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE
    };
    if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create sampler", std::source_location::current());
        return;
    }
    LOG_INFO_CAT("Vulkan", sampler, "sampler", std::source_location::current());
    VkDescriptorBufferInfo bufferInfo{
        .buffer = uniformBuffer,
        .offset = 0,
        .range = sizeof(UE::UniformBufferObject)
    };
    VkDescriptorImageInfo imageInfo{
        .sampler = sampler,
        .imageView = storageImageView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };
    VkWriteDescriptorSetAccelerationStructureKHR accelInfo{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .pNext = nullptr,
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &topLevelAS
    };
    VkWriteDescriptorSet descriptorWrites[] = {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = descriptorSet,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pImageInfo = nullptr,
            .pBufferInfo = &bufferInfo,
            .pTexelBufferView = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = descriptorSet,
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &imageInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = &accelInfo,
            .dstSet = descriptorSet,
            .dstBinding = 2,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            .pImageInfo = nullptr,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        }
    };
    vkUpdateDescriptorSets(device, 3, descriptorWrites, 0, nullptr);
    LOG_INFO_CAT("Vulkan", descriptorSet, "descriptorSet", std::source_location::current());
}

void VulkanInitializer::createStorageImage(VkDevice device, VkPhysicalDevice physicalDevice, VkImage& storageImage,
                                          VkDeviceMemory& storageImageMemory, VkImageView& storageImageView,
                                          uint32_t width, uint32_t height) {
    VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = {width, height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    if (vkCreateImage(device, &imageInfo, nullptr, &storageImage) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create storage image", std::source_location::current());
        return;
    }
    LOG_INFO_CAT("Vulkan", storageImage, "storageImage", std::source_location::current());
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, storageImage, &memRequirements);
    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    if (allocInfo.memoryTypeIndex == UINT32_MAX || vkAllocateMemory(device, &allocInfo, nullptr, &storageImageMemory) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to allocate storage image memory", std::source_location::current());
        return;
    }
    LOG_INFO_CAT("Vulkan", storageImageMemory, "storageImageMemory", std::source_location::current());
    vkBindImageMemory(device, storageImage, storageImageMemory, 0);
    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = storageImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY
        },
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };
    if (vkCreateImageView(device, &viewInfo, nullptr, &storageImageView) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create storage image view", std::source_location::current());
        return;
    }
    LOG_INFO_CAT("Vulkan", storageImageView, "storageImageView", std::source_location::current());
}

void VulkanInitializer::createAccelerationStructures(VulkanContext& context, std::span<const glm::vec3> vertices, std::span<const uint32_t> indices) {
    auto vkCreateAccelerationStructureKHR = (PFN_vkCreateAccelerationStructureKHR)vkGetDeviceProcAddr(context.device, "vkCreateAccelerationStructureKHR");
    auto vkGetAccelerationStructureBuildSizesKHR = (PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetDeviceProcAddr(context.device, "vkGetAccelerationStructureBuildSizesKHR");
    auto vkCmdBuildAccelerationStructuresKHR = (PFN_vkCmdBuildAccelerationStructuresKHR)vkGetDeviceProcAddr(context.device, "vkCmdBuildAccelerationStructuresKHR");
    if (!vkCreateAccelerationStructureKHR || !vkGetAccelerationStructureBuildSizesKHR || !vkCmdBuildAccelerationStructuresKHR) {
        LOG_ERROR_CAT("Vulkan", "Failed to load acceleration structure functions", std::source_location::current());
        return;
    }
    LOG_INFO("Loaded acceleration structure functions");
    VkAccelerationStructureGeometryTrianglesDataKHR triangles{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .pNext = nullptr,
        .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
        .vertexData = {.deviceAddress = getBufferDeviceAddress(context.device, context.vertexBuffer_)},
        .vertexStride = sizeof(glm::vec3),
        .maxVertex = static_cast<uint32_t>(vertices.size()),
        .indexType = VK_INDEX_TYPE_UINT32,
        .indexData = {.deviceAddress = getBufferDeviceAddress(context.device, context.indexBuffer_)},
        .transformData = {.deviceAddress = 0}
    };
    VkAccelerationStructureGeometryKHR geometry{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .pNext = nullptr,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry = {.triangles = triangles},
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
    };
    VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .pNext = nullptr,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = 0,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .srcAccelerationStructure = VK_NULL_HANDLE,
        .dstAccelerationStructure = VK_NULL_HANDLE,
        .geometryCount = 1,
        .pGeometries = &geometry,
        .ppGeometries = nullptr,
        .scratchData = {.deviceAddress = 0}
    };
    VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
        .pNext = nullptr,
        .accelerationStructureSize = 0,
        .updateScratchSize = 0,
        .buildScratchSize = 0
    };
    uint32_t primitiveCount = static_cast<uint32_t>(indices.size() / 3);
    vkGetAccelerationStructureBuildSizesKHR(context.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildGeometryInfo, &primitiveCount, &buildSizesInfo);
    LOG_INFO_CAT("Vulkan", "BLAS sizes: {} build: {}", std::source_location::current(), buildSizesInfo.accelerationStructureSize, buildSizesInfo.buildScratchSize);
    createBuffer(context.device, context.physicalDevice, buildSizesInfo.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, context.bottomLevelASBuffer, context.bottomLevelASBufferMemory);
    LOG_INFO_CAT("Vulkan", context.bottomLevelASBuffer, "bottomLevelASBuffer", std::source_location::current());
    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .createFlags = 0,
        .buffer = context.bottomLevelASBuffer,
        .offset = 0,
        .size = buildSizesInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .deviceAddress = 0
    };
    if (vkCreateAccelerationStructureKHR(context.device, &createInfo, nullptr, &context.bottomLevelAS) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create BLAS", std::source_location::current());
        return;
    }
    LOG_INFO_CAT("Vulkan", context.bottomLevelAS, "bottomLevelAS", std::source_location::current());
    VkBuffer scratchBuffer;
    VkDeviceMemory scratchBufferMemory;
    createBuffer(context.device, context.physicalDevice, buildSizesInfo.buildScratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer, scratchBufferMemory);
    LOG_INFO_CAT("Vulkan", scratchBuffer, "scratchBuffer", std::source_location::current());
    buildGeometryInfo.dstAccelerationStructure = context.bottomLevelAS;
    buildGeometryInfo.scratchData.deviceAddress = getBufferDeviceAddress(context.device, scratchBuffer);
    VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{
        .primitiveCount = primitiveCount,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0
    };
    VkAccelerationStructureBuildRangeInfoKHR* buildRangeInfos[] = {&buildRangeInfo};
    VkCommandBuffer commandBuffer;
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = context.commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    if (vkAllocateCommandBuffers(context.device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to allocate BLAS command buffer", std::source_location::current());
        return;
    }
    LOG_INFO_CAT("Vulkan", commandBuffer, "commandBuffer", std::source_location::current());
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to begin BLAS command buffer", std::source_location::current());
        return;
    }
    vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildGeometryInfo, buildRangeInfos);
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to end BLAS command buffer", std::source_location::current());
        return;
    }
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
    if (vkQueueSubmit(context.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to submit BLAS command buffer", std::source_location::current());
        return;
    }
    vkQueueWaitIdle(context.graphicsQueue);
    LOG_INFO("Submitted BLAS build commands");
    vkFreeCommandBuffers(context.device, context.commandPool, 1, &commandBuffer);
    vkDestroyBuffer(context.device, scratchBuffer, nullptr);
    vkFreeMemory(context.device, scratchBufferMemory, nullptr);
    LOG_INFO_CAT("Vulkan", scratchBuffer, "scratchBuffer", std::source_location::current());
    VkTransformMatrixKHR transformMatrix = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f
    };
    VkAccelerationStructureInstanceKHR instance{
        .transform = transformMatrix,
        .instanceCustomIndex = 0,
        .mask = 0xFF,
        .instanceShaderBindingTableRecordOffset = 0,
        .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
        .accelerationStructureReference = getBufferDeviceAddress(context.device, context.bottomLevelASBuffer)
    };
    VkBuffer instanceBuffer;
    VkDeviceMemory instanceBufferMemory;
    createBuffer(context.device, context.physicalDevice, sizeof(VkAccelerationStructureInstanceKHR),
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, instanceBuffer, instanceBufferMemory);
    LOG_INFO_CAT("Vulkan", instanceBuffer, "instanceBuffer", std::source_location::current());
    void* data;
    vkMapMemory(context.device, instanceBufferMemory, 0, sizeof(VkAccelerationStructureInstanceKHR), 0, &data);
    memcpy(data, &instance, sizeof(VkAccelerationStructureInstanceKHR));
    vkUnmapMemory(context.device, instanceBufferMemory);
    VkAccelerationStructureGeometryInstancesDataKHR instancesData{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        .pNext = nullptr,
        .arrayOfPointers = VK_FALSE,
        .data = {.deviceAddress = getBufferDeviceAddress(context.device, instanceBuffer)}
    };
    VkAccelerationStructureGeometryKHR topLevelGeometry{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .pNext = nullptr,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry = {.instances = instancesData},
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
    };
    VkAccelerationStructureBuildGeometryInfoKHR topLevelBuildInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .pNext = nullptr,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = 0,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .srcAccelerationStructure = VK_NULL_HANDLE,
        .dstAccelerationStructure = VK_NULL_HANDLE,
        .geometryCount = 1,
        .pGeometries = &topLevelGeometry,
        .ppGeometries = nullptr,
        .scratchData = {.deviceAddress = 0}
    };
    uint32_t topLevelPrimitiveCount = 1;
    vkGetAccelerationStructureBuildSizesKHR(context.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &topLevelBuildInfo, &topLevelPrimitiveCount, &buildSizesInfo);
    LOG_INFO_CAT("Vulkan", "TLAS sizes: {} build: {}", std::source_location::current(), buildSizesInfo.accelerationStructureSize, buildSizesInfo.buildScratchSize);
    createBuffer(context.device, context.physicalDevice, buildSizesInfo.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, context.topLevelASBuffer, context.topLevelASBufferMemory);
    LOG_INFO_CAT("Vulkan", context.topLevelASBuffer, "topLevelASBuffer", std::source_location::current());
    VkAccelerationStructureCreateInfoKHR topLevelCreateInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = context.topLevelASBuffer,
        .size = buildSizesInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
    };
    if (vkCreateAccelerationStructureKHR(context.device, &topLevelCreateInfo, nullptr, &context.topLevelAS) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create TLAS", std::source_location::current());
        return;
    }
    LOG_INFO_CAT("Vulkan", context.topLevelAS, "topLevelAS", std::source_location::current());
    VkBuffer topLevelScratchBuffer;
    VkDeviceMemory topLevelScratchBufferMemory;
    createBuffer(context.device, context.physicalDevice, buildSizesInfo.buildScratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, topLevelScratchBuffer, topLevelScratchBufferMemory);
    LOG_INFO_CAT("Vulkan", topLevelScratchBuffer, "topLevelScratchBuffer", std::source_location::current());
    topLevelBuildInfo.dstAccelerationStructure = context.topLevelAS;
    topLevelBuildInfo.scratchData.deviceAddress = getBufferDeviceAddress(context.device, topLevelScratchBuffer);
    VkAccelerationStructureBuildRangeInfoKHR topLevelBuildRangeInfo{.primitiveCount = topLevelPrimitiveCount};
    VkAccelerationStructureBuildRangeInfoKHR* topLevelBuildRangeInfos[] = {&topLevelBuildRangeInfo};
    VkCommandBuffer topLevelCommandBuffer;
    VkCommandBufferAllocateInfo topLevelAllocInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = context.commandPool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1};
    if (vkAllocateCommandBuffers(context.device, &topLevelAllocInfo, &topLevelCommandBuffer) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to allocate TLAS command buffer", std::source_location::current());
        return;
    }
    LOG_INFO_CAT("Vulkan", topLevelCommandBuffer, "topLevelCommandBuffer", std::source_location::current());
    VkCommandBufferBeginInfo topLevelBeginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    if (vkBeginCommandBuffer(topLevelCommandBuffer, &topLevelBeginInfo) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to begin TLAS command buffer", std::source_location::current());
        return;
    }
    vkCmdBuildAccelerationStructuresKHR(topLevelCommandBuffer, 1, &topLevelBuildInfo, topLevelBuildRangeInfos);
    if (vkEndCommandBuffer(topLevelCommandBuffer) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to end TLAS command buffer", std::source_location::current());
        return;
    }
    VkSubmitInfo topLevelSubmitInfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &topLevelCommandBuffer};
    if (vkQueueSubmit(context.graphicsQueue, 1, &topLevelSubmitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to submit TLAS command buffer", std::source_location::current());
        return;
    }
    vkQueueWaitIdle(context.graphicsQueue);
    LOG_INFO("Submitted TLAS build commands");
    vkFreeCommandBuffers(context.device, context.commandPool, 1, &topLevelCommandBuffer);
    vkDestroyBuffer(context.device, topLevelScratchBuffer, nullptr);
    vkFreeMemory(context.device, topLevelScratchBufferMemory, nullptr);
    LOG_INFO_CAT("Vulkan", topLevelScratchBuffer, "topLevelScratchBuffer", std::source_location::current());
    vkDestroyBuffer(context.device, instanceBuffer, nullptr);
    vkFreeMemory(context.device, instanceBufferMemory, nullptr);
    LOG_INFO_CAT("Vulkan", instanceBuffer, "instanceBuffer", std::source_location::current());
}

void VulkanInitializer::createRayTracingPipeline(VulkanContext& context) {
    auto vkCreateRayTracingPipelinesKHR = (PFN_vkCreateRayTracingPipelinesKHR)vkGetDeviceProcAddr(context.device, "vkCreateRayTracingPipelinesKHR");
    if (!vkCreateRayTracingPipelinesKHR) {
        LOG_ERROR_CAT("Vulkan", "Failed to load vkCreateRayTracingPipelinesKHR", std::source_location::current());
        return;
    }
    VkShaderModule raygenShader = loadShader(context.device, "shaders/raygen.rgen.spv");
    VkShaderModule missShader = loadShader(context.device, "shaders/miss.rmiss.spv");
    VkShaderModule closestHitShader = loadShader(context.device, "shaders/closesthit.rchit.spv");
    VkPipelineShaderStageCreateInfo shaderStages[] = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR, .module = raygenShader, .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_MISS_BIT_KHR, .module = missShader, .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, .module = closestHitShader, .pName = "main"}
    };
    VkRayTracingShaderGroupCreateInfoKHR shaderGroups[] = {
        {.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, .generalShader = 0},
        {.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, .generalShader = 1},
        {.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR, .closestHitShader = 2}
    };
    VkDescriptorSetLayoutBinding bindings[] = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {2, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR}
    };
    VkDescriptorSetLayoutCreateInfo layoutInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 3, .pBindings = bindings};
    if (vkCreateDescriptorSetLayout(context.device, &layoutInfo, nullptr, &context.rayTracingDescriptorSetLayout) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create ray-tracing descriptor set layout", std::source_location::current());
        return;
    }
    LOG_INFO_CAT("Vulkan", context.rayTracingDescriptorSetLayout, "rayTracingDescriptorSetLayout", std::source_location::current());
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 1, .pSetLayouts = &context.rayTracingDescriptorSetLayout};
    if (vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr, &context.rayTracingPipelineLayout) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create ray-tracing pipeline layout", std::source_location::current());
        return;
    }
    LOG_INFO_CAT("Vulkan", context.rayTracingPipelineLayout, "rayTracingPipelineLayout", std::source_location::current());
    VkRayTracingPipelineCreateInfoKHR pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .stageCount = 3,
        .pStages = shaderStages,
        .groupCount = 3,
        .pGroups = shaderGroups,
        .maxPipelineRayRecursionDepth = 1,
        .layout = context.rayTracingPipelineLayout
    };
    if (vkCreateRayTracingPipelinesKHR(context.device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &context.rayTracingPipeline) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create ray-tracing pipeline", std::source_location::current());
        return;
    }
    LOG_INFO_CAT("Vulkan", context.rayTracingPipeline, "rayTracingPipeline", std::source_location::current());
    vkDestroyShaderModule(context.device, raygenShader, nullptr);
    vkDestroyShaderModule(context.device, missShader, nullptr);
    vkDestroyShaderModule(context.device, closestHitShader, nullptr);
    LOG_INFO("Destroyed ray-tracing shader modules");
}

void VulkanInitializer::createShaderBindingTable(VulkanContext& context) {
    auto vkGetRayTracingShaderGroupHandlesKHR = (PFN_vkGetRayTracingShaderGroupHandlesKHR)vkGetDeviceProcAddr(context.device, "vkGetRayTracingShaderGroupHandlesKHR");
    if (!vkGetRayTracingShaderGroupHandlesKHR) {
        LOG_ERROR_CAT("Vulkan", "Failed to load vkGetRayTracingShaderGroupHandlesKHR", std::source_location::current());
        return;
    }
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};
    VkPhysicalDeviceProperties2 properties{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &rtProperties};
    vkGetPhysicalDeviceProperties2(context.physicalDevice, &properties);
    LOG_INFO_CAT("Vulkan", "SBT properties: handle size: {}, alignment: {}", std::source_location::current(),
                 rtProperties.shaderGroupHandleSize, rtProperties.shaderGroupBaseAlignment);
    const uint32_t groupCount = 3;
    const uint32_t sbtSize = rtProperties.shaderGroupHandleSize * groupCount;
    std::vector<uint8_t> shaderHandleStorage(sbtSize);
    if (vkGetRayTracingShaderGroupHandlesKHR(context.device, context.rayTracingPipeline, 0, groupCount, sbtSize, shaderHandleStorage.data()) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to get shader group handles", std::source_location::current());
        return;
    }
    createBuffer(context.device, context.physicalDevice, sbtSize,
                 VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 context.shaderBindingTable, context.shaderBindingTableMemory);
    LOG_INFO_CAT("Vulkan", context.shaderBindingTable, "shaderBindingTable", std::source_location::current());
    void* data;
    vkMapMemory(context.device, context.shaderBindingTableMemory, 0, sbtSize, 0, &data);
    memcpy(data, shaderHandleStorage.data(), sbtSize);
    vkUnmapMemory(context.device, context.shaderBindingTableMemory);
    LOG_INFO_CAT("Vulkan", context.shaderBindingTable, "shaderBindingTable", std::source_location::current());
    LOG_INFO("Populated shader binding table with size: {}", sbtSize);
}