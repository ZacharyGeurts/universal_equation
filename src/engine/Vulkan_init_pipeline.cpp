// AMOURANTH RTX Engine, October 2025 - Vulkan pipeline initialization.
// Manages render pass, descriptor sets, graphics pipeline creation, and shader modules.
// Dependencies: Vulkan 1.3+, GLM, C++20 standard library.
// Zachary Geurts 2025

#include "engine/Vulkan_init_pipeline.hpp"
#include "engine/Vulkan_init.hpp"
#include "engine/logging.hpp"
#include <stdexcept>
#include <format>
#include <fstream>

struct PushConstants {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

VulkanPipelineManager::VulkanPipelineManager(VulkanContext& context)
    : context_(context), vertShaderModule_(VK_NULL_HANDLE), fragShaderModule_(VK_NULL_HANDLE), logger_() {
    logger_.log(Logging::LogLevel::Debug, "VulkanPipelineManager initialized", std::source_location::current());
}

VulkanPipelineManager::~VulkanPipelineManager() {
    cleanupPipeline();
    logger_.log(Logging::LogLevel::Info, "VulkanPipelineManager destroyed", std::source_location::current());
}

VkShaderModule VulkanPipelineManager::createShaderModule(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        std::string error = std::format("Failed to open shader file: {}", filename);
        logger_.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    logger_.log(Logging::LogLevel::Info, "Loaded shader file: {}, size: {} bytes", std::source_location::current(), filename, fileSize);

    VkShaderModuleCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .codeSize = buffer.size(),
        .pCode = reinterpret_cast<const uint32_t*>(buffer.data()),
    };

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(context_.device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        std::string error = std::format("Failed to create shader module for {}", filename);
        logger_.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }

    logger_.log(Logging::LogLevel::Info, "Created shader module for {}", std::source_location::current(), filename);
    return shaderModule;
}

void VulkanPipelineManager::initializePipeline(int width, int height) {
    logger_.log(Logging::LogLevel::Debug, "Initializing pipeline", std::source_location::current());

    // Create shader modules
    try {
        vertShaderModule_ = createShaderModule("assets/shaders/rasterization/vertex.spv");
        fragShaderModule_ = createShaderModule("assets/shaders/rasterization/fragment.spv");
    } catch (const std::exception& e) {
        logger_.log(Logging::LogLevel::Error, "Shader module creation failed: {}", std::source_location::current(), e.what());
        throw;
    }

    // Validate shader modules
    if (vertShaderModule_ == VK_NULL_HANDLE || fragShaderModule_ == VK_NULL_HANDLE) {
        std::string error = "Invalid shader module provided";
        logger_.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }

    // Create render pass
    VkAttachmentDescription colorAttachment = {
        .flags = 0,
        .format = VK_FORMAT_B8G8R8A8_SRGB,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };
    VkAttachmentReference colorAttachmentRef = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
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
        .pPreserveAttachments = nullptr,
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
        .pDependencies = nullptr,
    };
    if (vkCreateRenderPass(context_.device, &renderPassInfo, nullptr, &context_.renderPass) != VK_SUCCESS) {
        std::string error = "Failed to create render pass";
        logger_.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }
    logger_.log(Logging::LogLevel::Info, "Render pass created", std::source_location::current());

    // Create descriptor set layout
    VkDescriptorSetLayoutBinding layoutBinding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .pImmutableSamplers = nullptr,
    };
    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = 1,
        .pBindings = &layoutBinding,
    };
    if (vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &context_.descriptorSetLayout) != VK_SUCCESS) {
        std::string error = "Failed to create descriptor set layout";
        logger_.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }
    logger_.log(Logging::LogLevel::Info, "Descriptor set layout created", std::source_location::current());

    // Create descriptor pool
    VkDescriptorPoolSize poolSize = {
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
    };
    VkDescriptorPoolCreateInfo descriptorPoolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = &poolSize,
    };
    if (vkCreateDescriptorPool(context_.device, &descriptorPoolInfo, nullptr, &context_.descriptorPool) != VK_SUCCESS) {
        std::string error = "Failed to create descriptor pool";
        logger_.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }
    logger_.log(Logging::LogLevel::Info, "Descriptor pool created", std::source_location::current());

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo descriptorAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = context_.descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &context_.descriptorSetLayout,
    };
    if (vkAllocateDescriptorSets(context_.device, &descriptorAllocInfo, &context_.descriptorSet) != VK_SUCCESS) {
        std::string error = "Failed to allocate descriptor set";
        logger_.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }
    logger_.log(Logging::LogLevel::Info, "Descriptor set allocated", std::source_location::current());

    // Vertex input setup
    VkVertexInputBindingDescription bindingDescriptions[] = {
        {
            .binding = 0,
            .stride = sizeof(glm::vec3),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        },
        {
            .binding = 1,
            .stride = sizeof(glm::mat4),
            .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE,
        }
    };
    VkVertexInputAttributeDescription attributeDescriptions[] = {
        {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = 0,
        },
        {
            .location = 1,
            .binding = 1,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = 0,
        },
        {
            .location = 2,
            .binding = 1,
            .format = VK_FORMAT_R32_SFLOAT,
            .offset = sizeof(glm::vec3),
        },
        {
            .location = 3,
            .binding = 1,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = sizeof(glm::vec3) + sizeof(float),
        }
    };
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .vertexBindingDescriptionCount = 2,
        .pVertexBindingDescriptions = bindingDescriptions,
        .vertexAttributeDescriptionCount = 4,
        .pVertexAttributeDescriptions = attributeDescriptions,
    };
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };
    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(width),
        .height = static_cast<float>(height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)},
    };
    VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
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
        .lineWidth = 1.0f,
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
        .alphaToOneEnable = VK_FALSE,
    };
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {
        .blendEnable = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
    VkPipelineColorBlendStateCreateInfo colorBlending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment,
        .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f},
    };
    VkPushConstantRange pushConstantRange = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(PushConstants),
    };
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = &context_.descriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange,
    };
    if (vkCreatePipelineLayout(context_.device, &pipelineLayoutInfo, nullptr, &context_.pipelineLayout) != VK_SUCCESS) {
        std::string error = "Failed to create pipeline layout";
        logger_.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }
    logger_.log(Logging::LogLevel::Info, "Pipeline layout created", std::source_location::current());

    // Create graphics pipeline
    VkPipelineShaderStageCreateInfo shaderStages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertShaderModule_,
            .pName = "main",
            .pSpecializationInfo = nullptr,
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragShaderModule_,
            .pName = "main",
            .pSpecializationInfo = nullptr,
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
        .basePipelineIndex = -1,
    };
    if (vkCreateGraphicsPipelines(context_.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &context_.pipeline) != VK_SUCCESS) {
        std::string error = "Failed to create graphics pipeline";
        logger_.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }
    logger_.log(Logging::LogLevel::Info, "Graphics pipeline created", std::source_location::current());

    // Clean up shader modules after pipeline creation
    vkDestroyShaderModule(context_.device, vertShaderModule_, nullptr);
    vkDestroyShaderModule(context_.device, fragShaderModule_, nullptr);
    vertShaderModule_ = VK_NULL_HANDLE;
    fragShaderModule_ = VK_NULL_HANDLE;

    // Create framebuffers
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
            .width = static_cast<uint32_t>(width),
            .height = static_cast<uint32_t>(height),
            .layers = 1,
        };
        if (vkCreateFramebuffer(context_.device, &framebufferInfo, nullptr, &context_.swapchainFramebuffers[i]) != VK_SUCCESS) {
            std::string error = "Failed to create framebuffer";
            logger_.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
            throw std::runtime_error(error);
        }
    }
    logger_.log(Logging::LogLevel::Info, "Created {} framebuffers", std::source_location::current(), context_.swapchainFramebuffers.size());
}

void VulkanPipelineManager::cleanupPipeline() {
    logger_.log(Logging::LogLevel::Debug, "Cleaning up pipeline", std::source_location::current());

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
        vkDestroyFramebuffer(context_.device, framebuffer, nullptr);
    }
    context_.swapchainFramebuffers.clear();
    logger_.log(Logging::LogLevel::Info, "Pipeline cleaned up", std::source_location::current());
}