#ifndef MAIN_HPP
#define MAIN_HPP

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <vulkan/vulkan.h>
#include <iostream>
#include <fstream>
#include <ctime>
#include <vector>
#include "universal_equation.hpp"

// Struct to store computation results
struct DimensionData {
    int dimension;
    double positive;
    double negative;
};

class DimensionalNavigator {
public:
    DimensionalNavigator(const char* title = "Dimensional Navigator",
                        int width = 1920, int height = 1280,
                        const char* fontPath = "arial.ttf", int fontSize = 16)
        : window_(nullptr), renderer_(nullptr), font_(nullptr), vulkanInstance_(VK_NULL_HANDLE) {
        initializeSDL(title, width, height, fontPath, fontSize);
        initializeVulkan();
        initializeCalculator();
    }

    ~DimensionalNavigator() {
        if (vulkanInstance_ != VK_NULL_HANDLE) vkDestroyInstance(vulkanInstance_, nullptr);
        if (font_) TTF_CloseFont(font_);
        if (renderer_) SDL_DestroyRenderer(renderer_);
        if (window_) SDL_DestroyWindow(window_);
        TTF_Quit();
        SDL_Quit();
    }

    // Run the main loop
    void run() {
        bool running = true;
        SDL_Event event;
        while (running) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) {
                    running = false;
                }
                handleInput(event);
            }
            render();
            SDL_Delay(16); // ~60 FPS
        }
    }

    // Adjust influence parameter interactively
    void adjustInfluence(double delta) {
        ue_.setInfluence(std::max(0.0, ue_.getInfluence() + delta));
        updateCache();
    }

private:
    SDL_Window* window_;
    SDL_Renderer* renderer_;
    TTF_Font* font_;
    VkInstance vulkanInstance_;
    UniversalEquation ue_;
    std::vector<DimensionData> cache_;
    float wavePhase_ = 0.0f;
    const float waveSpeed_ = 0.1f;

    void initializeSDL(const char* title, int width, int height, const char* fontPath, int fontSize) {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            throw std::runtime_error("SDL_Init failed: " + std::string(SDL_GetError()));
        }
        if (!TTF_Init()) {
            SDL_Quit();
            throw std::runtime_error("TTF_Init failed: " + std::string(SDL_GetError()));
        }
        window_ = SDL_CreateWindow(title, width, height, 0);
        if (!window_) {
            TTF_Quit();
            SDL_Quit();
            throw std::runtime_error("SDL_CreateWindow failed: " + std::string(SDL_GetError()));
        }
        SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        renderer_ = SDL_CreateRenderer(window_, nullptr);
        if (!renderer_) {
            SDL_DestroyWindow(window_);
            TTF_Quit();
            SDL_Quit();
            throw std::runtime_error("SDL_CreateRenderer failed: " + std::string(SDL_GetError()));
        }
        font_ = TTF_OpenFont(fontPath, fontSize);
        if (!font_) {
            std::cerr << "Warning: Could not load " << fontPath << ", trying fallback font\n";
            font_ = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", fontSize);
            if (!font_) {
                SDL_DestroyRenderer(renderer_);
                SDL_DestroyWindow(window_);
                TTF_Quit();
                SDL_Quit();
                throw std::runtime_error("TTF_OpenFont failed: " + std::string(SDL_GetError()));
            }
        }
    }

    void initializeVulkan() {
        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Dimensional Navigator";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "Zac Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        uint32_t count = 0;
        const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&count);
        if (!extensions) {
            throw std::runtime_error("SDL_Vulkan_GetInstanceExtensions failed: " + std::string(SDL_GetError()));
        }
        createInfo.enabledExtensionCount = count;
        createInfo.ppEnabledExtensionNames = extensions;
        if (vkCreateInstance(&createInfo, nullptr, &vulkanInstance_) != VK_SUCCESS) {
            throw std::runtime_error("vkCreateInstance failed");
        }
    }

    void initializeCalculator() {
        ue_ = UniversalEquation(9, 1.0, 0.5, 0.5, 0.5, 2.0, 5.0, 0.2);
        updateCache();
        std::ofstream logFile("dimensional_output.txt");
        if (logFile.is_open()) {
            logFile << "Dimension,Positive,Negative\n";
            for (const auto& data : cache_) {
                logFile << data.dimension << "," << data.positive << "," << data.negative << "\n";
            }
            logFile.close();
        }
    }

    void updateCache() {
        cache_.clear();
        for (int d = 1; d <= ue_.getMaxDimensions(); ++d) {
            ue_.setCurrentDimension(d);
            auto [pos, neg] = ue_.compute();
            cache_.push_back({d, pos, neg});
        }
    }

    void handleInput(const SDL_Event& event) {
        if (event.type == SDL_EVENT_KEY_DOWN) {
            if (event.key.key == SDLK_UP) {
                adjustInfluence(0.1);
            }
            if (event.key.key == SDLK_DOWN) {
                adjustInfluence(-0.1);
            }
        }
    }

    void render() {
        // Clear the screen
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
        SDL_RenderClear(renderer_);

        // Graph parameters
        const int width = 1820, height = 1180, xOffset = 50, yOffset = 50;
        const float scale = 25.0f;

        // Draw wave-like background for 1D influence
        SDL_SetRenderDrawColor(renderer_, 0, 100 + static_cast<int>(50 * std::sin(wavePhase_)), 100, 128);
        for (int x = xOffset; x < xOffset + width; x += 5) {
            int y = yOffset + height / 2 + static_cast<int>(20.0f * ue_.getInfluence() * std::sin((x - xOffset) * 0.01f + wavePhase_));
            SDL_RenderPoint(renderer_, x, y);
        }
        wavePhase_ += waveSpeed_;

        // Draw graph
        SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
        for (size_t i = 0; i < cache_.size(); ++i) {
            int x = xOffset + static_cast<int>(i * (width / (float)(cache_.size() - 1)));
            int yPositive = yOffset + height - static_cast<int>(cache_[i].positive * scale);
            int yNegative = yOffset + height - static_cast<int>(cache_[i].negative * scale);
            SDL_RenderPoint(renderer_, x, yPositive);
            SDL_RenderPoint(renderer_, x, yNegative);

            if (i > 0) {
                int prevX = xOffset + static_cast<int>((i - 1) * (width / (float)(cache_.size() - 1)));
                int prevYPos = yOffset + height - static_cast<int>(cache_[i - 1].positive * scale);
                int prevYNeg = yOffset + height - static_cast<int>(cache_[i - 1].negative * scale);
                SDL_RenderLine(renderer_, prevX, prevYPos, x, yPositive);
                SDL_RenderLine(renderer_, prevX, prevYNeg, x, yNegative);
            }
        }

        // Draw axes
        SDL_SetRenderDrawColor(renderer_, 128, 128, 128, 255);
        SDL_RenderLine(renderer_, xOffset, yOffset + height, xOffset + width, yOffset + height);
        SDL_RenderLine(renderer_, xOffset, yOffset, xOffset, yOffset + height);

        // Draw dimension labels
        SDL_Color white = {255, 255, 255, 255};
        for (int d = 1; d <= ue_.getMaxDimensions(); ++d) {
            char label[4];
            snprintf(label, sizeof(label), "%d", d);
            SDL_Surface* surface = TTF_RenderText_Solid(font_, label, strlen(label), white);
            if (!surface) continue;
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
            if (!texture) {
                SDL_DestroySurface(surface);
                continue;
            }
            int x = xOffset + (d - 1) * (width / (ue_.getMaxDimensions() - 1)) - 10;
            int y = yOffset + height + 10;
            SDL_FRect rect = {static_cast<float>(x), static_cast<float>(y), static_cast<float>(surface->w), static_cast<float>(surface->h)};
            SDL_RenderTexture(renderer_, texture, nullptr, &rect);
            SDL_DestroySurface(surface);
            SDL_DestroyTexture(texture);
        }

        // Draw timestamp
        time_t now = time(nullptr);
        char timeStr[50];
        strftime(timeStr, sizeof(timeStr), "%I:%M %p %Z, %B %d, %Y", localtime(&now));
        SDL_Surface* timeSurface = TTF_RenderText_Solid(font_, timeStr, strlen(timeStr), white);
        if (timeSurface) {
            SDL_Texture* timeTexture = SDL_CreateTextureFromSurface(renderer_, timeSurface);
            if (timeTexture) {
                SDL_FRect timeRect = {static_cast<float>(xOffset), static_cast<float>(yOffset - 20),
                                      static_cast<float>(timeSurface->w), static_cast<float>(timeSurface->h)};
                SDL_RenderTexture(renderer_, timeTexture, nullptr, &timeRect);
                SDL_DestroyTexture(timeTexture);
            }
            SDL_DestroySurface(timeSurface);
        }

        SDL_RenderPresent(renderer_);
    }
};

#endif // MAIN_HPP