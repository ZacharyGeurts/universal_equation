// AMOURANTH RTX Engine, October 2025 - SDL3 and Vulkan initialization implementation.
// Initializes SDL3 window and Vulkan instance/surface for rendering.
// Dependencies: SDL3, Vulkan 1.3, C++20 standard library, logging.hpp.
// Supported platforms: Windows, Linux.
// Zachary Geurts 2025

#include "engine/SDL3_init.hpp"
#include "engine/logging.hpp"
#include <stdexcept>
#include <source_location>

namespace SDL3Initializer {

SDL3Initializer::SDL3Initializer(const std::string& title, int width, int height) {
    LOG_INFO_CAT("General", "Initializing SDL3 with title={}, width={}, height={}", 
                 std::source_location::current(), title, width, height);

    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_VERBOSE);
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO) == 0) {
        LOG_ERROR_CAT("General", "SDL_Init failed: {}", std::source_location::current(), SDL_GetError());
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
    }
    LOG_DEBUG_CAT("General", "SDL initialized successfully", std::source_location::current());

    window_ = SDL_CreateWindow(title.c_str(), width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!window_) {
        LOG_ERROR_CAT("General", "SDL_CreateWindow failed: {}", std::source_location::current(), SDL_GetError());
        SDL_Quit();
        throw std::runtime_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
    }
    LOG_DEBUG_CAT("General", "SDL window created successfully", std::source_location::current());

    Uint32 extensionCount = 0;
    const char* const* extensionsRaw = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    if (!extensionsRaw) {
        LOG_ERROR_CAT("General", "SDL_Vulkan_GetInstanceExtensions failed: {}", std::source_location::current(), SDL_GetError());
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error(std::string("SDL_Vulkan_GetInstanceExtensions failed: ") + SDL_GetError());
    }
    std::vector<const char*> extensions(extensionsRaw, extensionsRaw + extensionCount);
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    LOG_DEBUG_CAT("General", "Vulkan instance extensions retrieved: count={}", std::source_location::current(), extensionCount);
    for (const auto* ext : extensions) {
        LOG_DEBUG_CAT("General", "Extension: {}", std::source_location::current(), ext);
    }

    const char* validationLayers[] = {"VK_LAYER_KHRONOS_validation"};
    uint32_t layerCount = 1;

    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = title.c_str(),
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

    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance_);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("General", "vkCreateInstance failed with result={}", std::source_location::current(), static_cast<int>(result));
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error("vkCreateInstance failed: " + std::to_string(static_cast<int>(result)));
    }
    LOG_DEBUG_CAT("General", "Vulkan instance created successfully: instance={:p}", 
                  std::source_location::current(), static_cast<void*>(instance_));

    if (!SDL_Vulkan_CreateSurface(window_, instance_, nullptr, &surface_)) {
        LOG_ERROR_CAT("General", "SDL_Vulkan_CreateSurface failed: {}", std::source_location::current(), SDL_GetError());
        vkDestroyInstance(instance_, nullptr);
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error(std::string("SDL_Vulkan_CreateSurface failed: ") + SDL_GetError());
    }
    LOG_DEBUG_CAT("General", "Vulkan surface created successfully: surface={:p}", 
                  std::source_location::current(), static_cast<void*>(surface_));

    LOG_INFO_CAT("General", "SDL3Initializer initialized successfully", std::source_location::current());
}

SDL3Initializer::~SDL3Initializer() {
    LOG_INFO_CAT("General", "Destroying SDL3Initializer", std::source_location::current());
    if (surface_ != VK_NULL_HANDLE) {
        LOG_DEBUG_CAT("General", "Destroying Vulkan surface: surface={:p}", 
                      std::source_location::current(), static_cast<void*>(surface_));
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }
    if (instance_ != VK_NULL_HANDLE) {
        LOG_DEBUG_CAT("General", "Destroying Vulkan instance: instance={:p}", 
                      std::source_location::current(), static_cast<void*>(instance_));
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
    if (window_) {
        LOG_DEBUG_CAT("General", "Destroying SDL window", std::source_location::current());
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    LOG_DEBUG_CAT("General", "SDL quitting", std::source_location::current());
    SDL_Quit();
}

bool SDL3Initializer::shouldQuit() const {
    bool minimized = SDL_GetWindowFlags(window_) & SDL_WINDOW_MINIMIZED;
    LOG_DEBUG_CAT("General", "shouldQuit checked, minimized={}", std::source_location::current(), minimized);
    return minimized;
}

void SDL3Initializer::pollEvents() {
    LOG_DEBUG_CAT("General", "Polling SDL events", std::source_location::current());
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        LOG_DEBUG_CAT("General", "Processing SDL event: type={}", std::source_location::current(), event.type);
        switch (event.type) {
            case SDL_EVENT_QUIT:
                LOG_INFO_CAT("General", "Quit event received", std::source_location::current());
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                LOG_INFO_CAT("General", "Window resized event: width={}, height={}", 
                             std::source_location::current(), event.window.data1, event.window.data2);
                break;
            default:
                LOG_DEBUG_CAT("General", "Unhandled SDL event: type={}", std::source_location::current(), event.type);
                break;
        }
    }
    LOG_DEBUG_CAT("General", "SDL event polling completed", std::source_location::current());
}

VkInstance SDL3Initializer::getInstance() const {
    LOG_DEBUG_CAT("General", "Retrieving Vulkan instance: instance={:p}", 
                  std::source_location::current(), static_cast<void*>(instance_));
    return instance_;
}

void SDL3Initializer::setInstance(VkInstance instance) {
    LOG_DEBUG_CAT("General", "Setting Vulkan instance: instance={:p}", 
                  std::source_location::current(), static_cast<void*>(instance));
    instance_ = instance;
}

VkSurfaceKHR SDL3Initializer::getSurface() const {
    LOG_DEBUG_CAT("General", "Retrieving Vulkan surface: surface={:p}", 
                  std::source_location::current(), static_cast<void*>(surface_));
    return surface_;
}

void SDL3Initializer::setSurface(VkSurfaceKHR surface) {
    LOG_DEBUG_CAT("General", "Setting Vulkan surface: surface={:p}", 
                  std::source_location::current(), static_cast<void*>(surface));
    surface_ = surface;
}

SDL_Window* SDL3Initializer::getWindow() const {
    LOG_DEBUG_CAT("General", "Retrieving SDL window: window={:p}", 
                  std::source_location::current(), static_cast<void*>(window_));
    return window_;
}

void SDL3Initializer::setWindow(SDL_Window* window) {
    LOG_DEBUG_CAT("General", "Setting SDL window: window={:p}", 
                  std::source_location::current(), static_cast<void*>(window));
    window_ = window;
}

} // namespace SDL3Initializer