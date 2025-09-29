#include "SDL3_vulkan.hpp"
#include <stdexcept>
// Manages Vulkan instance and surface creation, with platform-specific extensions.
// AMOURANTH RTX Engine September 2025
// Zachary Geurts 2025
namespace SDL3Initializer {

    void initVulkan(SDL_Window* window, std::unique_ptr<std::remove_pointer_t<VkInstance>, VulkanInstanceDeleter>& instance,
                    std::unique_ptr<std::remove_pointer_t<VkSurfaceKHR>, VulkanSurfaceDeleter>& surface,
                    bool enableValidation, bool preferNvidia, bool rt, const char* title,
                    std::function<void(const std::string&)> logMessage) {
        logMessage("Getting Vulkan instance extensions from SDL");
        Uint32 extCount;
        auto exts = SDL_Vulkan_GetInstanceExtensions(&extCount);
        if (!exts || !extCount) {
            std::string error = "SDL_Vulkan_GetInstanceExtensions failed: " + std::string(SDL_GetError());
            logMessage(error);
            throw std::runtime_error(error);
        }
        std::vector<const char*> extensions(exts, exts + extCount);
        logMessage("Required SDL Vulkan extensions: " + std::to_string(extCount));
        for (uint32_t i = 0; i < extCount; ++i) {
            logMessage("Extension " + std::to_string(i) + ": " + extensions[i]);
        }

        logMessage("Checking Vulkan instance extensions");
        uint32_t instanceExtCount;
        vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtCount, nullptr);
        std::vector<VkExtensionProperties> instanceExtensions(instanceExtCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtCount, instanceExtensions.data());
        bool hasSurfaceExtension = false;
        std::vector<std::string> platformSurfaceExtensions;
        std::string platform = SDL_GetPlatform();
        for (const auto& ext : instanceExtensions) {
            logMessage("Available instance extension: " + std::string(ext.extensionName));
            if (strcmp(ext.extensionName, VK_KHR_SURFACE_EXTENSION_NAME) == 0) {
                hasSurfaceExtension = true;
                logMessage("VK_KHR_surface extension found");
            }
            if (platform == "Linux" && (strcmp(ext.extensionName, VK_KHR_XLIB_SURFACE_EXTENSION_NAME) == 0 ||
                                        strcmp(ext.extensionName, VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME) == 0)) {
                platformSurfaceExtensions.push_back(ext.extensionName);
            } else if (platform == "Windows" && strcmp(ext.extensionName, "VK_KHR_win32_surface") == 0) {
                platformSurfaceExtensions.push_back(ext.extensionName);
            } else if (platform == "Android" && strcmp(ext.extensionName, "VK_KHR_android_surface") == 0) {
                platformSurfaceExtensions.push_back(ext.extensionName);
            } else if (platform == "Mac OS X" && strcmp(ext.extensionName, "VK_MVK_macos_surface") == 0) {
                platformSurfaceExtensions.push_back(ext.extensionName);
            }
        }

        for (const std::string& extName : platformSurfaceExtensions) {
            const char* cext = extName.c_str();
            auto it = std::find(extensions.begin(), extensions.end(), cext);
            if (it == extensions.end()) {
                logMessage("Adding platform surface extension: " + extName);
                extensions.push_back(cext);
            }
        }

        if (rt) {
            logMessage("Enumerating Vulkan instance extension properties for ray tracing");
            const char* rtExts[] = {
                VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
                VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
                VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
                VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
            };
            for (auto ext : rtExts) {
                if (std::any_of(instanceExtensions.begin(), instanceExtensions.end(), [ext](auto& p) { return strcmp(p.extensionName, ext) == 0; })) {
                    logMessage("Adding Vulkan extension: " + std::string(ext));
                    extensions.push_back(ext);
                }
            }
        }

        std::vector<const char*> layers;
#ifndef NDEBUG
        if (enableValidation) {
            logMessage("Adding Vulkan validation layer");
            layers.push_back("VK_LAYER_KHRONOS_validation");
        }
#endif

        if (hasSurfaceExtension && std::find(extensions.begin(), extensions.end(), VK_KHR_SURFACE_EXTENSION_NAME) == extensions.end()) {
            logMessage("Adding VK_KHR_SURFACE_EXTENSION_NAME to extensions");
            extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
        }

        logMessage("Creating Vulkan instance with " + std::to_string(extensions.size()) + " extensions and " + std::to_string(layers.size()) + " layers");
        VkApplicationInfo appInfo = {
            VK_STRUCTURE_TYPE_APPLICATION_INFO,
            nullptr,
            title,
            VK_MAKE_API_VERSION(0, 1, 0, 0),
            "AMOURANTH RTX",
            VK_MAKE_API_VERSION(0, 1, 0, 0),
            VK_API_VERSION_1_3
        };
        VkInstanceCreateInfo ci = {
            VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            nullptr,
            0,
            &appInfo,
            static_cast<uint32_t>(layers.size()),
            layers.data(),
            static_cast<uint32_t>(extensions.size()),
            extensions.data()
        };
        VkInstance vkInstance;
        VkResult result = vkCreateInstance(&ci, nullptr, &vkInstance);
        if (result != VK_SUCCESS) {
            std::string error = "vkCreateInstance failed: " + std::to_string(result);
            logMessage(error);
            throw std::runtime_error(error);
        }
        instance = std::unique_ptr<std::remove_pointer_t<VkInstance>, VulkanInstanceDeleter>(vkInstance);
        logMessage("Vulkan instance created");

        VkSurfaceKHR vkSurface = VK_NULL_HANDLE;
        logMessage("Creating Vulkan surface");
        if (!SDL_Vulkan_CreateSurface(window, vkInstance, nullptr, &vkSurface)) {
            std::string error = "SDL_Vulkan_CreateSurface failed: " + std::string(SDL_GetError());
            logMessage(error);
            throw std::runtime_error(error);
        }
        surface = std::unique_ptr<std::remove_pointer_t<VkSurfaceKHR>, VulkanSurfaceDeleter>(vkSurface, VulkanSurfaceDeleter{vkInstance});
        logMessage("Vulkan surface created successfully");

        logMessage("Selecting physical device");
        VkPhysicalDevice physicalDevice;
        uint32_t graphicsFamily, presentFamily;
        VulkanInitializer::createPhysicalDevice(vkInstance, physicalDevice, graphicsFamily, presentFamily, vkSurface, preferNvidia, logMessage);
    }

    VkInstance getVkInstance(const std::unique_ptr<std::remove_pointer_t<VkInstance>, VulkanInstanceDeleter>& instance) {
        return instance.get();
    }

    VkSurfaceKHR getVkSurface(const std::unique_ptr<std::remove_pointer_t<VkSurfaceKHR>, VulkanSurfaceDeleter>& surface) {
        return surface.get();
    }

    std::vector<std::string> getVulkanExtensions(std::function<void(const std::string&)> logMessage) {
        logMessage("Querying Vulkan instance extensions");
        uint32_t count;
        vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
        std::vector<VkExtensionProperties> props(count);
        vkEnumerateInstanceExtensionProperties(nullptr, &count, props.data());
        std::vector<std::string> extensions;
        for (const auto& prop : props) {
            extensions.push_back(prop.extensionName);
        }
        return extensions;
    }

}