// AMOURANTH RTX Engine, October 2025 - Vulkan pipeline utilities implementation.
// Supports Windows/Linux; no mutexes; voxel geometry via glm::vec3 vertices.
// Dependencies: Vulkan 1.3+, GLM, C++20 standard library.
// Integrates with VulkanRTX and SwapchainManager for rendering voxel scenes.
// Manages graphics pipeline, render pass, descriptor sets, and samplers.
// Zachary Geurts 2025

#include "engine/Vulkan/Vulkan_func_pipe.hpp"
#include <stdexcept>
#include <fstream>
#include <format>
#include <glm/glm.hpp>
#include <syncstream>
#include <iostream>

// ANSI color codes
#define RESET "\033[0m"
#define MAGENTA "\033[1;35m" // Bold magenta for errors
#define CYAN "\033[1;36m"    // Bold cyan for debug
#define YELLOW "\033[1;33m"  // Bold yellow for warnings
#define GREEN "\033[1;32m"   // Bold green for info
#define BOLD "\033[1m"

namespace VulkanInitializer {

VkShaderModule createShaderModule(VkDevice device, const std::string& filename) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Creating shader module from file: " << filename << RESET << std::endl;

    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to open shader file: " << filename << RESET << std::endl;
        throw std::runtime_error(std::format("Failed to open shader file: {}", filename));
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    if (fileSize == 0 || fileSize % 4 != 0) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Invalid shader file size (must be multiple of 4 bytes): " << filename << RESET << std::endl;
        throw std::runtime_error(std::format("Invalid shader file size (must be multiple of 4 bytes): {}", filename));
    }
    std::osyncstream(std::cout) << CYAN << "[DEBUG] Shader file size: " << fileSize << " bytes" << RESET << std::endl;

    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    VkShaderModuleCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = buffer.size(),
        .pCode = reinterpret_cast<const uint32_t*>(buffer.data())
    };

    VkShaderModule shaderModule;
    VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
    if (result != VK_SUCCESS) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to create shader module from " << filename << ": " << result << RESET << std::endl;
        throw std::runtime_error(std::format("Failed to create shader module from {}: {}", filename, result));
    }

    std::osyncstream(std::cout) << GREEN << "[INFO] Shader module created successfully for: " << filename << RESET << std::endl;
    return shaderModule;
}

void createRenderPass(VkDevice device, VkRenderPass& renderPass, VkFormat format) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Creating render pass with format: " << format << RESET << std::endl;

    VkAttachmentDescription colorAttachment{
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
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentRef
    };

    VkSubpassDependency dependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
    };

    VkRenderPassCreateInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colorAttachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency
    };

    VkResult result = vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass);
    if (result != VK_SUCCESS) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to create render pass: " << result << RESET << std::endl;
        throw std::runtime_error(std::format("Failed to create render pass: {}", result));
    }

    std::osyncstream(std::cout) << GREEN << "[INFO] Render pass created successfully" << RESET << std::endl;
}

void createDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout& descriptorSetLayout) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Creating descriptor set layout for sampler" << RESET << std::endl;

    VkDescriptorSetLayoutBinding samplerBinding{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &samplerBinding
    };

    VkResult result = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout);
    if (result != VK_SUCCESS) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to create descriptor set layout: " << result << RESET << std::endl;
        throw std::runtime_error(std::format("Failed to create descriptor set layout: {}", result));
    }

    std::osyncstream(std::cout) << GREEN << "[INFO] Descriptor set layout created successfully" << RESET << std::endl;
}

void createDescriptorPoolAndSet(VkDevice device, VkDescriptorSetLayout descriptorSetLayout,
                               VkDescriptorPool& descriptorPool, VkDescriptorSet& descriptorSet, VkSampler sampler) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Creating descriptor pool and set for sampler" << RESET << std::endl;

    VkDescriptorPoolSize poolSize{
        .type = VK_DESCRIPTOR_TYPE_SAMPLER,
        .descriptorCount = 1
    };

    VkDescriptorPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = &poolSize
    };

    VkResult result = vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);
    if (result != VK_SUCCESS) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to create descriptor pool: " << result << RESET << std::endl;
        throw std::runtime_error(std::format("Failed to create descriptor pool: {}", result));
    }
    std::osyncstream(std::cout) << CYAN << "[DEBUG] Created descriptor pool" << RESET << std::endl;

    VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &descriptorSetLayout
    };

    result = vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet);
    if (result != VK_SUCCESS) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to allocate descriptor set: " << result << RESET << std::endl;
        throw std::runtime_error(std::format("Failed to allocate descriptor set: {}", result));
    }
    std::osyncstream(std::cout) << CYAN << "[DEBUG] Allocated descriptor set" << RESET << std::endl;

    VkDescriptorImageInfo imageInfo{
        .sampler = sampler,
        .imageView = VK_NULL_HANDLE,
        .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VkWriteDescriptorSet descriptorWrite{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptorSet,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
        .pImageInfo = &imageInfo
    };

    vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
    std::osyncstream(std::cout) << GREEN << "[INFO] Descriptor pool and set created successfully" << RESET << std::endl;
}

void createSampler(VkDevice device, VkPhysicalDevice physicalDevice, VkSampler& sampler) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Creating sampler" << RESET << std::endl;

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);
    std::osyncstream(std::cout) << CYAN << "[DEBUG] Max sampler anisotropy: " << properties.limits.maxSamplerAnisotropy << RESET << std::endl;

    VkSamplerCreateInfo samplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
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
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to create sampler: " << result << RESET << std::endl;
        throw std::runtime_error(std::format("Failed to create sampler: {}", result));
    }

    std::osyncstream(std::cout) << GREEN << "[INFO] Sampler created successfully" << RESET << std::endl;
}

void createGraphicsPipeline(VkDevice device, VkRenderPass renderPass, VkPipeline& pipeline,
                           VkPipelineLayout& pipelineLayout, VkDescriptorSetLayout& descriptorSetLayout,
                           int width, int height, VkShaderModule& vertShaderModule, VkShaderModule& fragShaderModule) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Creating graphics pipeline with width=" << width << ", height=" << height << RESET << std::endl;

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vertShaderModule,
        .pName = "main"
    };

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = fragShaderModule,
        .pName = "main"
    };

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};
    std::osyncstream(std::cout) << CYAN << "[DEBUG] Configured vertex and fragment shader stages" << RESET << std::endl;

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
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindingDescription,
        .vertexAttributeDescriptionCount = 1,
        .pVertexAttributeDescriptions = &attributeDescription
    };
    std::osyncstream(std::cout) << CYAN << "[DEBUG] Configured vertex input for glm::vec3 vertices" << RESET << std::endl;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
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
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor
    };

    VkPipelineRasterizationStateCreateInfo rasterizer{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f
    };

    VkPipelineMultisampleStateCreateInfo multisampling{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 1.0f,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE
    };

    VkPipelineDepthStencilStateCreateInfo depthStencil{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_FALSE,
        .depthWriteEnable = VK_FALSE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE
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
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment
    };

    VkPushConstantRange pushConstantRange{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = sizeof(PushConstants)
    };
    std::osyncstream(std::cout) << CYAN << "[DEBUG] Configured pipeline states: vertex input, rasterization, multisampling, color blending" << RESET << std::endl;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &descriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange
    };

    VkResult result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);
    if (result != VK_SUCCESS) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to create pipeline layout: " << result << RESET << std::endl;
        throw std::runtime_error(std::format("Failed to create pipeline layout: {}", result));
    }
    std::osyncstream(std::cout) << CYAN << "[DEBUG] Created pipeline layout" << RESET << std::endl;

    VkGraphicsPipelineCreateInfo pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shaderStages,
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depthStencil,
        .pColorBlendState = &colorBlending,
        .layout = pipelineLayout,
        .renderPass = renderPass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };

    result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
    if (result != VK_SUCCESS) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to create graphics pipeline: " << result << RESET << std::endl;
        throw std::runtime_error(std::format("Failed to create graphics pipeline: {}", result));
    }

    std::osyncstream(std::cout) << GREEN << "[INFO] Graphics pipeline created successfully" << RESET << std::endl;
}

} // namespace VulkanInitializer