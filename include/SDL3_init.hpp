#ifndef SDL3_INIT_HPP
#define SDL3_INIT_HPP

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <vector>

class SDL3Initializer {
public:
    static void initializeSDL(SDL_Window*& window, VkInstance& vkInstance, VkSurfaceKHR& vkSurface,
                              TTF_Font*& font, const char* title, int width, int height,
                              const char* fontPath, int fontSize) {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            std::string err = SDL_GetError();
            throw std::runtime_error("SDL_Init failed: " + (err.empty() ? "Unknown error" : err));
        }

        if (!TTF_Init()) {
            //SDL_Quit();
            //throw std::runtime_error("TTF_Init failed: " + std::string(SDL_GetError()));
        }

        window = SDL_CreateWindow(title, width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
        if (!window) {
            TTF_Quit();
            SDL_Quit();
            throw std::runtime_error("SDL_CreateWindow failed: " + std::string(SDL_GetError()));
        }

        // Create Vulkan instance
        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = title;
        appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3;

        Uint32 extensionCount = 0;
        const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
        if (!extensions) {
            SDL_DestroyWindow(window);
            TTF_Quit();
            SDL_Quit();
            throw std::runtime_error("SDL_Vulkan_GetInstanceExtensions failed: " + std::string(SDL_GetError()));
        }

        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = extensionCount;
        createInfo.ppEnabledExtensionNames = extensions;

        if (vkCreateInstance(&createInfo, nullptr, &vkInstance) != VK_SUCCESS) {
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
        if (vkSurface) vkDestroySurfaceKHR(vkInstance, vkSurface, nullptr);
        if (vkInstance) vkDestroyInstance(vkInstance, nullptr);
        if (window) SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
    }
};

#endif // SDL3_INIT_HPP