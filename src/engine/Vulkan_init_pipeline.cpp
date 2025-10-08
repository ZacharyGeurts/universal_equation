// AMOURANTH RTX Engine, October 2025 - Vulkan pipeline initialization.
// Manages render pass, descriptor sets, graphics pipeline creation, and shader modules.
// Dependencies: Vulkan 1.3+, GLM, C++20 standard library.
// Zachary Geurts 2025

#include "engine/Vulkan_init.hpp"
#include <stdexcept>
#include <vector>
#include <cstdint>
#include <source_location>
#include <format>
#include <fstream>

VulkanPipelineManager::VulkanPipelineManager(VulkanContext& context)
    : context_(context), vertShaderModule_(VK_NULL_HANDLE), fragShaderModule_(VK_NULL_HANDLE), logger_() {
    logger_.log(Logging::LogLevel::Info, "Constructing VulkanPipelineManager",
                std::source_location::current());
}

VulkanPipelineManager::~VulkanPipelineManager() {
    logger_.log(Logging::LogLevel::Info, "Destroying VulkanPipelineManager",
                std::source_location::current());
}

VkShaderModule VulkanPipelineManager::createShaderModule(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        logger_.log(Logging::LogLevel::Error, "Failed to open shader file: {}", 
                    std::source_location::current(), filename);
        throw std::runtime_error("Failed to open shader file: " + filename);
    }
    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    VkShaderModuleCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .codeSize = buffer.size(),
        .pCode = reinterpret_cast<const uint32_t*>(buffer.data())
    };
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(context_.device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        logger_.log(Logging::LogLevel::Error, "Failed to create shader module for file: {}", 
                    std::source_location::current(), filename);
        throw std::runtime_error("Failed to create shader module");
    }
    logger_.log(Logging::LogLevel::Debug, "Shader module created for file: {}", 
                std::source_location::current(), filename);
    return shaderModule;
}

void VulkanPipelineManager::initializePipeline(int width, int height) {
    logger_.log(Logging::LogLevel::Debug, "Initializing pipeline with width={}, height={}", 
                std::source_location::current(), width, height);

    vertShaderModule_ = createShaderModule("assets/shaders/rasterization/vertex.spv");
    fragShaderModule_ = createShaderModule("assets/shaders/rasterization/fragment.spv");

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

    VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dependencyFlags = 0
    };

    VkRenderPassCreateInfo renderPassInfo = {
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

    if (vkCreateRenderPass(context_.device, &renderPassInfo, nullptr, &context_.renderPass) != VK_SUCCESS) {
        logger_.log(Logging::LogLevel::Error, "Failed to create render pass", 
                    std::source_location::current());
        throw std::runtime_error("Failed to create render pass");
    }
    logger_.log(Logging::LogLevel::Info, "Render pass created", 
                std::source_location::current());

    VkDescriptorSetLayoutBinding layoutBinding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .pImmutableSamplers = nullptr
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = 1,
        .pBindings = &layoutBinding
    };

    if (vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &context_.descriptorSetLayout) != VK_SUCCESS) {
        logger_.log(Logging::LogLevel::Error, "Failed to create descriptor set layout", 
                    std::source_location::current());
        throw std::runtime_error("Failed to create descriptor set layout");
    }
    logger_.log(Logging::LogLevel::Info, "Descriptor set layout created", 
                std::source_location::current());

    VkDescriptorPoolSize poolSize = {
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1
    };

    VkDescriptorPoolCreateInfo descriptorPoolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = &poolSize
    };

    if (vkCreateDescriptorPool(context_.device, &descriptorPoolInfo, nullptr, &context_.descriptorPool) != VK_SUCCESS) {
        logger_.log(Logging::LogLevel::Error, "Failed to create descriptor pool", 
                    std::source_location::current());
        throw std::runtime_error("Failed to create descriptor pool");
    }
    logger_.log(Logging::LogLevel::Info, "Descriptor pool created", 
                std::source_location::current());

    VkDescriptorSetAllocateInfo descriptorAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = context_.descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &context_.descriptorSetLayout
    };

    if (vkAllocateDescriptorSets(context_.device, &descriptorAllocInfo, &context_.descriptorSet) != VK_SUCCESS) {
        logger_.log(Logging::LogLevel::Error, "Failed to allocate descriptor set", 
                    std::source_location::current());
        throw std::runtime_error("Failed to allocate descriptor set");
    }
    logger_.log(Logging::LogLevel::Info, "Descriptor set allocated", 
                std::source_location::current());

    VkVertexInputBindingDescription bindingDescriptions[] = {
        {
            .binding = 0,
            .stride = sizeof(glm::vec3),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
        }
    };

    VkVertexInputAttributeDescription attributeDescriptions[] = {
        {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = 0
        }
    };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = bindingDescriptions,
        .vertexAttributeDescriptionCount = 1,
        .pVertexAttributeDescriptions = attributeDescriptions
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
        .width = static_cast<float>(width),
        .height = static_cast<float>(height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}
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

    VkPushConstantRange pushConstantRange = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(PushConstants)
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = &context_.descriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange
    };

    if (vkCreatePipelineLayout(context_.device, &pipelineLayoutInfo, nullptr, &context_.pipelineLayout) != VK_SUCCESS) {
        logger_.log(Logging::LogLevel::Error, "Failed to create pipeline layout", 
                    std::source_location::current());
        throw std::runtime_error("Failed to create pipeline layout");
    }
    logger_.log(Logging::LogLevel::Info, "Pipeline layout created", 
                std::source_location::current());

    VkPipelineShaderStageCreateInfo shaderStages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertShaderModule_,
            .pName = "main",
            .pSpecializationInfo = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragShaderModule_,
            .pName = "main",
            .pSpecializationInfo = nullptr
        }
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

    if (vkCreateGraphicsPipelines(context_.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &context_.pipeline) != VK_SUCCESS) {
        logger_.log(Logging::LogLevel::Error, "Failed to create graphics pipeline", 
                    std::source_location::current());
        throw std::runtime_error("Failed to create graphics pipeline");
    }
    logger_.log(Logging::LogLevel::Info, "Graphics pipeline created", 
                std::source_location::current());

    vkDestroyShaderModule(context_.device, vertShaderModule_, nullptr);
    vkDestroyShaderModule(context_.device, fragShaderModule_, nullptr);
    vertShaderModule_ = VK_NULL_HANDLE;
    fragShaderModule_ = VK_NULL_HANDLE;

    context_.swapchainFramebuffers.resize(context_.swapchainImageViews.size());
    for (size_t i = 0; i < context_.swapchainImageViews.size(); ++i) {
        VkImageView attachments[] = {context_.swapchainImageViews[i]};
        VkFramebufferCreateInfo framebufferInfo = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderPass = context_.renderPass,
            .attachmentCount = 1,
            .pAttachments = attachments,
            .width = context_.swapchainExtent.width,
            .height = context_.swapchainExtent.height,
            .layers = 1
        };
        if (vkCreateFramebuffer(context_.device, &framebufferInfo, nullptr, &context_.swapchainFramebuffers[i]) != VK_SUCCESS) {
            logger_.log(Logging::LogLevel::Error, "Failed to create framebuffer for index {}", 
                        std::source_location::current(), i);
            throw std::runtime_error("Failed to create framebuffer");
        }
    }
    logger_.log(Logging::LogLevel::Info, "Created {} framebuffers", 
                std::source_location::current(), context_.swapchainFramebuffers.size());
}

void VulkanPipelineManager::cleanupPipeline() {
    logger_.log(Logging::LogLevel::Debug, "Cleaning up pipeline", 
                std::source_location::current());

    if (context_.pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(context_.device, context_.pipeline, nullptr);
        context_.pipeline = VK_NULL_HANDLE;
    }
    if (context_.pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(context_.device, context_.pipelineLayout, nullptr);
        context_.pipelineLayout = VK_NULL_HANDLE;
    }
    if (context_.descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(context_.device, context_.descriptorSetLayout, nullptr);
        context_.descriptorSetLayout = VK_NULL_HANDLE;
    }
    if (context_.descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(context_.device, context_.descriptorPool, nullptr);
        context_.descriptorPool = VK_NULL_HANDLE;
    }
    if (context_.renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(context_.device, context_.renderPass, nullptr);
        context_.renderPass = VK_NULL_HANDLE;
    }
    for (auto framebuffer : context_.swapchainFramebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(context_.device, framebuffer, nullptr);
        }
    }
    context_.swapchainFramebuffers.clear();
    logger_.log(Logging::LogLevel::Info, "Pipeline cleaned up", 
                std::source_location::current());
}