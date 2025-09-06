#ifndef VULKAN_INIT_HPP
#define VULKAN_INIT_HPP

#include <vulkan/vulkan.h>
#include <SDL3/SDL_vulkan.h>
#include <vector>
#include <stdexcept>
#include <fstream>
#include <glm/glm.hpp>

class VulkanInitializer {
public:
    static void initializeVulkan(VkInstance& instance, VkPhysicalDevice& physicalDevice,
                                 VkDevice& device, VkSurfaceKHR& surface, VkQueue& graphicsQueue,
                                 VkQueue& presentQueue, uint32_t& graphicsFamily, uint32_t& presentFamily,
                                 VkSwapchainKHR& swapchain, std::vector<VkImage>& swapchainImages,
                                 std::vector<VkImageView>& swapchainImageViews, VkRenderPass& renderPass,
                                 VkPipeline& pipeline, VkPipelineLayout& pipelineLayout,
                                 std::vector<VkFramebuffer>& swapchainFramebuffers, VkCommandPool& commandPool,
                                 std::vector<VkCommandBuffer>& commandBuffers, VkSemaphore& imageAvailableSemaphore,
                                 VkSemaphore& renderFinishedSemaphore, VkFence& inFlightFence,
                                 VkBuffer& vertexBuffer, VkDeviceMemory& vertexBufferMemory,
                                 VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory,
                                 const std::vector<glm::vec3>& sphereVertices, const std::vector<uint32_t>& sphereIndices,
                                 int width, int height) {
        createPhysicalDevice(instance, physicalDevice, graphicsFamily, presentFamily, surface);
        createLogicalDevice(physicalDevice, device, graphicsQueue, presentQueue, graphicsFamily, presentFamily);
        createSwapchain(physicalDevice, device, surface, swapchain, swapchainImages, swapchainImageViews, width, height);
        createRenderPass(device, renderPass);
        createGraphicsPipeline(device, renderPass, pipeline, pipelineLayout, width, height);
        createFramebuffers(device, renderPass, swapchainImageViews, swapchainFramebuffers, width, height);
        createCommandPool(device, commandPool, graphicsFamily);
        createCommandBuffers(device, commandPool, commandBuffers, swapchainFramebuffers);
        createSyncObjects(device, imageAvailableSemaphore, renderFinishedSemaphore, inFlightFence);
        createVertexBuffer(device, physicalDevice, commandPool, graphicsQueue, vertexBuffer, vertexBufferMemory, sphereVertices);
        createIndexBuffer(device, physicalDevice, commandPool, graphicsQueue, indexBuffer, indexBufferMemory, sphereIndices);
    }

    static void cleanupVulkan(VkInstance instance, VkDevice device, VkSurfaceKHR surface,
                              VkSwapchainKHR swapchain, std::vector<VkImageView>& swapchainImageViews,
                              std::vector<VkFramebuffer>& swapchainFramebuffers, VkPipeline pipeline,
                              VkPipelineLayout pipelineLayout, VkRenderPass renderPass,
                              VkCommandPool commandPool, std::vector<VkCommandBuffer>& commandBuffers,
                              VkSemaphore imageAvailableSemaphore, VkSemaphore renderFinishedSemaphore,
                              VkFence inFlightFence, VkBuffer vertexBuffer, VkDeviceMemory vertexBufferMemory,
                              VkBuffer indexBuffer, VkDeviceMemory indexBufferMemory) {
        if (device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(device);
            vkDestroyBuffer(device, vertexBuffer, nullptr);
            vkFreeMemory(device, vertexBufferMemory, nullptr);
            vkDestroyBuffer(device, indexBuffer, nullptr);
            vkFreeMemory(device, indexBufferMemory, nullptr);
            vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
            vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
            vkDestroyFence(device, inFlightFence, nullptr);
            if (!commandBuffers.empty()) {
                vkFreeCommandBuffers(device, commandPool, commandBuffers.size(), commandBuffers.data());
            }
            vkDestroyCommandPool(device, commandPool, nullptr);
            for (auto framebuffer : swapchainFramebuffers) {
                vkDestroyFramebuffer(device, framebuffer, nullptr);
            }
            vkDestroyPipeline(device, pipeline, nullptr);
            vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
            vkDestroyRenderPass(device, renderPass, nullptr);
            for (auto imageView : swapchainImageViews) {
                vkDestroyImageView(device, imageView, nullptr);
            }
            vkDestroySwapchainKHR(device, swapchain, nullptr);
            vkDestroyDevice(device, nullptr);
        }
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
    }

private:
    static void createPhysicalDevice(VkInstance instance, VkPhysicalDevice& physicalDevice,
                                    uint32_t& graphicsFamily, uint32_t& presentFamily, VkSurfaceKHR surface) {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0) throw std::runtime_error("No Vulkan-capable devices found");

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        for (const auto& device : devices) {
            uint32_t queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

            graphicsFamily = UINT32_MAX;
            presentFamily = UINT32_MAX;
            for (uint32_t i = 0; i < queueFamilyCount; ++i) {
                if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) graphicsFamily = i;
                VkBool32 presentSupport = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
                if (presentSupport) presentFamily = i;
                if (graphicsFamily != UINT32_MAX && presentFamily != UINT32_MAX) {
                    physicalDevice = device;
                    return;
                }
            }
        }
        throw std::runtime_error("No suitable physical device found");
    }

    static void createLogicalDevice(VkPhysicalDevice physicalDevice, VkDevice& device,
                                   VkQueue& graphicsQueue, VkQueue& presentQueue,
                                   uint32_t graphicsFamily, uint32_t presentFamily) {
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueInfo = {};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = graphicsFamily;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueInfo);
        if (presentFamily != graphicsFamily) {
            queueInfo.queueFamilyIndex = presentFamily;
            queueCreateInfos.push_back(queueInfo);
        }

        VkDeviceCreateInfo deviceInfo = {};
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.queueCreateInfoCount = queueCreateInfos.size();
        deviceInfo.pQueueCreateInfos = queueCreateInfos.data();
        const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        deviceInfo.enabledExtensionCount = 1;
        deviceInfo.ppEnabledExtensionNames = deviceExtensions;

        if (vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create logical device");
        }

        vkGetDeviceQueue(device, graphicsFamily, 0, &graphicsQueue);
        vkGetDeviceQueue(device, presentFamily, 0, &presentQueue);
    }

    static void createSwapchain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface,
                               VkSwapchainKHR& swapchain, std::vector<VkImage>& swapchainImages,
                               std::vector<VkImageView>& swapchainImageViews, int width, int height) {
        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);

        VkSwapchainCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.minImageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0 && createInfo.minImageCount > capabilities.maxImageCount) {
            createInfo.minImageCount = capabilities.maxImageCount;
        }
        createInfo.imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
        createInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        createInfo.imageExtent = {(uint32_t)width, (uint32_t)height};
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.preTransform = capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        createInfo.clipped = VK_TRUE;

        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create swapchain");
        }

        uint32_t imageCount;
        vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
        swapchainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());

        swapchainImageViews.resize(imageCount);
        for (uint32_t i = 0; i < imageCount; ++i) {
            VkImageViewCreateInfo viewInfo = {};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = swapchainImages[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = VK_FORMAT_B8G8R8A8_SRGB;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(device, &viewInfo, nullptr, &swapchainImageViews[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create image view");
            }
        }
    }

    static void createRenderPass(VkDevice device, VkRenderPass& renderPass) {
        VkAttachmentDescription colorAttachment = {};
        colorAttachment.format = VK_FORMAT_B8G8R8A8_SRGB;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentRef = {};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;

        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create render pass");
        }
    }

    static VkShaderModule createShaderModule(VkDevice device, const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open()) throw std::runtime_error("Failed to open shader: " + filename);
        size_t fileSize = (size_t)file.tellg();
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();

        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = buffer.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shader module");
        }
        return shaderModule;
    }

    static void createGraphicsPipeline(VkDevice device, VkRenderPass renderPass,
                                      VkPipeline& pipeline, VkPipelineLayout& pipelineLayout,
                                      int width, int height) {
        VkShaderModule vertShaderModule = createShaderModule(device, "vertex.spv");
        VkShaderModule fragShaderModule = createShaderModule(device, "fragment.spv");

        VkPipelineShaderStageCreateInfo shaderStages[] = {
            { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertShaderModule, "main", nullptr },
            { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragShaderModule, "main", nullptr }
        };

        VkVertexInputBindingDescription bindingDesc = {};
        bindingDesc.binding = 0;
        bindingDesc.stride = sizeof(glm::vec3);
        bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription attributeDesc = {};
        attributeDesc.binding = 0;
        attributeDesc.location = 0;
        attributeDesc.format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDesc.offset = 0;

        VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
        vertexInputInfo.vertexAttributeDescriptionCount = 1;
        vertexInputInfo.pVertexAttributeDescriptions = &attributeDesc;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkViewport viewport = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
        VkRect2D scissor = { {0, 0}, {(uint32_t)width, (uint32_t)height} };
        VkPipelineViewportStateCreateInfo viewportState = {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizer = {};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        VkPipelineMultisampleStateCreateInfo multisampling = {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo colorBlending = {};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        VkPushConstantRange pushConstantRange = {};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        struct PushConstants {
            glm::mat4 model;
            glm::mat4 view;
            glm::mat4 proj;
            float radius;
            float wavePhase;
        };
        pushConstantRange.size = sizeof(PushConstants);
        
        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pipeline layout");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;

        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create graphics pipeline");
        }

        vkDestroyShaderModule(device, vertShaderModule, nullptr);
        vkDestroyShaderModule(device, fragShaderModule, nullptr);
    }

    static void createFramebuffers(VkDevice device, VkRenderPass renderPass,
                                  std::vector<VkImageView>& swapchainImageViews,
                                  std::vector<VkFramebuffer>& swapchainFramebuffers,
                                  int width, int height) {
        swapchainFramebuffers.resize(swapchainImageViews.size());
        for (size_t i = 0; i < swapchainImageViews.size(); ++i) {
            VkFramebufferCreateInfo framebufferInfo = {};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = &swapchainImageViews[i];
            framebufferInfo.width = width;
            framebufferInfo.height = height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapchainFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create framebuffer");
            }
        }
    }

    static void createCommandPool(VkDevice device, VkCommandPool& commandPool, uint32_t graphicsFamily) {
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = graphicsFamily;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create command pool");
        }
    }

    static void createCommandBuffers(VkDevice device, VkCommandPool commandPool,
                                    std::vector<VkCommandBuffer>& commandBuffers,
                                    std::vector<VkFramebuffer>& swapchainFramebuffers) {
        commandBuffers.resize(swapchainFramebuffers.size());
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

        if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffers");
        }
    }

    static void createSyncObjects(VkDevice device, VkSemaphore& imageAvailableSemaphore,
                                 VkSemaphore& renderFinishedSemaphore, VkFence& inFlightFence) {
        VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFence) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create sync objects");
        }
    }

    static void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size,
                            VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                            VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create buffer");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

        uint32_t memoryTypeIndex = UINT32_MAX;
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
            if ((memRequirements.memoryTypeBits & (1 << i)) &&
                (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                memoryTypeIndex = i;
                break;
            }
        }
        if (memoryTypeIndex == UINT32_MAX) throw std::runtime_error("Failed to find suitable memory type");

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = memoryTypeIndex;

        if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate memory");
        }

        vkBindBufferMemory(device, buffer, bufferMemory, 0);
    }

    static void copyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue,
                          VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);
        VkBufferCopy copyRegion = { 0, 0, size };
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue);
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    }

    static void createVertexBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
                                  VkCommandPool commandPool, VkQueue graphicsQueue,
                                  VkBuffer& vertexBuffer, VkDeviceMemory& vertexBufferMemory,
                                  const std::vector<glm::vec3>& sphereVertices) {
        VkDeviceSize bufferSize = sizeof(sphereVertices[0]) * sphereVertices.size();
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;

        createBuffer(device, physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingBuffer, stagingBufferMemory);

        void* data;
        vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, sphereVertices.data(), (size_t)bufferSize);
        vkUnmapMemory(device, stagingBufferMemory);

        createBuffer(device, physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

        copyBuffer(device, commandPool, graphicsQueue, stagingBuffer, vertexBuffer, bufferSize);

        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }

    static void createIndexBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
                                 VkCommandPool commandPool, VkQueue graphicsQueue,
                                 VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory,
                                 const std::vector<uint32_t>& sphereIndices) {
        VkDeviceSize bufferSize = sizeof(sphereIndices[0]) * sphereIndices.size();
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;

        createBuffer(device, physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingBuffer, stagingBufferMemory);

        void* data;
        vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, sphereIndices.data(), (size_t)bufferSize);
        vkUnmapMemory(device, stagingBufferMemory);

        createBuffer(device, physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

        copyBuffer(device, commandPool, graphicsQueue, stagingBuffer, indexBuffer, bufferSize);

        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }
};

#endif // VULKAN_INIT_HPP