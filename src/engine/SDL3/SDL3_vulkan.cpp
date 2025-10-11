// SDL3_vulkan.cpp
// AMOURANTH RTX Engine, October 2025 - SDL3 and Vulkan integration.
// Dependencies: SDL3, Vulkan 1.3, VulkanCore.hpp, logging.hpp.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#include "VulkanCore.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include "engine/logging.hpp"
#include <stdexcept>
#include <source_location>
#include <vector>
#include <string_view>

namespace SDL3Initializer {

using VulkanInstancePtr = VkInstance;
using VulkanSurfacePtr = VkSurfaceKHR;

void initVulkan(SDL_Window* window, VulkanInstancePtr& vkInstance, VulkanSurfacePtr& vkSurface,
                bool enableValidation, bool enableDebug, bool enableRayTracing, std::string_view appName) {
    LOG_INFO("Initializing Vulkan with appName={}", std::source_location::current(), appName);

    Uint32 extensionCount = 0;
    const char* const* extensionsRaw = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    if (!extensionsRaw) {
        LOG_ERROR("SDL_Vulkan_GetInstanceExtensions failed: {}", std::source_location::current(), SDL_GetError());
        throw std::runtime_error(std::string("SDL_Vulkan_GetInstanceExtensions failed: ") + SDL_GetError());
    }
    std::vector<const char*> extensions(extensionsRaw, extensionsRaw + extensionCount);
    if (enableDebug) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    if (enableRayTracing) {
        extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    }
    LOG_DEBUG("Vulkan instance extensions retrieved: count={}", std::source_location::current(), extensions.size());

    const char* validationLayers[] = {"VK_LAYER_KHRONOS_validation"};
    uint32_t layerCount = enableValidation ? 1 : 0;

    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = appName.data(),
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "AMOURANTH RTX Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3,
    };

    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = layerCount,
        .ppEnabledLayerNames = validationLayers,
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
    };

    VkResult result = vkCreateInstance(&createInfo, nullptr, &vkInstance);
    if (result != VK_SUCCESS) {
        LOG_ERROR("vkCreateInstance failed with result={}", std::source_location::current(), static_cast<int>(result));
        throw std::runtime_error("vkCreateInstance failed: " + std::to_string(static_cast<int>(result)));
    }
    LOG_DEBUG("Vulkan instance created: {:p}", std::source_location::current(), static_cast<void*>(vkInstance));

    if (!SDL_Vulkan_CreateSurface(window, vkInstance, nullptr, &vkSurface)) {
        LOG_ERROR("SDL_Vulkan_CreateSurface failed: {}", std::source_location::current(), SDL_GetError());
        vkDestroyInstance(vkInstance, nullptr);
        throw std::runtime_error(std::string("SDL_Vulkan_CreateSurface failed: ") + SDL_GetError());
    }
    LOG_DEBUG("Vulkan surface created: {:p}", std::source_location::current(), static_cast<void*>(vkSurface));

    VulkanContext context;
    context.instance = vkInstance;
    context.surface = vkSurface;
    VulkanInitializer::initializeVulkan(context, 1280, 720);
    vkInstance = context.instance;
    vkSurface = context.surface;
}

} // namespace SDL3Initializer