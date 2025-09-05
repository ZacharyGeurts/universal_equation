#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <iostream>
#include <ctime>
#include <vector>
#include "universal_equation.hpp"

// RAII struct to manage SDL resources
struct SDLResources {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    TTF_Font* font = nullptr;

    SDLResources(const char* title, int width, int height, const char* fontPath, int fontSize) {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            throw std::runtime_error("SDL_Init failed: " + std::string(SDL_GetError()));
        }
        if (!TTF_Init()) {
            SDL_Quit();
            throw std::runtime_error("TTF_Init failed: " + std::string(SDL_GetError()));
        }
        window = SDL_CreateWindow(title, width, height, 0);
        if (!window) {
            TTF_Quit();
            SDL_Quit();
            throw std::runtime_error("SDL_CreateWindow failed: " + std::string(SDL_GetError()));
        }
        renderer = SDL_CreateRenderer(window, nullptr); // Corrected: Removed extra flag argument
        if (!renderer) {
            SDL_DestroyWindow(window);
            TTF_Quit();
            SDL_Quit();
            throw std::runtime_error("SDL_CreateRenderer failed: " + std::string(SDL_GetError()));
        }
        font = TTF_OpenFont(fontPath, fontSize);
        if (!font) {
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            TTF_Quit();
            SDL_Quit();
            throw std::runtime_error("TTF_OpenFont failed: " + std::string(SDL_GetError()));
        }
    }

    ~SDLResources() {
        if (font) TTF_CloseFont(font);
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
    }
};

// Cache for UniversalEquation results
struct DimensionData {
    int dimension;
    double positive;
    double negative;
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    try {
        // Initialize SDL and resources
        SDLResources res("Dimension Dance", 800, 600, "arial.ttf", 16);

        // Center the window
        SDL_SetWindowPosition(res.window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

        // Set up UniversalEquation with wave-like parameters
        UniversalEquation ue(9, 1.0, 0.5, 0.5, 0.5, 2.0, 5.0, 0.2);

        // Cache computation results
        std::vector<DimensionData> cache;
        for (int d = 1; d <= ue.getMaxDimensions(); ++d) {
            ue.setCurrentDimension(d);
            auto [pos, neg] = ue.compute();
            cache.push_back({d, pos, neg});
        }

        // Animation parameter for wave-like effect
        float wavePhase = 0.0f;
        const float waveSpeed = 0.1f;

        // Main loop
        bool running = true;
        SDL_Event event;
        while (running) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) {
                    running = false;
                }
            }

            // Clear the screen
            SDL_SetRenderDrawColor(res.renderer, 0, 0, 0, 255);
            SDL_RenderClear(res.renderer);

            // Graph parameters
            const int width = 700, height = 500, xOffset = 50, yOffset = 50;
            const float scale = 25.0f;

            // Draw wave-like background for 1D influence
            SDL_SetRenderDrawColor(res.renderer, 0, 100, 100, 128);
            for (int x = xOffset; x < xOffset + width; x += 5) {
                int y = yOffset + height / 2 + static_cast<int>(20.0f * std::sin((x - xOffset) * 0.01f + wavePhase));
                SDL_RenderPoint(res.renderer, x, y);
            }
            wavePhase += waveSpeed;

            // Draw graph
            SDL_SetRenderDrawColor(res.renderer, 255, 255, 255, 255);
            for (size_t i = 0; i < cache.size(); ++i) {
                int x = xOffset + static_cast<int>(i * (width / (float)(cache.size() - 1)));
                int yPositive = yOffset + height - static_cast<int>(cache[i].positive * scale);
                int yNegative = yOffset + height - static_cast<int>(cache[i].negative * scale);
                SDL_RenderPoint(res.renderer, x, yPositive);
                SDL_RenderPoint(res.renderer, x, yNegative);

                if (i > 0) {
                    int prevX = xOffset + static_cast<int>((i - 1) * (width / (float)(cache.size() - 1)));
                    int prevYPos = yOffset + height - static_cast<int>(cache[i - 1].positive * scale);
                    int prevYNeg = yOffset + height - static_cast<int>(cache[i - 1].negative * scale);
                    SDL_RenderLine(res.renderer, prevX, prevYPos, x, yPositive);
                    SDL_RenderLine(res.renderer, prevX, prevYNeg, x, yNegative);
                }
            }

            // Draw axes
            SDL_SetRenderDrawColor(res.renderer, 128, 128, 128, 255);
            SDL_RenderLine(res.renderer, xOffset, yOffset + height, xOffset + width, yOffset + height);
            SDL_RenderLine(res.renderer, xOffset, yOffset, xOffset, yOffset + height);

            // Draw dimension labels
            SDL_Color white = {255, 255, 255, 255};
            for (int d = 1; d <= ue.getMaxDimensions(); ++d) {
                char label[4];
                snprintf(label, sizeof(label), "%d", d);
                SDL_Surface* surface = TTF_RenderText_Solid(res.font, label, strlen(label), white);
                if (!surface) continue;
                SDL_Texture* texture = SDL_CreateTextureFromSurface(res.renderer, surface);
                if (!texture) {
                    SDL_DestroySurface(surface);
                    continue;
                }
                int x = xOffset + (d - 1) * (width / (ue.getMaxDimensions() - 1)) - 10;
                int y = yOffset + height + 10;
                SDL_FRect rect = {static_cast<float>(x), static_cast<float>(y), static_cast<float>(surface->w), static_cast<float>(surface->h)};
                SDL_RenderTexture(res.renderer, texture, nullptr, &rect);
                SDL_DestroySurface(surface);
                SDL_DestroyTexture(texture);
            }

            // Draw timestamp
            time_t now = time(nullptr);
            char timeStr[50];
            strftime(timeStr, sizeof(timeStr), "%I:%M %p %Z, %B %d, %Y", localtime(&now));
            SDL_Surface* timeSurface = TTF_RenderText_Solid(res.font, timeStr, strlen(timeStr), white);
            if (timeSurface) {
                SDL_Texture* timeTexture = SDL_CreateTextureFromSurface(res.renderer, timeSurface);
                if (timeTexture) {
                    SDL_FRect timeRect = {static_cast<float>(xOffset), static_cast<float>(yOffset - 20),
                                          static_cast<float>(timeSurface->w), static_cast<float>(timeSurface->h)};
                    SDL_RenderTexture(res.renderer, timeTexture, nullptr, &timeRect);
                    SDL_DestroyTexture(timeTexture);
                }
                SDL_DestroySurface(timeSurface);
            }

            SDL_RenderPresent(res.renderer);
            SDL_Delay(16); // ~60 FPS
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}