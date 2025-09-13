#ifndef SDL3_INIT_HPP
#define SDL3_INIT_HPP

#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"
#include "SDL3_ttf/SDL_ttf.h"
#include "vulkan/vulkan.h"
#include <stdexcept>
#include <vector>
#include <iostream>
#include <algorithm>
#include <cstring>

class SDL3Initializer {
public:
    static void initializeSDL(SDL_Window*& window, VkInstance& vkInstance, VkSurfaceKHR& vkSurface,
                              TTF_Font*& font, const char* title, int width, int height,
                              const char* fontPath, int fontSize) {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            throw std::runtime_error("SDL_Init failed: " + std::string(SDL_GetError()));
        }
        if (!TTF_Init()) {
            SDL_Quit();
            throw std::runtime_error("TTF_Init failed: " + std::string(SDL_GetError()));
        }
        window = SDL_CreateWindow(title, width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
        if (!window) {
            TTF_Quit();
            SDL_Quit();
            throw std::runtime_error("SDL_CreateWindow failed: " + std::string(SDL_GetError()));
        }

        Uint32 extCount = 0;
        const char* const* exts = SDL_Vulkan_GetInstanceExtensions(&extCount);
        if (!exts) {
            SDL_DestroyWindow(window);
            TTF_Quit();
            SDL_Quit();
            throw std::runtime_error("SDL_Vulkan_GetInstanceExtensions failed: " + std::string(SDL_GetError()));
        }
        std::vector<const char*> extensions(exts, exts + extCount);
        bool hasSurface = false;
        for (const auto* ext : extensions) {
            if (std::strcmp(ext, "VK_KHR_surface") == 0) {
                hasSurface = true;
                break;
            }
        }
        if (!hasSurface) {
            extensions.push_back("VK_KHR_surface");
        }
        const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
        std::cerr << "Vulkan instance extensions (" << extensions.size() << "):\n";
        for (const auto& ext : extensions) std::cerr << ext << "\n";
        VkApplicationInfo appInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO, nullptr, title, VK_MAKE_API_VERSION(0, 1, 0, 0), "No Engine", VK_MAKE_API_VERSION(0, 1, 0, 0), VK_API_VERSION_1_3 };
        VkInstanceCreateInfo createInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, nullptr, 0, &appInfo, 1, layers, (uint32_t)extensions.size(), extensions.data() };
        VkResult result = vkCreateInstance(&createInfo, nullptr, &vkInstance);
        if (result != VK_SUCCESS) {
            std::cerr << "vkCreateInstance failed with error: " << result << "\n";
            SDL_DestroyWindow(window);
            TTF_Quit();
            SDL_Quit();
            throw std::runtime_error("vkCreateInstance failed");
        }
        if (!SDL_Vulkan_CreateSurface(window, vkInstance, nullptr, &vkSurface)) {
            vkDestroyInstance(vkInstance, nullptr);
            SDL_DestroyWindow(window);
            TTF_Quit();
            SDL_Quit();
            throw std::runtime_error("SDL_Vulkan_CreateSurface failed: " + std::string(SDL_GetError()));
        }
        font = TTF_OpenFont(fontPath, fontSize);
        if (!font) {
            vkDestroySurfaceKHR(vkInstance, vkSurface, nullptr);
            vkDestroyInstance(vkInstance, nullptr);
            SDL_DestroyWindow(window);
            TTF_Quit();
            SDL_Quit();
            throw std::runtime_error("TTF_OpenFont failed: " + std::string(SDL_GetError()));
        }
    }

    static void cleanupSDL(SDL_Window* window, VkInstance vkInstance, VkSurfaceKHR vkSurface, TTF_Font* font) {
        if (font) TTF_CloseFont(font);
        if (vkInstance && vkSurface) vkDestroySurfaceKHR(vkInstance, vkSurface, nullptr);
        if (vkInstance) vkDestroyInstance(vkInstance, nullptr);
        if (window) SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
    }
};

#endif // SDL3_INIT_HPP