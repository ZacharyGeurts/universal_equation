// AMOURANTH RTX Engine, October 2025 - Manages Vulkan instance and surface creation with platform-specific extensions.
// Dependencies: SDL3, Vulkan 1.3+, C++20 standard library.
// Zachary Geurts 2025

#include "engine/SDL3/SDL3_vulkan.hpp"
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <format>
#include <syncstream>

// ANSI color codes for consistent logging
#define RESET "\033[0m"
#define MAGENTA "\033[1;35m" // Bold magenta for errors
#define CYAN "\033[1;36m"    // Bold cyan for debug
#define YELLOW "\033[1;33m"  // Bold yellow for warnings
#define GREEN "\033[1;32m"   // Bold green for info

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

void VulkanInstanceDeleter::operator()(VkInstance instance) const {
    if (instance) {
        std::osyncstream(std::cout) << GREEN << "[INFO] Destroying Vulkan instance" << RESET << std::endl;
        vkDestroyInstance(instance, nullptr);
    }
}

void VulkanSurfaceDeleter::operator()(VkSurfaceKHR surface) const {
    if (surface && m_instance) {
        std::osyncstream(std::cout) << GREEN << "[INFO] Destroying Vulkan surface" << RESET << std::endl;
        vkDestroySurfaceKHR(m_instance, surface, nullptr);
    } else if (surface) {
        std::osyncstream(std::cout) << YELLOW << "[WARNING] Cannot destroy VkSurfaceKHR because VkInstance is invalid" << RESET << std::endl;
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
    std::osyncstream(std::cout) << GREEN << "[INFO] Initializing Vulkan with title: " << title << RESET << std::endl;

    std::osyncstream(std::cout) << CYAN << "[DEBUG] Getting Vulkan instance extensions from SDL" << RESET << std::endl;
    Uint32 extCount;
    auto exts = SDL_Vulkan_GetInstanceExtensions(&extCount);
    if (!exts || !extCount) {
        auto error = std::format("SDL_Vulkan_GetInstanceExtensions failed: {}", SDL_GetError());
        std::osyncstream(std::cout) << MAGENTA << "[ERROR] " << error << RESET << std::endl;
        throw std::runtime_error(error);
    }
    std::vector<const char*> extensions(exts, exts + extCount);
    std::osyncstream(std::cout) << GREEN << "[INFO] Required SDL Vulkan extensions: " << extCount << RESET << std::endl;
    for (uint32_t i = 0; i < extCount; ++i) {
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Extension " << i << ": " << extensions[i] << RESET << std::endl;
    }

    std::osyncstream(std::cout) << CYAN << "[DEBUG] Checking Vulkan instance extensions" << RESET << std::endl;
    uint32_t instanceExtCount;
    vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtCount, nullptr);
    std::vector<VkExtensionProperties> instanceExtensions(instanceExtCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtCount, instanceExtensions.data());

    bool hasSurfaceExtension = false;
    std::vector<std::string> platformSurfaceExtensions;
    std::string_view platform = SDL_GetPlatform();
    std::osyncstream(std::cout) << GREEN << "[INFO] Detected platform: " << platform << RESET << std::endl;

    for (const auto& ext : instanceExtensions) {
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Available instance extension: " << ext.extensionName << RESET << std::endl;
        if (std::strcmp(ext.extensionName, VK_KHR_SURFACE_EXTENSION_NAME) == 0) {
            hasSurfaceExtension = true;
            std::osyncstream(std::cout) << GREEN << "[INFO] VK_KHR_surface extension found" << RESET << std::endl;
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
            std::osyncstream(std::cout) << CYAN << "[DEBUG] Adding platform surface extension: " << extName << RESET << std::endl;
            extensions.push_back(extName.c_str());
        }
    }

    if (rt) {
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Enumerating Vulkan instance extension properties for ray tracing" << RESET << std::endl;
        const char* rtExts[] = {
            VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
            VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
            VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
        };
        for (auto ext : rtExts) {
            if (std::any_of(instanceExtensions.begin(), instanceExtensions.end(), 
                            [ext](const auto& p) { return std::strcmp(p.extensionName, ext) == 0; })) {
                std::osyncstream(std::cout) << CYAN << "[DEBUG] Adding Vulkan extension: " << ext << RESET << std::endl;
                extensions.push_back(ext);
            }
        }
    }

    std::vector<const char*> layers;
#ifndef NDEBUG
    if (enableValidation) {
        std::osyncstream(std::cout) << GREEN << "[INFO] Adding Vulkan validation layer" << RESET << std::endl;
        layers.push_back("VK_LAYER_KHRONOS_validation");
    }
#endif

    if (hasSurfaceExtension && std::find(extensions.begin(), extensions.end(), VK_KHR_SURFACE_EXTENSION_NAME) == extensions.end()) {
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Adding VK_KHR_SURFACE_EXTENSION_NAME to extensions" << RESET << std::endl;
        extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    }

    std::osyncstream(std::cout) << GREEN << "[INFO] Creating Vulkan instance with " << extensions.size() << " extensions and " << layers.size() << " layers" << RESET << std::endl;
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
        std::osyncstream(std::cout) << MAGENTA << "[ERROR] " << error << RESET << std::endl;
        throw std::runtime_error(error);
    }
    instance = VulkanInstancePtr(vkInstance);
    std::osyncstream(std::cout) << GREEN << "[INFO] Vulkan instance created" << RESET << std::endl;

    VkSurfaceKHR vkSurface = VK_NULL_HANDLE;
    std::osyncstream(std::cout) << CYAN << "[DEBUG] Creating Vulkan surface" << RESET << std::endl;
    if (!SDL_Vulkan_CreateSurface(window, vkInstance, nullptr, &vkSurface)) {
        auto error = std::format("SDL_Vulkan_CreateSurface failed: {}", SDL_GetError());
        std::osyncstream(std::cout) << MAGENTA << "[ERROR] " << error << RESET << std::endl;
        throw std::runtime_error(error);
    }
    surface = VulkanSurfacePtr(vkSurface, VulkanSurfaceDeleter{vkInstance});
    std::osyncstream(std::cout) << GREEN << "[INFO] Vulkan surface created successfully" << RESET << std::endl;

    std::osyncstream(std::cout) << CYAN << "[DEBUG] Selecting physical device" << RESET << std::endl;
    // Note: createPhysicalDevice is not implemented here; assume it exists elsewhere or is a placeholder
}

VkInstance getVkInstance(const VulkanInstancePtr& instance) {
    return instance.get();
}

VkSurfaceKHR getVkSurface(const VulkanSurfacePtr& surface) {
    return surface.get();
}

std::vector<std::string> getVulkanExtensions() {
    std::osyncstream(std::cout) << CYAN << "[DEBUG] Querying Vulkan instance extensions" << RESET << std::endl;
    uint32_t count;
    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> props(count);
    vkEnumerateInstanceExtensionProperties(nullptr, &count, props.data());
    std::vector<std::string> extensions;
    for (const auto& prop : props) {
        extensions.emplace_back(prop.extensionName);
    }
    std::osyncstream(std::cout) << GREEN << "[INFO] Found " << extensions.size() << " Vulkan instance extensions" << RESET << std::endl;
    return extensions;
}

} // namespace SDL3Initializer