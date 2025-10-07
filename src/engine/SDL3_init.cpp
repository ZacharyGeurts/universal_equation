#include "engine/SDL3_init.hpp"
#include <stdexcept>

SDL3Initializer::SDL3Initializer(const std::string& title, int width, int height) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        throw std::runtime_error("SDL_Init failed: " + std::string(SDL_GetError()));
    }

    window_ = SDL_CreateWindow(title.c_str(), width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!window_) {
        SDL_Quit();
        throw std::runtime_error("SDL_CreateWindow failed: " + std::string(SDL_GetError()));
    }

    Uint32 extensionCount = 0;
    const char* const* extensionsRaw = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    if (!extensionsRaw) {
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error("SDL_Vulkan_GetInstanceExtensions failed to get extensions: " + std::string(SDL_GetError()));
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
        .enabledExtensionCount = extensionCount,
        .ppEnabledExtensionNames = extensions.data(),
    };

    if (vkCreateInstance(&createInfo, nullptr, &instance_) != VK_SUCCESS) {
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error("vkCreateInstance failed");
    }

    if (!SDL_Vulkan_CreateSurface(window_, instance_, nullptr, &surface_)) {
        vkDestroyInstance(instance_, nullptr);
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error("SDL_Vulkan_CreateSurface failed: " + std::string(SDL_GetError()));
    }
}

SDL3Initializer::~SDL3Initializer() {
    vkDestroySurfaceKHR(instance_, surface_, nullptr);
    vkDestroyInstance(instance_, nullptr);
    SDL_DestroyWindow(window_);
    SDL_Quit();
}

bool SDL3Initializer::shouldQuit() const {
    return SDL_GetWindowFlags(window_) & SDL_WINDOW_MINIMIZED;
}

void SDL3Initializer::pollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            SDL_Quit();
        }
    }
}