// Vulkan_init.cpp
// AMOURANTH RTX Engine, October 2025 - Vulkan initialization and utility functions.
// Dependencies: Vulkan 1.3+, GLM, VulkanCore.hpp, VulkanBufferManager.hpp, ue_init.hpp, logging.hpp.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#include "engine/Vulkan_init.hpp"
#include "VulkanBufferManager.hpp"
#include "ue_init.hpp"
#include "engine/logging.hpp"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <fstream>
#include <algorithm>
#include <stdexcept>
#include <source_location>
#include <filesystem>

namespace VulkanInitializer {

uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    LOG_ERROR("Failed to find suitable memory type");
    throw std::runtime_error("Failed to find suitable memory type");
}

VkPhysicalDevice findPhysicalDevice(VkInstance instance, bool requireRayTracing) {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        LOG_ERROR("No physical devices found");
        throw std::runtime_error("No physical devices found");
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    VkPhysicalDevice selectedDevice = VK_NULL_HANDLE;
    const char* requiredExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
    };
    constexpr uint32_t requiredExtensionCount = 5;

    for (const auto& device : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(device, &props);
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());
        bool hasAllExtensions = !requireRayTracing;
        if (requireRayTracing) {
            hasAllExtensions = true;
            for (uint32_t i = 0; i < requiredExtensionCount; ++i) {
                bool found = false;
                for (const auto& ext : availableExtensions) {
                    if (strcmp(ext.extensionName, requiredExtensions[i]) == 0) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    hasAllExtensions = false;
                    break;
                }
            }
        }
        if (hasAllExtensions) {
            selectedDevice = device;
            break;
        }
    }
    if (selectedDevice == VK_NULL_HANDLE) {
        LOG_ERROR("No physical device with required ray tracing extensions found");
        throw std::runtime_error("No physical device with required ray tracing extensions found");
    }

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(selectedDevice, &props);
    LOG_INFO("Selected physical device: {}", props.deviceName);
    return selectedDevice;
}

void initializeVulkan(VulkanContext& context, int width, int height) {
    (void)width;
    (void)height;
    context.physicalDevice = findPhysicalDevice(context.instance, true);

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(context.physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(context.physicalDevice, &queueFamilyCount, queueFamilies.data());

    context.graphicsQueueFamilyIndex = UINT32_MAX;
    context.presentQueueFamilyIndex = UINT32_MAX;
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) {
            context.graphicsQueueFamilyIndex = i;
        }
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(context.physicalDevice, i, context.surface, &presentSupport);
        if (presentSupport) {
            context.presentQueueFamilyIndex = i;
        }
        if (context.graphicsQueueFamilyIndex != UINT32_MAX && context.presentQueueFamilyIndex != UINT32_MAX) {
            break;
        }
    }
    if (context.graphicsQueueFamilyIndex == UINT32_MAX) {
        LOG_ERROR("Failed to find graphics or compute queue family");
        throw std::runtime_error("Failed to find graphics or compute queue family");
    }
    if (context.presentQueueFamilyIndex == UINT32_MAX) {
        LOG_ERROR("Failed to find present queue family");
        throw std::runtime_error("Failed to find present queue family");
    }
    LOG_INFO("Graphics queue family: {}, present queue family: {}", context.graphicsQueueFamilyIndex, context.presentQueueFamilyIndex);

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
    uint32_t queueCreateInfoCount = (context.graphicsQueueFamilyIndex == context.presentQueueFamilyIndex) ? 1 : 2;

    VkPhysicalDeviceFeatures deviceFeatures{};
    VkPhysicalDeviceBufferDeviceAddressFeaturesKHR bufferDeviceAddressFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR,
        .pNext = nullptr,
        .bufferDeviceAddress = VK_TRUE,
        .bufferDeviceAddressCaptureReplay = VK_FALSE,
        .bufferDeviceAddressMultiDevice = VK_FALSE
    };
    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
        .pNext = &bufferDeviceAddressFeatures,
        .accelerationStructure = VK_TRUE,
        .accelerationStructureCaptureReplay = VK_FALSE,
        .accelerationStructureIndirectBuild = VK_FALSE,
        .accelerationStructureHostCommands = VK_FALSE,
        .descriptorBindingAccelerationStructureUpdateAfterBind = VK_FALSE
    };
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
        .pNext = &accelFeatures,
        .rayTracingPipeline = VK_TRUE,
        .rayTracingPipelineShaderGroupHandleCaptureReplay = VK_FALSE,
        .rayTracingPipelineShaderGroupHandleCaptureReplayMixed = VK_FALSE,
        .rayTracingPipelineTraceRaysIndirect = VK_FALSE,
        .rayTraversalPrimitiveCulling = VK_FALSE
    };

    const char* deviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
    };

    VkDeviceCreateInfo deviceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &rtPipelineFeatures,
        .flags = 0,
        .queueCreateInfoCount = queueCreateInfoCount,
        .pQueueCreateInfos = queueCreateInfos,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = 5,
        .ppEnabledExtensionNames = deviceExtensions,
        .pEnabledFeatures = &deviceFeatures
    };
    if (vkCreateDevice(context.physicalDevice, &deviceCreateInfo, nullptr, &context.device) != VK_SUCCESS) {
        LOG_ERROR("Failed to create logical device");
        throw std::runtime_error("Failed to create logical device");
    }
    LOG_INFO("Created logical device");

    vkGetDeviceQueue(context.device, context.graphicsQueueFamilyIndex, 0, &context.graphicsQueue);
    vkGetDeviceQueue(context.device, context.presentQueueFamilyIndex, 0, &context.presentQueue);
    LOG_INFO("Acquired graphics and present queues");

    VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = context.graphicsQueueFamilyIndex
    };
    if (vkCreateCommandPool(context.device, &poolInfo, nullptr, &context.commandPool) != VK_SUCCESS) {
        LOG_ERROR("Failed to create command pool");
        throw std::runtime_error("Failed to create command pool");
    }
    LOG_INFO("Created command pool");

    vkGetPhysicalDeviceMemoryProperties(context.physicalDevice, &context.memoryProperties);
}

void createSwapchain(VulkanContext& context, int width, int height) {
    VkSurfaceCapabilitiesKHR capabilities;
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context.physicalDevice, context.surface, &capabilities) != VK_SUCCESS) {
        LOG_ERROR("Failed to get surface capabilities");
        throw std::runtime_error("Failed to get surface capabilities");
    }
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(context.physicalDevice, context.surface, &formatCount, nullptr);
    if (formatCount == 0) {
        LOG_ERROR("No surface formats available");
        throw std::runtime_error("No surface formats available");
    }
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
    VkExtent2D extent = capabilities.currentExtent.width == UINT32_MAX
        ? VkExtent2D{std::clamp(static_cast<uint32_t>(width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
                     std::clamp(static_cast<uint32_t>(height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height)}
        : capabilities.currentExtent;
    uint32_t imageCount = std::min(capabilities.minImageCount + 1, capabilities.maxImageCount > 0 ? capabilities.maxImageCount : UINT32_MAX);
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
    if (vkCreateSwapchainKHR(context.device, &createInfo, nullptr, &context.swapchain) != VK_SUCCESS) {
        LOG_ERROR("Failed to create swapchain");
        throw std::runtime_error("Failed to create swapchain");
    }
    LOG_INFO("Created swapchain: format={}, extent={}x{}", surfaceFormat.format, extent.width, extent.height);
    context.swapchainImageFormat_ = surfaceFormat.format;
    context.swapchainExtent_ = extent;
    vkGetSwapchainImagesKHR(context.device, context.swapchain, &imageCount, nullptr);
    context.swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(context.device, context.swapchain, &imageCount, context.swapchainImages.data());
}

void createImageViews(VulkanContext& context) {
    context.swapchainImageViews.resize(context.swapchainImages.size());
    for (size_t i = 0; i < context.swapchainImages.size(); ++i) {
        VkImageViewCreateInfo viewInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = context.swapchainImages[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = context.swapchainImageFormat_,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };
        if (vkCreateImageView(context.device, &viewInfo, nullptr, &context.swapchainImageViews[i]) != VK_SUCCESS) {
            LOG_ERROR("Failed to create image view for image {}", i);
            throw std::runtime_error("Failed to create image view");
        }
        LOG_INFO("Created image view for image {}", i);
    }
}

VkShaderModule loadShader(VkDevice device, const std::string& filepath) {
    std::filesystem::path shaderPath(filepath);
    if (!std::filesystem::exists(shaderPath)) {
        LOG_ERROR("Shader file does not exist: {}", filepath);
        throw std::runtime_error("Shader file does not exist: " + filepath);
    }
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open shader file: {}", filepath);
        throw std::runtime_error("Failed to open shader file: " + filepath);
    }
    size_t size = static_cast<size_t>(file.tellg());
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
    if (vkCreateShaderModule(device, &createInfo, nullptr, &module) != VK_SUCCESS) {
        LOG_ERROR("Failed to create shader module: {}", filepath);
        throw std::runtime_error("Failed to create shader module");
    }
    LOG_INFO("Loaded shader: {}", filepath);
    return module;
}

void createRenderPass(VkDevice device, VkRenderPass& renderPass, VkFormat format) {
    if (format == VK_FORMAT_UNDEFINED) {
        LOG_ERROR("Invalid format for render pass: VK_FORMAT_UNDEFINED");
        throw std::runtime_error("Invalid format for render pass");
    }
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
    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        LOG_ERROR("Failed to create render pass");
        throw std::runtime_error("Failed to create render pass");
    }
    LOG_INFO("Created render pass");
}

void createDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout& descriptorSetLayout) {
    VkDescriptorSetLayoutBinding tlasBinding{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        .pImmutableSamplers = nullptr
    };
    VkDescriptorSetLayoutBinding uModeBinding{
        .binding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        .pImmutableSamplers = nullptr
    };
    VkDescriptorSetLayoutBinding uboBinding{
        .binding = 2,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = nullptr
    };
    VkDescriptorSetLayoutBinding samplerBinding{
        .binding = 3,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = nullptr
    };
    VkDescriptorSetLayoutBinding bindings[] = {tlasBinding, uModeBinding, uboBinding, samplerBinding};
    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = 4,
        .pBindings = bindings
    };
    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        LOG_ERROR("Failed to create descriptor set layout");
        throw std::runtime_error("Failed to create descriptor set layout");
    }
    LOG_INFO("Created descriptor set layout");
}

void createDescriptorPoolAndSet(VkDevice device, VkDescriptorSetLayout descriptorSetLayout,
                               VkDescriptorPool& descriptorPool, VkDescriptorSet& descriptorSet,
                               VkSampler& sampler, VkBuffer uniformBuffer, VkImageView storageImageView,
                               VkAccelerationStructureKHR topLevelAS) {
    if (descriptorSetLayout == VK_NULL_HANDLE) {
        LOG_ERROR("Invalid descriptor set layout: VK_NULL_HANDLE");
        throw std::runtime_error("Invalid descriptor set layout");
    }
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}
    };
    VkDescriptorPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 1,
        .poolSizeCount = 3,
        .pPoolSizes = poolSizes
    };
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        LOG_ERROR("Failed to create descriptor pool");
        throw std::runtime_error("Failed to create descriptor pool");
    }
    LOG_INFO("Created descriptor pool");
    VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &descriptorSetLayout
    };
    if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate descriptor set");
        throw std::runtime_error("Failed to allocate descriptor set");
    }
    LOG_INFO("Allocated descriptor set");
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
    if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
        LOG_ERROR("Failed to create sampler");
        throw std::runtime_error("Failed to create sampler");
    }
    LOG_INFO("Created sampler");
    VkDescriptorBufferInfo modeInfo{
        .buffer = uniformBuffer,
        .offset = sizeof(UE::UniformBufferObject),
        .range = sizeof(int)
    };
    VkDescriptorBufferInfo uboInfo{
        .buffer = uniformBuffer,
        .offset = 0,
        .range = sizeof(UE::UniformBufferObject)
    };
    VkDescriptorImageInfo imageInfo{
        .sampler = sampler,
        .imageView = storageImageView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    VkWriteDescriptorSetAccelerationStructureKHR accelInfo{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .pNext = nullptr,
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &topLevelAS
    };
    VkWriteDescriptorSet descriptorWrites[] = {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = &accelInfo,
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
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pImageInfo = nullptr,
            .pBufferInfo = &modeInfo,
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
            .pBufferInfo = &uboInfo,
            .pTexelBufferView = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = descriptorSet,
            .dstBinding = 3,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &imageInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        }
    };
    vkUpdateDescriptorSets(device, 4, descriptorWrites, 0, nullptr);
    LOG_INFO("Updated descriptor sets");
}

void createStorageImage(VkDevice device, VkPhysicalDevice physicalDevice, VkImage& storageImage,
                       VkDeviceMemory& storageImageMemory, VkImageView& storageImageView,
                       uint32_t width, uint32_t height) {
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
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    if (vkCreateImage(device, &imageInfo, nullptr, &storageImage) != VK_SUCCESS) {
        LOG_ERROR("Failed to create storage image");
        throw std::runtime_error("Failed to create storage image");
    }
    LOG_INFO("Created storage image");
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, storageImage, &memRequirements);
    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    if (vkAllocateMemory(device, &allocInfo, nullptr, &storageImageMemory) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate storage image memory");
        throw std::runtime_error("Failed to allocate storage image memory");
    }
    LOG_INFO("Allocated storage image memory");
    vkBindImageMemory(device, storageImage, storageImageMemory, 0);
    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = storageImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY
        },
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };
    if (vkCreateImageView(device, &viewInfo, nullptr, &storageImageView) != VK_SUCCESS) {
        LOG_ERROR("Failed to create storage image view");
        throw std::runtime_error("Failed to create storage image view");
    }
    LOG_INFO("Created storage image view");
}

void createAccelerationStructures(VulkanContext& context, std::span<const glm::vec3> vertices, std::span<const uint32_t> indices) {
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;

    VkDeviceSize vertexBufferSize = vertices.size() * sizeof(glm::vec3);
    VkDeviceSize indexBufferSize = indices.size() * sizeof(uint32_t);

    // Create vertex buffer
    VkBufferCreateInfo vertexBufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = vertexBufferSize,
        .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr
    };
    if (vkCreateBuffer(context.device, &vertexBufferInfo, nullptr, &vertexBuffer) != VK_SUCCESS) {
        LOG_ERROR("Failed to create vertex buffer for AS");
        throw std::runtime_error("Failed to create vertex buffer for AS");
    }
    VkMemoryRequirements vertexMemRequirements;
    vkGetBufferMemoryRequirements(context.device, vertexBuffer, &vertexMemRequirements);
    VkMemoryAllocateInfo vertexAllocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = vertexMemRequirements.size,
        .memoryTypeIndex = findMemoryType(context.physicalDevice, vertexMemRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };
    if (vkAllocateMemory(context.device, &vertexAllocInfo, nullptr, &vertexBufferMemory) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate vertex buffer memory for AS");
        throw std::runtime_error("Failed to allocate vertex buffer memory for AS");
    }
    vkBindBufferMemory(context.device, vertexBuffer, vertexBufferMemory, 0);
    void* vertexData;
    vkMapMemory(context.device, vertexBufferMemory, 0, vertexBufferSize, 0, &vertexData);
    memcpy(vertexData, vertices.data(), vertexBufferSize);
    vkUnmapMemory(context.device, vertexBufferMemory);

    // Create index buffer
    VkBufferCreateInfo indexBufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = indexBufferSize,
        .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr
    };
    if (vkCreateBuffer(context.device, &indexBufferInfo, nullptr, &indexBuffer) != VK_SUCCESS) {
        LOG_ERROR("Failed to create index buffer for AS");
        throw std::runtime_error("Failed to create index buffer for AS");
    }
    VkMemoryRequirements indexMemRequirements;
    vkGetBufferMemoryRequirements(context.device, indexBuffer, &indexMemRequirements);
    VkMemoryAllocateInfo indexAllocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = indexMemRequirements.size,
        .memoryTypeIndex = findMemoryType(context.physicalDevice, indexMemRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };
    if (vkAllocateMemory(context.device, &indexAllocInfo, nullptr, &indexBufferMemory) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate index buffer memory for AS");
        throw std::runtime_error("Failed to allocate index buffer memory for AS");
    }
    vkBindBufferMemory(context.device, indexBuffer, indexBufferMemory, 0);
    void* indexData;
    vkMapMemory(context.device, indexBufferMemory, 0, indexBufferSize, 0, &indexData);
    memcpy(indexData, indices.data(), indexBufferSize);
    vkUnmapMemory(context.device, indexBufferMemory);

    // Get buffer device addresses
    VkBufferDeviceAddressInfo vertexBufferAddressInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = nullptr,
        .buffer = vertexBuffer
    };
    VkBufferDeviceAddressInfo indexBufferAddressInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = nullptr,
        .buffer = indexBuffer
    };
    auto vkGetBufferDeviceAddressKHR = reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>(vkGetDeviceProcAddr(context.device, "vkGetBufferDeviceAddressKHR"));
    VkDeviceAddress vertexBufferAddress = vkGetBufferDeviceAddressKHR(context.device, &vertexBufferAddressInfo);
    VkDeviceAddress indexBufferAddress = vkGetBufferDeviceAddressKHR(context.device, &indexBufferAddressInfo);

    // Create BLAS
    VkAccelerationStructureGeometryTrianglesDataKHR triangles{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .pNext = nullptr,
        .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
        .vertexData = {vertexBufferAddress},
        .vertexStride = sizeof(glm::vec3),
        .maxVertex = static_cast<uint32_t>(vertices.size()),
        .indexType = VK_INDEX_TYPE_UINT32,
        .indexData = {indexBufferAddress},
        .transformData = {0}
    };
    VkAccelerationStructureGeometryKHR geometry{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .pNext = nullptr,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry = {},
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
    };
    geometry.geometry.triangles = triangles;
    VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .pNext = nullptr,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .srcAccelerationStructure = VK_NULL_HANDLE,
        .dstAccelerationStructure = VK_NULL_HANDLE,
        .geometryCount = 1,
        .pGeometries = &geometry,
        .ppGeometries = nullptr,
        .scratchData = {}
    };
    VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
        .pNext = nullptr,
        .accelerationStructureSize = 0,
        .updateScratchSize = 0,
        .buildScratchSize = 0
    };
    uint32_t primitiveCount = static_cast<uint32_t>(indices.size() / 3);
    auto vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(context.device, "vkGetAccelerationStructureBuildSizesKHR"));
    vkGetAccelerationStructureBuildSizesKHR(context.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildGeometryInfo, &primitiveCount, &buildSizesInfo);

    VkBuffer scratchBuffer;
    VkDeviceMemory scratchBufferMemory;
    VkBufferCreateInfo scratchBufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = buildSizesInfo.buildScratchSize,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr
    };
    if (vkCreateBuffer(context.device, &scratchBufferInfo, nullptr, &scratchBuffer) != VK_SUCCESS) {
        LOG_ERROR("Failed to create scratch buffer for AS");
        throw std::runtime_error("Failed to create scratch buffer for AS");
    }
    VkMemoryRequirements scratchMemRequirements;
    vkGetBufferMemoryRequirements(context.device, scratchBuffer, &scratchMemRequirements);
    VkMemoryAllocateInfo scratchAllocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = scratchMemRequirements.size,
        .memoryTypeIndex = findMemoryType(context.physicalDevice, scratchMemRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    if (vkAllocateMemory(context.device, &scratchAllocInfo, nullptr, &scratchBufferMemory) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate scratch buffer memory for AS");
        throw std::runtime_error("Failed to allocate scratch buffer memory for AS");
    }
    vkBindBufferMemory(context.device, scratchBuffer, scratchBufferMemory, 0);

    VkBufferDeviceAddressInfo scratchBufferAddressInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = nullptr,
        .buffer = scratchBuffer
    };
    VkDeviceAddress scratchBufferAddress = vkGetBufferDeviceAddressKHR(context.device, &scratchBufferAddressInfo);

    VkAccelerationStructureCreateInfoKHR asCreateInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .createFlags = 0,
        .buffer = nullptr,
        .offset = 0,
        .size = buildSizesInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .deviceAddress = 0
    };

    VkBuffer asBuffer;
    VkDeviceMemory asBufferMemory;
    VkBufferCreateInfo asBufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = buildSizesInfo.accelerationStructureSize,
        .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr
    };
    if (vkCreateBuffer(context.device, &asBufferInfo, nullptr, &asBuffer) != VK_SUCCESS) {
        LOG_ERROR("Failed to create AS buffer");
        throw std::runtime_error("Failed to create AS buffer");
    }
    VkMemoryRequirements asMemRequirements;
    vkGetBufferMemoryRequirements(context.device, asBuffer, &asMemRequirements);
    VkMemoryAllocateInfo asAllocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = asMemRequirements.size,
        .memoryTypeIndex = findMemoryType(context.physicalDevice, asMemRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    if (vkAllocateMemory(context.device, &asAllocInfo, nullptr, &asBufferMemory) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate AS buffer memory");
        throw std::runtime_error("Failed to allocate AS buffer memory");
    }
    vkBindBufferMemory(context.device, asBuffer, asBufferMemory, 0);
    asCreateInfo.buffer = asBuffer;

    auto vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(context.device, "vkCreateAccelerationStructureKHR"));
    if (vkCreateAccelerationStructureKHR(context.device, &asCreateInfo, nullptr, &context.bottomLevelAS) != VK_SUCCESS) {
        LOG_ERROR("Failed to create bottom level acceleration structure");
        throw std::runtime_error("Failed to create bottom level acceleration structure");
    }

    buildGeometryInfo.dstAccelerationStructure = context.bottomLevelAS;
    buildGeometryInfo.scratchData.deviceAddress = scratchBufferAddress;

    VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{
        .primitiveCount = primitiveCount,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0
    };
    VkAccelerationStructureBuildRangeInfoKHR* buildRangeInfos[] = {&buildRangeInfo};

    VkCommandBuffer commandBuffer;
    VkCommandBufferAllocateInfo cmdAllocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = context.commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    if (vkAllocateCommandBuffers(context.device, &cmdAllocInfo, &commandBuffer) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate command buffer for AS build");
        throw std::runtime_error("Failed to allocate command buffer for AS build");
    }

    VkCommandBufferBeginInfo cmdBeginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    if (vkBeginCommandBuffer(commandBuffer, &cmdBeginInfo) != VK_SUCCESS) {
        LOG_ERROR("Failed to begin command buffer for AS build");
        throw std::runtime_error("Failed to begin command buffer for AS build");
    }

    auto vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(context.device, "vkCmdBuildAccelerationStructuresKHR"));
    vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildGeometryInfo, buildRangeInfos);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        LOG_ERROR("Failed to end command buffer for AS build");
        throw std::runtime_error("Failed to end command buffer for AS build");
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
    if (vkQueueSubmit(context.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        LOG_ERROR("Failed to submit command buffer for AS build");
        throw std::runtime_error("Failed to submit command buffer for AS build");
    }
    vkQueueWaitIdle(context.graphicsQueue);
    vkFreeCommandBuffers(context.device, context.commandPool, 1, &commandBuffer);

    // Create TLAS
    VkTransformMatrixKHR transformMatrix = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f
    };
    VkAccelerationStructureInstanceKHR instance{
        .transform = transformMatrix,
        .instanceCustomIndex = 0,
        .mask = 0xFF,
        .instanceShaderBindingTableRecordOffset = 0,
        .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
        .accelerationStructureReference = 0
    };
    VkBufferDeviceAddressInfo asBufferAddressInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = nullptr,
        .buffer = asBuffer
    };
    instance.accelerationStructureReference = vkGetBufferDeviceAddressKHR(context.device, &asBufferAddressInfo);

    VkBuffer instanceBuffer;
    VkDeviceMemory instanceBufferMemory;
    VkBufferCreateInfo instanceBufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = sizeof(VkAccelerationStructureInstanceKHR),
        .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr
    };
    if (vkCreateBuffer(context.device, &instanceBufferInfo, nullptr, &instanceBuffer) != VK_SUCCESS) {
        LOG_ERROR("Failed to create instance buffer for TLAS");
        throw std::runtime_error("Failed to create instance buffer for TLAS");
    }
    VkMemoryRequirements instanceMemRequirements;
    vkGetBufferMemoryRequirements(context.device, instanceBuffer, &instanceMemRequirements);
    VkMemoryAllocateInfo instanceAllocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = instanceMemRequirements.size,
        .memoryTypeIndex = findMemoryType(context.physicalDevice, instanceMemRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };
    if (vkAllocateMemory(context.device, &instanceAllocInfo, nullptr, &instanceBufferMemory) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate instance buffer memory for TLAS");
        throw std::runtime_error("Failed to allocate instance buffer memory for TLAS");
    }
    vkBindBufferMemory(context.device, instanceBuffer, instanceBufferMemory, 0);
    void* instanceData;
    vkMapMemory(context.device, instanceBufferMemory, 0, sizeof(VkAccelerationStructureInstanceKHR), 0, &instanceData);
    memcpy(instanceData, &instance, sizeof(VkAccelerationStructureInstanceKHR));
    vkUnmapMemory(context.device, instanceBufferMemory);

    VkBufferDeviceAddressInfo instanceBufferAddressInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = nullptr,
        .buffer = instanceBuffer
    };
    VkDeviceAddress instanceBufferAddress = vkGetBufferDeviceAddressKHR(context.device, &instanceBufferAddressInfo);

    VkAccelerationStructureGeometryInstancesDataKHR instancesData{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        .pNext = nullptr,
        .arrayOfPointers = VK_FALSE,
        .data = {instanceBufferAddress}
    };
    VkAccelerationStructureGeometryKHR tlasGeometry{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .pNext = nullptr,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry = {},
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
    };
    tlasGeometry.geometry.instances = instancesData;
    VkAccelerationStructureBuildGeometryInfoKHR tlasBuildGeometryInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .pNext = nullptr,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .srcAccelerationStructure = VK_NULL_HANDLE,
        .dstAccelerationStructure = VK_NULL_HANDLE,
        .geometryCount = 1,
        .pGeometries = &tlasGeometry,
        .ppGeometries = nullptr,
        .scratchData = {}
    };
    VkAccelerationStructureBuildSizesInfoKHR tlasBuildSizesInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
        .pNext = nullptr,
        .accelerationStructureSize = 0,
        .updateScratchSize = 0,
        .buildScratchSize = 0
    };
    uint32_t tlasPrimitiveCount = 1;
    vkGetAccelerationStructureBuildSizesKHR(context.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &tlasBuildGeometryInfo, &tlasPrimitiveCount, &tlasBuildSizesInfo);

    VkBuffer tlasBuffer;
    VkDeviceMemory tlasBufferMemory;
    VkBufferCreateInfo tlasBufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = tlasBuildSizesInfo.accelerationStructureSize,
        .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr
    };
    if (vkCreateBuffer(context.device, &tlasBufferInfo, nullptr, &tlasBuffer) != VK_SUCCESS) {
        LOG_ERROR("Failed to create TLAS buffer");
        throw std::runtime_error("Failed to create TLAS buffer");
    }
    VkMemoryRequirements tlasMemRequirements;
    vkGetBufferMemoryRequirements(context.device, tlasBuffer, &tlasMemRequirements);
    VkMemoryAllocateInfo tlasAllocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = tlasMemRequirements.size,
        .memoryTypeIndex = findMemoryType(context.physicalDevice, tlasMemRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    if (vkAllocateMemory(context.device, &tlasAllocInfo, nullptr, &tlasBufferMemory) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate TLAS buffer memory");
        throw std::runtime_error("Failed to allocate TLAS buffer memory");
    }
    vkBindBufferMemory(context.device, tlasBuffer, tlasBufferMemory, 0);

    VkAccelerationStructureCreateInfoKHR tlasCreateInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .createFlags = 0,
        .buffer = tlasBuffer,
        .offset = 0,
        .size = tlasBuildSizesInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .deviceAddress = 0
    };
    if (vkCreateAccelerationStructureKHR(context.device, &tlasCreateInfo, nullptr, &context.topLevelAS) != VK_SUCCESS) {
        LOG_ERROR("Failed to create top level acceleration structure");
        throw std::runtime_error("Failed to create top level acceleration structure");
    }

    VkBuffer tlasScratchBuffer;
    VkDeviceMemory tlasScratchBufferMemory;
    VkBufferCreateInfo tlasScratchBufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = tlasBuildSizesInfo.buildScratchSize,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr
    };
    if (vkCreateBuffer(context.device, &tlasScratchBufferInfo, nullptr, &tlasScratchBuffer) != VK_SUCCESS) {
        LOG_ERROR("Failed to create TLAS scratch buffer");
        throw std::runtime_error("Failed to create TLAS scratch buffer");
    }
    VkMemoryRequirements tlasScratchMemRequirements;
    vkGetBufferMemoryRequirements(context.device, tlasScratchBuffer, &tlasScratchMemRequirements);
    VkMemoryAllocateInfo tlasScratchAllocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = tlasScratchMemRequirements.size,
        .memoryTypeIndex = findMemoryType(context.physicalDevice, tlasScratchMemRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    if (vkAllocateMemory(context.device, &tlasScratchAllocInfo, nullptr, &tlasScratchBufferMemory) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate TLAS scratch buffer memory");
        throw std::runtime_error("Failed to allocate TLAS scratch buffer memory");
    }
    vkBindBufferMemory(context.device, tlasScratchBuffer, tlasScratchBufferMemory, 0);

    VkBufferDeviceAddressInfo tlasScratchBufferAddressInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = nullptr,
        .buffer = tlasScratchBuffer
    };
    VkDeviceAddress tlasScratchBufferAddress = vkGetBufferDeviceAddressKHR(context.device, &tlasScratchBufferAddressInfo);

    tlasBuildGeometryInfo.dstAccelerationStructure = context.topLevelAS;
    tlasBuildGeometryInfo.scratchData.deviceAddress = tlasScratchBufferAddress;

    VkAccelerationStructureBuildRangeInfoKHR tlasBuildRangeInfo{
        .primitiveCount = tlasPrimitiveCount,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0
    };
    VkAccelerationStructureBuildRangeInfoKHR* tlasBuildRangeInfos[] = {&tlasBuildRangeInfo};

    VkCommandBuffer tlasCommandBuffer;
    if (vkAllocateCommandBuffers(context.device, &cmdAllocInfo, &tlasCommandBuffer) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate command buffer for TLAS build");
        throw std::runtime_error("Failed to allocate command buffer for TLAS build");
    }
    if (vkBeginCommandBuffer(tlasCommandBuffer, &cmdBeginInfo) != VK_SUCCESS) {
        LOG_ERROR("Failed to begin command buffer for TLAS build");
        throw std::runtime_error("Failed to begin command buffer for TLAS build");
    }
    vkCmdBuildAccelerationStructuresKHR(tlasCommandBuffer, 1, &tlasBuildGeometryInfo, tlasBuildRangeInfos);
    if (vkEndCommandBuffer(tlasCommandBuffer) != VK_SUCCESS) {
        LOG_ERROR("Failed to end command buffer for TLAS build");
        throw std::runtime_error("Failed to end command buffer for TLAS build");
    }
    if (vkQueueSubmit(context.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        LOG_ERROR("Failed to submit command buffer for TLAS build");
        throw std::runtime_error("Failed to submit command buffer for TLAS build");
    }
    vkQueueWaitIdle(context.graphicsQueue);
    vkFreeCommandBuffers(context.device, context.commandPool, 1, &tlasCommandBuffer);

    // Cleanup temporary buffers
    vkDestroyBuffer(context.device, vertexBuffer, nullptr);
    vkFreeMemory(context.device, vertexBufferMemory, nullptr);
    vkDestroyBuffer(context.device, indexBuffer, nullptr);
    vkFreeMemory(context.device, indexBufferMemory, nullptr);
    vkDestroyBuffer(context.device, scratchBuffer, nullptr);
    vkFreeMemory(context.device, scratchBufferMemory, nullptr);
    vkDestroyBuffer(context.device, instanceBuffer, nullptr);
    vkFreeMemory(context.device, instanceBufferMemory, nullptr);
    vkDestroyBuffer(context.device, asBuffer, nullptr);
    vkFreeMemory(context.device, asBufferMemory, nullptr);
    vkDestroyBuffer(context.device, tlasScratchBuffer, nullptr);
    vkFreeMemory(context.device, tlasScratchBufferMemory, nullptr);
    vkDestroyBuffer(context.device, tlasBuffer, nullptr);
    vkFreeMemory(context.device, tlasBufferMemory, nullptr);
}

void createShaderBindingTable(VulkanContext& context) {
    const uint32_t handleSize = 32; // Adjust based on device properties
    const uint32_t handleSizeAligned = (handleSize + 15) & ~15; // Align to 16 bytes
    const uint32_t groupCount = 3; // Raygen, Miss, Hit
    const uint32_t sbtSize = groupCount * handleSizeAligned;

    VkBuffer sbtBuffer;
    VkDeviceMemory sbtBufferMemory;
    VkBufferCreateInfo sbtBufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = sbtSize,
        .usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr
    };
    if (vkCreateBuffer(context.device, &sbtBufferInfo, nullptr, &sbtBuffer) != VK_SUCCESS) {
        LOG_ERROR("Failed to create SBT buffer");
        throw std::runtime_error("Failed to create SBT buffer");
    }
    VkMemoryRequirements sbtMemRequirements;
    vkGetBufferMemoryRequirements(context.device, sbtBuffer, &sbtMemRequirements);
    VkMemoryAllocateInfo sbtAllocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = sbtMemRequirements.size,
        .memoryTypeIndex = findMemoryType(context.physicalDevice, sbtMemRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };
    if (vkAllocateMemory(context.device, &sbtAllocInfo, nullptr, &sbtBufferMemory) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate SBT buffer memory");
        throw std::runtime_error("Failed to allocate SBT buffer memory");
    }
    vkBindBufferMemory(context.device, sbtBuffer, sbtBufferMemory, 0);

    auto vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddr(context.device, "vkGetRayTracingShaderGroupHandlesKHR"));
    std::vector<uint8_t> shaderGroupHandles(sbtSize);
    if (vkGetRayTracingShaderGroupHandlesKHR(context.device, context.rayTracingPipeline, 0, groupCount, sbtSize, shaderGroupHandles.data()) != VK_SUCCESS) {
        LOG_ERROR("Failed to get shader group handles");
        throw std::runtime_error("Failed to get shader group handles");
    }

    void* sbtData;
    vkMapMemory(context.device, sbtBufferMemory, 0, sbtSize, 0, &sbtData);
    memcpy(sbtData, shaderGroupHandles.data(), sbtSize);
    vkUnmapMemory(context.device, sbtBufferMemory);

    VkBufferDeviceAddressInfo sbtBufferAddressInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = nullptr,
        .buffer = sbtBuffer
    };
    auto vkGetBufferDeviceAddressKHR = reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>(vkGetDeviceProcAddr(context.device, "vkGetBufferDeviceAddressKHR"));
    VkDeviceAddress sbtBufferAddress = vkGetBufferDeviceAddressKHR(context.device, &sbtBufferAddressInfo);

    context.raygenSbtAddress = sbtBufferAddress;
    context.missSbtAddress = sbtBufferAddress + handleSizeAligned;
    context.hitSbtAddress = sbtBufferAddress + 2 * handleSizeAligned;
    context.sbtRecordSize = handleSizeAligned;

    context.sbtBuffer = sbtBuffer;
    context.sbtBufferMemory = sbtBufferMemory;

    LOG_INFO("Created shader binding table");
}

void createRayTracingPipeline(VulkanContext& context, VkPipelineLayout pipelineLayout, VkShaderModule rayGenShader, VkShaderModule missShader, VkShaderModule closestHitShader, VkPipeline& pipeline) {
    VkPipelineShaderStageCreateInfo shaderStages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
            .module = rayGenShader,
            .pName = "main",
            .pSpecializationInfo = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_MISS_BIT_KHR,
            .module = missShader,
            .pName = "main",
            .pSpecializationInfo = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
            .module = closestHitShader,
            .pName = "main",
            .pSpecializationInfo = nullptr
        }
    };

    VkRayTracingShaderGroupCreateInfoKHR shaderGroups[] = {
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .pNext = nullptr,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = 0,
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
            .pShaderGroupCaptureReplayHandle = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .pNext = nullptr,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = 1,
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
            .pShaderGroupCaptureReplayHandle = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .pNext = nullptr,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
            .generalShader = VK_SHADER_UNUSED_KHR,
            .closestHitShader = 2,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
            .pShaderGroupCaptureReplayHandle = nullptr
        }
    };

    VkRayTracingPipelineCreateInfoKHR pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .stageCount = 3,
        .pStages = shaderStages,
        .groupCount = 3,
        .pGroups = shaderGroups,
        .maxPipelineRayRecursionDepth = 1,
        .pLibraryInfo = nullptr,
        .pLibraryInterface = nullptr,
        .pDynamicState = nullptr,
        .layout = pipelineLayout,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };

    auto vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddr(context.device, "vkCreateRayTracingPipelinesKHR"));
    if (vkCreateRayTracingPipelinesKHR(context.device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
        LOG_ERROR("Failed to create ray tracing pipeline");
        throw std::runtime_error("Failed to create ray tracing pipeline");
    }
    LOG_INFO("Created ray tracing pipeline");
}

void cleanupVulkan(VulkanContext& context) {
    if (context.topLevelAS != VK_NULL_HANDLE) {
        auto vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(context.device, "vkDestroyAccelerationStructureKHR"));
        vkDestroyAccelerationStructureKHR(context.device, context.topLevelAS, nullptr);
        context.topLevelAS = VK_NULL_HANDLE;
        LOG_INFO("Destroyed top level acceleration structure");
    }
    if (context.bottomLevelAS != VK_NULL_HANDLE) {
        auto vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(context.device, "vkDestroyAccelerationStructureKHR"));
        vkDestroyAccelerationStructureKHR(context.device, context.bottomLevelAS, nullptr);
        context.bottomLevelAS = VK_NULL_HANDLE;
        LOG_INFO("Destroyed bottom level acceleration structure");
    }
    if (context.sbtBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(context.device, context.sbtBuffer, nullptr);
        context.sbtBuffer = VK_NULL_HANDLE;
        LOG_INFO("Destroyed SBT buffer");
    }
    if (context.sbtBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context.device, context.sbtBufferMemory, nullptr);
        context.sbtBufferMemory = VK_NULL_HANDLE;
        LOG_INFO("Freed SBT buffer memory");
    }
    if (context.storageImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(context.device, context.storageImageView, nullptr);
        context.storageImageView = VK_NULL_HANDLE;
        LOG_INFO("Destroyed storage image view");
    }
    if (context.storageImage != VK_NULL_HANDLE) {
        vkDestroyImage(context.device, context.storageImage, nullptr);
        context.storageImage = VK_NULL_HANDLE;
        LOG_INFO("Destroyed storage image");
    }
    if (context.storageImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context.device, context.storageImageMemory, nullptr);
        context.storageImageMemory = VK_NULL_HANDLE;
        LOG_INFO("Freed storage image memory");
    }
    if (context.sampler != VK_NULL_HANDLE) {
        vkDestroySampler(context.device, context.sampler, nullptr);
        context.sampler = VK_NULL_HANDLE;
        LOG_INFO("Destroyed sampler");
    }
    if (context.descriptorSet != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(context.device, context.descriptorPool, 1, &context.descriptorSet);
        context.descriptorSet = VK_NULL_HANDLE;
        LOG_INFO("Freed descriptor set");
    }
    if (context.descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(context.device, context.descriptorPool, nullptr);
        context.descriptorPool = VK_NULL_HANDLE;
        LOG_INFO("Destroyed descriptor pool");
    }
    if (context.rayTracingDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(context.device, context.rayTracingDescriptorSetLayout, nullptr);
        context.rayTracingDescriptorSetLayout = VK_NULL_HANDLE;
        LOG_INFO("Destroyed descriptor set layout");
    }
    if (context.rayTracingPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(context.device, context.rayTracingPipeline, nullptr);
        context.rayTracingPipeline = VK_NULL_HANDLE;
        LOG_INFO("Destroyed ray tracing pipeline");
    }
    if (context.rayTracingPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(context.device, context.rayTracingPipelineLayout, nullptr);
        context.rayTracingPipelineLayout = VK_NULL_HANDLE;
        LOG_INFO("Destroyed pipeline layout");
    }
    if (context.commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(context.device, context.commandPool, nullptr);
        context.commandPool = VK_NULL_HANDLE;
        LOG_INFO("Destroyed command pool");
    }
    if (context.device != VK_NULL_HANDLE) {
        vkDestroyDevice(context.device, nullptr);
        context.device = VK_NULL_HANDLE;
        LOG_INFO("Destroyed logical device");
    }
}

} // namespace VulkanInitializer

VulkanSwapchainManager::VulkanSwapchainManager(VulkanContext& context, VkSurfaceKHR surface)
    : context_(context), swapchain_(VK_NULL_HANDLE), imageCount_(0), swapchainImageFormat_(VK_FORMAT_UNDEFINED), swapchainExtent_{0, 0} {
    context_.surface = surface;
    LOG_INFO("Initialized VulkanSwapchainManager");
}

VulkanSwapchainManager::~VulkanSwapchainManager() {
    cleanupSwapchain();
}

void VulkanSwapchainManager::initializeSwapchain(int width, int height) {
    VulkanInitializer::createSwapchain(context_, width, height);
    VulkanInitializer::createImageViews(context_);
    swapchain_ = context_.swapchain;
    swapchainImageFormat_ = context_.swapchainImageFormat_;
    swapchainExtent_ = context_.swapchainExtent_;
    swapchainImages_ = context_.swapchainImages;
    swapchainImageViews_ = context_.swapchainImageViews;
    imageCount_ = static_cast<uint32_t>(swapchainImages_.size());
    LOG_INFO("Swapchain initialized: {} images, format={}, extent={}x{}", 
             imageCount_, swapchainImageFormat_, 
             swapchainExtent_.width, swapchainExtent_.height);
}

void VulkanSwapchainManager::cleanupSwapchain() {
    for (auto imageView : swapchainImageViews_) {
        if (imageView != VK_NULL_HANDLE) {
            LOG_INFO("Destroying image view");
            vkDestroyImageView(context_.device, imageView, nullptr);
        }
    }
    swapchainImageViews_.clear();
    swapchainImages_.clear();
    if (swapchain_ != VK_NULL_HANDLE) {
        LOG_INFO("Destroying swapchain");
        vkDestroySwapchainKHR(context_.device, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
    LOG_INFO("Swapchain cleaned up");
}

void VulkanSwapchainManager::handleResize(int width, int height) {
    cleanupSwapchain();
    initializeSwapchain(width, height);
    LOG_INFO("Resized swapchain to {}x{}", width, height);
}

VulkanPipelineManager::VulkanPipelineManager(VulkanContext& context, int width, int height)
    : context_(context), width_(width), height_(height), renderPass_(VK_NULL_HANDLE),
      graphicsPipeline_(VK_NULL_HANDLE), rayGenShaderModule_(VK_NULL_HANDLE), 
      missShaderModule_(VK_NULL_HANDLE), closestHitShaderModule_(VK_NULL_HANDLE) {
    createRenderPass();
    VulkanInitializer::createDescriptorSetLayout(context_.device, context_.rayTracingDescriptorSetLayout);
    createPipelineLayout();
    createRayTracingPipeline();
    createGraphicsPipeline();
    LOG_INFO("Initialized VulkanPipelineManager {}x{}", width, height);
}

VulkanPipelineManager::~VulkanPipelineManager() {
    cleanupPipeline();
}

void VulkanPipelineManager::createRenderPass() {
    VulkanInitializer::createRenderPass(context_.device, renderPass_, context_.swapchainImageFormat_);
    LOG_INFO("Created render pass");
}

void VulkanPipelineManager::createPipelineLayout() {
    VkPushConstantRange pushConstantRange{
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        .offset = 0,
        .size = sizeof(int)
    };
    VkPipelineLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = &context_.rayTracingDescriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange
    };
    if (vkCreatePipelineLayout(context_.device, &layoutInfo, nullptr, &context_.rayTracingPipelineLayout) != VK_SUCCESS) {
        LOG_ERROR("Failed to create pipeline layout");
        throw std::runtime_error("Failed to create pipeline layout");
    }
    LOG_INFO("Created pipeline layout");
}

void VulkanPipelineManager::createRayTracingPipeline() {
    std::filesystem::path rayGenPath = "assets/shaders/raytracing/raygen.spv";
    std::filesystem::path missPath = "assets/shaders/raytracing/miss.spv";
    std::filesystem::path closestHitPath = "assets/shaders/raytracing/closesthit.spv";

    if (!std::filesystem::exists(rayGenPath)) {
        LOG_ERROR("Ray generation shader file does not exist: {}", rayGenPath.string());
        throw std::runtime_error("Ray generation shader file does not exist: " + rayGenPath.string());
    }
    if (!std::filesystem::exists(missPath)) {
        LOG_ERROR("Miss shader file does not exist: {}", missPath.string());
        throw std::runtime_error("Miss shader file does not exist: " + missPath.string());
    }
    if (!std::filesystem::exists(closestHitPath)) {
        LOG_ERROR("Closest hit shader file does not exist: {}", closestHitPath.string());
        throw std::runtime_error("Closest hit shader file does not exist: " + closestHitPath.string());
    }

    rayGenShaderModule_ = VulkanInitializer::loadShader(context_.device, rayGenPath.string());
    missShaderModule_ = VulkanInitializer::loadShader(context_.device, missPath.string());
    closestHitShaderModule_ = VulkanInitializer::loadShader(context_.device, closestHitPath.string());

    VulkanInitializer::createRayTracingPipeline(context_, context_.rayTracingPipelineLayout, rayGenShaderModule_, missShaderModule_, closestHitShaderModule_, context_.rayTracingPipeline);
    LOG_INFO("Created ray tracing pipeline");
}

void VulkanPipelineManager::createGraphicsPipeline() {
    std::filesystem::path vertPath = "assets/shaders/rasterization/vertex.spv";
    std::filesystem::path fragPath = "assets/shaders/rasterization/fragment.spv";
    VkShaderModule vertShaderModule = VulkanInitializer::loadShader(context_.device, vertPath.string());
    VkShaderModule fragShaderModule = VulkanInitializer::loadShader(context_.device, fragPath.string());

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
        .width = static_cast<float>(width_),
        .height = static_cast<float>(height_),
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    VkRect2D scissor{{0, 0}, {static_cast<uint32_t>(width_), static_cast<uint32_t>(height_)}};
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
        .cullMode = VK_CULL_MODE_NONE,
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
        .layout = context_.rayTracingPipelineLayout,
        .renderPass = renderPass_,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };
    if (vkCreateGraphicsPipelines(context_.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline_) != VK_SUCCESS) {
        LOG_ERROR("Failed to create graphics pipeline");
        throw std::runtime_error("Failed to create graphics pipeline");
    }
    vkDestroyShaderModule(context_.device, vertShaderModule, nullptr);
    vkDestroyShaderModule(context_.device, fragShaderModule, nullptr);
    LOG_INFO("Created graphics pipeline");
}

void VulkanPipelineManager::cleanupPipeline() {
    if (graphicsPipeline_ != VK_NULL_HANDLE) {
        LOG_INFO("Destroying graphics pipeline");
        vkDestroyPipeline(context_.device, graphicsPipeline_, nullptr);
        graphicsPipeline_ = VK_NULL_HANDLE;
    }
    if (renderPass_ != VK_NULL_HANDLE) {
        LOG_INFO("Destroying render pass");
        vkDestroyRenderPass(context_.device, renderPass_, nullptr);
        renderPass_ = VK_NULL_HANDLE;
    }
    if (rayGenShaderModule_ != VK_NULL_HANDLE) {
        LOG_INFO("Destroying ray gen shader module");
        vkDestroyShaderModule(context_.device, rayGenShaderModule_, nullptr);
        rayGenShaderModule_ = VK_NULL_HANDLE;
    }
    if (missShaderModule_ != VK_NULL_HANDLE) {
        LOG_INFO("Destroying miss shader module");
        vkDestroyShaderModule(context_.device, missShaderModule_, nullptr);
        missShaderModule_ = VK_NULL_HANDLE;
    }
    if (closestHitShaderModule_ != VK_NULL_HANDLE) {
        LOG_INFO("Destroying closest hit shader module");
        vkDestroyShaderModule(context_.device, closestHitShaderModule_, nullptr);
        closestHitShaderModule_ = VK_NULL_HANDLE;
    }
    LOG_INFO("Pipeline cleaned up");
}

VulkanRenderer::VulkanRenderer(VkInstance instance, VkSurfaceKHR surface,
                              std::span<const glm::vec3> vertices, std::span<const uint32_t> indices,
                              int width, int height)
    : context_{}, width_(width), height_(height), imageAvailableSemaphore_(VK_NULL_HANDLE),
      renderFinishedSemaphore_(VK_NULL_HANDLE), inFlightFence_(VK_NULL_HANDLE) {
    context_.instance = instance;
    context_.surface = surface;
    VulkanInitializer::initializeVulkan(context_, width, height);
    swapchainManager_ = std::make_unique<VulkanSwapchainManager>(context_, surface);
    swapchainManager_->initializeSwapchain(width, height);
    context_.swapchain = swapchainManager_->getSwapchain();
    context_.swapchainImageFormat_ = swapchainManager_->getSwapchainImageFormat();
    context_.swapchainExtent_ = swapchainManager_->getSwapchainExtent();
    context_.swapchainImages = swapchainManager_->getSwapchainImages();
    context_.swapchainImageViews = swapchainManager_->getSwapchainImageViews();
    bufferManager_ = std::make_unique<VulkanBufferManager>(context_);
    bufferManager_->initializeBuffers(vertices, indices);
    context_.vertexBuffer_ = bufferManager_->getVertexBuffer();
    context_.indexBuffer_ = bufferManager_->getIndexBuffer();
    bufferManager_->createUniformBuffers(static_cast<uint32_t>(swapchainManager_->getSwapchainImageViews().size()));
    pipelineManager_ = std::make_unique<VulkanPipelineManager>(context_, width, height);
    VulkanInitializer::createStorageImage(context_.device, context_.physicalDevice, context_.storageImage,
                                         context_.storageImageMemory, context_.storageImageView, width, height);
    VulkanInitializer::createAccelerationStructures(context_, vertices, indices);
    VulkanInitializer::createShaderBindingTable(context_);
    VulkanInitializer::createDescriptorPoolAndSet(context_.device, context_.rayTracingDescriptorSetLayout,
                                                 context_.descriptorPool, context_.descriptorSet, context_.sampler,
                                                 bufferManager_->getUniformBuffer(0), context_.storageImageView, context_.topLevelAS);
    createFramebuffers();
    createCommandBuffers();
    createSyncObjects();
    LOG_INFO("VulkanRenderer initialized {}x{}", width, height);
}

VulkanRenderer::~VulkanRenderer() {
    vkDeviceWaitIdle(context_.device);
    LOG_INFO("Destroying VulkanRenderer");

    // Free command buffers
    if (!commandBuffers_.empty() && context_.commandPool != VK_NULL_HANDLE) {
        LOG_INFO("Freeing command buffers");
        vkFreeCommandBuffers(context_.device, context_.commandPool, static_cast<uint32_t>(commandBuffers_.size()), commandBuffers_.data());
        commandBuffers_.clear();
    }

    // Destroy sync objects
    if (imageAvailableSemaphore_ != VK_NULL_HANDLE) {
        LOG_INFO("Destroying image available semaphore");
        vkDestroySemaphore(context_.device, imageAvailableSemaphore_, nullptr);
        imageAvailableSemaphore_ = VK_NULL_HANDLE;
    }
    if (renderFinishedSemaphore_ != VK_NULL_HANDLE) {
        LOG_INFO("Destroying render finished semaphore");
        vkDestroySemaphore(context_.device, renderFinishedSemaphore_, nullptr);
        renderFinishedSemaphore_ = VK_NULL_HANDLE;
    }
    if (inFlightFence_ != VK_NULL_HANDLE) {
        LOG_INFO("Destroying in-flight fence");
        vkDestroyFence(context_.device, inFlightFence_, nullptr);
        inFlightFence_ = VK_NULL_HANDLE;
    }

    // Destroy framebuffers
    for (auto framebuffer : framebuffers_) {
        if (framebuffer != VK_NULL_HANDLE) {
            LOG_INFO("Destroying framebuffer");
            vkDestroyFramebuffer(context_.device, framebuffer, nullptr);
        }
    }
    framebuffers_.clear();

    // Reset managers (they clean up their owned resources)
    pipelineManager_.reset();
    swapchainManager_.reset();
    bufferManager_.reset();

    // Clear context members destroyed by managers
    context_.swapchain = VK_NULL_HANDLE;
    context_.swapchainImages.clear();
    context_.swapchainImageViews.clear();
    context_.swapchainImageFormat_ = VK_FORMAT_UNDEFINED;
    context_.swapchainExtent_ = {0, 0};

    // Cleanup all remaining Vulkan resources in context
    VulkanInitializer::cleanupVulkan(context_);
}

void VulkanRenderer::createFramebuffers() {
    framebuffers_.resize(swapchainManager_->getSwapchainImageViews().size());
    for (size_t i = 0; i < framebuffers_.size(); ++i) {
        VkImageView attachments[] = {swapchainManager_->getSwapchainImageViews()[i]};
        VkFramebufferCreateInfo framebufferInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderPass = pipelineManager_->getRenderPass(),
            .attachmentCount = 1,
            .pAttachments = attachments,
            .width = swapchainManager_->getSwapchainExtent().width,
            .height = swapchainManager_->getSwapchainExtent().height,
            .layers = 1
        };
        if (vkCreateFramebuffer(context_.device, &framebufferInfo, nullptr, &framebuffers_[i]) != VK_SUCCESS) {
            LOG_ERROR("Failed to create framebuffer[{}]", i);
            throw std::runtime_error("Failed to create framebuffer");
        }
        LOG_INFO("Created framebuffer[{}]", i);
    }
}

void VulkanRenderer::createCommandBuffers() {
    commandBuffers_.resize(swapchainManager_->getSwapchainImageViews().size());
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = context_.commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<uint32_t>(commandBuffers_.size())
    };
    if (vkAllocateCommandBuffers(context_.device, &allocInfo, commandBuffers_.data()) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate command buffers");
        throw std::runtime_error("Failed to allocate command buffers");
    }
    LOG_INFO("Allocated {} command buffers", commandBuffers_.size());
}

void VulkanRenderer::createSyncObjects() {
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
    if (vkCreateSemaphore(context_.device, &semaphoreInfo, nullptr, &imageAvailableSemaphore_) != VK_SUCCESS ||
        vkCreateSemaphore(context_.device, &semaphoreInfo, nullptr, &renderFinishedSemaphore_) != VK_SUCCESS ||
        vkCreateFence(context_.device, &fenceInfo, nullptr, &inFlightFence_) != VK_SUCCESS) {
        LOG_ERROR("Failed to create sync objects");
        throw std::runtime_error("Failed to create sync objects");
    }
    LOG_INFO("Created sync objects");
}

void VulkanRenderer::renderFrame(const AMOURANTH& camera) {
    vkWaitForFences(context_.device, 1, &inFlightFence_, VK_TRUE, UINT64_MAX);
    vkResetFences(context_.device, 1, &inFlightFence_);
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(context_.device, swapchainManager_->getSwapchain(), 
                                            UINT64_MAX, imageAvailableSemaphore_, VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        handleResize(width_, height_);
        return;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        LOG_ERROR("Failed to acquire swapchain image: {}", result);
        throw std::runtime_error("Failed to acquire swapchain image");
    }

    struct UniformData {
        UE::UniformBufferObject ubo;
        int mode;
    } uniformData;
    uniformData.ubo = {
        .model = glm::mat4(1.0f),
        .view = camera.getViewMatrix(),
        .proj = camera.getProjectionMatrix()
    };
    uniformData.mode = camera.getMode();

    void* data;
    vkMapMemory(context_.device, bufferManager_->getUniformBufferMemory(imageIndex), 
                0, sizeof(UniformData), 0, &data);
    memcpy(data, &uniformData, sizeof(UniformData));
    vkUnmapMemory(context_.device, bufferManager_->getUniformBufferMemory(imageIndex));
    LOG_DEBUG("Updated uniform buffer for image {}", imageIndex);

    VkCommandBuffer commandBuffer = commandBuffers_[imageIndex];
    vkResetCommandBuffer(commandBuffer, 0);
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pInheritanceInfo = nullptr
    };
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        LOG_ERROR("Failed to begin command buffer: {}", commandBuffer);
        throw std::runtime_error("Failed to begin command buffer");
    }

    VkImageMemoryBarrier imageBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = context_.storageImage,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, context_.rayTracingPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, context_.rayTracingPipelineLayout,
                            0, 1, &context_.descriptorSet, 0, nullptr);

    VkStridedDeviceAddressRegionKHR raygenSbt{}, missSbt{}, hitSbt{}, callableSbt{};
    raygenSbt.deviceAddress = context_.raygenSbtAddress;
    raygenSbt.stride = context_.sbtRecordSize;
    raygenSbt.size = context_.sbtRecordSize;
    missSbt.deviceAddress = context_.missSbtAddress;
    missSbt.stride = context_.sbtRecordSize;
    missSbt.size = context_.sbtRecordSize;
    hitSbt.deviceAddress = context_.hitSbtAddress;
    hitSbt.stride = context_.sbtRecordSize;
    hitSbt.size = context_.sbtRecordSize;
    callableSbt.deviceAddress = 0;
    callableSbt.stride = 0;
    callableSbt.size = 0;

    auto vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(context_.device, "vkCmdTraceRaysKHR"));
    if (!vkCmdTraceRaysKHR) {
        LOG_ERROR("Failed to get vkCmdTraceRaysKHR function pointer");
        throw std::runtime_error("Failed to get vkCmdTraceRaysKHR");
    }
    vkCmdTraceRaysKHR(commandBuffer, &raygenSbt, &missSbt, &hitSbt, &callableSbt,
                      swapchainManager_->getSwapchainExtent().width, swapchainManager_->getSwapchainExtent().height, 1);

    VkImageMemoryBarrier storageToShaderReadBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = context_.storageImage,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &storageToShaderReadBarrier);

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = pipelineManager_->getRenderPass(),
        .framebuffer = framebuffers_[imageIndex],
        .renderArea = {{0, 0}, swapchainManager_->getSwapchainExtent()},
        .clearValueCount = 1,
        .pClearValues = &clearColor
    };
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineManager_->getGraphicsPipeline());
    VkBuffer vertexBuffers[] = {context_.vertexBuffer_};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, context_.indexBuffer_, 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineManager_->getPipelineLayout(),
                            0, 1, &context_.descriptorSet, 0, nullptr);
    vkCmdDrawIndexed(commandBuffer, bufferManager_->getIndexCount(), 1, 0, 0, 0);
    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        LOG_ERROR("Failed to end command buffer: {}", commandBuffer);
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
    if (vkQueueSubmit(context_.graphicsQueue, 1, &submitInfo, inFlightFence_) != VK_SUCCESS) {
        LOG_ERROR("Failed to submit draw command buffer");
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
        LOG_ERROR("Failed to present swapchain image: {}", result);
        throw std::runtime_error("Failed to present swapchain image");
    }
}

void VulkanRenderer::handleResize(int width, int height) {
    vkDeviceWaitIdle(context_.device);
    width_ = width;
    height_ = height;
    swapchainManager_->handleResize(width, height);
    pipelineManager_->cleanupPipeline();
    pipelineManager_ = std::make_unique<VulkanPipelineManager>(context_, width, height);
    for (auto framebuffer : framebuffers_) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(context_.device, framebuffer, nullptr);
        }
    }
    framebuffers_.clear();
    createFramebuffers();
    createCommandBuffers();
    LOG_INFO("Handled resize to {}x{}", width, height);
}

VkDeviceMemory VulkanRenderer::getVertexBufferMemory() const {
    return bufferManager_ ? bufferManager_->getVertexBufferMemory() : VK_NULL_HANDLE;
}

VkDeviceMemory VulkanRenderer::getIndexBufferMemory() const {
    return bufferManager_ ? bufferManager_->getIndexBufferMemory() : VK_NULL_HANDLE;
}