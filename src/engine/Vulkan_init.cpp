// src/engine/Vulkan_init.cpp
#include "engine/Vulkan_init.hpp"
#include "engine/Vulkan/Vulkan_func_swapchain.hpp"
#include "engine/Vulkan/Vulkan_func_pipe.hpp"
#include "engine/Vulkan_init_pipeline.hpp"
#include "engine/Vulkan_init_buffers.hpp"
#include "engine/logging.hpp"
#include <stdexcept>
#include <source_location>

VulkanRenderer::VulkanRenderer(VkInstance instance, VkSurfaceKHR surface,
                               std::span<const glm::vec3> vertices, std::span<const uint32_t> indices,
                               int width, int height)
    : context_{}, swapchainManager_(nullptr), pipelineManager_(nullptr), bufferManager_(nullptr) {
    LOG_INFO_CAT("VulkanRenderer", "Constructing VulkanRenderer with width={}, height={}",
                 std::source_location::current(), width, height);
    LOG_DEBUG_CAT("VulkanRenderer", "Received instance={:p}, surface={:p}, vertices.size={}, indices.size={}",
                  std::source_location::current(), static_cast<void*>(instance), static_cast<void*>(surface),
                  vertices.size(), indices.size());

    context_.instance = instance;
    context_.surface = surface;

    swapchainManager_ = std::make_unique<VulkanSwapchainManager>(context_, context_.surface);
    LOG_DEBUG_CAT("VulkanRenderer", "VulkanSwapchainManager created", std::source_location::current());
    swapchainManager_->initializeSwapchain(width, height);
    LOG_DEBUG_CAT("VulkanRenderer", "Swapchain initialized", std::source_location::current());

    pipelineManager_ = std::make_unique<VulkanPipelineManager>(context_);
    LOG_DEBUG_CAT("VulkanRenderer", "VulkanPipelineManager created", std::source_location::current());

    bufferManager_ = std::make_unique<VulkanBufferManager>(context_);
    bufferManager_->initializeBuffers(vertices, indices);
    LOG_DEBUG_CAT("VulkanRenderer", "VulkanBufferManager created and buffers initialized", std::source_location::current());

    VulkanInitializer::createDescriptorPoolAndSet(context_.device, context_.descriptorSetLayout,
                                                 context_.descriptorPool, context_.descriptorSet,
                                                 context_.sampler, context_.uniformBuffer);
    LOG_DEBUG_CAT("VulkanRenderer", "Descriptor pool and set created", std::source_location::current());
}

VulkanRenderer::~VulkanRenderer() {
    LOG_DEBUG_CAT("VulkanRenderer", "Destroying VulkanRenderer", std::source_location::current());
    if (context_.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(context_.device);
        LOG_DEBUG_CAT("VulkanRenderer", "Device idle, proceeding with cleanup", std::source_location::current());
    }

    bufferManager_.reset();
    pipelineManager_.reset();
    swapchainManager_.reset();

    if (context_.uniformBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(context_.device, context_.uniformBuffer, nullptr);
        context_.uniformBuffer = VK_NULL_HANDLE;
    }
    if (context_.uniformBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context_.device, context_.uniformBufferMemory, nullptr);
        context_.uniformBufferMemory = VK_NULL_HANDLE;
    }
    if (context_.commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(context_.device, context_.commandPool, nullptr);
        context_.commandPool = VK_NULL_HANDLE;
    }
    if (context_.device != VK_NULL_HANDLE) {
        vkDestroyDevice(context_.device, nullptr);
        context_.device = VK_NULL_HANDLE;
    }
    if (context_.instance != VK_NULL_HANDLE && context_.surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(context_.instance, context_.surface, nullptr);
        context_.surface = VK_NULL_HANDLE;
    }
    if (context_.instance != VK_NULL_HANDLE) {
        vkDestroyInstance(context_.instance, nullptr);
        context_.instance = VK_NULL_HANDLE;
    }
}

void VulkanRenderer::renderFrame(const AMOURANTH& amouranth) {
    LOG_DEBUG_CAT("VulkanRenderer", "renderFrame called with transform=[{:f}, {:f}, {:f}, {:f}]",
                  std::source_location::current(),
                  amouranth.transform[0][0], amouranth.transform[1][1],
                  amouranth.transform[2][2], amouranth.transform[3][3]);
    // TODO: Implement rendering logic using amouranth.transform
}

void VulkanRenderer::handleResize(int width, int height) {
    LOG_DEBUG_CAT("VulkanRenderer", "Handling resize to width={}, height={}",
                  std::source_location::current(), width, height);
    if (swapchainManager_) {
        swapchainManager_->initializeSwapchain(width, height);
    }
}