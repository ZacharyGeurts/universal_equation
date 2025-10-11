// SDL3_vulkan.cpp
// AMOURANTH RTX Engine, October 2025 - Vulkan instance and surface initialization with SDL3.
// Dependencies: SDL3, Vulkan 1.3+, C++20 standard library.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#include "engine/SDL3/SDL3_vulkan.hpp"
#include "engine/logging.hpp"
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>
#include <source_location>
#include <cstring>

namespace SDL3Initializer {

void VulkanInstanceDeleter::operator()(VkInstance instance) {
    if (instance != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", "Destroying Vulkan instance: {:p}", std::source_location::current(), static_cast<void*>(instance));
        vkDestroyInstance(instance, nullptr);
    }
}

void VulkanSurfaceDeleter::operator()(VkSurfaceKHR surface) {
    if (surface != VK_NULL_HANDLE && m_instance != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", "Destroying Vulkan surface: {:p}", std::source_location::current(), static_cast<void*>(surface));
        vkDestroySurfaceKHR(m_instance, surface, nullptr);
    }
}

void initVulkan(
    SDL_Window* window, 
    VulkanInstancePtr& instance,
    VulkanSurfacePtr& surface,
    bool enableValidation, 
    bool preferNvidia, 
    bool rt, 
    std::string_view title) {
    if (!window) {
        LOG_ERROR("Invalid SDL window pointer", std::source_location::current());
        throw std::runtime_error("Invalid SDL window pointer");
    }

    // Get required Vulkan extensions from SDL
    uint32_t extensionCount = 0;
    const char* const* extensionNames = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    if (!extensionNames) {
        LOG_ERROR("Failed to get Vulkan extensions from SDL: {}", std::source_location::current(), SDL_GetError());
        throw std::runtime_error("Failed to get Vulkan extensions from SDL");
    }
    std::vector<const char*> extensions(extensionNames, extensionNames + extensionCount);

    // Add additional extensions for ray tracing if required
    if (rt) {
        extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    }

    // Validation layers
    std::vector<const char*> layers;
    if (enableValidation) {
        layers.push_back("VK_LAYER_KHRONOS_validation");
        // Verify validation layer support
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
        bool validationSupported = false;
        for (const auto& layer : availableLayers) {
            if (strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
                validationSupported = true;
                break;
            }
        }
        if (!validationSupported) {
            LOG_WARNING("Validation layers requested but not available", std::source_location::current());
            layers.clear();
        }
    }

    // Vulkan instance creation
    VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = title.data(),
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "AMOURANTH RTX Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3
    };

    VkInstanceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(layers.size()),
        .ppEnabledLayerNames = layers.empty() ? nullptr : layers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data()
    };

    VkInstance rawInstance;
    if (vkCreateInstance(&createInfo, nullptr, &rawInstance) != VK_SUCCESS) {
        LOG_ERROR("Failed to create Vulkan instance", std::source_location::current());
        throw std::runtime_error("Failed to create Vulkan instance");
    }
    instance = VulkanInstancePtr(rawInstance, VulkanInstanceDeleter());
    LOG_INFO("Created Vulkan instance: {:p}", std::source_location::current(), static_cast<void*>(rawInstance));

    // Create Vulkan surface
    VkSurfaceKHR rawSurface;
    if (!SDL_Vulkan_CreateSurface(window, rawInstance, nullptr, &rawSurface)) {
        LOG_ERROR("Failed to create Vulkan surface: {}", std::source_location::current(), SDL_GetError());
        throw std::runtime_error("Failed to create Vulkan surface: " + std::string(SDL_GetError()));
    }
    surface = VulkanSurfacePtr(rawSurface, VulkanSurfaceDeleter(rawInstance));
    LOG_INFO("Created Vulkan surface: {:p}", std::source_location::current(), static_cast<void*>(rawSurface));
}

VkInstance getVkInstance(const VulkanInstancePtr& instance) {
    return instance.get();
}

VkSurfaceKHR getVkSurface(const VulkanSurfacePtr& surface) {
    return surface.get();
}

std::vector<std::string> getVulkanExtensions() {
    uint32_t extensionCount = 0;
    const char* const* extensionNames = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    if (!extensionNames) {
        LOG_ERROR("Failed to get Vulkan extensions from SDL: {}", std::source_location::current(), SDL_GetError());
        throw std::runtime_error("Failed to get Vulkan extensions from SDL");
    }
    std::vector<std::string> result(extensionCount);
    for (uint32_t i = 0; i < extensionCount; ++i) {
        result[i] = extensionNames[i];
    }
    return result;
}

} // namespace SDL3Initializer