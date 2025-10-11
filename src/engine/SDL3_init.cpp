// SDL3_init.cpp
// AMOURANTH RTX Engine, October 2025 - SDL3 and Vulkan initialization implementation.
// Initializes SDL3 window and Vulkan instance/surface for rendering.
// Dependencies: SDL3, Vulkan 1.3, C++20 standard library, logging.hpp, VulkanCore.hpp.
// Supported platforms: Windows, Linux.
// Zachary Geurts 2025

#include "engine/SDL3_init.hpp"
#include "VulkanCore.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include "engine/logging.hpp"
#include <stdexcept>
#include <source_location>
#include <vector>
#include <string>

namespace SDL3Initializer {

SDL3Initializer::SDL3Initializer(const std::string& title, int width, int height) {
    LOG_INFO("Initializing SDL3 with title={}, width={}, height={}", 
             std::source_location::current(), title, width, height);

    // Set SDL hints for Linux (Wayland or X11)
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland,x11");
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_VERBOSE);

    // Initialize SDL with video, events, and audio
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO) == 0) {
        LOG_ERROR("SDL_Init failed: {}", std::source_location::current(), SDL_GetError());
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
    }
    LOG_DEBUG("SDL initialized successfully", std::source_location::current());

    // Create SDL window with Vulkan and resizable flags
    window_ = SDL_CreateWindow(title.c_str(), width, height, 
                               SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!window_) {
        LOG_ERROR("SDL_CreateWindow failed: {}", std::source_location::current(), SDL_GetError());
        SDL_Quit();
        throw std::runtime_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
    }
    LOG_DEBUG("SDL window created successfully: window={:p}", 
              std::source_location::current(), static_cast<void*>(window_));

    // Retrieve Vulkan instance extensions
    Uint32 extensionCount = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(&extensionCount)) {
        LOG_ERROR("SDL_Vulkan_GetInstanceExtensions failed to get count: {}", 
                  std::source_location::current(), SDL_GetError());
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error(std::string("SDL_Vulkan_GetInstanceExtensions failed: ") + SDL_GetError());
    }
    std::vector<const char*> extensions(extensionCount);
    if (SDL_Vulkan_GetInstanceExtensions(&extensionCount) == 0) {
        LOG_ERROR("SDL_Vulkan_GetInstanceExtensions failed: {}", 
                  std::source_location::current(), SDL_GetError());
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error(std::string("SDL_Vulkan_GetInstanceExtensions failed: ") + SDL_GetError());
    }
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    LOG_DEBUG("Vulkan instance extensions retrieved: count={}", 
              std::source_location::current(), extensionCount + 1);
    for (const auto* ext : extensions) {
        LOG_DEBUG("Extension: {}", std::source_location::current(), ext);
    }

    // Check for validation layer support
    const char* validationLayers[] = {"VK_LAYER_KHRONOS_validation"};
    uint32_t layerCount = 1;
    uint32_t availableLayerCount = 0;
    vkEnumerateInstanceLayerProperties(&availableLayerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(availableLayerCount);
    vkEnumerateInstanceLayerProperties(&availableLayerCount, availableLayers.data());
    bool validationLayerSupported = false;
    for (const auto& layer : availableLayers) {
        if (strcmp(layer.layerName, validationLayers[0]) == 0) {
            validationLayerSupported = true;
            break;
        }
    }
    if (!validationLayerSupported) {
        LOG_WARNING("Validation layer VK_LAYER_KHRONOS_validation not supported", 
                    std::source_location::current());
        layerCount = 0;
    }

    // Create Vulkan instance
    VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = title.c_str(),
        .applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
        .pEngineName = "AMOURANTH RTX Engine",
        .engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3
    };

    VkInstanceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = layerCount,
        .ppEnabledLayerNames = validationLayers,
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data()
    };

    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance_);
    if (result != VK_SUCCESS) {
        LOG_ERROR("vkCreateInstance failed with result={}", 
                  std::source_location::current(), static_cast<int>(result));
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error("vkCreateInstance failed: " + std::to_string(static_cast<int>(result)));
    }
    LOG_DEBUG("Vulkan instance created successfully: instance={:p}", 
              std::source_location::current(), static_cast<void*>(instance_));

    // Create Vulkan surface
    if (!SDL_Vulkan_CreateSurface(window_, instance_, nullptr, &surface_)) {
        LOG_ERROR("SDL_Vulkan_CreateSurface failed: {}", 
                  std::source_location::current(), SDL_GetError());
        vkDestroyInstance(instance_, nullptr);
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error(std::string("SDL_Vulkan_CreateSurface failed: ") + SDL_GetError());
    }
    LOG_DEBUG("Vulkan surface created successfully: surface={:p}", 
              std::source_location::current(), static_cast<void*>(surface_));

    // Verify surface support
    VkPhysicalDevice physicalDevice = ::VulkanInitializer::findPhysicalDevice(instance_);
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    bool surfaceSupported = false;
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface_, &presentSupport);
        if (presentSupport) {
            surfaceSupported = true;
            break;
        }
    }
    if (!surfaceSupported) {
        LOG_ERROR("Vulkan surface not supported by any queue family", 
                  std::source_location::current());
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        vkDestroyInstance(instance_, nullptr);
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error("Vulkan surface not supported by any queue family");
    }

    LOG_INFO("SDL3Initializer initialized successfully", std::source_location::current());
}

SDL3Initializer::~SDL3Initializer() {
    LOG_INFO("Destroying SDL3Initializer", std::source_location::current());
    if (surface_ != VK_NULL_HANDLE) {
        LOG_DEBUG("Destroying Vulkan surface: surface={:p}", 
                  std::source_location::current(), static_cast<void*>(surface_));
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }
    if (instance_ != VK_NULL_HANDLE) {
        LOG_DEBUG("Destroying Vulkan instance: instance={:p}", 
                  std::source_location::current(), static_cast<void*>(instance_));
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
    if (window_) {
        LOG_DEBUG("Destroying SDL window: window={:p}", 
                  std::source_location::current(), static_cast<void*>(window_));
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    LOG_DEBUG("SDL quitting", std::source_location::current());
    SDL_Quit();
}

bool SDL3Initializer::shouldQuit() const {
    bool minimized = (SDL_GetWindowFlags(window_) & SDL_WINDOW_MINIMIZED) != 0;
    bool hasQuitEvent = false;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            hasQuitEvent = true;
            break;
        }
    }
    LOG_DEBUG("shouldQuit checked, minimized={}, hasQuitEvent={}", 
              std::source_location::current(), minimized, hasQuitEvent);
    return minimized || hasQuitEvent;
}

void SDL3Initializer::pollEvents() {
    LOG_DEBUG("Polling SDL events", std::source_location::current());
    SDL_PumpEvents();
}

VkInstance SDL3Initializer::getInstance() const {
    LOG_DEBUG("Retrieving Vulkan instance: instance={:p}", 
              std::source_location::current(), static_cast<void*>(instance_));
    return instance_;
}

void SDL3Initializer::setInstance(VkInstance instance) {
    LOG_DEBUG("Setting Vulkan instance: instance={:p}", 
              std::source_location::current(), static_cast<void*>(instance));
    instance_ = instance;
}

VkSurfaceKHR SDL3Initializer::getSurface() const {
    LOG_DEBUG("Retrieving Vulkan surface: surface={:p}", 
              std::source_location::current(), static_cast<void*>(surface_));
    return surface_;
}

void SDL3Initializer::setSurface(VkSurfaceKHR surface) {
    LOG_DEBUG("Setting Vulkan surface: surface={:p}", 
              std::source_location::current(), static_cast<void*>(surface));
    surface_ = surface;
}

SDL_Window* SDL3Initializer::getWindow() const {
    LOG_DEBUG("Retrieving SDL window: window={:p}", 
              std::source_location::current(), static_cast<void*>(window_));
    return window_;
}

void SDL3Initializer::setWindow(SDL_Window* window) {
    LOG_DEBUG("Setting SDL window: window={:p}", 
              std::source_location::current(), static_cast<void*>(window));
    window_ = window;
}

} // namespace SDL3Initializer