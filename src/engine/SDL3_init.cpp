// AMOURANTH RTX Engine, October 2025 - SDL3 and Vulkan initialization implementation.
// Initializes SDL3 window and Vulkan instance/surface for rendering.
// Dependencies: SDL3, Vulkan 1.3, C++20 standard library.
// Supported platforms: Windows, Linux.
// Zachary Geurts 2025

#include "engine/SDL3_init.hpp"
#include "engine/logging.hpp"
#include <stdexcept>
#include <sstream>
#include <source_location>

SDL3Initializer::SDL3Initializer(const std::string& title, int width, int height, const Logging::Logger& logger)
    : window_(nullptr), instance_(VK_NULL_HANDLE), surface_(VK_NULL_HANDLE), logger_(logger) {
    std::stringstream ss;
    ss << "Initializing SDL3 with title=" << title << ", width=" << width << ", height=" << height;
    logger_.log(Logging::LogLevel::Info, ss.str(), std::source_location::current());

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO) == 0) {
        std::stringstream error_ss;
        error_ss << "SDL_Init failed: " << SDL_GetError();
        logger_.log(Logging::LogLevel::Error, error_ss.str(), std::source_location::current());
        throw std::runtime_error(error_ss.str());
    }
    logger_.log(Logging::LogLevel::Debug, "SDL initialized successfully", std::source_location::current());

    window_ = SDL_CreateWindow(title.c_str(), width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!window_) {
        std::stringstream error_ss;
        error_ss << "SDL_CreateWindow failed: " << SDL_GetError();
        logger_.log(Logging::LogLevel::Error, error_ss.str(), std::source_location::current());
        SDL_Quit();
        throw std::runtime_error(error_ss.str());
    }
    logger_.log(Logging::LogLevel::Debug, "SDL window created successfully", std::source_location::current());

    Uint32 extensionCount = 0;
    const char* const* extensionsRaw = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    if (!extensionsRaw) {
        std::stringstream error_ss;
        error_ss << "SDL_Vulkan_GetInstanceExtensions failed: " << SDL_GetError();
        logger_.log(Logging::LogLevel::Error, error_ss.str(), std::source_location::current());
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error(error_ss.str());
    }
    std::vector<const char*> extensions(extensionsRaw, extensionsRaw + extensionCount);
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    std::stringstream ext_ss;
    ext_ss << "Vulkan instance extensions retrieved: count=" << extensionCount;
    logger_.log(Logging::LogLevel::Debug, ext_ss.str(), std::source_location::current());
    for (const auto* ext : extensions) {
        std::stringstream ext_log_ss;
        ext_log_ss << "Extension: " << ext;
        logger_.log(Logging::LogLevel::Debug, ext_log_ss.str(), std::source_location::current());
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
        std::stringstream error_ss;
        error_ss << "vkCreateInstance failed with result=" << result;
        logger_.log(Logging::LogLevel::Error, error_ss.str(), std::source_location::current());
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error(error_ss.str());
    }
    std::stringstream instance_ss;
    instance_ss << "Vulkan instance created successfully: instance=" << std::hex << std::showbase << reinterpret_cast<uintptr_t>(instance_);
    logger_.log(Logging::LogLevel::Debug, instance_ss.str(), std::source_location::current());

    if (!SDL_Vulkan_CreateSurface(window_, instance_, nullptr, &surface_)) {
        std::stringstream error_ss;
        error_ss << "SDL_Vulkan_CreateSurface failed: " << SDL_GetError();
        logger_.log(Logging::LogLevel::Error, error_ss.str(), std::source_location::current());
        vkDestroyInstance(instance_, nullptr);
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error(error_ss.str());
    }
    std::stringstream surface_ss;
    surface_ss << "Vulkan surface created successfully: surface=" << std::hex << std::showbase << reinterpret_cast<uintptr_t>(surface_);
    logger_.log(Logging::LogLevel::Debug, surface_ss.str(), std::source_location::current());

    logger_.log(Logging::LogLevel::Info, "SDL3Initializer initialized successfully", std::source_location::current());
}

SDL3Initializer::~SDL3Initializer() {
    logger_.log(Logging::LogLevel::Info, "Destroying SDL3Initializer", std::source_location::current());
    if (surface_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
        logger_.log(Logging::LogLevel::Debug, "Vulkan surface destroyed", std::source_location::current());
    }
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
        logger_.log(Logging::LogLevel::Debug, "Vulkan instance destroyed", std::source_location::current());
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        logger_.log(Logging::LogLevel::Debug, "SDL window destroyed", std::source_location::current());
    }
    SDL_Quit();
    logger_.log(Logging::LogLevel::Debug, "SDL quit", std::source_location::current());
}

bool SDL3Initializer::shouldQuit() const {
    bool minimized = SDL_GetWindowFlags(window_) & SDL_WINDOW_MINIMIZED;
    std::stringstream ss;
    ss << "shouldQuit checked, minimized=" << (minimized ? "true" : "false");
    logger_.log(Logging::LogLevel::Debug, ss.str(), std::source_location::current());
    return minimized;
}

void SDL3Initializer::pollEvents() {
    logger_.log(Logging::LogLevel::Debug, "Polling SDL events", std::source_location::current());
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        std::stringstream ss;
        ss << "Processing SDL event: type=" << event.type;
        logger_.log(Logging::LogLevel::Debug, ss.str(), std::source_location::current());
        switch (event.type) {
            case SDL_EVENT_QUIT:
                logger_.log(Logging::LogLevel::Info, "Quit event received", std::source_location::current());
                break;
            case SDL_EVENT_WINDOW_RESIZED: {
                std::stringstream resize_ss;
                resize_ss << "Window resized event: width=" << event.window.data1 << ", height=" << event.window.data2;
                logger_.log(Logging::LogLevel::Info, resize_ss.str(), std::source_location::current());
                break;
            }
            default:
                std::stringstream default_ss;
                default_ss << "Unhandled SDL event: type=" << event.type;
                logger_.log(Logging::LogLevel::Debug, default_ss.str(), std::source_location::current());
                break;
        }
    }
    logger_.log(Logging::LogLevel::Debug, "SDL event polling completed", std::source_location::current());
}