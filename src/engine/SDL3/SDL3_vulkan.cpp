// AMOURANTH RTX Engine, October 2025 - Manages Vulkan instance and surface creation with platform-specific extensions.
// Dependencies: SDL3, Vulkan 1.3+, C++20 standard library, logging.hpp.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#include "engine/SDL3/SDL3_vulkan.hpp"
#include "engine/logging.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#ifdef _WIN32
#include <vulkan/vulkan_win32.h> // For VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#endif
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <source_location>

namespace SDL3Initializer {

void VulkanInstanceDeleter::operator()(VkInstance instance) {
    if (instance) {
        LOG_INFO_CAT("Vulkan", "Destroying Vulkan instance", std::source_location::current());
        vkDestroyInstance(instance, nullptr);
    }
}

void VulkanSurfaceDeleter::operator()(VkSurfaceKHR surface) {
    if (surface && m_instance) {
        LOG_INFO_CAT("Vulkan", "Destroying Vulkan surface", std::source_location::current());
        vkDestroySurfaceKHR(m_instance, surface, nullptr);
    } else if (surface) {
        LOG_WARNING_CAT("Vulkan", "Cannot destroy VkSurfaceKHR because VkInstance is invalid", std::source_location::current());
    }
}

void initVulkan(
    SDL_Window* window, 
    VulkanInstancePtr& instance,
    VulkanSurfacePtr& surface,
    bool enableValidation, 
    [[maybe_unused]] bool preferNvidia, // Marked as unused to suppress warning
    bool rt, 
    std::string_view title) 
{
    LOG_INFO_CAT("Vulkan", "Initializing Vulkan with title: {}", std::source_location::current(), title);

    // Verify SDL_Window has Vulkan flag
    if ((SDL_GetWindowFlags(window) & SDL_WINDOW_VULKAN) == 0) {
        LOG_ERROR_CAT("Vulkan", "SDL_Window not created with SDL_WINDOW_VULKAN flag", std::source_location::current());
        throw std::runtime_error("SDL_Window not created with SDL_WINDOW_VULKAN flag");
    }

    // Check SDL video driver to prefer Wayland if available (avoids X11 issues)
    const char* videoDriver = SDL_GetCurrentVideoDriver();
    LOG_INFO_CAT("Vulkan", "Current SDL video driver: {}", std::source_location::current(), videoDriver ? videoDriver : "none");

    // Get Vulkan instance extensions from SDL (handles KHR surface extensions for AMD/Intel/NVIDIA)
    LOG_DEBUG_CAT("Vulkan", "Getting Vulkan instance extensions from SDL", std::source_location::current());
    Uint32 extCount;
    auto exts = SDL_Vulkan_GetInstanceExtensions(&extCount);
    if (!exts || !extCount) {
        LOG_ERROR_CAT("Vulkan", "SDL_Vulkan_GetInstanceExtensions failed: {}", std::source_location::current(), SDL_GetError());
        throw std::runtime_error(std::string("SDL_Vulkan_GetInstanceExtensions failed: ") + SDL_GetError());
    }
    std::vector<const char*> extensions(exts, exts + extCount);
    LOG_INFO_CAT("Vulkan", "Required SDL Vulkan extensions: {}", std::source_location::current(), extCount);
    for (uint32_t i = 0; i < extCount; ++i) {
        LOG_DEBUG_CAT("Vulkan", "Extension {}: {}", std::source_location::current(), i, extensions[i]);
    }

    // Check available Vulkan instance extensions
    LOG_DEBUG_CAT("Vulkan", "Checking Vulkan instance extensions", std::source_location::current());
    uint32_t instanceExtCount;
    VkResult result = vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtCount, nullptr);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "vkEnumerateInstanceExtensionProperties failed: {}", std::source_location::current(), static_cast<int>(result));
        throw std::runtime_error("vkEnumerateInstanceExtensionProperties failed: " + std::to_string(static_cast<int>(result)));
    }
    std::vector<VkExtensionProperties> instanceExtensions(instanceExtCount);
    result = vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtCount, instanceExtensions.data());
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "vkEnumerateInstanceExtensionProperties failed: {}", std::source_location::current(), static_cast<int>(result));
        throw std::runtime_error("vkEnumerateInstanceExtensionProperties failed: " + std::to_string(static_cast<int>(result)));
    }

    bool hasSurfaceExtension = false;
    for (const auto& ext : instanceExtensions) {
        LOG_DEBUG_CAT("Vulkan", "Available instance extension: {}", std::source_location::current(), ext.extensionName);
        if (std::strcmp(ext.extensionName, VK_KHR_SURFACE_EXTENSION_NAME) == 0) {
            hasSurfaceExtension = true;
            LOG_INFO_CAT("Vulkan", "VK_KHR_surface extension found", std::source_location::current());
        }
    }

    // Add ray-tracing KHR extensions if requested
    if (rt) {
        LOG_DEBUG_CAT("Vulkan", "Enumerating Vulkan instance extension properties for ray tracing", std::source_location::current());
        const char* rtExts[] = {
            VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
            VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
            VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
        };
        for (auto ext : rtExts) {
            if (std::any_of(instanceExtensions.begin(), instanceExtensions.end(), 
                            [ext](const auto& p) { return std::strcmp(p.extensionName, ext) == 0; })) {
                LOG_DEBUG_CAT("Vulkan", "Adding Vulkan KHR extension: {}", std::source_location::current(), ext);
                extensions.push_back(ext);
            } else {
                LOG_WARNING_CAT("Vulkan", "Ray tracing extension {} not supported", std::source_location::current(), ext);
            }
        }
    }

    std::vector<const char*> layers;
#ifndef NDEBUG
    if (enableValidation) {
        LOG_INFO_CAT("Vulkan", "Adding Vulkan validation layer", std::source_location::current());
        uint32_t layerCount;
        result = vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        if (result != VK_SUCCESS) {
            LOG_ERROR_CAT("Vulkan", "vkEnumerateInstanceLayerProperties failed: {}", std::source_location::current(), static_cast<int>(result));
            throw std::runtime_error("vkEnumerateInstanceLayerProperties failed: " + std::to_string(static_cast<int>(result)));
        }
        std::vector<VkLayerProperties> availableLayers(layerCount);
        result = vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
        if (result != VK_SUCCESS) {
            LOG_ERROR_CAT("Vulkan", "vkEnumerateInstanceLayerProperties failed: {}", std::source_location::current(), static_cast<int>(result));
            throw std::runtime_error("vkEnumerateInstanceLayerProperties failed: " + std::to_string(static_cast<int>(result)));
        }
        if (std::any_of(availableLayers.begin(), availableLayers.end(), 
                        [](const auto& layer) { return std::strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0; })) {
            layers.push_back("VK_LAYER_KHRONOS_validation");
            LOG_INFO_CAT("Vulkan", "Validation layer VK_LAYER_KHRONOS_validation enabled", std::source_location::current());
        } else {
            LOG_WARNING_CAT("Vulkan", "Validation layer VK_LAYER_KHRONOS_validation not available", std::source_location::current());
        }
    }
#endif

    if (hasSurfaceExtension && std::find(extensions.begin(), extensions.end(), VK_KHR_SURFACE_EXTENSION_NAME) == extensions.end()) {
        LOG_DEBUG_CAT("Vulkan", "Adding VK_KHR_SURFACE_EXTENSION_NAME to extensions", std::source_location::current());
        extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    }

    LOG_INFO_CAT("Vulkan", "Creating Vulkan instance with {} extensions and {} layers", 
                 std::source_location::current(), extensions.size(), layers.size());
    VkApplicationInfo appInfo {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = title.data(),
        .applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
        .pEngineName = "AMOURANTH RTX",
        .engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3
    };
    VkInstanceCreateInfo ci {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(layers.size()),
        .ppEnabledLayerNames = layers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data()
    };
    VkInstance vkInstance;
    result = vkCreateInstance(&ci, nullptr, &vkInstance);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "vkCreateInstance failed with VkResult: {}", std::source_location::current(), static_cast<int>(result));
        throw std::runtime_error("vkCreateInstance failed: " + std::to_string(static_cast<int>(result)));
    }
    instance = VulkanInstancePtr(vkInstance);
    LOG_INFO_CAT("Vulkan", "Vulkan instance created", std::source_location::current());

    VkSurfaceKHR vkSurface = VK_NULL_HANDLE;
    LOG_DEBUG_CAT("Vulkan", "Creating Vulkan surface", std::source_location::current());
    if (!SDL_Vulkan_CreateSurface(window, vkInstance, nullptr, &vkSurface)) {
        LOG_ERROR_CAT("Vulkan", "SDL_Vulkan_CreateSurface failed: {}", std::source_location::current(), SDL_GetError());
        throw std::runtime_error(std::string("SDL_Vulkan_CreateSurface failed: ") + SDL_GetError());
    }
    surface = VulkanSurfacePtr(vkSurface, VulkanSurfaceDeleter{vkInstance});
    LOG_INFO_CAT("Vulkan", "Vulkan surface created successfully", std::source_location::current());

    LOG_DEBUG_CAT("Vulkan", "Selecting physical device", std::source_location::current());
    // Note: createPhysicalDevice is not implemented here; assume it exists elsewhere or is a placeholder
}

VkInstance getVkInstance(const VulkanInstancePtr& instance) {
    LOG_DEBUG_CAT("Vulkan", "Retrieving Vulkan instance", std::source_location::current());
    return instance.get();
}

VkSurfaceKHR getVkSurface(const VulkanSurfacePtr& surface) {
    LOG_DEBUG_CAT("Vulkan", "Retrieving Vulkan surface", std::source_location::current());
    return surface.get();
}

std::vector<std::string> getVulkanExtensions() {
    LOG_DEBUG_CAT("Vulkan", "Querying Vulkan instance extensions", std::source_location::current());
    uint32_t count;
    VkResult result = vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "vkEnumerateInstanceExtensionProperties failed: {}", std::source_location::current(), static_cast<int>(result));
        throw std::runtime_error("vkEnumerateInstanceExtensionProperties failed: " + std::to_string(static_cast<int>(result)));
    }
    std::vector<VkExtensionProperties> props(count);
    result = vkEnumerateInstanceExtensionProperties(nullptr, &count, props.data());
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "vkEnumerateInstanceExtensionProperties failed: {}", std::source_location::current(), static_cast<int>(result));
        throw std::runtime_error("vkEnumerateInstanceExtensionProperties failed: " + std::to_string(static_cast<int>(result)));
    }
    std::vector<std::string> extensions;
    for (const auto& prop : props) {
        extensions.emplace_back(prop.extensionName);
        LOG_DEBUG_CAT("Vulkan", "Found Vulkan instance extension: {}", std::source_location::current(), prop.extensionName);
    }
    LOG_INFO_CAT("Vulkan", "Found {} Vulkan instance extensions", std::source_location::current(), extensions.size());
    return extensions;
}

} // namespace SDL3Initializer