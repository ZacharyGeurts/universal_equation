// src/engine/Vulkan_init_pipeline.cpp
#include "engine/Vulkan_init_pipeline.hpp"
#include "engine/Vulkan/Vulkan_func_pipe.hpp"
#include "engine/Vulkan_utils.hpp"
#include "engine/logging.hpp"
#include <stdexcept>
#include <source_location>
#include <fstream>

VulkanPipelineManager::VulkanPipelineManager(VulkanContext& context)
    : context_(context) {
    LOG_INFO_CAT("Vulkan", "Constructing VulkanPipelineManager with context.device={:p}",
                 std::source_location::current(), static_cast<void*>(context_.device));
    createRenderPass();
    createPipelineLayout();
    createGraphicsPipeline();
}

VulkanPipelineManager::~VulkanPipelineManager() {
    LOG_INFO_CAT("Vulkan", "Destroying VulkanPipelineManager", std::source_location::current());
    cleanupPipeline();
}

void VulkanPipelineManager::createRenderPass() {
    LOG_DEBUG_CAT("Vulkan", "Creating render pass", std::source_location::current());
    VulkanInitializer::createRenderPass(context_.device, context_.renderPass, VK_FORMAT_B8G8R8A8_SRGB);
}

void VulkanPipelineManager::createPipelineLayout() {
    LOG_DEBUG_CAT("Vulkan", "Creating pipeline layout", std::source_location::current());

    VulkanInitializer::createDescriptorSetLayout(context_.device, context_.descriptorSetLayout);

    VkPushConstantRange pushConstantRange{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = sizeof(PushConstants)
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = &context_.descriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange
    };

    VkResult result = vkCreatePipelineLayout(context_.device, &pipelineLayoutInfo, nullptr, &context_.pipelineLayout);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create pipeline layout: result={}",
                      std::source_location::current(), VulkanInitializer::vkResultToString(result));
        throw std::runtime_error("Failed to create pipeline layout");
    }
    LOG_DEBUG_CAT("Vulkan", "Pipeline layout created: pipelineLayout={:p}",
                  std::source_location::current(), static_cast<void*>(context_.pipelineLayout));
}

void VulkanPipelineManager::createGraphicsPipeline() {
    LOG_DEBUG_CAT("Vulkan", "Creating graphics pipeline", std::source_location::current());

    auto vertShaderCode = readFile("shaders/shader.vert.spv");
    auto fragShaderCode = readFile("shaders/shader.frag.spv");

    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    VulkanInitializer::createGraphicsPipeline(context_.device, context_.renderPass, context_.pipeline,
                                             context_.pipelineLayout, context_.descriptorSetLayout,
                                             800, 600, vertShaderModule, fragShaderModule);

    vkDestroyShaderModule(context_.device, vertShaderModule, nullptr);
    vkDestroyShaderModule(context_.device, fragShaderModule, nullptr);
}

void VulkanPipelineManager::cleanupPipeline() {
    LOG_DEBUG_CAT("Vulkan", "Cleaning up pipeline", std::source_location::current());
    if (context_.device == VK_NULL_HANDLE) {
        LOG_WARNING_CAT("Vulkan", "Device is null, skipping cleanup", std::source_location::current());
        return;
    }

    if (context_.pipeline != VK_NULL_HANDLE) {
        LOG_DEBUG_CAT("Vulkan", "Destroying pipeline={:p}", std::source_location::current(), static_cast<void*>(context_.pipeline));
        vkDestroyPipeline(context_.device, context_.pipeline, nullptr);
        context_.pipeline = VK_NULL_HANDLE;
    }
    if (context_.pipelineLayout != VK_NULL_HANDLE) {
        LOG_DEBUG_CAT("Vulkan", "Destroying pipelineLayout={:p}", std::source_location::current(), static_cast<void*>(context_.pipelineLayout));
        vkDestroyPipelineLayout(context_.device, context_.pipelineLayout, nullptr);
        context_.pipelineLayout = VK_NULL_HANDLE;
    }
    if (context_.descriptorSetLayout != VK_NULL_HANDLE) {
        LOG_DEBUG_CAT("Vulkan", "Destroying descriptorSetLayout={:p}", std::source_location::current(), static_cast<void*>(context_.descriptorSetLayout));
        vkDestroyDescriptorSetLayout(context_.device, context_.descriptorSetLayout, nullptr);
        context_.descriptorSetLayout = VK_NULL_HANDLE;
    }
    if (context_.renderPass != VK_NULL_HANDLE) {
        LOG_DEBUG_CAT("Vulkan", "Destroying renderPass={:p}", std::source_location::current(), static_cast<void*>(context_.renderPass));
        vkDestroyRenderPass(context_.device, context_.renderPass, nullptr);
        context_.renderPass = VK_NULL_HANDLE;
    }
}

std::vector<char> VulkanPipelineManager::readFile(const std::string& filename) {
    LOG_DEBUG_CAT("Vulkan", "Reading file: {}", std::source_location::current(), filename);
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR_CAT("Vulkan", "Failed to open file: {}", std::source_location::current(), filename);
        throw std::runtime_error("Failed to open file");
    }
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    LOG_DEBUG_CAT("Vulkan", "Read {} bytes from file: {}", std::source_location::current(), fileSize, filename);
    return buffer;
}

VkShaderModule VulkanPipelineManager::createShaderModule(const std::vector<char>& code) {
    LOG_DEBUG_CAT("Vulkan", "Creating shader module", std::source_location::current());
    VkShaderModuleCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .codeSize = code.size(),
        .pCode = reinterpret_cast<const uint32_t*>(code.data())
    };
    VkShaderModule shaderModule;
    VkResult result = vkCreateShaderModule(context_.device, &createInfo, nullptr, &shaderModule);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create shader module: result={}",
                      std::source_location::current(), VulkanInitializer::vkResultToString(result));
        throw std::runtime_error("Failed to create shader module");
    }
    LOG_DEBUG_CAT("Vulkan", "Shader module created: shaderModule={:p}", std::source_location::current(), static_cast<void*>(shaderModule));
    return shaderModule;
}