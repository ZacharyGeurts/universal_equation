// Vulkan_init.cpp
// AMOURANTH RTX Engine, October 2025 - Vulkan initialization and utility functions.
// Dependencies: Vulkan 1.3+, GLM, VulkanCore.hpp, VulkanBufferManager.hpp, ue_init.hpp, logging.hpp.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#include "engine/Vulkan_init.hpp"
#include "VulkanBufferManager.hpp"
#include "ue_init.hpp"
#include "engine/logging.hpp"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <fstream>
#include <algorithm>
#include <stdexcept>
#include <source_location>

VulkanSwapchainManager::VulkanSwapchainManager(VulkanContext& context, VkSurfaceKHR surface)
    : context_(context), swapchain_(VK_NULL_HANDLE), imageCount_(0), swapchainImageFormat_(VK_FORMAT_UNDEFINED), swapchainExtent_{0, 0} {
    context_.surface = surface;
    LOG_INFO("Initialized VulkanSwapchainManager", std::source_location::current());
}

VulkanSwapchainManager::~VulkanSwapchainManager() {
    cleanupSwapchain();
}

void VulkanSwapchainManager::initializeSwapchain(int width, int height) {
    VulkanInitializer::createSwapchain(context_, width, height);
    VulkanInitializer::createImageViews(context_);
    swapchain_ = context_.swapchain;
    swapchainImageFormat_ = context_.swapchainImageFormat_;
    swapchainExtent_ = context_.swapchainExtent_;
    swapchainImages_ = context_.swapchainImages;
    swapchainImageViews_ = context_.swapchainImageViews;
    imageCount_ = static_cast<uint32_t>(swapchainImages_.size());
    LOG_INFO("Swapchain initialized: {} images, format={}, extent={}x{}", 
             std::source_location::current(), imageCount_, swapchainImageFormat_, 
             swapchainExtent_.width, swapchainExtent_.height);
}

void VulkanSwapchainManager::cleanupSwapchain() {
    for (auto imageView : swapchainImageViews_) {
        if (imageView != VK_NULL_HANDLE) {
            LOG_INFO_CAT("Vulkan", "Destroying image view: {:p}", std::source_location::current(), static_cast<void*>(imageView));
            vkDestroyImageView(context_.device, imageView, nullptr);
        }
    }
    swapchainImageViews_.clear();
    swapchainImages_.clear();
    if (swapchain_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", "Destroying swapchain: {:p}", std::source_location::current(), static_cast<void*>(swapchain_));
        vkDestroySwapchainKHR(context_.device, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
    LOG_DEBUG("Swapchain cleaned up", std::source_location::current());
}

void VulkanSwapchainManager::handleResize(int width, int height) {
    cleanupSwapchain();
    initializeSwapchain(width, height);
    LOG_INFO("Resized swapchain to {}x{}", std::source_location::current(), width, height);
}

VulkanPipelineManager::VulkanPipelineManager(VulkanContext& context, int width, int height)
    : context_(context), width_(width), height_(height), renderPass_(VK_NULL_HANDLE), 
      graphicsPipeline_(VK_NULL_HANDLE), pipelineLayout_(VK_NULL_HANDLE), descriptorSetLayout_(VK_NULL_HANDLE) {
    createRenderPass();
    VulkanInitializer::createDescriptorSetLayout(context_.device, descriptorSetLayout_);
    createPipelineLayout();
    createGraphicsPipeline();
    LOG_INFO("Initialized VulkanPipelineManager {}x{}", std::source_location::current(), width, height);
}

VulkanPipelineManager::~VulkanPipelineManager() {
    cleanupPipeline();
}

void VulkanPipelineManager::createRenderPass() {
    VulkanInitializer::createRenderPass(context_.device, renderPass_, context_.swapchainImageFormat_);
    LOG_INFO_CAT("Vulkan", "Created render pass: {:p}", std::source_location::current(), static_cast<void*>(renderPass_));
}

void VulkanPipelineManager::createPipelineLayout() {
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
        LOG_ERROR("Failed to create pipeline layout", std::source_location::current());
        throw std::runtime_error("Failed to create pipeline layout");
    }
    LOG_INFO_CAT("Vulkan", "Created pipeline layout: {:p}", std::source_location::current(), static_cast<void*>(pipelineLayout_));
}

void VulkanPipelineManager::createGraphicsPipeline() {
    vertexShaderModule_ = VulkanInitializer::loadShader(context_.device, "assets/shaders/rasterization/vertex.spv");
    fragmentShaderModule_ = VulkanInitializer::loadShader(context_.device, "assets/shaders/rasterization/fragment.spv");
    VulkanInitializer::createGraphicsPipeline(context_.device, renderPass_, graphicsPipeline_, 
                                             pipelineLayout_, descriptorSetLayout_, 
                                             width_, height_, vertexShaderModule_, fragmentShaderModule_);
    LOG_INFO_CAT("Vulkan", "Created graphics pipeline: {:p}", std::source_location::current(), static_cast<void*>(graphicsPipeline_));
}

void VulkanPipelineManager::cleanupPipeline() {
    if (graphicsPipeline_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", "Destroying graphics pipeline: {:p}", std::source_location::current(), static_cast<void*>(graphicsPipeline_));
        vkDestroyPipeline(context_.device, graphicsPipeline_, nullptr);
        graphicsPipeline_ = VK_NULL_HANDLE;
    }
    if (pipelineLayout_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", "Destroying pipeline layout: {:p}", std::source_location::current(), static_cast<void*>(pipelineLayout_));
        vkDestroyPipelineLayout(context_.device, pipelineLayout_, nullptr);
        pipelineLayout_ = VK_NULL_HANDLE;
    }
    if (renderPass_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", "Destroying render pass: {:p}", std::source_location::current(), static_cast<void*>(renderPass_));
        vkDestroyRenderPass(context_.device, renderPass_, nullptr);
        renderPass_ = VK_NULL_HANDLE;
    }
    if (descriptorSetLayout_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", "Destroying descriptor set layout: {:p}", std::source_location::current(), static_cast<void*>(descriptorSetLayout_));
        vkDestroyDescriptorSetLayout(context_.device, descriptorSetLayout_, nullptr);
        descriptorSetLayout_ = VK_NULL_HANDLE;
    }
    if (vertexShaderModule_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", "Destroying vertex shader module: {:p}", std::source_location::current(), static_cast<void*>(vertexShaderModule_));
        vkDestroyShaderModule(context_.device, vertexShaderModule_, nullptr);
        vertexShaderModule_ = VK_NULL_HANDLE;
    }
    if (fragmentShaderModule_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", "Destroying fragment shader module: {:p}", std::source_location::current(), static_cast<void*>(fragmentShaderModule_));
        vkDestroyShaderModule(context_.device, fragmentShaderModule_, nullptr);
        fragmentShaderModule_ = VK_NULL_HANDLE;
    }
    LOG_DEBUG("Pipeline cleaned up", std::source_location::current());
}

VulkanRenderer::VulkanRenderer(VkInstance instance, VkSurfaceKHR surface,
                              std::span<const glm::vec3> vertices, std::span<const uint32_t> indices,
                              int width, int height)
    : context_{}, width_(width), height_(height) {
    context_.instance = instance;
    context_.surface = surface;
    VulkanInitializer::initializeVulkan(context_, width_, height_);
    swapchainManager_ = std::make_unique<VulkanSwapchainManager>(context_, surface);
    swapchainManager_->initializeSwapchain(width, height);
    context_.swapchain = swapchainManager_->getSwapchain();
    context_.swapchainImageFormat_ = swapchainManager_->getSwapchainImageFormat();
    context_.swapchainExtent_ = swapchainManager_->getSwapchainExtent();
    context_.swapchainImages = swapchainManager_->getSwapchainImages();
    context_.swapchainImageViews = swapchainManager_->getSwapchainImageViews();
    bufferManager_ = std::make_unique<VulkanBufferManager>(context_);
    bufferManager_->initializeBuffers(vertices, indices);
    context_.vertexBuffer_ = bufferManager_->getVertexBuffer();
    context_.indexBuffer_ = bufferManager_->getIndexBuffer();
    bufferManager_->createUniformBuffers(static_cast<uint32_t>(swapchainManager_->getSwapchainImageViews().size()));
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
    LOG_INFO("VulkanRenderer initialized {}x{}", std::source_location::current(), width, height);
}

VulkanRenderer::~VulkanRenderer() {
    vkDeviceWaitIdle(context_.device);
    LOG_INFO("Destroying VulkanRenderer", std::source_location::current());

    // Destroy pipeline manager first to clean up pipeline-related objects
    pipelineManager_.reset();

    // Destroy synchronization objects
    if (imageAvailableSemaphore_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", "Destroying image available semaphore: {:p}", std::source_location::current(), static_cast<void*>(imageAvailableSemaphore_));
        vkDestroySemaphore(context_.device, imageAvailableSemaphore_, nullptr);
    }
    if (renderFinishedSemaphore_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", "Destroying render finished semaphore: {:p}", std::source_location::current(), static_cast<void*>(renderFinishedSemaphore_));
        vkDestroySemaphore(context_.device, renderFinishedSemaphore_, nullptr);
    }
    if (inFlightFence_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", "Destroying in-flight fence: {:p}", std::source_location::current(), static_cast<void*>(inFlightFence_));
        vkDestroyFence(context_.device, inFlightFence_, nullptr);
    }

    // Destroy framebuffers
    for (auto framebuffer : framebuffers_) {
        if (framebuffer != VK_NULL_HANDLE) {
            LOG_INFO_CAT("Vulkan", "Destroying framebuffer: {:p}", std::source_location::current(), static_cast<void*>(framebuffer));
            vkDestroyFramebuffer(context_.device, framebuffer, nullptr);
        }
    }
    framebuffers_.clear();

    // Destroy swapchain manager (cleans up swapchain and image views)
    swapchainManager_.reset();

    // Destroy buffer manager (cleans up vertex, index, and uniform buffers)
    bufferManager_.reset();

    // Destroy command pool
    if (context_.commandPool != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", "Destroying command pool: {:p}", std::source_location::current(), static_cast<void*>(context_.commandPool));
        vkDestroyCommandPool(context_.device, context_.commandPool, nullptr);
    }

    // Destroy storage image resources
    if (context_.storageImageView != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", "Destroying storage image view: {:p}", std::source_location::current(), static_cast<void*>(context_.storageImageView));
        vkDestroyImageView(context_.device, context_.storageImageView, nullptr);
    }
    if (context_.storageImage != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", "Destroying storage image: {:p}", std::source_location::current(), static_cast<void*>(context_.storageImage));
        vkDestroyImage(context_.device, context_.storageImage, nullptr);
    }
    if (context_.storageImageMemory != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", "Freeing storage image memory: {:p}", std::source_location::current(), static_cast<void*>(context_.storageImageMemory));
        vkFreeMemory(context_.device, context_.storageImageMemory, nullptr);
    }

    // Destroy sampler
    if (context_.sampler != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", "Destroying sampler: {:p}", std::source_location::current(), static_cast<void*>(context_.sampler));
        vkDestroySampler(context_.device, context_.sampler, nullptr);
    }

    // Destroy descriptor pool
    if (context_.descriptorPool != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", "Destroying descriptor pool: {:p}", std::source_location::current(), static_cast<void*>(context_.descriptorPool));
        vkDestroyDescriptorPool(context_.device, context_.descriptorPool, nullptr);
    }

    // Destroy acceleration structures
    auto vkDestroyAccelerationStructureKHR = (PFN_vkDestroyAccelerationStructureKHR)vkGetDeviceProcAddr(context_.device, "vkDestroyAccelerationStructureKHR");
    if (context_.topLevelAS != VK_NULL_HANDLE && vkDestroyAccelerationStructureKHR) {
        LOG_INFO_CAT("Vulkan", "Destroying top-level AS: {:p}", std::source_location::current(), static_cast<void*>(context_.topLevelAS));
        vkDestroyAccelerationStructureKHR(context_.device, context_.topLevelAS, nullptr);
    }
    if (context_.topLevelASBuffer != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", "Destroying top-level AS buffer: {:p}", std::source_location::current(), static_cast<void*>(context_.topLevelASBuffer));
        vkDestroyBuffer(context_.device, context_.topLevelASBuffer, nullptr);
    }
    if (context_.topLevelASBufferMemory != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", "Freeing top-level AS buffer memory: {:p}", std::source_location::current(), static_cast<void*>(context_.topLevelASBufferMemory));
        vkFreeMemory(context_.device, context_.topLevelASBufferMemory, nullptr);
    }
    if (context_.bottomLevelAS != VK_NULL_HANDLE && vkDestroyAccelerationStructureKHR) {
        LOG_INFO_CAT("Vulkan", "Destroying bottom-level AS: {:p}", std::source_location::current(), static_cast<void*>(context_.bottomLevelAS));
        vkDestroyAccelerationStructureKHR(context_.device, context_.bottomLevelAS, nullptr);
    }
    if (context_.bottomLevelASBuffer != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", "Destroying bottom-level AS buffer: {:p}", std::source_location::current(), static_cast<void*>(context_.bottomLevelASBuffer));
        vkDestroyBuffer(context_.device, context_.bottomLevelASBuffer, nullptr);
    }
    if (context_.bottomLevelASBufferMemory != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", "Freeing bottom-level AS buffer memory: {:p}", std::source_location::current(), static_cast<void*>(context_.bottomLevelASBufferMemory));
        vkFreeMemory(context_.device, context_.bottomLevelASBufferMemory, nullptr);
    }

    // Destroy ray-tracing pipeline resources
    if (context_.rayTracingPipeline != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", "Destroying ray-tracing pipeline: {:p}", std::source_location::current(), static_cast<void*>(context_.rayTracingPipeline));
        vkDestroyPipeline(context_.device, context_.rayTracingPipeline, nullptr);
    }
    if (context_.rayTracingPipelineLayout != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", "Destroying ray-tracing pipeline layout: {:p}", std::source_location::current(), static_cast<void*>(context_.rayTracingPipelineLayout));
        vkDestroyPipelineLayout(context_.device, context_.rayTracingPipelineLayout, nullptr);
    }
    if (context_.rayTracingDescriptorSetLayout != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", "Destroying ray-tracing descriptor set layout: {:p}", std::source_location::current(), static_cast<void*>(context_.rayTracingDescriptorSetLayout));
        vkDestroyDescriptorSetLayout(context_.device, context_.rayTracingDescriptorSetLayout, nullptr);
    }
    if (context_.shaderBindingTable != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", "Destroying shader binding table: {:p}", std::source_location::current(), static_cast<void*>(context_.shaderBindingTable));
        vkDestroyBuffer(context_.device, context_.shaderBindingTable, nullptr);
    }
    if (context_.shaderBindingTableMemory != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", "Freeing shader binding table memory: {:p}", std::source_location::current(), static_cast<void*>(context_.shaderBindingTableMemory));
        vkFreeMemory(context_.device, context_.shaderBindingTableMemory, nullptr);
    }

    // Destroy device last
    if (context_.device != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", "Destroying device: {:p}", std::source_location::current(), static_cast<void*>(context_.device));
        vkDestroyDevice(context_.device, nullptr);
    }
}

void VulkanRenderer::createFramebuffers() {
    framebuffers_.resize(swapchainManager_->getSwapchainImageViews().size());
    for (size_t i = 0; i < framebuffers_.size(); ++i) {
        VkImageView attachments[] = {swapchainManager_->getSwapchainImageViews()[i]};
        VkFramebufferCreateInfo framebufferInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderPass = pipelineManager_->getRenderPass(),
            .attachmentCount = 1,
            .pAttachments = attachments,
            .width = swapchainManager_->getSwapchainExtent().width,
            .height = swapchainManager_->getSwapchainExtent().height,
            .layers = 1
        };
        if (vkCreateFramebuffer(context_.device, &framebufferInfo, nullptr, &framebuffers_[i]) != VK_SUCCESS) {
            LOG_ERROR("Failed to create framebuffer[{}]", std::source_location::current(), i);
            throw std::runtime_error("Failed to create framebuffer");
        }
        LOG_INFO_CAT("Vulkan", "Created framebuffer: {:p}", std::source_location::current(), static_cast<void*>(framebuffers_[i]));
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
        LOG_ERROR("Failed to allocate command buffers", std::source_location::current());
        throw std::runtime_error("Failed to allocate command buffers");
    }
    LOG_INFO("Allocated {} command buffers", std::source_location::current(), commandBuffers_.size());
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
        LOG_ERROR("Failed to create sync objects", std::source_location::current());
        throw std::runtime_error("Failed to create sync objects");
    }
    LOG_INFO_CAT("Vulkan", "Created image available semaphore: {:p}", std::source_location::current(), static_cast<void*>(imageAvailableSemaphore_));
    LOG_INFO_CAT("Vulkan", "Created render finished semaphore: {:p}", std::source_location::current(), static_cast<void*>(renderFinishedSemaphore_));
    LOG_INFO_CAT("Vulkan", "Created in-flight fence: {:p}", std::source_location::current(), static_cast<void*>(inFlightFence_));
}

void VulkanRenderer::renderFrame(const AMOURANTH& camera) {
    vkWaitForFences(context_.device, 1, &inFlightFence_, VK_TRUE, UINT64_MAX);
    vkResetFences(context_.device, 1, &inFlightFence_);
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(context_.device, swapchainManager_->getSwapchain(), 
                                            UINT64_MAX, imageAvailableSemaphore_, VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        handleResize(width_, height_);
        return;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        LOG_ERROR("Failed to acquire swapchain image: {}", std::source_location::current(), result);
        throw std::runtime_error("Failed to acquire swapchain image");
    }
    void* data;
    UE::UniformBufferObject ubo{
        .model = glm::mat4(1.0f),
        .view = camera.getViewMatrix(),
        .proj = camera.getProjectionMatrix()
    };
    vkMapMemory(context_.device, bufferManager_->getUniformBufferMemory(imageIndex), 
                0, sizeof(UE::UniformBufferObject), 0, &data);
    memcpy(data, &ubo, sizeof(UE::UniformBufferObject));
    vkUnmapMemory(context_.device, bufferManager_->getUniformBufferMemory(imageIndex));
    LOG_DEBUG("Updated uniform buffer for image {}", std::source_location::current(), imageIndex);
    VkCommandBuffer commandBuffer = commandBuffers_[imageIndex];
    vkResetCommandBuffer(commandBuffer, 0);
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pInheritanceInfo = nullptr
    };
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        LOG_ERROR("Failed to begin command buffer: {:p}", std::source_location::current(), static_cast<void*>(commandBuffer));
        throw std::runtime_error("Failed to begin command buffer");
    }
    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = pipelineManager_->getRenderPass(),
        .framebuffer = framebuffers_[imageIndex],
        .renderArea = {{0, 0}, swapchainManager_->getSwapchainExtent()},
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
        LOG_ERROR("Failed to end command buffer: {:p}", std::source_location::current(), static_cast<void*>(commandBuffer));
        throw std::runtime_error("Failed to end command buffer");
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
        LOG_ERROR("Failed to submit draw command buffer: {:p}", std::source_location::current(), static_cast<void*>(context_.graphicsQueue));
        throw std::runtime_error("Failed to submit draw command buffer");
    }
    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &renderFinishedSemaphore_,
        .swapchainCount = 1,
        .pSwapchains = &swapchainManager_->getSwapchain(),
        .pImageIndices = &imageIndex,
        .pResults = nullptr
    };
    result = vkQueuePresentKHR(context_.presentQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        handleResize(width_, height_);
    } else if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to present swapchain image: {}", std::source_location::current(), result);
        throw std::runtime_error("Failed to present swapchain image");
    }
}

void VulkanRenderer::handleResize(int width, int height) {
    vkDeviceWaitIdle(context_.device);
    width_ = width;
    height_ = height;
    swapchainManager_->handleResize(width, height);
    pipelineManager_->cleanupPipeline();
    pipelineManager_ = std::make_unique<VulkanPipelineManager>(context_, width, height);
    for (auto framebuffer : framebuffers_) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(context_.device, framebuffer, nullptr);
        }
    }
    framebuffers_.clear();
    createFramebuffers();
    createCommandBuffers();
    LOG_INFO("Handled resize to {}x{}", std::source_location::current(), width, height);
}

VkDeviceMemory VulkanRenderer::getVertexBufferMemory() const {
    return bufferManager_ ? bufferManager_->getVertexBufferMemory() : VK_NULL_HANDLE;
}

VkDeviceMemory VulkanRenderer::getIndexBufferMemory() const {
    return bufferManager_ ? bufferManager_->getIndexBufferMemory() : VK_NULL_HANDLE;
}

namespace VulkanInitializer {

void initializeVulkan(VulkanContext& context, [[maybe_unused]] int width, [[maybe_unused]] int height) {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(context.instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        LOG_ERROR("No physical devices found", std::source_location::current());
        throw std::runtime_error("No physical devices found");
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(context.instance, &deviceCount, devices.data());
    context.physicalDevice = devices[0];
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(context.physicalDevice, &props);
    LOG_INFO("Selected physical device: {}", std::source_location::current(), props.deviceName);
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
    LOG_INFO("Graphics queue family: {}, present: {}", std::source_location::current(),
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
    VkPhysicalDeviceBufferDeviceAddressFeaturesKHR bufferDeviceAddressFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR,
        .pNext = nullptr,
        .bufferDeviceAddress = VK_TRUE,
        .bufferDeviceAddressCaptureReplay = VK_FALSE,
        .bufferDeviceAddressMultiDevice = VK_FALSE
    };
    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
        .pNext = &bufferDeviceAddressFeatures,
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
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
    };
    VkDeviceCreateInfo deviceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &rtPipelineFeatures,
        .flags = 0,
        .queueCreateInfoCount = static_cast<uint32_t>(context.graphicsQueueFamilyIndex == context.presentQueueFamilyIndex ? 1 : 2),
        .pQueueCreateInfos = queueCreateInfos,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = 5,
        .ppEnabledExtensionNames = deviceExtensions,
        .pEnabledFeatures = &deviceFeatures
    };
    if (vkCreateDevice(context.physicalDevice, &deviceCreateInfo, nullptr, &context.device) != VK_SUCCESS) {
        LOG_ERROR("Failed to create logical device", std::source_location::current());
        throw std::runtime_error("Failed to create logical device");
    }
    LOG_INFO("Created logical device: {:p}", std::source_location::current(), static_cast<void*>(context.device));
    vkGetDeviceQueue(context.device, context.graphicsQueueFamilyIndex, 0, &context.graphicsQueue);
    vkGetDeviceQueue(context.device, context.presentQueueFamilyIndex, 0, &context.presentQueue);
    LOG_INFO("Graphics queue: {:p}, present queue: {:p}", std::source_location::current(),
             static_cast<void*>(context.graphicsQueue), static_cast<void*>(context.presentQueue));
    VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = context.graphicsQueueFamilyIndex
    };
    if (vkCreateCommandPool(context.device, &poolInfo, nullptr, &context.commandPool) != VK_SUCCESS) {
        LOG_ERROR("Failed to create command pool", std::source_location::current());
        throw std::runtime_error("Failed to create command pool");
    }
    LOG_INFO("Created command pool: {:p}", std::source_location::current(), static_cast<void*>(context.commandPool));
    vkGetPhysicalDeviceMemoryProperties(context.physicalDevice, &context.memoryProperties);
}

void createSwapchain(VulkanContext& context, int width, int height) {
    VkSurfaceCapabilitiesKHR capabilities;
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context.physicalDevice, context.surface, &capabilities) != VK_SUCCESS) {
        LOG_ERROR("Failed to get surface capabilities", std::source_location::current());
        throw std::runtime_error("Failed to get surface capabilities");
    }
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(context.physicalDevice, context.surface, &formatCount, nullptr);
    if (formatCount == 0) {
        LOG_ERROR("No surface formats available", std::source_location::current());
        throw std::runtime_error("No surface formats available");
    }
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
        ? VkExtent2D{std::clamp(static_cast<uint32_t>(width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
                     std::clamp(static_cast<uint32_t>(height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height)}
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
        LOG_ERROR("Failed to create swapchain", std::source_location::current());
        throw std::runtime_error("Failed to create swapchain");
    }
    LOG_INFO("Created swapchain: {:p}, format: {}, extent: {}x{}", std::source_location::current(),
             static_cast<void*>(context.swapchain), surfaceFormat.format, extent.width, extent.height);
    context.swapchainImageFormat_ = surfaceFormat.format;
    context.swapchainExtent_ = extent;
    vkGetSwapchainImagesKHR(context.device, context.swapchain, &imageCount, nullptr);
    context.swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(context.device, context.swapchain, &imageCount, context.swapchainImages.data());
}

void createImageViews(VulkanContext& context) {
    context.swapchainImageViews.resize(context.swapchainImages.size());
    for (size_t i = 0; i < context.swapchainImages.size(); ++i) {
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
            LOG_ERROR("Failed to create image view for image {}", std::source_location::current(), i);
            throw std::runtime_error("Failed to create image view");
        }
        LOG_INFO("Created image view: {:p} for image {}", std::source_location::current(), static_cast<void*>(context.swapchainImageViews[i]), i);
    }
}

VkShaderModule loadShader(VkDevice device, const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open shader file: {}", std::source_location::current(), filepath);
        throw std::runtime_error("Failed to open shader file: " + filepath);
    }
    size_t size = static_cast<size_t>(file.tellg());
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
        LOG_ERROR("Failed to create shader module: {}", std::source_location::current(), filepath);
        throw std::runtime_error("Failed to create shader module");
    }
    LOG_INFO("Loaded shader: {:p} from {}", std::source_location::current(), static_cast<void*>(module), filepath);
    return module;
}

void createRenderPass(VkDevice device, VkRenderPass& renderPass, VkFormat format) {
    if (format == VK_FORMAT_UNDEFINED) {
        LOG_ERROR("Invalid format for render pass: VK_FORMAT_UNDEFINED", std::source_location::current());
        throw std::runtime_error("Invalid format for render pass");
    }
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
    VkSubpassDependency dependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dependencyFlags = 0
    };
    VkRenderPassCreateInfo renderPassInfo{
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
    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        LOG_ERROR("Failed to create render pass", std::source_location::current());
        throw std::runtime_error("Failed to create render pass");
    }
    LOG_INFO("Created render pass: {:p}", std::source_location::current(), static_cast<void*>(renderPass));
}

void createDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout& descriptorSetLayout) {
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
        LOG_ERROR("Failed to create descriptor set layout", std::source_location::current());
        throw std::runtime_error("Failed to create descriptor set layout");
    }
    LOG_INFO("Created descriptor set layout: {:p}", std::source_location::current(), static_cast<void*>(descriptorSetLayout));
}

void createGraphicsPipeline(VkDevice device, VkRenderPass renderPass, VkPipeline& pipeline,
                           VkPipelineLayout& pipelineLayout, [[maybe_unused]] VkDescriptorSetLayout& descriptorSetLayout,
                           int width, int height, VkShaderModule& vertexShaderModule, VkShaderModule& fragmentShaderModule) {
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
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
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
        LOG_ERROR("Failed to create graphics pipeline", std::source_location::current());
        throw std::runtime_error("Failed to create graphics pipeline");
    }
    LOG_INFO("Created graphics pipeline: {:p}", std::source_location::current(), static_cast<void*>(pipeline));
}

void createDescriptorPoolAndSet(VkDevice device, VkDescriptorSetLayout descriptorSetLayout,
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
        LOG_ERROR("Failed to create descriptor pool", std::source_location::current());
        throw std::runtime_error("Failed to create descriptor pool");
    }
    LOG_INFO("Created descriptor pool: {:p}", std::source_location::current(), static_cast<void*>(descriptorPool));
    VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &descriptorSetLayout
    };
    if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate descriptor set", std::source_location::current());
        throw std::runtime_error("Failed to allocate descriptor set");
    }
    LOG_INFO("Allocated descriptor set: {:p}", std::source_location::current(), static_cast<void*>(descriptorSet));
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
        LOG_ERROR("Failed to create sampler", std::source_location::current());
        throw std::runtime_error("Failed to create sampler");
    }
    LOG_INFO("Created sampler: {:p}", std::source_location::current(), static_cast<void*>(sampler));
    VkDescriptorBufferInfo bufferInfo{
        .buffer = uniformBuffer,
        .offset = 0,
        .range = sizeof(UE::UniformBufferObject)
    };
    VkDescriptorImageInfo imageInfo{
        .sampler = sampler,
        .imageView = storageImageView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
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
    LOG_INFO("Updated descriptor sets", std::source_location::current());
}

void createStorageImage(VkDevice device, VkPhysicalDevice physicalDevice, VkImage& storageImage,
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
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    if (vkCreateImage(device, &imageInfo, nullptr, &storageImage) != VK_SUCCESS) {
        LOG_ERROR("Failed to create storage image", std::source_location::current());
        throw std::runtime_error("Failed to create storage image");
    }
    LOG_INFO("Created storage image: {:p}", std::source_location::current(), static_cast<void*>(storageImage));
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, storageImage, &memRequirements);
    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    if (vkAllocateMemory(device, &allocInfo, nullptr, &storageImageMemory) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate storage image memory", std::source_location::current());
        throw std::runtime_error("Failed to allocate storage image memory");
    }
    LOG_INFO("Allocated storage image memory: {:p}", std::source_location::current(), static_cast<void*>(storageImageMemory));
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
        LOG_ERROR("Failed to create storage image view", std::source_location::current());
        throw std::runtime_error("Failed to create storage image view");
    }
    LOG_INFO("Created storage image view: {:p}", std::source_location::current(), static_cast<void*>(storageImageView));
}

} // namespace VulkanInitializer