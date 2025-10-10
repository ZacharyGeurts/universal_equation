#include "engine/Vulkan_init.hpp"
#include "engine/logging.hpp"
#include <stdexcept>
#include <fstream>

VulkanRenderer::VulkanRenderer(VkInstance instance, VkSurfaceKHR surface,
                              std::span<const glm::vec3> vertices, std::span<const uint32_t> indices,
                              int width, int height)
    : context_(), swapchainManager_(nullptr), pipelineManager_(nullptr), bufferManager_(nullptr),
      width_(width), height_(height) {
    LOG_INFO_CAT("Vulkan", "Constructing VulkanRenderer with instance={:p}, surface={:p}, width={}, height={}",
                 std::source_location::current(), instance, surface, width, height);

    context_.instance = instance;
    context_.surface = surface;

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
    context_.physicalDevice = devices[0];

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(context_.physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(context_.physicalDevice, &queueFamilyCount, queueFamilies.data());
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            context_.graphicsQueueFamilyIndex = i;
        }
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(context_.physicalDevice, i, surface, &presentSupport);
        if (presentSupport) {
            context_.presentQueueFamilyIndex = i;
        }
    }

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfos[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = context_.graphicsQueueFamilyIndex,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority
        },
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = context_.presentQueueFamilyIndex,
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
        .queueCreateInfoCount = static_cast<uint32_t>(context_.graphicsQueueFamilyIndex == context_.presentQueueFamilyIndex ? 1 : 2),
        .pQueueCreateInfos = queueCreateInfos,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = 3,
        .ppEnabledExtensionNames = deviceExtensions,
        .pEnabledFeatures = &deviceFeatures
    };
    VkResult result = vkCreateDevice(context_.physicalDevice, &deviceCreateInfo, nullptr, &context_.device);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create logical device: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to create logical device");
    }

    vkGetDeviceQueue(context_.device, context_.graphicsQueueFamilyIndex, 0, &context_.graphicsQueue);
    vkGetDeviceQueue(context_.device, context_.presentQueueFamilyIndex, 0, &context_.presentQueue);

    VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = context_.graphicsQueueFamilyIndex
    };
    result = vkCreateCommandPool(context_.device, &poolInfo, nullptr, &context_.commandPool);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create command pool: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to create command pool");
    }

    swapchainManager_ = std::make_unique<VulkanSwapchainManager>(context_, surface);
    pipelineManager_ = std::make_unique<VulkanPipelineManager>(context_, width, height);
    bufferManager_ = std::make_unique<VulkanBufferManager>(context_);

    swapchainManager_->initializeSwapchain(width, height);
    pipelineManager_->createRenderPass();
    pipelineManager_->createPipelineLayout();
    pipelineManager_->createGraphicsPipeline();
    bufferManager_->initializeBuffers(vertices, indices);

    const auto& imageViews = swapchainManager_->getSwapchainImageViews();
    framebuffers_.resize(imageViews.size());
    for (size_t i = 0; i < imageViews.size(); ++i) {
        VkFramebufferCreateInfo framebufferInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderPass = pipelineManager_->getRenderPass(),
            .attachmentCount = 1,
            .pAttachments = &imageViews[i],
            .width = swapchainManager_->getSwapchainExtent().width,
            .height = swapchainManager_->getSwapchainExtent().height,
            .layers = 1
        };
        result = vkCreateFramebuffer(context_.device, &framebufferInfo, nullptr, &framebuffers_[i]);
        if (result != VK_SUCCESS) {
            LOG_ERROR_CAT("Vulkan", "Failed to create framebuffer {}: result={}", std::source_location::current(), i, result);
            throw std::runtime_error("Failed to create framebuffer");
        }
    }

    commandBuffers_.resize(imageViews.size());
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = context_.commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<uint32_t>(commandBuffers_.size())
    };
    result = vkAllocateCommandBuffers(context_.device, &allocInfo, commandBuffers_.data());
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to allocate command buffers: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to allocate command buffers");
    }

    VkSemaphoreCreateInfo semaphoreInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0
    };
    VkFenceCreateInfo fenceInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };
    result = vkCreateSemaphore(context_.device, &semaphoreInfo, nullptr, &imageAvailableSemaphore_);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create image available semaphore: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to create image available semaphore");
    }
    result = vkCreateSemaphore(context_.device, &semaphoreInfo, nullptr, &renderFinishedSemaphore_);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create render finished semaphore: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to create render finished semaphore");
    }
    result = vkCreateFence(context_.device, &fenceInfo, nullptr, &inFlightFence_);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create fence: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to create fence");
    }

    VulkanInitializer::createAccelerationStructures(context_, vertices, indices);
    VulkanInitializer::createStorageImage(context_, width, height);
    VulkanInitializer::createRayTracingPipeline(context_);
    VulkanInitializer::createShaderBindingTable(context_);
}

VulkanRenderer::~VulkanRenderer() {
    if (context_.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(context_.device);
    }
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
        vkDestroyCommandPool(context_.device, context_.commandPool, nullptr);
    }
    if (context_.device != VK_NULL_HANDLE) {
        vkDestroyDevice(context_.device, nullptr);
    }
}

void VulkanRenderer::renderFrame(const AMOURANTH& camera) {
    vkWaitForFences(context_.device, 1, &inFlightFence_, VK_TRUE, UINT64_MAX);
    vkResetFences(context_.device, 1, &inFlightFence_);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(context_.device, swapchainManager_->getSwapchain(), UINT64_MAX,
                                            imageAvailableSemaphore_, VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        handleResize(width_, height_);
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        LOG_ERROR_CAT("Vulkan", "Failed to acquire swapchain image: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to acquire swapchain image");
    }

    vkResetCommandBuffer(commandBuffers_[imageIndex], 0);
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pInheritanceInfo = nullptr
    };
    result = vkBeginCommandBuffer(commandBuffers_[imageIndex], &beginInfo);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to begin command buffer: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to begin command buffer");
    }

    UniversalEquation::UniformBufferObject ubo{
        .model = glm::mat4(1.0f),
        .view = glm::lookAt(glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
        .proj = glm::perspective(glm::radians(45.0f), static_cast<float>(width_) / height_, 0.1f, 100.0f)
    };
    void* data;
    result = vkMapMemory(context_.device, bufferManager_->getUniformBufferMemory(), 0, sizeof(ubo), 0, &data);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to map uniform buffer memory: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to map uniform buffer memory");
    }
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(context_.device, bufferManager_->getUniformBufferMemory());

    camera.render(imageIndex, bufferManager_->getVertexBuffer(), commandBuffers_[imageIndex],
                 bufferManager_->getIndexBuffer(), pipelineManager_->getPipelineLayout(),
                 context_.descriptorSet, pipelineManager_->getRenderPass(), framebuffers_[imageIndex], 0.016f);

    result = vkEndCommandBuffer(commandBuffers_[imageIndex]);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to end command buffer: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to end command buffer");
    }

    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &imageAvailableSemaphore_,
        .pWaitDstStageMask = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffers_[imageIndex],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &renderFinishedSemaphore_
    };
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.pWaitDstStageMask = waitStages;

    result = vkQueueSubmit(context_.graphicsQueue, 1, &submitInfo, inFlightFence_);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to submit draw command buffer: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to submit draw command buffer");
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
        handleResize(width_, height_);
    } else if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to present swapchain image: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to present swapchain image");
    }
}

void VulkanRenderer::handleResize(int width, int height) {
    if (context_.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(context_.device);
    }
    for (auto framebuffer : framebuffers_) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(context_.device, framebuffer, nullptr);
        }
    }
    framebuffers_.clear();
    swapchainManager_->handleResize(width, height);
    width_ = width;
    height_ = height;

    const auto& imageViews = swapchainManager_->getSwapchainImageViews();
    framebuffers_.resize(imageViews.size());
    for (size_t i = 0; i < imageViews.size(); ++i) {
        VkFramebufferCreateInfo framebufferInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderPass = pipelineManager_->getRenderPass(),
            .attachmentCount = 1,
            .pAttachments = &imageViews[i],
            .width = swapchainManager_->getSwapchainExtent().width,
            .height = swapchainManager_->getSwapchainExtent().height,
            .layers = 1
        };
        VkResult result = vkCreateFramebuffer(context_.device, &framebufferInfo, nullptr, &framebuffers_[i]);
        if (result != VK_SUCCESS) {
            LOG_ERROR_CAT("Vulkan", "Failed to create framebuffer {}: result={}", std::source_location::current(), i, result);
            throw std::runtime_error("Failed to create framebuffer");
        }
    }
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

void VulkanInitializer::createAccelerationStructures(VulkanContext& context, std::span<const glm::vec3> vertices, std::span<const uint32_t> indices) {
    LOG_DEBUG_CAT("Vulkan", "Creating acceleration structures", std::source_location::current());
    // Placeholder: Implement acceleration structure creation using VK_KHR_acceleration_structure
}

void VulkanInitializer::createStorageImage(VulkanContext& context, uint32_t width, uint32_t height) {
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
    VkResult result = vkCreateImage(context.device, &imageInfo, nullptr, &context.storageImage);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create storage image: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to create storage image");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(context.device, context.storageImage, &memRequirements);
    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = findMemoryType(context.physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    result = vkAllocateMemory(context.device, &allocInfo, nullptr, &context.storageImageMemory);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to allocate storage image memory: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to allocate storage image memory");
    }
    vkBindImageMemory(context.device, context.storageImage, context.storageImageMemory, 0);

    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = context.storageImage,
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
    result = vkCreateImageView(context.device, &viewInfo, nullptr, &context.storageImageView);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create storage image view: result={}", std::source_location::current(), result);
        throw std::runtime_error("Failed to create storage image view");
    }
}

void VulkanInitializer::createRayTracingPipeline(VulkanContext& context) {
    LOG_DEBUG_CAT("Vulkan", "Creating ray tracing pipeline", std::source_location::current());
    // Placeholder: Implement ray tracing pipeline using VK_KHR_ray_tracing_pipeline
}

void VulkanInitializer::createShaderBindingTable(VulkanContext& context) {
    LOG_DEBUG_CAT("Vulkan", "Creating shader binding table", std::source_location::current());
    // Placeholder: Implement shader binding table for ray tracing
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