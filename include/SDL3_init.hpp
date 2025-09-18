#ifndef SDL3_INIT_HPP
#define SDL3_INIT_HPP

#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"
#include "vulkan/vulkan.h"
#include <stdexcept>
#include <vector>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <functional>

class SDL3Initializer {
public:
    using ResizeCallback = std::function<void(int, int)>;

    static void initializeSDL(
        SDL_Window*& window,
        VkInstance& vkInstance,
        VkSurfaceKHR& vkSurface,
        const char* title,
        int width,
        int height,
        Uint32 windowFlags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    ) {
        if (SDL_Init(SDL_INIT_VIDEO) == 0) {
            throw std::runtime_error("SDL_Init failed: " + std::string(SDL_GetError()));
        }

        window = SDL_CreateWindow(title, width, height, windowFlags);
        if (!window) {
            SDL_Quit();
            throw std::runtime_error("SDL_CreateWindow failed: " + std::string(SDL_GetError()));
        }

        // Vulkan Instance Extensions
        Uint32 extCount = 0;
        const char* const* exts = SDL_Vulkan_GetInstanceExtensions(&extCount);
        if (!exts || extCount == 0) {
            SDL_DestroyWindow(window);
            SDL_Quit();
            throw std::runtime_error("SDL_Vulkan_GetInstanceExtensions failed: " + std::string(SDL_GetError()));
        }

        std::vector<const char*> extensions(exts, exts + extCount);
        if (std::find_if(extensions.begin(), extensions.end(),
                [](const char* ext){ return std::strcmp(ext, "VK_KHR_surface") == 0; }) == extensions.end()) {
            extensions.push_back("VK_KHR_surface");
        }

        const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = title;
        appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledLayerCount = 1;
        createInfo.ppEnabledLayerNames = layers;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        VkResult result = vkCreateInstance(&createInfo, nullptr, &vkInstance);
        if (result != VK_SUCCESS) {
            SDL_DestroyWindow(window);
            SDL_Quit();
            throw std::runtime_error("vkCreateInstance failed");
        }

        if (!SDL_Vulkan_CreateSurface(window, vkInstance, nullptr, &vkSurface)) {
            vkDestroyInstance(vkInstance, nullptr);
            SDL_DestroyWindow(window);
            SDL_Quit();
            throw std::runtime_error("SDL_Vulkan_CreateSurface failed: " + std::string(SDL_GetError()));
        }
    }

    static void eventLoop(
        SDL_Window* window,
        std::function<void()> renderCallback,
        ResizeCallback onResize = nullptr,
        bool exitOnClose = true
    ) {
        bool running = true;
        while (running) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                switch (event.type) {
                case SDL_EVENT_QUIT:
                    if (exitOnClose) running = false;
                    break;
                case SDL_EVENT_WINDOW_RESIZED:
                    if (onResize) {
                        int newW = event.window.data1;
                        int newH = event.window.data2;
                        onResize(newW, newH);
                    } else {
                        std::cout << "Window resized to: " << event.window.data1 << "x" << event.window.data2 << std::endl;
                    }
                    break;
                case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                    if (exitOnClose) running = false;
                    break;
                case SDL_EVENT_KEY_DOWN:
                    if (event.key.key == SDLK_F) {
                        // Toggle fullscreen
                        Uint32 flags = SDL_GetWindowFlags(window);
                        if (flags & SDL_WINDOW_FULLSCREEN) {
                            SDL_SetWindowFullscreen(window, 0);
                        } else {
                            SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
                        }
                    }
                    break;
                default:
                    break;
                }
            }
            if (renderCallback) renderCallback();
        }
    }

    static void cleanupSDL(SDL_Window* window, VkInstance vkInstance, VkSurfaceKHR vkSurface) {
        if (vkInstance && vkSurface) vkDestroySurfaceKHR(vkInstance, vkSurface, nullptr);
        if (vkInstance) vkDestroyInstance(vkInstance, nullptr);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
    }
};

#endif // SDL3_INIT_HPP