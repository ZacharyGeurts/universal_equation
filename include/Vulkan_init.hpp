#ifndef VULKAN_INIT_HPP
#define VULKAN_INIT_HPP

#include "vulkan/vulkan.h"
#include "SDL3/SDL_vulkan.h"
#include <vector>
#include <stdexcept>
#include <fstream>
#include "glm/glm.hpp"
#include <iostream>
#include <algorithm>
#include <cstring>

class VulkanInitializer {
public:
    static void initializeVulkan(
        VkInstance& instance, VkPhysicalDevice& physicalDevice, VkDevice& device, VkSurfaceKHR& surface,
        VkQueue& graphicsQueue, VkQueue& presentQueue, uint32_t& graphicsFamily, uint32_t& presentFamily,
        VkSwapchainKHR& swapchain, std::vector<VkImage>& swapchainImages, std::vector<VkImageView>& swapchainImageViews,
        VkRenderPass& renderPass, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout,
        std::vector<VkFramebuffer>& swapchainFramebuffers, VkCommandPool& commandPool,
        std::vector<VkCommandBuffer>& commandBuffers, VkSemaphore& imageAvailableSemaphore,
        VkSemaphore& renderFinishedSemaphore, VkFence& inFlightFence, VkBuffer& vertexBuffer,
        VkDeviceMemory& vertexBufferMemory, VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory,
        const std::vector<glm::vec3>& sphereVertices, const std::vector<uint32_t>& sphereIndices, int width, int height
    ) {
        VkFormat swapchainFormat;
        createPhysicalDevice(instance, physicalDevice, graphicsFamily, presentFamily, surface);
        createLogicalDevice(physicalDevice, device, graphicsQueue, presentQueue, graphicsFamily, presentFamily);
        createSwapchain(physicalDevice, device, surface, swapchain, swapchainImages, swapchainImageViews, swapchainFormat, width, height);
        createRenderPass(device, renderPass, swapchainFormat);
        createGraphicsPipeline(device, renderPass, pipeline, pipelineLayout, width, height);
        createFramebuffers(device, renderPass, swapchainImageViews, swapchainFramebuffers, width, height);
        createCommandPool(device, commandPool, graphicsFamily);
        createCommandBuffers(device, commandPool, commandBuffers, swapchainFramebuffers);
        createSyncObjects(device, imageAvailableSemaphore, renderFinishedSemaphore, inFlightFence);
        createVertexBuffer(device, physicalDevice, commandPool, graphicsQueue, vertexBuffer, vertexBufferMemory, sphereVertices);
        createIndexBuffer(device, physicalDevice, commandPool, graphicsQueue, indexBuffer, indexBufferMemory, sphereIndices);
    }

    static void initializeQuadBuffers(
        VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
        VkBuffer& vertexBuffer, VkDeviceMemory& vertexBufferMemory, VkBuffer& indexBuffer,
        VkDeviceMemory& indexBufferMemory, const std::vector<glm::vec3>& vertices,
        const std::vector<uint32_t>& indices
    ) {
        createVertexBuffer(device, physicalDevice, commandPool, graphicsQueue, vertexBuffer, vertexBufferMemory, vertices);
        createIndexBuffer(device, physicalDevice, commandPool, graphicsQueue, indexBuffer, indexBufferMemory, indices);
    }

    static void cleanupVulkan(
        VkInstance instance, VkDevice& device, VkSurfaceKHR surface, VkSwapchainKHR& swapchain,
        std::vector<VkImageView>& swapchainImageViews, std::vector<VkFramebuffer>& swapchainFramebuffers,
        VkPipeline& pipeline, VkPipelineLayout& pipelineLayout, VkRenderPass& renderPass, VkCommandPool& commandPool,
        std::vector<VkCommandBuffer>& commandBuffers, VkSemaphore& imageAvailableSemaphore,
        VkSemaphore& renderFinishedSemaphore, VkFence& inFlightFence, VkBuffer& vertexBuffer,
        VkDeviceMemory& vertexBufferMemory, VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory,
        VkBuffer& quadVertexBuffer, VkDeviceMemory& quadVertexBufferMemory, VkBuffer& quadIndexBuffer,
        VkDeviceMemory& quadIndexBufferMemory
    ) {
        if (device == VK_NULL_HANDLE) {
            std::cerr << "Device is already null, skipping Vulkan cleanup\n";
            return;
        }

        // Wait for device to be idle
        VkResult result = vkDeviceWaitIdle(device);
        if (result != VK_SUCCESS) {
            std::cerr << "vkDeviceWaitIdle failed in cleanupVulkan: " << result << "\n";
        }

        // Destroy quad buffers and memory first
        if (quadVertexBuffer != VK_NULL_HANDLE) {
            std::cerr << "Destroying quad vertex buffer\n";
            vkDestroyBuffer(device, quadVertexBuffer, nullptr);
            quadVertexBuffer = VK_NULL_HANDLE;
        }
        if (quadVertexBufferMemory != VK_NULL_HANDLE) {
            std::cerr << "Freeing quad vertex buffer memory\n";
            vkFreeMemory(device, quadVertexBufferMemory, nullptr);
            quadVertexBufferMemory = VK_NULL_HANDLE;
        }
        if (quadIndexBuffer != VK_NULL_HANDLE) {
            std::cerr << "Destroying quad index buffer\n";
            vkDestroyBuffer(device, quadIndexBuffer, nullptr);
            quadIndexBuffer = VK_NULL_HANDLE;
        }
        if (quadIndexBufferMemory != VK_NULL_HANDLE) {
            std::cerr << "Freeing quad index buffer memory\n";
            vkFreeMemory(device, quadIndexBufferMemory, nullptr);
            quadIndexBufferMemory = VK_NULL_HANDLE;
        }

        // Destroy sphere buffers and memory
        if (vertexBuffer != VK_NULL_HANDLE) {
            std::cerr << "Destroying vertex buffer\n";
            vkDestroyBuffer(device, vertexBuffer, nullptr);
            vertexBuffer = VK_NULL_HANDLE;
        }
        if (vertexBufferMemory != VK_NULL_HANDLE) {
            std::cerr << "Freeing vertex buffer memory\n";
            vkFreeMemory(device, vertexBufferMemory, nullptr);
            vertexBufferMemory = VK_NULL_HANDLE;
        }
        if (indexBuffer != VK_NULL_HANDLE) {
            std::cerr << "Destroying index buffer\n";
            vkDestroyBuffer(device, indexBuffer, nullptr);
            indexBuffer = VK_NULL_HANDLE;
        }
        if (indexBufferMemory != VK_NULL_HANDLE) {
            std::cerr << "Freeing index buffer memory\n";
            vkFreeMemory(device, indexBufferMemory, nullptr);
            indexBufferMemory = VK_NULL_HANDLE;
        }

        // Destroy sync objects
        if (imageAvailableSemaphore != VK_NULL_HANDLE) {
            std::cerr << "Destroying image available semaphore\n";
            vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
            imageAvailableSemaphore = VK_NULL_HANDLE;
        }
        if (renderFinishedSemaphore != VK_NULL_HANDLE) {
            std::cerr << "Destroying render finished semaphore\n";
            vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
            renderFinishedSemaphore = VK_NULL_HANDLE;
        }
        if (inFlightFence != VK_NULL_HANDLE) {
            std::cerr << "Destroying in-flight fence\n";
            vkDestroyFence(device, inFlightFence, nullptr);
            inFlightFence = VK_NULL_HANDLE;
        }

        // Destroy command buffers and pool
        if (!commandBuffers.empty()) {
            std::cerr << "Freeing command buffers\n";
            vkFreeCommandBuffers(device, commandPool, commandBuffers.size(), commandBuffers.data());
            commandBuffers.clear();
        }
        if (commandPool != VK_NULL_HANDLE) {
            std::cerr << "Destroying command pool\n";
            vkDestroyCommandPool(device, commandPool, nullptr);
            commandPool = VK_NULL_HANDLE;
        }

        // Destroy framebuffers, pipeline, and render pass
        for (auto& fb : swapchainFramebuffers) {
            if (fb != VK_NULL_HANDLE) {
                std::cerr << "Destroying framebuffer\n";
                vkDestroyFramebuffer(device, fb, nullptr);
                fb = VK_NULL_HANDLE;
            }
        }
        swapchainFramebuffers.clear();
        if (pipeline != VK_NULL_HANDLE) {
            std::cerr << "Destroying pipeline\n";
            vkDestroyPipeline(device, pipeline, nullptr);
            pipeline = VK_NULL_HANDLE;
        }
        if (pipelineLayout != VK_NULL_HANDLE) {
            std::cerr << "Destroying pipeline layout\n";
            vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
            pipelineLayout = VK_NULL_HANDLE;
        }
        if (renderPass != VK_NULL_HANDLE) {
            std::cerr << "Destroying render pass\n";
            vkDestroyRenderPass(device, renderPass, nullptr);
            renderPass = VK_NULL_HANDLE;
        }

        // Destroy swapchain and image views
        for (auto& iv : swapchainImageViews) {
            if (iv != VK_NULL_HANDLE) {
                std::cerr << "Destroying image view\n";
                vkDestroyImageView(device, iv, nullptr);
                iv = VK_NULL_HANDLE;
            }
        }
        swapchainImageViews.clear();
        if (swapchain != VK_NULL_HANDLE) {
            std::cerr << "Destroying swapchain\n";
            vkDestroySwapchainKHR(device, swapchain, nullptr);
            swapchain = VK_NULL_HANDLE;
        }

        // Destroy device last
        std::cerr << "Destroying device\n";
        vkDestroyDevice(device, nullptr);
        device = VK_NULL_HANDLE;

        // Do NOT destroy instance or surface during swapchain recreation
        // These are handled in SDL3Initializer::cleanupSDL
    }

private:
    static void createPhysicalDevice(VkInstance instance, VkPhysicalDevice& physicalDevice, uint32_t& graphicsFamily, uint32_t& presentFamily, VkSurfaceKHR surface) {
        if (instance == VK_NULL_HANDLE) {
            throw std::runtime_error("Invalid Vulkan instance");
        }
        uint32_t deviceCount = 0;
        VkResult result = vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("vkEnumeratePhysicalDevices failed: " + std::to_string(result));
        }
        if (!deviceCount) throw std::runtime_error("No Vulkan devices found");
        std::vector<VkPhysicalDevice> devices(deviceCount);
        result = vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
        if (result != VK_SUCCESS) {
            throw std::runtime_error("vkEnumeratePhysicalDevices failed: " + std::to_string(result));
        }
        for (const auto& dev : devices) {
            uint32_t queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, nullptr);
            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, queueFamilies.data());
            graphicsFamily = presentFamily = UINT32_MAX;
            for (uint32_t i = 0; i < queueFamilyCount; ++i) {
                if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) graphicsFamily = i;
                VkBool32 presentSupport = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &presentSupport);
                if (presentSupport) presentFamily = i;
                if (graphicsFamily != UINT32_MAX && presentFamily != UINT32_MAX) {
                    physicalDevice = dev;
                    return;
                }
            }
        }
        throw std::runtime_error("No suitable Vulkan device found");
    }

    static void createLogicalDevice(VkPhysicalDevice physicalDevice, VkDevice& device, VkQueue& graphicsQueue, VkQueue& presentQueue, uint32_t graphicsFamily, uint32_t presentFamily) {
        uint32_t extCount;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> exts(extCount);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, exts.data());
        bool hasSwapchain = false;
        for (const auto& ext : exts) {
            if (strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                hasSwapchain = true;
                break;
            }
        }
        if (!hasSwapchain) throw std::runtime_error("No swapchain support");
        float priority = 1.0f;
        std::vector<VkDeviceQueueCreateInfo> queueInfos;
        VkDeviceQueueCreateInfo info = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, 0, graphicsFamily, 1, &priority };
        queueInfos.push_back(info);
        if (presentFamily != graphicsFamily) {
            info.queueFamilyIndex = presentFamily;
            queueInfos.push_back(info);
        }
        const char* extsArray[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        VkPhysicalDeviceFeatures features = {};
        VkDeviceCreateInfo deviceInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, nullptr, 0, (uint32_t)queueInfos.size(), queueInfos.data(), 0, nullptr, 1, extsArray, &features };
        if (vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device) != VK_SUCCESS) throw std::runtime_error("Device creation failed");
        vkGetDeviceQueue(device, graphicsFamily, 0, &graphicsQueue);
        vkGetDeviceQueue(device, presentFamily, 0, &presentQueue);
    }

    static void createSwapchain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, VkSwapchainKHR& swapchain,
                                std::vector<VkImage>& swapchainImages, std::vector<VkImageView>& swapchainImageViews, VkFormat& swapchainFormat, int width, int height) {
        VkSurfaceCapabilitiesKHR caps;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &caps);
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());
        VkFormat format = formats[0].format;
        VkColorSpaceKHR colorSpace = formats[0].colorSpace;
        for (const auto& f : formats) {
            if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                format = f.format;
                colorSpace = f.colorSpace;
                break;
            }
        }
        swapchainFormat = format;
        uint32_t pmCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &pmCount, nullptr);
        std::vector<VkPresentModeKHR> pms(pmCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &pmCount, pms.data());
        VkPresentModeKHR pm = VK_PRESENT_MODE_FIFO_KHR;
        for (const auto& m : pms) {
            if (m == VK_PRESENT_MODE_MAILBOX_KHR) {
                pm = m;
                break;
            }
        }
        VkExtent2D extent = caps.currentExtent.width != UINT32_MAX ? caps.currentExtent : VkExtent2D{ std::min(std::max((uint32_t)width, caps.minImageExtent.width), caps.maxImageExtent.width), std::min(std::max((uint32_t)height, caps.minImageExtent.height), caps.maxImageExtent.height) };
        VkSwapchainCreateInfoKHR info = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, nullptr, 0, surface, caps.minImageCount + 1, format, colorSpace, extent, 1, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, nullptr, caps.currentTransform, VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, pm, VK_TRUE, VK_NULL_HANDLE };
        if (caps.maxImageCount > 0 && info.minImageCount > caps.maxImageCount) info.minImageCount = caps.maxImageCount;
        if (vkCreateSwapchainKHR(device, &info, nullptr, &swapchain) != VK_SUCCESS) throw std::runtime_error("Swapchain creation failed");
        uint32_t imgCount;
        vkGetSwapchainImagesKHR(device, swapchain, &imgCount, nullptr);
        swapchainImages.resize(imgCount);
        vkGetSwapchainImagesKHR(device, swapchain, &imgCount, swapchainImages.data());
        swapchainImageViews.resize(imgCount);
        for (uint32_t i = 0; i < imgCount; ++i) {
            VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, nullptr, 0, swapchainImages[i], VK_IMAGE_VIEW_TYPE_2D, format, {}, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
            if (vkCreateImageView(device, &viewInfo, nullptr, &swapchainImageViews[i]) != VK_SUCCESS) throw std::runtime_error("Image view creation failed");
        }
    }

    static void createRenderPass(VkDevice device, VkRenderPass& renderPass, VkFormat format) {
        VkAttachmentDescription color = { 0, format, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR };
        VkAttachmentReference colorRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        VkSubpassDescription subpass = { 0, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, nullptr, 1, &colorRef, nullptr, nullptr, 0, nullptr };
        VkSubpassDependency dependency = {
            VK_SUBPASS_EXTERNAL, 0,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            0
        };
        VkRenderPassCreateInfo info = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, nullptr, 0, 1, &color, 1, &subpass, 1, &dependency };
        if (vkCreateRenderPass(device, &info, nullptr, &renderPass) != VK_SUCCESS) throw std::runtime_error("Render pass creation failed");
    }

    static VkShaderModule createShaderModule(VkDevice device, const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file) throw std::runtime_error("Failed to open shader: " + filename);
        std::vector<char> buffer((size_t)file.tellg());
        file.seekg(0);
        file.read(buffer.data(), buffer.size());
        file.close();
        size_t paddedSize = (buffer.size() + 3) & ~3; // Align to 4 bytes
        buffer.resize(paddedSize, 0);
        VkShaderModuleCreateInfo info = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr, 0, buffer.size(), reinterpret_cast<const uint32_t*>(buffer.data()) };
        VkShaderModule module;
        if (vkCreateShaderModule(device, &info, nullptr, &module) != VK_SUCCESS) throw std::runtime_error("Shader module creation failed");
        return module;
    }

    static void createGraphicsPipeline(VkDevice device, VkRenderPass renderPass, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout, int width, int height) {
        VkShaderModule vert = createShaderModule(device, "vertex.spv");
        VkShaderModule frag = createShaderModule(device, "fragment.spv");
        VkPipelineShaderStageCreateInfo stages[] = {
            { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vert, "main", nullptr },
            { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, frag, "main", nullptr }
        };
        VkVertexInputBindingDescription binding = { 0, sizeof(glm::vec3), VK_VERTEX_INPUT_RATE_VERTEX };
        VkVertexInputAttributeDescription attr = { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 };
        VkPipelineVertexInputStateCreateInfo vertexInput = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, nullptr, 0, 1, &binding, 1, &attr };
        VkPipelineInputAssemblyStateCreateInfo inputAssembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE };
        VkViewport viewport = { 0, 0, (float)width, (float)height, 0, 1 };
        VkRect2D scissor = { {0, 0}, {(uint32_t)width, (uint32_t)height} };
        VkPipelineViewportStateCreateInfo viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr, 0, 1, &viewport, 1, &scissor };
        VkPipelineRasterizationStateCreateInfo rasterizer = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, nullptr, 0, VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_FALSE, 0, 0, 0, 1.0f };
        VkPipelineMultisampleStateCreateInfo multisampling = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, nullptr, 0, VK_SAMPLE_COUNT_1_BIT, VK_FALSE, 0, nullptr, VK_FALSE, VK_FALSE };
        VkPipelineDepthStencilStateCreateInfo depthStencil = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, nullptr, 0, VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS, VK_FALSE, VK_FALSE, {}, {}, 0, 1.0f };
        VkPipelineColorBlendAttachmentState blendAttachment = { VK_TRUE, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD, VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT };
        VkPipelineColorBlendStateCreateInfo blending = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, nullptr, 0, VK_FALSE, VK_LOGIC_OP_COPY, 1, &blendAttachment, {0} };
        struct PushConstants {
            glm::mat4 model;      // 64 bytes
            glm::mat4 view;       // 64 bytes
            glm::mat4 proj;       // 64 bytes
            glm::vec3 baseColor;  // 12 bytes
            float value;          // 4 bytes
            float dimension;      // 4 bytes
            float wavePhase;      // 4 bytes
            float cycleProgress;  // 4 bytes
            float darkMatter;     // 4 bytes
            float darkEnergy;     // 4 bytes
        }; // Total: 228 bytes
        VkPushConstantRange pushConstant = { VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants) };
        VkPipelineLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 0, nullptr, 1, &pushConstant };
        std::cerr << "PushConstants size: " << sizeof(PushConstants) << " bytes\n";
        if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) throw std::runtime_error("Pipeline layout creation failed");
        VkGraphicsPipelineCreateInfo pipelineInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, nullptr, 0, 2, stages, &vertexInput, &inputAssembly, nullptr, &viewportState, &rasterizer, &multisampling, &depthStencil, &blending, nullptr, pipelineLayout, renderPass, 0, VK_NULL_HANDLE, -1 };
        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) throw std::runtime_error("Graphics pipeline creation failed");
        vkDestroyShaderModule(device, vert, nullptr);
        vkDestroyShaderModule(device, frag, nullptr);
    }

    static void createFramebuffers(VkDevice device, VkRenderPass renderPass, std::vector<VkImageView>& swapchainImageViews, std::vector<VkFramebuffer>& swapchainFramebuffers, int width, int height) {
        swapchainFramebuffers.resize(swapchainImageViews.size());
        for (size_t i = 0; i < swapchainImageViews.size(); ++i) {
            VkFramebufferCreateInfo info = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, nullptr, 0, renderPass, 1, &swapchainImageViews[i], (uint32_t)width, (uint32_t)height, 1 };
            if (vkCreateFramebuffer(device, &info, nullptr, &swapchainFramebuffers[i]) != VK_SUCCESS) throw std::runtime_error("Framebuffer creation failed");
        }
    }

    static void createCommandPool(VkDevice device, VkCommandPool& commandPool, uint32_t graphicsFamily) {
        VkCommandPoolCreateInfo info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, graphicsFamily };
        if (vkCreateCommandPool(device, &info, nullptr, &commandPool) != VK_SUCCESS) throw std::runtime_error("Command pool creation failed");
    }

    static void createCommandBuffers(VkDevice device, VkCommandPool commandPool, std::vector<VkCommandBuffer>& commandBuffers, std::vector<VkFramebuffer>& swapchainFramebuffers) {
        commandBuffers.resize(swapchainFramebuffers.size());
        VkCommandBufferAllocateInfo info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, (uint32_t)commandBuffers.size() };
        if (vkAllocateCommandBuffers(device, &info, commandBuffers.data()) != VK_SUCCESS) throw std::runtime_error("Command buffer allocation failed");
    }

    static void createSyncObjects(VkDevice device, VkSemaphore& imageAvailableSemaphore, VkSemaphore& renderFinishedSemaphore, VkFence& inFlightFence) {
        VkSemaphoreCreateInfo semInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, nullptr, 0 };
        VkFenceCreateInfo fenceInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT };
        if (vkCreateSemaphore(device, &semInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFence) != VK_SUCCESS) {
            throw std::runtime_error("Sync object creation failed");
        }
    }

    static void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer& buffer, VkDeviceMemory& memory) {
        VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, size, usage, VK_SHARING_MODE_EXCLUSIVE, 0, nullptr };
        if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) throw std::runtime_error("Buffer creation failed");
        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(device, buffer, &memReqs);
        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
        uint32_t memType = UINT32_MAX;
        for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
            if ((memReqs.memoryTypeBits & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props) {
                memType = i;
                break;
            }
        }
        if (memType == UINT32_MAX) {
            std::cerr << "No suitable memory type for props: " << props << "\nAvailable types:\n";
            for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
                std::cerr << "Type " << i << ": Flags " << memProps.memoryTypes[i].propertyFlags << "\n";
            }
            throw std::runtime_error("No suitable memory type");
        }
        VkMemoryAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, memReqs.size, memType };
        VkResult result = vkAllocateMemory(device, &allocInfo, nullptr, &memory);
        if (result != VK_SUCCESS) {
            std::cerr << "Memory allocation failed with error: " << result << "\n";
            throw std::runtime_error("Memory allocation failed");
        }
        if (vkBindBufferMemory(device, buffer, memory, 0) != VK_SUCCESS) throw std::runtime_error("Buffer memory binding failed");
    }

    static void copyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkBuffer src, VkBuffer dst, VkDeviceSize size) {
        VkCommandBufferAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
        VkCommandBuffer cmd;
        if (vkAllocateCommandBuffers(device, &allocInfo, &cmd) != VK_SUCCESS) throw std::runtime_error("Command buffer allocation failed");
        VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr };
        if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) throw std::runtime_error("Command buffer begin failed");
        VkBufferCopy copy = { 0, 0, size };
        vkCmdCopyBuffer(cmd, src, dst, 1, &copy);
        if (vkEndCommandBuffer(cmd) != VK_SUCCESS) throw std::runtime_error("Command buffer end failed");
        VkSubmitInfo submit = { VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &cmd, 0, nullptr };
        if (vkQueueSubmit(graphicsQueue, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS) throw std::runtime_error("Queue submit failed");
        vkQueueWaitIdle(graphicsQueue);
        vkFreeCommandBuffers(device, commandPool, 1, &cmd);
    }

    static void createVertexBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                                  VkBuffer& vertexBuffer, VkDeviceMemory& vertexBufferMemory, const std::vector<glm::vec3>& vertices) {
        if (vertices.empty()) throw std::runtime_error("Vertex buffer is empty");
        VkDeviceSize size = sizeof(vertices[0]) * vertices.size();
        std::cerr << "Vertex buffer size: " << size / (1024.0 * 1024.0) << " MB (" << vertices.size() << " vertices)\n";
        VkBuffer staging;
        VkDeviceMemory stagingMem;
        createBuffer(device, physicalDevice, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging, stagingMem);
        void* data;
        if (vkMapMemory(device, stagingMem, 0, size, 0, &data) != VK_SUCCESS) throw std::runtime_error("Memory mapping failed");
        memcpy(data, vertices.data(), size);
        vkUnmapMemory(device, stagingMem);
        createBuffer(device, physicalDevice, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);
        copyBuffer(device, commandPool, graphicsQueue, staging, vertexBuffer, size);
        vkDestroyBuffer(device, staging, nullptr);
        vkFreeMemory(device, stagingMem, nullptr);
    }

    static void createIndexBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                                 VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory, const std::vector<uint32_t>& indices) {
        if (indices.empty()) throw std::runtime_error("Index buffer is empty");
        VkDeviceSize size = sizeof(indices[0]) * indices.size();
        std::cerr << "Index buffer size: " << size / (1024.0 * 1024.0) << " MB (" << indices.size() << " indices)\n";
        for (const auto& index : indices) {
            if (index >= indices.size()) throw std::runtime_error("Invalid index in index buffer");
        }
        VkBuffer staging;
        VkDeviceMemory stagingMem;
        createBuffer(device, physicalDevice, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging, stagingMem);
        void* data;
        if (vkMapMemory(device, stagingMem, 0, size, 0, &data) != VK_SUCCESS) throw std::runtime_error("Memory mapping failed");
        memcpy(data, indices.data(), size);
        vkUnmapMemory(device, stagingMem);
        createBuffer(device, physicalDevice, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);
        copyBuffer(device, commandPool, graphicsQueue, staging, indexBuffer, size);
        vkDestroyBuffer(device, staging, nullptr);
        vkFreeMemory(device, stagingMem, nullptr);
    }
};

#endif // VULKAN_INIT_HPP