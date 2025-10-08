// AMOURANTH RTX Engine, October 2025 - SDL3 and Vulkan initialization implementation.
// Initializes SDL3 window and Vulkan instance/surface for rendering.
// Dependencies: SDL3, Vulkan 1.3, C++20 standard library.
// Zachary Geurts 2025

#include "engine/SDL3_init.hpp"
#include "engine/logging.hpp"
#include <stdexcept>
#include <format>

SDL3Initializer::SDL3Initializer(const std::string& title, int width, int height, const Logging::Logger& logger)
    : window_(nullptr), instance_(VK_NULL_HANDLE), surface_(VK_NULL_HANDLE), logger_(logger) {
    logger_.log(Logging::LogLevel::Info, "Initializing SDL3 with title={}, width={}, height={}",
                std::source_location::current(), title, width, height);

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        logger_.log(Logging::LogLevel::Error, "SDL_Init failed: {}", std::source_location::current(), SDL_GetError());
        throw std::runtime_error("SDL_Init failed: " + std::string(SDL_GetError()));
    }

    window_ = SDL_CreateWindow(title.c_str(), width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!window_) {
        logger_.log(Logging::LogLevel::Error, "SDL_CreateWindow failed: {}", std::source_location::current(), SDL_GetError());
        SDL_Quit();
        throw std::runtime_error("SDL_CreateWindow failed: " + std::string(SDL_GetError()));
    }

    Uint32 extensionCount = 0;
    const char* const* extensionsRaw = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    if (!extensionsRaw) {
        logger_.log(Logging::LogLevel::Error, "SDL_Vulkan_GetInstanceExtensions failed: {}", std::source_location::current(), SDL_GetError());
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error("SDL_Vulkan_GetInstanceExtensions failed: " + std::string(SDL_GetError()));
    }
    std::vector<const char*> extensions(extensionsRaw, extensionsRaw + extensionCount);

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
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = static_cast<uint32_t>(extensionCount),
        .ppEnabledExtensionNames = extensions.data(),
    };

    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance_);
    if (result != VK_SUCCESS) {
        logger_.log(Logging::LogLevel::Error, "vkCreateInstance failed with result={}", std::source_location::current(), result);
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error("vkCreateInstance failed");
    }

    if (!SDL_Vulkan_CreateSurface(window_, instance_, nullptr, &surface_)) {
        logger_.log(Logging::LogLevel::Error, "SDL_Vulkan_CreateSurface failed: {}", std::source_location::current(), SDL_GetError());
        vkDestroyInstance(instance_, nullptr);
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error("SDL_Vulkan_CreateSurface failed: " + std::string(SDL_GetError()));
    }

    logger_.log(Logging::LogLevel::Info, "SDL3Initializer initialized successfully", std::source_location::current());
}

SDL3Initializer::~SDL3Initializer() {
    logger_.log(Logging::LogLevel::Info, "Destroying SDL3Initializer", std::source_location::current());
    if (surface_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
}

bool SDL3Initializer::shouldQuit() const {
    bool minimized = SDL_GetWindowFlags(window_) & SDL_WINDOW_MINIMIZED;
    if (minimized) {
        logger_.log(Logging::LogLevel::Debug, "Window is minimized, shouldQuit=true", std::source_location::current());
    }
    return minimized;
}

void SDL3Initializer::pollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                logger_.log(Logging::LogLevel::Info, "Quit event received in SDL3Initializer", std::source_location::current());
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                logger_.log(Logging::LogLevel::Debug, "Window resized event: width={}, height={}",
                            std::source_location::current(), event.window.data1, event.window.data2);
                break;
            default:
                logger_.log(Logging::LogLevel::Debug, "Unhandled SDL event in SDL3Initializer: type={}",
                            std::source_location::current(), static_cast<int>(event.type));
                break;
        }
    }
}