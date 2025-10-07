// Vulkan_init.cpp
// AMOURANTH RTX Engine, October 2025 - VulkanRenderer implementation for Vulkan initialization and rendering.
// Initializes Vulkan resources, including swapchain, pipeline, and geometry buffers (vertex, index, quad, voxel).
// Thread-safe with no mutexes; designed for Windows/Linux (X11/Wayland).
// Dependencies: Vulkan 1.3+, GLM, C++20 standard library.
// Zachary Geurts 2025

#include "engine/Vulkan_init.hpp"
#include <stdexcept>
#include <algorithm>
#include <limits>

namespace {

void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
                 VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
    };
    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    uint32_t memoryTypeIndex = std::numeric_limits<uint32_t>::max();
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((memRequirements.memoryTypeBits & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            memoryTypeIndex = i;
            break;
        }
    }
    if (memoryTypeIndex == std::numeric_limits<uint32_t>::max()) {
        vkDestroyBuffer(device, buffer, nullptr);
        throw std::runtime_error("Failed to find suitable memory type");
    }

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = memoryTypeIndex,
    };
    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        vkDestroyBuffer(device, buffer, nullptr);
        throw std::runtime_error("Failed to allocate buffer memory");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

void copyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkBufferCopy copyRegion = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = size,
    };
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .pWaitDstStageMask = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = nullptr,
    };
    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void createVulkanResources(VulkanContext& context, VkInstance instance, VkSurfaceKHR surface,
                          std::span<const glm::vec3> vertices, std::span<const uint32_t> indices,
                          VkShaderModule vertShaderModule, VkShaderModule fragShaderModule,
                          int width, int height) {
    // Select physical device
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        throw std::runtime_error("No Vulkan physical devices found");
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    context.physicalDevice = devices[0]; // Select first device (simplified)
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(context.physicalDevice, &deviceProperties);

    // Find queue families
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(context.physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(context.physicalDevice, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            context.graphicsFamily = i;
        }
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(context.physicalDevice, i, surface, &presentSupport);
        if (presentSupport) {
            context.presentFamily = i;
        }
    }

    // Create logical device
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queueFamilyIndex = context.graphicsFamily,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority,
    };
    queueCreateInfos.push_back(queueCreateInfo);
    if (context.graphicsFamily != context.presentFamily) {
        queueCreateInfo.queueFamilyIndex = context.presentFamily;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures = {};
    const char* extensionNames[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = extensionNames,
        .pEnabledFeatures = &deviceFeatures,
    };
    if (vkCreateDevice(context.physicalDevice, &createInfo, nullptr, &context.device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device");
    }

    vkGetDeviceQueue(context.device, context.graphicsFamily, 0, &context.graphicsQueue);
    vkGetDeviceQueue(context.device, context.presentFamily, 0, &context.presentQueue);

    // Create swapchain
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context.physicalDevice, surface, &capabilities);

    VkSwapchainCreateInfoKHR swapchainCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .surface = surface,
        .minImageCount = capabilities.minImageCount + 1,
        .imageFormat = VK_FORMAT_B8G8R8A8_SRGB,
        .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)},
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };
    if (vkCreateSwapchainKHR(context.device, &swapchainCreateInfo, nullptr, &context.swapchain) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swapchain");
    }

    // Get swapchain images
    uint32_t imageCount = 0;
    vkGetSwapchainImagesKHR(context.device, context.swapchain, &imageCount, nullptr);
    context.swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(context.device, context.swapchain, &imageCount, context.swapchainImages.data());

    // Create image views
    context.swapchainImageViews.resize(imageCount);
    for (size_t i = 0; i < imageCount; ++i) {
        VkImageViewCreateInfo viewInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = context.swapchainImages[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = VK_FORMAT_B8G8R8A8_SRGB,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
        if (vkCreateImageView(context.device, &viewInfo, nullptr, &context.swapchainImageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image view");
        }
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
    if (vkCreateRenderPass(context.device, &renderPassInfo, nullptr, &context.renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass");
    }

    // Create pipeline
    VkPipelineShaderStageCreateInfo shaderStages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertShaderModule,
            .pName = "main",
            .pSpecializationInfo = nullptr,
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragShaderModule,
            .pName = "main",
            .pSpecializationInfo = nullptr,
        },
    };
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .vertexBindingDescriptionCount = 0,
        .pVertexBindingDescriptions = nullptr,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions = nullptr,
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
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 0,
        .pSetLayouts = nullptr,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr,
    };
    if (vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr, &context.pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout");
    }
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
        .layout = context.pipelineLayout,
        .renderPass = context.renderPass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1,
    };
    if (vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &context.pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline");
    }

    // Create framebuffers
    context.swapchainFramebuffers.resize(context.swapchainImageViews.size());
    for (size_t i = 0; i < context.swapchainFramebuffers.size(); ++i) {
        VkFramebufferCreateInfo framebufferInfo = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderPass = context.renderPass,
            .attachmentCount = 1,
            .pAttachments = &context.swapchainImageViews[i],
            .width = static_cast<uint32_t>(width),
            .height = static_cast<uint32_t>(height),
            .layers = 1,
        };
        if (vkCreateFramebuffer(context.device, &framebufferInfo, nullptr, &context.swapchainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer");
        }
    }

    // Create command pool
    VkCommandPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queueFamilyIndex = context.graphicsFamily,
    };
    if (vkCreateCommandPool(context.device, &poolInfo, nullptr, &context.commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool");
    }

    // Create command buffers
    context.commandBuffers.resize(context.swapchainFramebuffers.size());
    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = context.commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<uint32_t>(context.commandBuffers.size()),
    };
    if (vkAllocateCommandBuffers(context.device, &allocInfo, context.commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers");
    }

    // Create vertex buffer
    VkDeviceSize vertexBufferSize = vertices.size() * sizeof(glm::vec3);
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(context.device, context.physicalDevice, vertexBufferSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(context.device, stagingBufferMemory, 0, vertexBufferSize, 0, &data);
    memcpy(data, vertices.data(), vertexBufferSize);
    vkUnmapMemory(context.device, stagingBufferMemory);

    createBuffer(context.device, context.physicalDevice, vertexBufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 context.vertexBuffer, context.vertexBufferMemory);

    copyBuffer(context.device, context.commandPool, context.graphicsQueue, stagingBuffer, context.vertexBuffer, vertexBufferSize);

    vkDestroyBuffer(context.device, stagingBuffer, nullptr);
    vkFreeMemory(context.device, stagingBufferMemory, nullptr);

    // Create index buffer
    VkDeviceSize indexBufferSize = indices.size() * sizeof(uint32_t);
    createBuffer(context.device, context.physicalDevice, indexBufferSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);

    vkMapMemory(context.device, stagingBufferMemory, 0, indexBufferSize, 0, &data);
    memcpy(data, indices.data(), indexBufferSize);
    vkUnmapMemory(context.device, stagingBufferMemory);

    createBuffer(context.device, context.physicalDevice, indexBufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 context.indexBuffer, context.indexBufferMemory);

    copyBuffer(context.device, context.commandPool, context.graphicsQueue, stagingBuffer, context.indexBuffer, indexBufferSize);

    vkDestroyBuffer(context.device, stagingBuffer, nullptr);
    vkFreeMemory(context.device, stagingBufferMemory, nullptr);

    // Create semaphores and fences
    context.imageAvailableSemaphores.resize(context.swapchainImages.size());
    context.renderFinishedSemaphores.resize(context.swapchainImages.size());
    context.inFlightFences.resize(context.swapchainImages.size());
    VkSemaphoreCreateInfo semaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
    };
    VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    for (size_t i = 0; i < context.swapchainImages.size(); ++i) {
        if (vkCreateSemaphore(context.device, &semaphoreInfo, nullptr, &context.imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(context.device, &semaphoreInfo, nullptr, &context.renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(context.device, &fenceInfo, nullptr, &context.inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create synchronization objects");
        }
    }

    context.vertShaderModule = vertShaderModule;
    context.fragShaderModule = fragShaderModule;
}

void cleanupVulkanResources(VulkanContext& context) {
    if (!context.device) {
        return;
    }

    for (auto semaphore : context.imageAvailableSemaphores) {
        vkDestroySemaphore(context.device, semaphore, nullptr);
    }
    for (auto semaphore : context.renderFinishedSemaphores) {
        vkDestroySemaphore(context.device, semaphore, nullptr);
    }
    for (auto fence : context.inFlightFences) {
        vkDestroyFence(context.device, fence, nullptr);
    }
    vkDestroyCommandPool(context.device, context.commandPool, nullptr);
    for (auto framebuffer : context.swapchainFramebuffers) {
        vkDestroyFramebuffer(context.device, framebuffer, nullptr);
    }
    vkDestroyPipeline(context.device, context.pipeline, nullptr);
    vkDestroyPipelineLayout(context.device, context.pipelineLayout, nullptr);
    vkDestroyRenderPass(context.device, context.renderPass, nullptr);
    for (auto imageView : context.swapchainImageViews) {
        vkDestroyImageView(context.device, imageView, nullptr);
    }
    vkDestroySwapchainKHR(context.device, context.swapchain, nullptr);
    vkDestroyBuffer(context.device, context.vertexBuffer, nullptr);
    vkFreeMemory(context.device, context.vertexBufferMemory, nullptr);
    vkDestroyBuffer(context.device, context.indexBuffer, nullptr);
    vkFreeMemory(context.device, context.indexBufferMemory, nullptr);
    vkDestroyBuffer(context.device, context.quadVertexBuffer, nullptr);
    vkFreeMemory(context.device, context.quadVertexBufferMemory, nullptr);
    vkDestroyBuffer(context.device, context.quadIndexBuffer, nullptr);
    vkFreeMemory(context.device, context.quadIndexBufferMemory, nullptr);
    vkDestroyBuffer(context.device, context.voxelVertexBuffer, nullptr);
    vkFreeMemory(context.device, context.voxelVertexBufferMemory, nullptr);
    vkDestroyBuffer(context.device, context.voxelIndexBuffer, nullptr);
    vkFreeMemory(context.device, context.voxelIndexBufferMemory, nullptr);
    vkDestroyDevice(context.device, nullptr);
}

} // namespace

VulkanRenderer::VulkanRenderer(
    VkInstance instance, VkSurfaceKHR surface,
    std::span<const glm::vec3> vertices, std::span<const uint32_t> indices,
    VkShaderModule vertShaderModule, VkShaderModule fragShaderModule,
    int width, int height)
    : instance_(instance), surface_(surface), currentImageIndex_(0) {
    createVulkanResources(context_, instance, surface, vertices, indices, vertShaderModule, fragShaderModule, width, height);
}

VulkanRenderer::~VulkanRenderer() {
    cleanupVulkan();
}

const VulkanContext& VulkanRenderer::getContext() const {
    return context_;
}

void VulkanRenderer::initializeVulkan(
    std::span<const glm::vec3> vertices, std::span<const uint32_t> indices,
    VkShaderModule vertShaderModule, VkShaderModule fragShaderModule,
    int width, int height) {
    cleanupVulkan();
    createVulkanResources(context_, instance_, surface_, vertices, indices, vertShaderModule, fragShaderModule, width, height);
}

void VulkanRenderer::initializeQuadBuffers(std::span<const glm::vec3> quadVertices, std::span<const uint32_t> quadIndices) {
    VkDeviceSize vertexBufferSize = quadVertices.size() * sizeof(glm::vec3);
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(context_.device, context_.physicalDevice, vertexBufferSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(context_.device, stagingBufferMemory, 0, vertexBufferSize, 0, &data);
    memcpy(data, quadVertices.data(), vertexBufferSize);
    vkUnmapMemory(context_.device, stagingBufferMemory);

    createBuffer(context_.device, context_.physicalDevice, vertexBufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 context_.quadVertexBuffer, context_.quadVertexBufferMemory);

    copyBuffer(context_.device, context_.commandPool, context_.graphicsQueue, stagingBuffer, context_.quadVertexBuffer, vertexBufferSize);

    vkDestroyBuffer(context_.device, stagingBuffer, nullptr);
    vkFreeMemory(context_.device, stagingBufferMemory, nullptr);

    VkDeviceSize indexBufferSize = quadIndices.size() * sizeof(uint32_t);
    createBuffer(context_.device, context_.physicalDevice, indexBufferSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);

    vkMapMemory(context_.device, stagingBufferMemory, 0, indexBufferSize, 0, &data);
    memcpy(data, quadIndices.data(), indexBufferSize);
    vkUnmapMemory(context_.device, stagingBufferMemory);

    createBuffer(context_.device, context_.physicalDevice, indexBufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 context_.quadIndexBuffer, context_.quadIndexBufferMemory);

    copyBuffer(context_.device, context_.commandPool, context_.graphicsQueue, stagingBuffer, context_.quadIndexBuffer, indexBufferSize);

    vkDestroyBuffer(context_.device, stagingBuffer, nullptr);
    vkFreeMemory(context_.device, stagingBufferMemory, nullptr);
}

void VulkanRenderer::initializeVoxelBuffers(std::span<const glm::vec3> voxelVertices, std::span<const uint32_t> voxelIndices) {
    VkDeviceSize vertexBufferSize = voxelVertices.size() * sizeof(glm::vec3);
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(context_.device, context_.physicalDevice, vertexBufferSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(context_.device, stagingBufferMemory, 0, vertexBufferSize, 0, &data);
    memcpy(data, voxelVertices.data(), vertexBufferSize);
    vkUnmapMemory(context_.device, stagingBufferMemory);

    createBuffer(context_.device, context_.physicalDevice, vertexBufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 context_.voxelVertexBuffer, context_.voxelVertexBufferMemory);

    copyBuffer(context_.device, context_.commandPool, context_.graphicsQueue, stagingBuffer, context_.voxelVertexBuffer, vertexBufferSize);

    vkDestroyBuffer(context_.device, stagingBuffer, nullptr);
    vkFreeMemory(context_.device, stagingBufferMemory, nullptr);

    VkDeviceSize indexBufferSize = voxelIndices.size() * sizeof(uint32_t);
    createBuffer(context_.device, context_.physicalDevice, indexBufferSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);

    vkMapMemory(context_.device, stagingBufferMemory, 0, indexBufferSize, 0, &data);
    memcpy(data, voxelIndices.data(), indexBufferSize);
    vkUnmapMemory(context_.device, stagingBufferMemory);

    createBuffer(context_.device, context_.physicalDevice, indexBufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 context_.voxelIndexBuffer, context_.voxelIndexBufferMemory);

    copyBuffer(context_.device, context_.commandPool, context_.graphicsQueue, stagingBuffer, context_.voxelIndexBuffer, indexBufferSize);

    vkDestroyBuffer(context_.device, stagingBuffer, nullptr);
    vkFreeMemory(context_.device, stagingBufferMemory, nullptr);
}

void VulkanRenderer::beginFrame() {
    vkWaitForFences(context_.device, 1, &context_.inFlightFences[currentImageIndex_], VK_TRUE, UINT64_MAX);

    VkResult result = vkAcquireNextImageKHR(context_.device, context_.swapchain, UINT64_MAX,
                                            context_.imageAvailableSemaphores[currentImageIndex_], VK_NULL_HANDLE,
                                            &currentImageIndex_);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        throw std::runtime_error("Swapchain out of date");
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swapchain image");
    }

    vkResetFences(context_.device, 1, &context_.inFlightFences[currentImageIndex_]);

    vkResetCommandBuffer(context_.commandBuffers[currentImageIndex_], 0);
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pInheritanceInfo = nullptr,
    };
    if (vkBeginCommandBuffer(context_.commandBuffers[currentImageIndex_], &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin command buffer");
    }

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = context_.renderPass,
        .framebuffer = context_.swapchainFramebuffers[currentImageIndex_],
        .renderArea = {{0, 0}, {static_cast<uint32_t>(context_.swapchainImageViews.size()), static_cast<uint32_t>(context_.swapchainImageViews.size())}},
        .clearValueCount = 1,
        .pClearValues = &clearColor,
    };
    vkCmdBeginRenderPass(context_.commandBuffers[currentImageIndex_], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanRenderer::endFrame() {
    vkCmdEndRenderPass(context_.commandBuffers[currentImageIndex_]);
    if (vkEndCommandBuffer(context_.commandBuffers[currentImageIndex_]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to end command buffer");
    }

    VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &context_.imageAvailableSemaphores[currentImageIndex_],
        .pWaitDstStageMask = &waitStages,
        .commandBufferCount = 1,
        .pCommandBuffers = &context_.commandBuffers[currentImageIndex_],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &context_.renderFinishedSemaphores[currentImageIndex_],
    };
    if (vkQueueSubmit(context_.graphicsQueue, 1, &submitInfo, context_.inFlightFences[currentImageIndex_]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer");
    }

    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &context_.renderFinishedSemaphores[currentImageIndex_],
        .swapchainCount = 1,
        .pSwapchains = &context_.swapchain,
        .pImageIndices = &currentImageIndex_,
        .pResults = nullptr,
    };
    VkResult result = vkQueuePresentKHR(context_.presentQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Swapchain out of date");
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swapchain image");
    }

    currentImageIndex_ = (currentImageIndex_ + 1) % context_.swapchainImages.size();
}

void VulkanRenderer::handleResize(int width, int height) {
    vkDeviceWaitIdle(context_.device);
    cleanupVulkan();

    createVulkanResources(context_, instance_, surface_, {}, {}, context_.vertShaderModule, context_.fragShaderModule, width, height);
}

void VulkanRenderer::cleanupVulkan() {
    cleanupVulkanResources(context_);
}

VkBuffer VulkanRenderer::getVertexBuffer() const {
    return context_.vertexBuffer;
}

VkBuffer VulkanRenderer::getIndexBuffer() const {
    return context_.indexBuffer;
}

VkCommandBuffer VulkanRenderer::getCommandBuffer() const {
    return context_.commandBuffers.empty() ? VK_NULL_HANDLE : context_.commandBuffers[currentImageIndex_];
}

VkPipelineLayout VulkanRenderer::getPipelineLayout() const {
    return context_.pipelineLayout;
}

VkDescriptorSet VulkanRenderer::getDescriptorSet() const {
    return context_.descriptorSet;
}

uint32_t VulkanRenderer::getCurrentImageIndex() const {
    return currentImageIndex_;
}