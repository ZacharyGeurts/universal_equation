// AMOURANTH RTX Engine, October 2025 - Manages Vulkan instance and surface creation with platform-specific extensions.
// Dependencies: SDL3, Vulkan 1.3+, C++20 standard library.
// Zachary Geurts 2025

#include "engine/SDL3/SDL3_vulkan.hpp"
#include <stdexcept>
#include <algorithm>
#include <cstring>

namespace std {
// Custom formatter for VkResult to enable std::format support
template <>
struct formatter<VkResult, char> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(VkResult value, FormatContext& ctx) const {
        std::string_view name;
        switch (value) {
            case VK_SUCCESS: name = "VK_SUCCESS"; break;
            case VK_NOT_READY: name = "VK_NOT_READY"; break;
            case VK_TIMEOUT: name = "VK_TIMEOUT"; break;
            case VK_EVENT_SET: name = "VK_EVENT_SET"; break;
            case VK_EVENT_RESET: name = "VK_EVENT_RESET"; break;
            case VK_INCOMPLETE: name = "VK_INCOMPLETE"; break;
            case VK_ERROR_OUT_OF_HOST_MEMORY: name = "VK_ERROR_OUT_OF_HOST_MEMORY"; break;
            case VK_ERROR_OUT_OF_DEVICE_MEMORY: name = "VK_ERROR_OUT_OF_DEVICE_MEMORY"; break;
            case VK_ERROR_INITIALIZATION_FAILED: name = "VK_ERROR_INITIALIZATION_FAILED"; break;
            case VK_ERROR_DEVICE_LOST: name = "VK_ERROR_DEVICE_LOST"; break;
            case VK_ERROR_MEMORY_MAP_FAILED: name = "VK_ERROR_MEMORY_MAP_FAILED"; break;
            case VK_ERROR_LAYER_NOT_PRESENT: name = "VK_ERROR_LAYER_NOT_PRESENT"; break;
            case VK_ERROR_EXTENSION_NOT_PRESENT: name = "VK_ERROR_EXTENSION_NOT_PRESENT"; break;
            case VK_ERROR_FEATURE_NOT_PRESENT: name = "VK_ERROR_FEATURE_NOT_PRESENT"; break;
            case VK_ERROR_INCOMPATIBLE_DRIVER: name = "VK_ERROR_INCOMPATIBLE_DRIVER"; break;
            case VK_ERROR_TOO_MANY_OBJECTS: name = "VK_ERROR_TOO_MANY_OBJECTS"; break;
            case VK_ERROR_FORMAT_NOT_SUPPORTED: name = "VK_ERROR_FORMAT_NOT_SUPPORTED"; break;
            default: name = "UNKNOWN_VK_RESULT"; break;
        }
        return format_to(ctx.out(), "{}", name);
    }
};
} // namespace std

namespace SDL3Initializer {

void initVulkan(
    SDL_Window* window, 
    VulkanInstancePtr& instance,
    VulkanSurfacePtr& surface,
    bool enableValidation, 
    bool preferNvidia, 
    bool rt, 
    std::string_view title,
    const Logging::Logger& logger) 
{
    logger.log(Logging::LogLevel::Info, std::format("Initializing Vulkan with title: {}", title));

    logger.log(Logging::LogLevel::Debug, "Getting Vulkan instance extensions from SDL");
    Uint32 extCount;
    auto exts = SDL_Vulkan_GetInstanceExtensions(&extCount);
    if (!exts || !extCount) {
        auto error = std::format("SDL_Vulkan_GetInstanceExtensions failed: {}", SDL_GetError());
        logger.log(Logging::LogLevel::Error, error);
        throw std::runtime_error(error);
    }
    std::vector<const char*> extensions(exts, exts + extCount);
    logger.log(Logging::LogLevel::Info, std::format("Required SDL Vulkan extensions: {}", extCount));
    for (uint32_t i = 0; i < extCount; ++i) {
        logger.log(Logging::LogLevel::Debug, std::format("Extension {}: {}", i, extensions[i]));
    }

    logger.log(Logging::LogLevel::Debug, "Checking Vulkan instance extensions");
    uint32_t instanceExtCount;
    vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtCount, nullptr);
    std::vector<VkExtensionProperties> instanceExtensions(instanceExtCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtCount, instanceExtensions.data());

    bool hasSurfaceExtension = false;
    std::vector<std::string> platformSurfaceExtensions;
    std::string_view platform = SDL_GetPlatform();
    logger.log(Logging::LogLevel::Info, std::format("Detected platform: {}", platform));

    for (const auto& ext : instanceExtensions) {
        logger.log(Logging::LogLevel::Debug, std::format("Available instance extension: {}", ext.extensionName));
        if (std::strcmp(ext.extensionName, VK_KHR_SURFACE_EXTENSION_NAME) == 0) {
            hasSurfaceExtension = true;
            logger.log(Logging::LogLevel::Info, "VK_KHR_surface extension found");
        }
        if (platform == "Linux" && (std::strcmp(ext.extensionName, VK_KHR_XLIB_SURFACE_EXTENSION_NAME) == 0 ||
                                    std::strcmp(ext.extensionName, VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME) == 0)) {
            platformSurfaceExtensions.emplace_back(ext.extensionName);
        } else if (platform == "Windows" && std::strcmp(ext.extensionName, "VK_KHR_win32_surface") == 0) {
            platformSurfaceExtensions.emplace_back(ext.extensionName);
        } else if (platform == "Android" && std::strcmp(ext.extensionName, "VK_KHR_android_surface") == 0) {
            platformSurfaceExtensions.emplace_back(ext.extensionName);
        } else if (platform == "Mac OS X" && std::strcmp(ext.extensionName, "VK_MVK_macos_surface") == 0) {
            platformSurfaceExtensions.emplace_back(ext.extensionName);
        }
    }

    for (const auto& extName : platformSurfaceExtensions) {
        if (std::find(extensions.begin(), extensions.end(), extName.c_str()) == extensions.end()) {
            logger.log(Logging::LogLevel::Debug, std::format("Adding platform surface extension: {}", extName));
            extensions.push_back(extName.c_str());
        }
    }

    if (rt) {
        logger.log(Logging::LogLevel::Debug, "Enumerating Vulkan instance extension properties for ray tracing");
        const char* rtExts[] = {
            VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
            VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
            VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
        };
        for (auto ext : rtExts) {
            if (std::any_of(instanceExtensions.begin(), instanceExtensions.end(), 
                            [ext](const auto& p) { return std::strcmp(p.extensionName, ext) == 0; })) {
                logger.log(Logging::LogLevel::Debug, std::format("Adding Vulkan extension: {}", ext));
                extensions.push_back(ext);
            }
        }
    }

    std::vector<const char*> layers;
#ifndef NDEBUG
    if (enableValidation) {
        logger.log(Logging::LogLevel::Info, "Adding Vulkan validation layer");
        layers.push_back("VK_LAYER_KHRONOS_validation");
    }
#endif

    if (hasSurfaceExtension && std::find(extensions.begin(), extensions.end(), VK_KHR_SURFACE_EXTENSION_NAME) == extensions.end()) {
        logger.log(Logging::LogLevel::Debug, "Adding VK_KHR_SURFACE_EXTENSION_NAME to extensions");
        extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    }

    logger.log(Logging::LogLevel::Info, std::format("Creating Vulkan instance with {} extensions and {} layers", 
                                          extensions.size(), layers.size()));
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
        auto error = std::format("vkCreateInstance failed: {}", result);
        logger.log(Logging::LogLevel::Error, error);
        throw std::runtime_error(error);
    }
    instance = VulkanInstancePtr(vkInstance);
    logger.log(Logging::LogLevel::Info, "Vulkan instance created");

    VkSurfaceKHR vkSurface = VK_NULL_HANDLE;
    logger.log(Logging::LogLevel::Debug, "Creating Vulkan surface");
    if (!SDL_Vulkan_CreateSurface(window, vkInstance, nullptr, &vkSurface)) {
        auto error = std::format("SDL_Vulkan_CreateSurface failed: {}", SDL_GetError());
        logger.log(Logging::LogLevel::Error, error);
        throw std::runtime_error(error);
    }
    surface = VulkanSurfacePtr(vkSurface, VulkanSurfaceDeleter{vkInstance});
    logger.log(Logging::LogLevel::Info, "Vulkan surface created successfully");

    logger.log(Logging::LogLevel::Debug, "Selecting physical device");
    VkPhysicalDevice physicalDevice;
    uint32_t graphicsFamily, presentFamily;
    VulkanInitializer::createPhysicalDevice(vkInstance, physicalDevice, graphicsFamily, presentFamily, 
                                           vkSurface, preferNvidia, logger);
}

VkInstance getVkInstance(const VulkanInstancePtr& instance) {
    return instance.get();
}

VkSurfaceKHR getVkSurface(const VulkanSurfacePtr& surface) {
    return surface.get();
}

std::vector<std::string> getVulkanExtensions(const Logging::Logger& logger) {
    logger.log(Logging::LogLevel::Debug, "Querying Vulkan instance extensions");
    uint32_t count;
    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> props(count);
    vkEnumerateInstanceExtensionProperties(nullptr, &count, props.data());
    std::vector<std::string> extensions;
    for (const auto& prop : props) {
        extensions.emplace_back(prop.extensionName);
    }
    logger.log(Logging::LogLevel::Info, std::format("Found {} Vulkan instance extensions", extensions.size()));
    return extensions;
}

} // namespace SDL3Initializer