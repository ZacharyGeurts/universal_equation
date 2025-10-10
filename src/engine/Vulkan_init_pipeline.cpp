#include "engine/Vulkan_init.hpp"
#include <stdexcept>

VulkanPipelineManager::VulkanPipelineManager(VulkanContext& context, int width, int height)
    : context_(context), width_(width), height_(height), renderPass_(VK_NULL_HANDLE),
      graphicsPipeline_(VK_NULL_HANDLE), pipelineLayout_(VK_NULL_HANDLE),
      descriptorSetLayout_(VK_NULL_HANDLE), vertexShaderModule_(VK_NULL_HANDLE),
      fragmentShaderModule_(VK_NULL_HANDLE) {
    LOG_INFO_CAT("Vulkan", "Constructing VulkanPipelineManager", std::source_location::current());
}

VulkanPipelineManager::~VulkanPipelineManager() {
    cleanupPipeline();
}

void VulkanPipelineManager::createRenderPass() {
    VulkanInitializer::createRenderPass(context_.device, renderPass_, VK_FORMAT_B8G8R8A8_SRGB);
}

void VulkanPipelineManager::createPipelineLayout() {
    VulkanInitializer::createDescriptorSetLayout(context_.device, descriptorSetLayout_);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = &descriptorSetLayout_,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr
    };
    VkResult result = vkCreatePipelineLayout(context_.device, &pipelineLayoutInfo, nullptr, &pipelineLayout_);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create pipeline layout: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to create pipeline layout");
    }
}

void VulkanPipelineManager::createGraphicsPipeline() {
    vertexShaderModule_ = VulkanInitializer::loadShader(context_.device, "shaders/shader.vert.spv");
    fragmentShaderModule_ = VulkanInitializer::loadShader(context_.device, "shaders/shader.frag.spv");

    VulkanInitializer::createGraphicsPipeline(context_.device, renderPass_, graphicsPipeline_, pipelineLayout_,
                                             descriptorSetLayout_, width_, height_, vertexShaderModule_, fragmentShaderModule_);
}

void VulkanPipelineManager::cleanupPipeline() {
    if (context_.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(context_.device);
    }
    if (graphicsPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(context_.device, graphicsPipeline_, nullptr);
        graphicsPipeline_ = VK_NULL_HANDLE;
    }
    if (pipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(context_.device, pipelineLayout_, nullptr);
        pipelineLayout_ = VK_NULL_HANDLE;
    }
    if (descriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(context_.device, descriptorSetLayout_, nullptr);
        descriptorSetLayout_ = VK_NULL_HANDLE;
    }
    if (renderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(context_.device, renderPass_, nullptr);
        renderPass_ = VK_NULL_HANDLE;
    }
    if (vertexShaderModule_ != VK_NULL_HANDLE) {
        vkDestroyShaderModule(context_.device, vertexShaderModule_, nullptr);
        vertexShaderModule_ = VK_NULL_HANDLE;
    }
    if (fragmentShaderModule_ != VK_NULL_HANDLE) {
        vkDestroyShaderModule(context_.device, fragmentShaderModule_, nullptr);
        fragmentShaderModule_ = VK_NULL_HANDLE;
    }
}