// AMOURANTH RTX Engine, October 2025 - Vulkan pipeline utilities implementation.
// Supports Windows/Linux; no mutexes; voxel geometry via glm::vec3 vertices.
// Dependencies: Vulkan 1.3+, GLM, C++20 standard library.
// Integrates with VulkanRTX and SwapchainManager for rendering voxel scenes.
// Manages graphics pipeline, render pass, descriptor sets, and samplers.
// Zachary Geurts 2025

#include "engine/Vulkan/Vulkan_func_pipe.hpp"
#include "engine/logging.hpp" // For Logging::Logger
#include <stdexcept>
#include <fstream>
#include <format>
#include <glm/glm.hpp>
#include <syncstream>
#include <string>

// Helper function to convert VkResult to string for logging
std::string vkResultToString(VkResult result) {
    switch (result) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
        default: return std::to_string(static_cast<int>(result));
    }
}

// Helper function to convert VkFormat to string for logging
std::string vkFormatToString(VkFormat format) {
    switch (format) {
        case VK_FORMAT_UNDEFINED: return "VK_FORMAT_UNDEFINED";
        case VK_FORMAT_R8G8B8A8_SRGB: return "VK_FORMAT_R8G8B8A8_SRGB";
        case VK_FORMAT_R8G8B8A8_UNORM: return "VK_FORMAT_R8G8B8A8_UNORM";
        case VK_FORMAT_B8G8R8A8_SRGB: return "VK_FORMAT_B8G8R8A8_SRGB";
        case VK_FORMAT_B8G8R8A8_UNORM: return "VK_FORMAT_B8G8R8A8_UNORM";
        case VK_FORMAT_R32G32B32_SFLOAT: return "VK_FORMAT_R32G32B32_SFLOAT";
        case VK_FORMAT_R32G32B32A32_SFLOAT: return "VK_FORMAT_R32G32B32A32_SFLOAT";
        case VK_FORMAT_D32_SFLOAT: return "VK_FORMAT_D32_SFLOAT";
        case VK_FORMAT_S8_UINT: return "VK_FORMAT_S8_UINT";
        case VK_FORMAT_D24_UNORM_S8_UINT: return "VK_FORMAT_D24_UNORM_S8_UINT";
        default: return std::to_string(static_cast<int>(format));
    }
}

namespace VulkanInitializer {

VkShaderModule createShaderModule(VkDevice device, const std::string& filename, const Logging::Logger& logger) {
    logger.log(Logging::LogLevel::Info, "Creating shader module from file: {}", std::source_location::current(), filename);

    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        logger.log(Logging::LogLevel::Error, "Failed to open shader file: {}", std::source_location::current(), filename);
        throw std::runtime_error(std::format("Failed to open shader file: {}", filename));
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    if (fileSize == 0 || fileSize % 4 != 0) {
        logger.log(Logging::LogLevel::Error, "Invalid shader file size (must be multiple of 4 bytes): {}", std::source_location::current(), filename);
        throw std::runtime_error(std::format("Invalid shader file size (must be multiple of 4 bytes): {}", filename));
    }
    logger.log(Logging::LogLevel::Debug, "Shader file size: {} bytes", std::source_location::current(), fileSize);

    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    VkShaderModuleCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .codeSize = buffer.size(),
        .pCode = reinterpret_cast<const uint32_t*>(buffer.data())
    };

    VkShaderModule shaderModule;
    VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
    if (result != VK_SUCCESS) {
        logger.log(Logging::LogLevel::Error, "Failed to create shader module from {}: {}", std::source_location::current(), filename, vkResultToString(result));
        throw std::runtime_error(std::format("Failed to create shader module from {}: {}", filename, vkResultToString(result)));
    }

    logger.log(Logging::LogLevel::Info, "Shader module created successfully for: {}", std::source_location::current(), filename);
    return shaderModule;
}

void createRenderPass(VkDevice device, VkRenderPass& renderPass, VkFormat format, const Logging::Logger& logger) {
    logger.log(Logging::LogLevel::Info, "Creating render pass with format: {}", std::source_location::current(), vkFormatToString(format));

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

    VkResult result = vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass);
    if (result != VK_SUCCESS) {
        logger.log(Logging::LogLevel::Error, "Failed to create render pass: {}", std::source_location::current(), vkResultToString(result));
        throw std::runtime_error(std::format("Failed to create render pass: {}", vkResultToString(result)));
    }

    logger.log(Logging::LogLevel::Info, "Render pass created successfully", std::source_location::current());
}

void createDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout& descriptorSetLayout, const Logging::Logger& logger) {
    logger.log(Logging::LogLevel::Info, "Creating descriptor set layout for sampler", std::source_location::current());

    VkDescriptorSetLayoutBinding samplerBinding{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = nullptr
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = 1,
        .pBindings = &samplerBinding
    };

    VkResult result = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout);
    if (result != VK_SUCCESS) {
        logger.log(Logging::LogLevel::Error, "Failed to create descriptor set layout: {}", std::source_location::current(), vkResultToString(result));
        throw std::runtime_error(std::format("Failed to create descriptor set layout: {}", vkResultToString(result)));
    }

    logger.log(Logging::LogLevel::Info, "Descriptor set layout created successfully", std::source_location::current());
}

void createDescriptorPoolAndSet(VkDevice device, VkDescriptorSetLayout descriptorSetLayout,
                               VkDescriptorPool& descriptorPool, VkDescriptorSet& descriptorSet, VkSampler sampler,
                               const Logging::Logger& logger) {
    logger.log(Logging::LogLevel::Info, "Creating descriptor pool and set for sampler", std::source_location::current());

    VkDescriptorPoolSize poolSize{
        .type = VK_DESCRIPTOR_TYPE_SAMPLER,
        .descriptorCount = 1
    };

    VkDescriptorPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = &poolSize
    };

    VkResult result = vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);
    if (result != VK_SUCCESS) {
        logger.log(Logging::LogLevel::Error, "Failed to create descriptor pool: {}", std::source_location::current(), vkResultToString(result));
        throw std::runtime_error(std::format("Failed to create descriptor pool: {}", vkResultToString(result)));
    }
    logger.log(Logging::LogLevel::Debug, "Created descriptor pool", std::source_location::current());

    VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &descriptorSetLayout
    };

    result = vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet);
    if (result != VK_SUCCESS) {
        logger.log(Logging::LogLevel::Error, "Failed to allocate descriptor set: {}", std::source_location::current(), vkResultToString(result));
        throw std::runtime_error(std::format("Failed to allocate descriptor set: {}", vkResultToString(result)));
    }
    logger.log(Logging::LogLevel::Debug, "Allocated descriptor set", std::source_location::current());

    VkDescriptorImageInfo imageInfo{
        .sampler = sampler,
        .imageView = VK_NULL_HANDLE,
        .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VkWriteDescriptorSet descriptorWrite{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = descriptorSet,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
        .pImageInfo = &imageInfo,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr
    };

    vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
    logger.log(Logging::LogLevel::Info, "Descriptor pool and set created successfully", std::source_location::current());
}

void createSampler(VkDevice device, VkPhysicalDevice physicalDevice, VkSampler& sampler, const Logging::Logger& logger) {
    logger.log(Logging::LogLevel::Info, "Creating sampler", std::source_location::current());

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);
    logger.log(Logging::LogLevel::Debug, "Max sampler anisotropy: {}", std::source_location::current(), properties.limits.maxSamplerAnisotropy);

    VkSamplerCreateInfo samplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_TRUE,
        .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE
    };

    VkResult result = vkCreateSampler(device, &samplerInfo, nullptr, &sampler);
    if (result != VK_SUCCESS) {
        logger.log(Logging::LogLevel::Error, "Failed to create sampler: {}", std::source_location::current(), vkResultToString(result));
        throw std::runtime_error(std::format("Failed to create sampler: {}", vkResultToString(result)));
    }

    logger.log(Logging::LogLevel::Info, "Sampler created successfully", std::source_location::current());
}

void createGraphicsPipeline(VkDevice device, VkRenderPass renderPass, VkPipeline& pipeline,
                           VkPipelineLayout& pipelineLayout, VkDescriptorSetLayout& descriptorSetLayout,
                           int width, int height, VkShaderModule& vertShaderModule, VkShaderModule& fragShaderModule,
                           const Logging::Logger& logger) {
    logger.log(Logging::LogLevel::Info, "Creating graphics pipeline with width={}, height={}", std::source_location::current(), width, height);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vertShaderModule,
        .pName = "main",
        .pSpecializationInfo = nullptr
    };

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = fragShaderModule,
        .pName = "main",
        .pSpecializationInfo = nullptr
    };

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};
    logger.log(Logging::LogLevel::Debug, "Configured vertex and fragment shader stages", std::source_location::current());

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
    logger.log(Logging::LogLevel::Debug, "Configured vertex input for glm::vec3 vertices", std::source_location::current());

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

    VkPipelineDepthStencilStateCreateInfo depthStencil{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthTestEnable = VK_FALSE,
        .depthWriteEnable = VK_FALSE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
        .front = {},
        .back = {},
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f
    };

    VkPipelineColorBlendAttachmentState colorBlendAttachment{
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
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

    VkPushConstantRange pushConstantRange{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = sizeof(PushConstants)
    };
    logger.log(Logging::LogLevel::Debug, "Configured pipeline states: vertex input, rasterization, multisampling, color blending", std::source_location::current());

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = &descriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange
    };

    VkResult result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);
    if (result != VK_SUCCESS) {
        logger.log(Logging::LogLevel::Error, "Failed to create pipeline layout: {}", std::source_location::current(), vkResultToString(result));
        throw std::runtime_error(std::format("Failed to create pipeline layout: {}", vkResultToString(result)));
    }
    logger.log(Logging::LogLevel::Debug, "Created pipeline layout", std::source_location::current());

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
        .pDepthStencilState = &depthStencil,
        .pColorBlendState = &colorBlending,
        .pDynamicState = nullptr,
        .layout = pipelineLayout,
        .renderPass = renderPass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };

    result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
    if (result != VK_SUCCESS) {
        logger.log(Logging::LogLevel::Error, "Failed to create graphics pipeline: {}", std::source_location::current(), vkResultToString(result));
        throw std::runtime_error(std::format("Failed to create graphics pipeline: {}", vkResultToString(result)));
    }

    logger.log(Logging::LogLevel::Info, "Graphics pipeline created successfully", std::source_location::current());
}

} // namespace VulkanInitializer