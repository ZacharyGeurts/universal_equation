#include "engine/Vulkan_init.hpp"
#include "universal_equation.hpp"
#include <stdexcept>
#include <fstream>
#include <algorithm>

void VulkanInitializer::initializeVulkan(VulkanContext& context, int width, int height) {
    LOG_INFO_CAT("Vulkan", "Initializing Vulkan with instance={:p}, surface={:p}, width={}, height={}",
                 std::source_location::current(), reinterpret_cast<void*>(context.instance),
                 reinterpret_cast<void*>(context.surface), width, height);

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(context.instance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(context.instance, &deviceCount, devices.data());
    context.physicalDevice = devices[0];

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(context.physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(context.physicalDevice, &queueFamilyCount, queueFamilies.data());
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            context.graphicsQueueFamilyIndex = i;
        }
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(context.physicalDevice, i, context.surface, &presentSupport);
        if (presentSupport) {
            context.presentQueueFamilyIndex = i;
        }
    }

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfos[2] = {
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
    const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME};
    VkDeviceCreateInfo deviceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queueCreateInfoCount = static_cast<uint32_t>(context.graphicsQueueFamilyIndex == context.presentQueueFamilyIndex ? 1 : 2),
        .pQueueCreateInfos = queueCreateInfos,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = 3,
        .ppEnabledExtensionNames = deviceExtensions,
        .pEnabledFeatures = &deviceFeatures
    };
    VkResult result = vkCreateDevice(context.physicalDevice, &deviceCreateInfo, nullptr, &context.device);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create logical device: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to create logical device");
    }

    vkGetDeviceQueue(context.device, context.graphicsQueueFamilyIndex, 0, &context.graphicsQueue);
    vkGetDeviceQueue(context.device, context.presentQueueFamilyIndex, 0, &context.presentQueue);

    VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = context.graphicsQueueFamilyIndex
    };
    result = vkCreateCommandPool(context.device, &poolInfo, nullptr, &context.commandPool);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create command pool: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to create command pool");
    }

    vkGetPhysicalDeviceMemoryProperties(context.physicalDevice, &context.memoryProperties);
}

void VulkanInitializer::createSwapchain(VulkanContext& context) {
    LOG_INFO_CAT("Vulkan", "Creating swapchain with device={:p}, surface={:p}",
                 std::source_location::current(), reinterpret_cast<void*>(context.device),
                 reinterpret_cast<void*>(context.surface));

    VkSurfaceCapabilitiesKHR capabilities;
    VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context.physicalDevice, context.surface, &capabilities);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to get surface capabilities: result={}",
                      std::source_location::current(), result);
        throw std::runtime_error("Failed to get surface capabilities");
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

    VkExtent2D extent = capabilities.currentExtent;
    if (capabilities.currentExtent.width == UINT32_MAX) {
        extent = {800, 600}; // Fallback default
        extent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, extent.width));
        extent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, extent.height));
    }

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

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

    result = vkCreateSwapchainKHR(context.device, &createInfo, nullptr, &context.swapchain);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create swapchain: result={}",
                      std::source_location::current(), result);
        throw std::runtime_error("Failed to create swapchain");
    }
    LOG_DEBUG_CAT("Vulkan", "Created swapchain: swapchain={:p}",
                  std::source_location::current(), reinterpret_cast<void*>(context.swapchain));
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
            .format = VK_FORMAT_B8G8R8A8_SRGB,
            .components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };
        VkResult result = vkCreateImageView(context.device, &viewInfo, nullptr, &context.swapchainImageViews[i]);
        if (result != VK_SUCCESS) {
            LOG_ERROR_CAT("Vulkan", "Failed to create image view {}: result={}",
                          std::source_location::current(), i, result);
            throw std::runtime_error("Failed to create image view");
        }
        LOG_DEBUG_CAT("Vulkan", "Created image view {}: imageView={:p}",
                      std::source_location::current(), i, reinterpret_cast<void*>(context.swapchainImageViews[i]));
    }
    LOG_DEBUG_CAT("Vulkan", "Created image view {:p}",
                  std::source_location::current(), reinterpret_cast<void*>(context.swapchainImageViews.back()));
}

void VulkanInitializer::createAccelerationStructures([[maybe_unused]] VulkanContext& context,
                                                    [[maybe_unused]] std::span<const glm::vec3> vertices,
                                                    [[maybe_unused]] std::span<const uint32_t> indices) {
    LOG_DEBUG_CAT("Vulkan", "Creating acceleration structures", std::source_location::current());
}

void VulkanInitializer::createRayTracingPipeline([[maybe_unused]] VulkanContext& context) {
    LOG_DEBUG_CAT("Vulkan", "Creating ray tracing pipeline", std::source_location::current());
}

void VulkanInitializer::createShaderBindingTable([[maybe_unused]] VulkanContext& context) {
    LOG_DEBUG_CAT("Vulkan", "Creating shader binding table", std::source_location::current());
}

VkDeviceAddress VulkanInitializer::getBufferDeviceAddress(VkDevice device, VkBuffer buffer) {
    VkBufferDeviceAddressInfo addressInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = nullptr,
        .buffer = buffer
    };
    return vkGetBufferDeviceAddress(device, &addressInfo);
}

uint32_t VulkanInitializer::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    LOG_ERROR_CAT("Vulkan", "Failed to find suitable memory type", std::source_location::current());
    throw std::runtime_error("Failed to find suitable memory type");
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

    VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create buffer: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to create buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties)
    };

    result = vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to allocate buffer memory: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to allocate buffer memory");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

VkShaderModule VulkanInitializer::loadShader(VkDevice device, const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file) {
        LOG_ERROR_CAT("Vulkan", "Failed to open shader file: {}", std::source_location::current(), filepath);
        throw std::runtime_error("Failed to open shader file");
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
    VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, &module);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create shader module: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to create shader module");
    }
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

    VkResult result = vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create render pass: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to create render pass");
    }
}

void VulkanInitializer::createDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout& descriptorSetLayout) {
    VkDescriptorSetLayoutBinding bindings[3] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
            .pImmutableSamplers = nullptr
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
            .pImmutableSamplers = nullptr
        },
        {
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
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

    VkResult result = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create descriptor set layout: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to create descriptor set layout");
    }
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
    VkResult result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create pipeline layout: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to create pipeline layout");
    }

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

    result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create graphics pipeline: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to create graphics pipeline");
    }
}

void VulkanInitializer::createDescriptorPoolAndSet(VkDevice device, VkDescriptorSetLayout descriptorSetLayout,
                                                  VkDescriptorPool& descriptorPool, VkDescriptorSet& descriptorSet,
                                                  VkSampler& sampler, VkBuffer uniformBuffer, VkImageView storageImageView,
                                                  VkAccelerationStructureKHR topLevelAS) {
    LOG_DEBUG_CAT("Vulkan", "Creating descriptor pool and set", std::source_location::current());

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
    VkResult result = vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create descriptor pool: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to create descriptor pool");
    }

    VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &descriptorSetLayout
    };
    result = vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to allocate descriptor set: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to allocate descriptor set");
    }

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
    result = vkCreateSampler(device, &samplerInfo, nullptr, &sampler);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create sampler: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to create sampler");
    }

    VkDescriptorBufferInfo bufferInfo{
        .buffer = uniformBuffer,
        .offset = 0,
        .range = sizeof(UniversalEquation::UniformBufferObject)
    };
    VkWriteDescriptorSetAccelerationStructureKHR asInfo{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .pNext = nullptr,
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &topLevelAS
    };
    VkDescriptorImageInfo imageInfo{
        .sampler = sampler,
        .imageView = storageImageView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };
    VkWriteDescriptorSet descriptorWrites[3] = {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = &asInfo,
            .dstSet = descriptorSet,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            .pImageInfo = nullptr,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = descriptorSet,
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &imageInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = descriptorSet,
            .dstBinding = 2,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pImageInfo = nullptr,
            .pBufferInfo = &bufferInfo,
            .pTexelBufferView = nullptr
        }
    };
    vkUpdateDescriptorSets(device, 3, descriptorWrites, 0, nullptr);
}

void VulkanInitializer::createStorageImage(VkDevice device, VkPhysicalDevice physicalDevice, VkImage& storageImage,
                                          VkDeviceMemory& storageImageMemory, VkImageView& storageImageView,
                                          uint32_t width, uint32_t height) {
    LOG_DEBUG_CAT("Vulkan", "Creating storage image with width={}, height={}", std::source_location::current(), width, height);
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
    VkResult result = vkCreateImage(device, &imageInfo, nullptr, &storageImage);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create storage image: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to create storage image");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, storageImage, &memRequirements);
    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    result = vkAllocateMemory(device, &allocInfo, nullptr, &storageImageMemory);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to allocate storage image memory: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to allocate storage image memory");
    }
    vkBindImageMemory(device, storageImage, storageImageMemory, 0);

    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = storageImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };
    result = vkCreateImageView(device, &viewInfo, nullptr, &storageImageView);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create storage image view: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to create storage image view");
    }
}

VulkanSwapchainManager::VulkanSwapchainManager(VulkanContext& context, VkSurfaceKHR surface)
    : context_(context), swapchain_(VK_NULL_HANDLE), imageCount_(0), swapchainImageFormat_(VK_FORMAT_UNDEFINED), swapchainExtent_{0, 0} {
    LOG_INFO_CAT("VulkanSwapchain", "Constructing VulkanSwapchainManager with context.device={:p}, surface={:p}",
                 std::source_location::current(), reinterpret_cast<void*>(context_.device), reinterpret_cast<void*>(surface));
    context_.surface = surface;
}

VulkanSwapchainManager::~VulkanSwapchainManager() {
    LOG_INFO_CAT("VulkanSwapchain", "Destroying VulkanSwapchainManager", std::source_location::current());
    cleanupSwapchain();
}

void VulkanSwapchainManager::initializeSwapchain(int width, int height) {
    LOG_DEBUG_CAT("VulkanSwapchain", "Initializing swapchain with width={}, height={}",
                  std::source_location::current(), width, height);

    VkSurfaceCapabilitiesKHR capabilities;
    VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context_.physicalDevice, context_.surface, &capabilities);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanSwapchain", "Failed to get surface capabilities: result={}",
                      std::source_location::current(), result);
        throw std::runtime_error("Failed to get surface capabilities");
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

    VkExtent2D extent;
    if (capabilities.currentExtent.width != UINT32_MAX) {
        extent = capabilities.currentExtent;
    } else {
        extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
        extent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, extent.width));
        extent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, extent.height));
    }

    imageCount_ = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount_ > capabilities.maxImageCount) {
        imageCount_ = capabilities.maxImageCount;
    }

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

    result = vkCreateSwapchainKHR(context_.device, &createInfo, nullptr, &swapchain_);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanSwapchain", "Failed to create swapchain: result={}",
                      std::source_location::current(), result);
        throw std::runtime_error("Failed to create swapchain");
    }
    LOG_DEBUG_CAT("VulkanSwapchain", "Created swapchain: swapchain={:p}",
                  std::source_location::current(), reinterpret_cast<void*>(swapchain_));

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
            .components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };
        result = vkCreateImageView(context_.device, &viewInfo, nullptr, &swapchainImageViews_[i]);
        if (result != VK_SUCCESS) {
            LOG_ERROR_CAT("VulkanSwapchain", "Failed to create image view {}: result={}",
                          std::source_location::current(), i, result);
            throw std::runtime_error("Failed to create image view");
        }
        LOG_DEBUG_CAT("VulkanSwapchain", "Created image view {}: imageView={:p}",
                      std::source_location::current(), i, reinterpret_cast<void*>(swapchainImageViews_[i]));
    }

    LOG_DEBUG_CAT("VulkanSwapchain", "Created image view {:p}",
                  std::source_location::current(), reinterpret_cast<void*>(swapchainImageViews_.back()));

    swapchainExtent_ = extent;
    swapchainImageFormat_ = surfaceFormat.format;
    LOG_DEBUG_CAT("VulkanSwapchain", "Swapchain initialized with format={}, extent=[{}, {}]",
                  std::source_location::current(), swapchainImageFormat_, extent.width, extent.height);
}

void VulkanSwapchainManager::cleanupSwapchain() {
    LOG_DEBUG_CAT("VulkanSwapchain", "Cleaning up swapchain", std::source_location::current());
    if (context_.device == VK_NULL_HANDLE) {
        LOG_WARNING_CAT("VulkanSwapchain", "Device is null, skipping cleanup", std::source_location::current());
        return;
    }

    for (auto imageView : swapchainImageViews_) {
        if (imageView != VK_NULL_HANDLE) {
            LOG_DEBUG_CAT("VulkanSwapchain", "Destroying imageView={:p}", std::source_location::current(), reinterpret_cast<void*>(imageView));
            vkDestroyImageView(context_.device, imageView, nullptr);
        }
    }
    swapchainImageViews_.clear();
    swapchainImages_.clear();

    if (swapchain_ != VK_NULL_HANDLE) {
        LOG_DEBUG_CAT("VulkanSwapchain", "Destroying swapchain={:p}", std::source_location::current(), reinterpret_cast<void*>(swapchain_));
        vkDestroySwapchainKHR(context_.device, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
}

void VulkanSwapchainManager::handleResize(int width, int height) {
    LOG_DEBUG_CAT("VulkanSwapchain", "Handling resize to width={}, height={}",
                  std::source_location::current(), width, height);
    cleanupSwapchain();
    initializeSwapchain(width, height);
}

VulkanBufferManager::VulkanBufferManager(VulkanContext& context) : context_(context) {
    LOG_INFO_CAT("VulkanBuffer", "Constructing VulkanBufferManager with context.device={:p}",
                 std::source_location::current(), reinterpret_cast<void*>(context_.device));
}

VulkanBufferManager::~VulkanBufferManager() {
    LOG_INFO_CAT("VulkanBuffer", "Destroying VulkanBufferManager", std::source_location::current());
    cleanupBuffers();
}

void VulkanBufferManager::initializeBuffers(std::span<const glm::vec3> vertices, std::span<const uint32_t> indices) {
    LOG_DEBUG_CAT("VulkanBuffer", "Initializing buffers with vertex count={}, index count={}",
                  std::source_location::current(), vertices.size(), indices.size());

    VkDeviceSize vertexBufferSize = sizeof(glm::vec3) * vertices.size();
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, vertexBufferSize,
                                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(context_.device, stagingBufferMemory, 0, vertexBufferSize, 0, &data);
    memcpy(data, vertices.data(), vertexBufferSize);
    vkUnmapMemory(context_.device, stagingBufferMemory);

    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, vertexBufferSize,
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                   vertexBuffer_, vertexBufferMemory_);

    copyBuffer(stagingBuffer, vertexBuffer_, vertexBufferSize);

    vkDestroyBuffer(context_.device, stagingBuffer, nullptr);
    vkFreeMemory(context_.device, stagingBufferMemory, nullptr);

    VkDeviceSize indexBufferSize = sizeof(uint32_t) * indices.size();
    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, indexBufferSize,
                                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   stagingBuffer, stagingBufferMemory);

    vkMapMemory(context_.device, stagingBufferMemory, 0, indexBufferSize, 0, &data);
    memcpy(data, indices.data(), indexBufferSize);
    vkUnmapMemory(context_.device, stagingBufferMemory);

    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, indexBufferSize,
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                   indexBuffer_, indexBufferMemory_);

    copyBuffer(stagingBuffer, indexBuffer_, indexBufferSize);

    vkDestroyBuffer(context_.device, stagingBuffer, nullptr);
    vkFreeMemory(context_.device, stagingBufferMemory, nullptr);

    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, sizeof(UniversalEquation::UniformBufferObject),
                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   uniformBuffer_, uniformBufferMemory_);
}

void VulkanBufferManager::cleanupBuffers() {
    LOG_DEBUG_CAT("VulkanBuffer", "Cleaning up buffers", std::source_location::current());
    if (context_.device == VK_NULL_HANDLE) {
        LOG_WARNING_CAT("VulkanBuffer", "Device is null, skipping cleanup", std::source_location::current());
        return;
    }

    if (vertexBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(context_.device, vertexBuffer_, nullptr);
        vertexBuffer_ = VK_NULL_HANDLE;
    }
    if (vertexBufferMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(context_.device, vertexBufferMemory_, nullptr);
        vertexBufferMemory_ = VK_NULL_HANDLE;
    }
    if (indexBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(context_.device, indexBuffer_, nullptr);
        indexBuffer_ = VK_NULL_HANDLE;
    }
    if (indexBufferMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(context_.device, indexBufferMemory_, nullptr);
        indexBufferMemory_ = VK_NULL_HANDLE;
    }
    if (uniformBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(context_.device, uniformBuffer_, nullptr);
        uniformBuffer_ = VK_NULL_HANDLE;
    }
    if (uniformBufferMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(context_.device, uniformBufferMemory_, nullptr);
        uniformBufferMemory_ = VK_NULL_HANDLE;
    }
}

void VulkanBufferManager::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = context_.commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VkCommandBuffer commandBuffer;
    VkResult result = vkAllocateCommandBuffers(context_.device, &allocInfo, &commandBuffer);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanBuffer", "Failed to allocate command buffer: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to allocate command buffer");
    }

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanBuffer", "Failed to begin command buffer: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to begin command buffer");
    }

    VkBufferCopy copyRegion{.srcOffset = 0, .dstOffset = 0, .size = size};
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    result = vkEndCommandBuffer(commandBuffer);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanBuffer", "Failed to end command buffer: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to end command buffer");
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
    result = vkQueueSubmit(context_.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanBuffer", "Failed to submit copy command: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to submit copy command");
    }

    vkQueueWaitIdle(context_.graphicsQueue);
    vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &commandBuffer);
}

VulkanPipelineManager::VulkanPipelineManager(VulkanContext& context, int width, int height)
    : context_(context), width_(width), height_(height) {
    LOG_INFO_CAT("VulkanPipeline", "Constructing VulkanPipelineManager with width={}, height={}",
                 std::source_location::current(), width, height);
}

VulkanPipelineManager::~VulkanPipelineManager() {
    LOG_INFO_CAT("VulkanPipeline", "Destroying VulkanPipelineManager", std::source_location::current());
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
        LOG_ERROR_CAT("VulkanPipeline", "Failed to create pipeline layout: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to create pipeline layout");
    }
}

void VulkanPipelineManager::createGraphicsPipeline() {
    vertexShaderModule_ = VulkanInitializer::loadShader(context_.device, "shaders/vert.spv");
    fragmentShaderModule_ = VulkanInitializer::loadShader(context_.device, "shaders/frag.spv");
    VulkanInitializer::createGraphicsPipeline(context_.device, renderPass_, graphicsPipeline_, pipelineLayout_,
                                             descriptorSetLayout_, width_, height_, vertexShaderModule_, fragmentShaderModule_);
}

void VulkanPipelineManager::cleanupPipeline() {
    LOG_DEBUG_CAT("VulkanPipeline", "Cleaning up pipeline", std::source_location::current());
    if (context_.device == VK_NULL_HANDLE) {
        LOG_WARNING_CAT("VulkanPipeline", "Device is null, skipping cleanup", std::source_location::current());
        return;
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
    if (vertexShaderModule_ != VK_NULL_HANDLE) {
        vkDestroyShaderModule(context_.device, vertexShaderModule_, nullptr);
        vertexShaderModule_ = VK_NULL_HANDLE;
    }
    if (fragmentShaderModule_ != VK_NULL_HANDLE) {
        vkDestroyShaderModule(context_.device, fragmentShaderModule_, nullptr);
        fragmentShaderModule_ = VK_NULL_HANDLE;
    }
    if (renderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(context_.device, renderPass_, nullptr);
        renderPass_ = VK_NULL_HANDLE;
    }
}

VulkanRenderer::VulkanRenderer(VkInstance instance, VkSurfaceKHR surface,
                              std::span<const glm::vec3> vertices, std::span<const uint32_t> indices,
                              int width, int height)
    : width_(width), height_(height) {
    LOG_INFO_CAT("Vulkan", "Initializing VulkanRenderer with width={}, height={}", std::source_location::current(), width, height);

    context_.instance = instance;
    context_.surface = surface;
    VulkanInitializer::initializeVulkan(context_, width, height);
    VulkanInitializer::createSwapchain(context_);
    VulkanInitializer::createImageViews(context_);

    swapchainManager_ = std::make_unique<VulkanSwapchainManager>(context_, surface);
    swapchainManager_->initializeSwapchain(width, height);

    bufferManager_ = std::make_unique<VulkanBufferManager>(context_);
    bufferManager_->initializeBuffers(vertices, indices);

    pipelineManager_ = std::make_unique<VulkanPipelineManager>(context_, width, height);
    pipelineManager_->createRenderPass();
    pipelineManager_->createPipelineLayout();
    pipelineManager_->createGraphicsPipeline();

    VulkanInitializer::createStorageImage(context_.device, context_.physicalDevice, context_.storageImage,
                                         context_.storageImageMemory, context_.storageImageView, width, height);
    VulkanInitializer::createDescriptorPoolAndSet(context_.device, pipelineManager_->getDescriptorSetLayout(),
                                                 context_.descriptorPool, context_.descriptorSet, context_.sampler,
                                                 bufferManager_->getUniformBuffer(), context_.storageImageView, context_.topLevelAS);

    framebuffers_.resize(swapchainManager_->getSwapchainImageViews().size());
    for (size_t i = 0; i < framebuffers_.size(); ++i) {
        VkFramebufferCreateInfo framebufferInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderPass = pipelineManager_->getRenderPass(),
            .attachmentCount = 1,
            .pAttachments = &swapchainManager_->getSwapchainImageViews()[i],
            .width = static_cast<uint32_t>(width),
            .height = static_cast<uint32_t>(height),
            .layers = 1
        };
        VkResult result = vkCreateFramebuffer(context_.device, &framebufferInfo, nullptr, &framebuffers_[i]);
        if (result != VK_SUCCESS) {
            LOG_ERROR_CAT("Vulkan", "Failed to create framebuffer {}: result={}", std::source_location::current(), i, result);
            throw std::runtime_error("Failed to create framebuffer");
        }
    }

    commandBuffers_.resize(framebuffers_.size());
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = context_.commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<uint32_t>(commandBuffers_.size())
    };
    VkResult result = vkAllocateCommandBuffers(context_.device, &allocInfo, commandBuffers_.data());
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to allocate command buffers: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to allocate command buffers");
    }

    VkSemaphoreCreateInfo semaphoreInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = nullptr, .flags = 0};
    VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .pNext = nullptr, .flags = VK_FENCE_CREATE_SIGNALED_BIT};
    result = vkCreateSemaphore(context_.device, &semaphoreInfo, nullptr, &imageAvailableSemaphore_);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create image available semaphore: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to create semaphore");
    }
    result = vkCreateSemaphore(context_.device, &semaphoreInfo, nullptr, &renderFinishedSemaphore_);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create render finished semaphore: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to create semaphore");
    }
    result = vkCreateFence(context_.device, &fenceInfo, nullptr, &inFlightFence_);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create fence: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to create fence");
    }
}

VulkanRenderer::~VulkanRenderer() {
    LOG_INFO_CAT("Vulkan", "Destroying VulkanRenderer", std::source_location::current());

    if (imageAvailableSemaphore_ != VK_NULL_HANDLE) {
        vkDestroySemaphore(context_.device, imageAvailableSemaphore_, nullptr);
    }
    if (renderFinishedSemaphore_ != VK_NULL_HANDLE) {
        vkDestroySemaphore(context_.device, renderFinishedSemaphore_, nullptr);
    }
    if (inFlightFence_ != VK_NULL_HANDLE) {
        vkDestroyFence(context_.device, inFlightFence_, nullptr);
    }
    for (auto framebuffer : framebuffers_) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(context_.device, framebuffer, nullptr);
        }
    }
    if (context_.commandPool != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(context_.device, context_.commandPool, static_cast<uint32_t>(commandBuffers_.size()), commandBuffers_.data());
    }
    if (context_.descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(context_.device, context_.descriptorPool, nullptr);
    }
    if (context_.sampler != VK_NULL_HANDLE) {
        vkDestroySampler(context_.device, context_.sampler, nullptr);
    }
    if (context_.storageImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(context_.device, context_.storageImageView, nullptr);
    }
    if (context_.storageImage != VK_NULL_HANDLE) {
        vkDestroyImage(context_.device, context_.storageImage, nullptr);
    }
    if (context_.storageImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context_.device, context_.storageImageMemory, nullptr);
    }
    if (context_.topLevelAS != VK_NULL_HANDLE) {
        // Placeholder for acceleration structure cleanup
    }
    if (context_.topLevelASBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(context_.device, context_.topLevelASBuffer, nullptr);
    }
    if (context_.topLevelASBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context_.device, context_.topLevelASBufferMemory, nullptr);
    }
    if (context_.bottomLevelAS != VK_NULL_HANDLE) {
        // Placeholder for acceleration structure cleanup
    }
    if (context_.bottomLevelASBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(context_.device, context_.bottomLevelASBuffer, nullptr);
    }
    if (context_.bottomLevelASBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context_.device, context_.bottomLevelASBufferMemory, nullptr);
    }
    if (context_.rayTracingPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(context_.device, context_.rayTracingPipeline, nullptr);
    }
    if (context_.rayTracingPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(context_.device, context_.rayTracingPipelineLayout, nullptr);
    }
    if (context_.rayTracingDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(context_.device, context_.rayTracingDescriptorSetLayout, nullptr);
    }
    if (context_.shaderBindingTable != VK_NULL_HANDLE) {
        vkDestroyBuffer(context_.device, context_.shaderBindingTable, nullptr);
    }
    if (context_.shaderBindingTableMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context_.device, context_.shaderBindingTableMemory, nullptr);
    }
    if (context_.commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(context_.device, context_.commandPool, nullptr);
    }
    if (context_.device != VK_NULL_HANDLE) {
        vkDestroyDevice(context_.device, nullptr);
    }
}

void VulkanRenderer::renderFrame(const AMOURANTH& camera) {
    LOG_DEBUG_CAT("Vulkan", "Rendering frame", std::source_location::current());

    vkWaitForFences(context_.device, 1, &inFlightFence_, VK_TRUE, UINT64_MAX);
    vkResetFences(context_.device, 1, &inFlightFence_);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(context_.device, swapchainManager_->getSwapchain(), UINT64_MAX,
                                            imageAvailableSemaphore_, VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        swapchainManager_->handleResize(width_, height_);
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        LOG_ERROR_CAT("Vulkan", "Failed to acquire swapchain image: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to acquire swapchain image");
    }

    UniversalEquation::UniformBufferObject ubo{};
    // Populate ubo with camera data (example, adjust according to AMOURANTH class)
    ubo.view = camera.getViewMatrix();
    ubo.proj = camera.getProjectionMatrix();
    ubo.model = glm::mat4(1.0f); // Example model matrix

    void* data;
    vkMapMemory(context_.device, bufferManager_->getUniformBufferMemory(), 0, sizeof(UniversalEquation::UniformBufferObject), 0, &data);
    memcpy(data, &ubo, sizeof(UniversalEquation::UniformBufferObject));
    vkUnmapMemory(context_.device, bufferManager_->getUniformBufferMemory());

    VkCommandBuffer commandBuffer = commandBuffers_[imageIndex];
    vkResetCommandBuffer(commandBuffer, 0);
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pInheritanceInfo = nullptr
    };
    result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to begin command buffer: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to begin command buffer");
    }

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = pipelineManager_->getRenderPass(),
        .framebuffer = framebuffers_[imageIndex],
        .renderArea = {{0, 0}, {static_cast<uint32_t>(width_), static_cast<uint32_t>(height_)}},
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
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(bufferManager_->getIndexCount()), 1, 0, 0, 0);
    vkCmdEndRenderPass(commandBuffer);

    result = vkEndCommandBuffer(commandBuffer);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to end command buffer: result={}", std::source_location::current(), result);
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
    result = vkQueueSubmit(context_.graphicsQueue, 1, &submitInfo, inFlightFence_);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to submit draw command: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to submit draw command");
    }

    VkSwapchainKHR swapchain = swapchainManager_->getSwapchain();
    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &renderFinishedSemaphore_,
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &imageIndex,
        .pResults = nullptr
    };
    result = vkQueuePresentKHR(context_.presentQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        swapchainManager_->handleResize(width_, height_);
    } else if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to present swapchain image: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to present swapchain image");
    }
}

void VulkanRenderer::handleResize(int width, int height) {
    LOG_INFO_CAT("Vulkan", "Handling resize to width={}, height={}", std::source_location::current(), width, height);
    width_ = width;
    height_ = height;
    swapchainManager_->handleResize(width, height);

    for (auto framebuffer : framebuffers_) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(context_.device, framebuffer, nullptr);
        }
    }
    framebuffers_.resize(swapchainManager_->getSwapchainImageViews().size());
    for (size_t i = 0; i < framebuffers_.size(); ++i) {
        VkFramebufferCreateInfo framebufferInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderPass = pipelineManager_->getRenderPass(),
            .attachmentCount = 1,
            .pAttachments = &swapchainManager_->getSwapchainImageViews()[i],
            .width = static_cast<uint32_t>(width),
            .height = static_cast<uint32_t>(height),
            .layers = 1
        };
        VkResult result = vkCreateFramebuffer(context_.device, &framebufferInfo, nullptr, &framebuffers_[i]);
        if (result != VK_SUCCESS) {
            LOG_ERROR_CAT("Vulkan", "Failed to create framebuffer {}: result={}", std::source_location::current(), i, result);
            throw std::runtime_error("Failed to create framebuffer");
        }
    }
}