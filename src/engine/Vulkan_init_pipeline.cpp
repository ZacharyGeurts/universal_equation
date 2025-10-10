// src/engine/Vulkan_init_pipeline.cpp
#include "engine/Vulkan_init_pipeline.hpp"
#include "engine/logging.hpp"
#include <stdexcept>
#include <source_location>
#include <fstream>
#include <vector>

std::string vkFormatToString(VkFormat format) {
    switch (format) {
        case VK_FORMAT_B8G8R8A8_SRGB: return "VK_FORMAT_B8G8R8A8_SRGB";
        case VK_FORMAT_R8G8B8A8_SRGB: return "VK_FORMAT_R8G8B8A8_SRGB";
        case VK_FORMAT_R32G32B32_SFLOAT: return "VK_FORMAT_R32G32B32_SFLOAT";
        default: return std::format("VkFormat({})", static_cast<int>(format));
    }
}

std::string vkResultToString(VkResult result) {
    switch (result) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_NOT_READY: return "VK_NOT_READY";
        case VK_TIMEOUT: return "VK_TIMEOUT";
        case VK_EVENT_SET: return "VK_EVENT_SET";
        case VK_EVENT_RESET: return "VK_EVENT_RESET";
        case VK_INCOMPLETE: return "VK_INCOMPLETE";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        default: return std::format("VkResult({})", static_cast<int>(result));
    }
}

VulkanPipelineManager::VulkanPipelineManager(VulkanContext& context)
    : context_(context) {
    LOG_INFO_CAT("VulkanPipeline", "Constructing VulkanPipelineManager", std::source_location::current());
    createRenderPass();
    createPipelineLayout();
    createGraphicsPipeline();
}

VulkanPipelineManager::~VulkanPipelineManager() noexcept {
    LOG_INFO_CAT("VulkanPipeline", "Cleaning up pipeline", std::source_location::current());
    cleanupPipeline();
}

void VulkanPipelineManager::createRenderPass() {
    LOG_DEBUG_CAT("VulkanPipeline", "Creating render pass with format={}", std::source_location::current(), vkFormatToString(VK_FORMAT_B8G8R8A8_SRGB));
    VkAttachmentDescription colorAttachment = {
        .flags = 0,
        .format = VK_FORMAT_B8G8R8A8_SRGB,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };

    VkAttachmentReference colorAttachmentRef = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subpass = {
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

    VkRenderPassCreateInfo renderPassInfo = {
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

    VkResult result = vkCreateRenderPass(context_.device, &renderPassInfo, nullptr, &context_.renderPass);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanPipeline", "Failed to create render pass: result={}", std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to create render pass");
    }
    LOG_DEBUG_CAT("VulkanPipeline", "Render pass created: renderPass={:p}", std::source_location::current(), context_.renderPass);
}

void VulkanPipelineManager::createPipelineLayout() {
    LOG_DEBUG_CAT("VulkanPipeline", "Creating pipeline layout", std::source_location::current());
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 0,
        .pSetLayouts = nullptr,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr
    };

    VkResult result = vkCreatePipelineLayout(context_.device, &pipelineLayoutInfo, nullptr, &context_.pipelineLayout);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanPipeline", "Failed to create pipeline layout: result={}", std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to create pipeline layout");
    }
    LOG_DEBUG_CAT("VulkanPipeline", "Pipeline layout created: pipelineLayout={:p}", std::source_location::current(), context_.pipelineLayout);
}

void VulkanPipelineManager::createGraphicsPipeline() {
    LOG_DEBUG_CAT("VulkanPipeline", "Creating graphics pipeline", std::source_location::current());
    auto vertShaderCode = readFile("shaders/vert.spv");
    auto fragShaderCode = readFile("shaders/frag.spv");
    LOG_DEBUG_CAT("VulkanPipeline", "Read shader files: vertShaderCode.size={}, fragShaderCode.size={}",
                  std::source_location::current(), vertShaderCode.size(), fragShaderCode.size());

    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);
    LOG_DEBUG_CAT("VulkanPipeline", "Created shader modules: vertShaderModule={:p}, fragShaderModule={:p}",
                  std::source_location::current(), vertShaderModule, fragShaderModule);

    VkPipelineShaderStageCreateInfo shaderStages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertShaderModule,
            .pName = "main",
            .pSpecializationInfo = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragShaderModule,
            .pName = "main",
            .pSpecializationInfo = nullptr
        }
    };

    VkVertexInputBindingDescription bindingDescription = {
        .binding = 0,
        .stride = sizeof(glm::vec3),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };
    VkVertexInputAttributeDescription attributeDescription = {
        .location = 0,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = 0
    };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindingDescription,
        .vertexAttributeDescriptionCount = 1,
        .pVertexAttributeDescriptions = &attributeDescription
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = 1280.0f,
        .height = 720.0f,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = {1280, 720}
    };

    VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor
    };

    VkPipelineRasterizationStateCreateInfo rasterizer = {
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

    VkPipelineMultisampleStateCreateInfo multisampling = {
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

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {
        .blendEnable = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };

    VkPipelineColorBlendStateCreateInfo colorBlending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment,
        .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f}
    };

    VkGraphicsPipelineCreateInfo pipelineInfo = {
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
        .layout = context_.pipelineLayout,
        .renderPass = context_.renderPass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };

    VkResult result = vkCreateGraphicsPipelines(context_.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &context_.pipeline);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanPipeline", "Failed to create graphics pipeline: result={}", std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to create graphics pipeline");
    }
    LOG_DEBUG_CAT("VulkanPipeline", "Graphics pipeline created: pipeline={:p}", std::source_location::current(), context_.pipeline);

    vkDestroyShaderModule(context_.device, vertShaderModule, nullptr);
    vkDestroyShaderModule(context_.device, fragShaderModule, nullptr);
    LOG_DEBUG_CAT("VulkanPipeline", "Shader modules destroyed", std::source_location::current());
}

void VulkanPipelineManager::cleanupPipeline() {
    LOG_DEBUG_CAT("VulkanPipeline", "Cleaning up pipeline resources", std::source_location::current());
    if (context_.device == VK_NULL_HANDLE) {
        LOG_WARNING_CAT("VulkanPipeline", "Device is null, skipping cleanup", std::source_location::current());
        return;
    }

    vkDeviceWaitIdle(context_.device);
    LOG_DEBUG_CAT("VulkanPipeline", "Device idle, proceeding with cleanup", std::source_location::current());

    if (context_.pipeline != VK_NULL_HANDLE) {
        LOG_DEBUG_CAT("VulkanPipeline", "Destroying pipeline={:p}", std::source_location::current(), context_.pipeline);
        vkDestroyPipeline(context_.device, context_.pipeline, nullptr);
        context_.pipeline = VK_NULL_HANDLE;
    }
    if (context_.pipelineLayout != VK_NULL_HANDLE) {
        LOG_DEBUG_CAT("VulkanPipeline", "Destroying pipelineLayout={:p}", std::source_location::current(), context_.pipelineLayout);
        vkDestroyPipelineLayout(context_.device, context_.pipelineLayout, nullptr);
        context_.pipelineLayout = VK_NULL_HANDLE;
    }
    if (context_.renderPass != VK_NULL_HANDLE) {
        LOG_DEBUG_CAT("VulkanPipeline", "Destroying renderPass={:p}", std::source_location::current(), context_.renderPass);
        vkDestroyRenderPass(context_.device, context_.renderPass, nullptr);
        context_.renderPass = VK_NULL_HANDLE;
    }
}

std::vector<char> VulkanPipelineManager::readFile(const std::string& filename) {
    LOG_DEBUG_CAT("VulkanPipeline", "Reading shader file: {}", std::source_location::current(), filename);
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR_CAT("VulkanPipeline", "Failed to open shader file: {}", std::source_location::current(), filename);
        throw std::runtime_error("Failed to open shader file: " + filename);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    LOG_DEBUG_CAT("VulkanPipeline", "Read {} bytes from shader file", std::source_location::current(), fileSize);
    return buffer;
}

VkShaderModule VulkanPipelineManager::createShaderModule(const std::vector<char>& code) {
    LOG_DEBUG_CAT("VulkanPipeline", "Creating shader module with code size={}", std::source_location::current(), code.size());
    VkShaderModuleCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .codeSize = code.size(),
        .pCode = reinterpret_cast<const uint32_t*>(code.data())
    };

    VkShaderModule shaderModule;
    VkResult result = vkCreateShaderModule(context_.device, &createInfo, nullptr, &shaderModule);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanPipeline", "Failed to create shader module: result={}", std::source_location::current(), vkResultToString(result));
        throw std::runtime_error("Failed to create shader module");
    }
    LOG_DEBUG_CAT("VulkanPipeline", "Shader module created: shaderModule={:p}", std::source_location::current(), shaderModule);
    return shaderModule;
}