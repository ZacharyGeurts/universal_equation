// AMOURANTH RTX Engine, October 2025 - Manages Vulkan instance and surface creation with platform-specific extensions.
// Dependencies: SDL3, Vulkan 1.3+, C++20 standard library.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#include "engine/SDL3/SDL3_vulkan.hpp"
#include "engine/logging.hpp"
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#ifdef _WIN32
#include <vulkan/vulkan_win32.h> // For VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#endif
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <format>

namespace SDL3Initializer {

void VulkanInstanceDeleter::operator()(VkInstance instance) const {
    Logging::Logger logger;
    if (instance) {
        logger.log(Logging::LogLevel::Info, "Destroying Vulkan instance", std::source_location::current());
        vkDestroyInstance(instance, nullptr);
    }
}

void VulkanSurfaceDeleter::operator()(VkSurfaceKHR surface) const {
    Logging::Logger logger;
    if (surface && m_instance) {
        logger.log(Logging::LogLevel::Info, "Destroying Vulkan surface", std::source_location::current());
        vkDestroySurfaceKHR(m_instance, surface, nullptr);
    } else if (surface) {
        logger.log(Logging::LogLevel::Warning, "Cannot destroy VkSurfaceKHR because VkInstance is invalid", std::source_location::current());
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
    Logging::Logger logger;
    logger.log(Logging::LogLevel::Info, "Initializing Vulkan with title: {}", std::source_location::current(), title);

    // Get Vulkan instance extensions from SDL
    logger.log(Logging::LogLevel::Debug, "Getting Vulkan instance extensions from SDL", std::source_location::current());
    Uint32 extCount;
    auto exts = SDL_Vulkan_GetInstanceExtensions(&extCount);
    if (!exts || !extCount) {
        std::string error = std::format("SDL_Vulkan_GetInstanceExtensions failed: {}", SDL_GetError());
        logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }
    std::vector<const char*> extensions(exts, exts + extCount);
    logger.log(Logging::LogLevel::Info, "Required SDL Vulkan extensions: {}", std::source_location::current(), extCount);
    for (uint32_t i = 0; i < extCount; ++i) {
        logger.log(Logging::LogLevel::Debug, "Extension {}: {}", std::source_location::current(), i, extensions[i]);
    }

    // Check available Vulkan instance extensions
    logger.log(Logging::LogLevel::Debug, "Checking Vulkan instance extensions", std::source_location::current());
    uint32_t instanceExtCount;
    vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtCount, nullptr);
    std::vector<VkExtensionProperties> instanceExtensions(instanceExtCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtCount, instanceExtensions.data());

    bool hasSurfaceExtension = false;
    std::vector<std::string> platformSurfaceExtensions;
    std::string_view platform = SDL_GetPlatform();
    logger.log(Logging::LogLevel::Info, "Detected platform: {}", std::source_location::current(), platform);

    for (const auto& ext : instanceExtensions) {
        logger.log(Logging::LogLevel::Debug, "Available instance extension: {}", std::source_location::current(), ext.extensionName);
        if (std::strcmp(ext.extensionName, VK_KHR_SURFACE_EXTENSION_NAME) == 0) {
            hasSurfaceExtension = true;
            logger.log(Logging::LogLevel::Info, "VK_KHR_surface extension found", std::source_location::current());
        }
        if (platform == "Linux") {
            if (std::strcmp(ext.extensionName, VK_KHR_XLIB_SURFACE_EXTENSION_NAME) == 0 ||
                std::strcmp(ext.extensionName, VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME) == 0) {
                platformSurfaceExtensions.emplace_back(ext.extensionName);
            }
        }
#ifdef _WIN32
        else if (platform == "Windows" && std::strcmp(ext.extensionName, VK_KHR_WIN32_SURFACE_EXTENSION_NAME) == 0) {
            platformSurfaceExtensions.emplace_back(ext.extensionName);
        }
#endif
    }

    for (const auto& extName : platformSurfaceExtensions) {
        if (std::find(extensions.begin(), extensions.end(), extName.c_str()) == extensions.end()) {
            logger.log(Logging::LogLevel::Debug, "Adding platform surface extension: {}", std::source_location::current(), extName);
            extensions.push_back(extName.c_str());
        }
    }

    if (rt) {
        logger.log(Logging::LogLevel::Debug, "Enumerating Vulkan instance extension properties for ray tracing", std::source_location::current());
        const char* rtExts[] = {
            VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
            VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
            VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
        };
        for (auto ext : rtExts) {
            if (std::any_of(instanceExtensions.begin(), instanceExtensions.end(), 
                            [ext](const auto& p) { return std::strcmp(p.extensionName, ext) == 0; })) {
                logger.log(Logging::LogLevel::Debug, "Adding Vulkan extension: {}", std::source_location::current(), ext);
                extensions.push_back(ext);
            }
        }
    }

    std::vector<const char*> layers;
#ifndef NDEBUG
    if (enableValidation) {
        logger.log(Logging::LogLevel::Info, "Adding Vulkan validation layer", std::source_location::current());
        layers.push_back("VK_LAYER_KHRONOS_validation");
    }
#endif

    if (hasSurfaceExtension && std::find(extensions.begin(), extensions.end(), VK_KHR_SURFACE_EXTENSION_NAME) == extensions.end()) {
        logger.log(Logging::LogLevel::Debug, "Adding VK_KHR_SURFACE_EXTENSION_NAME to extensions", std::source_location::current());
        extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    }

    logger.log(Logging::LogLevel::Info, "Creating Vulkan instance with {} extensions and {} layers", 
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
    VkResult result = vkCreateInstance(&ci, nullptr, &vkInstance);
    if (result != VK_SUCCESS) {
        std::string error = std::format("vkCreateInstance failed: {}", result);
        logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }
    instance = VulkanInstancePtr(vkInstance);
    logger.log(Logging::LogLevel::Info, "Vulkan instance created", std::source_location::current());

    VkSurfaceKHR vkSurface = VK_NULL_HANDLE;
    logger.log(Logging::LogLevel::Debug, "Creating Vulkan surface", std::source_location::current());
    if (!SDL_Vulkan_CreateSurface(window, vkInstance, nullptr, &vkSurface)) {
        std::string error = std::format("SDL_Vulkan_CreateSurface failed: {}", SDL_GetError());
        logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }
    surface = VulkanSurfacePtr(vkSurface, VulkanSurfaceDeleter{vkInstance});
    logger.log(Logging::LogLevel::Info, "Vulkan surface created successfully", std::source_location::current());

    logger.log(Logging::LogLevel::Debug, "Selecting physical device", std::source_location::current());
    // Note: createPhysicalDevice is not implemented here; assume it exists elsewhere or is a placeholder
}

VkInstance getVkInstance(const VulkanInstancePtr& instance) {
    return instance.get();
}

VkSurfaceKHR getVkSurface(const VulkanSurfacePtr& surface) {
    return surface.get();
}

std::vector<std::string> getVulkanExtensions() {
    Logging::Logger logger;
    logger.log(Logging::LogLevel::Debug, "Querying Vulkan instance extensions", std::source_location::current());
    uint32_t count;
    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> props(count);
    vkEnumerateInstanceExtensionProperties(nullptr, &count, props.data());
    std::vector<std::string> extensions;
    for (const auto& prop : props) {
        extensions.emplace_back(prop.extensionName);
    }
    logger.log(Logging::LogLevel::Info, "Found {} Vulkan instance extensions", std::source_location::current(), extensions.size());
    return extensions;
}

} // namespace SDL3Initializer