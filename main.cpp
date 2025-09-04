#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h> // Include Vulkan header for types and functions
#include <SDL3_ttf/SDL_ttf.h>
#include <iostream>
#include <ctime>
#include "universal_equation.hpp"

int main(int argc, char* argv[]) {
    // Turn on the playground lights with SDL!
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Oops! Couldn't start SDL: " << SDL_GetError() << std::endl;
        return 1;
    }
    // Turn on the text magic with SDL_ttf!
    if (TTF_Init() < 0) {
        std::cerr << "Oops! Couldn't start text: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    // Create our playground window with centered position!
    SDL_Window* window = SDL_CreateWindow("Dimension Dance", 800, 600, SDL_WINDOWPOS_CENTERED_MASK);
    if (!window) {
        std::cerr << "Oops! Couldn't make a window: " << SDL_GetError() << std::endl;
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // Initialize Vulkan instance
    VkInstance vulkanInstance = VK_NULL_HANDLE;
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Dimension Dance";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    unsigned int count = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(&count)) { // Use SDL_Window* and nullptr for first call
        std::cerr << "Oops! Couldn't get Vulkan extension count: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    const char** names = new const char*[count];
    if (!SDL_Vulkan_GetInstanceExtensions(&count)) { // Get extension names
        std::cerr << "Oops! Couldn't get Vulkan extension names: " << SDL_GetError() << std::endl;
        delete[] names;
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    createInfo.enabledExtensionCount = count;
    createInfo.ppEnabledExtensionNames = names;
    if (vkCreateInstance(&createInfo, nullptr, &vulkanInstance) != VK_SUCCESS) {
        std::cerr << "Oops! Couldn't create Vulkan instance: " << SDL_GetError() << std::endl;
        delete[] names;
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // Get our paintbrush (renderer) for now
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr); // No flags, default renderer
    if (!renderer) {
        std::cerr << "Oops! Couldn't get a paintbrush: " << SDL_GetError() << std::endl;
        vkDestroyInstance(vulkanInstance, nullptr);
        delete[] names;
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // Load the magic letters from arial.ttf!
    TTF_Font* font = TTF_OpenFont("arial.ttf", 16); // Size 16 text
    if (!font) {
        std::cerr << "Oops! Couldn't load the font: " << SDL_GetError() << std::endl;
        SDL_DestroyRenderer(renderer);
        vkDestroyInstance(vulkanInstance, nullptr);
        delete[] names;
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // Set up our dimension dance calculator!
    UniversalEquation ue;

    // Keep playing until we close the window!
    bool running = true;
    SDL_Event event;
    while (running) {
        // Check if we want to quit (close the playground!)
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        // Wipe the canvas clean!
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Black background
        SDL_RenderClear(renderer);

        // Draw the dimension dance graph!
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // White lines
        int width = 700; // Graph width
        int height = 500; // Graph height
        int xOffset = 50; // Left margin
        int yOffset = 50; // Top margin
        for (int d = 1; d <= 9; ++d) {
            ue.setCurrentDimension(d); // Pick a dimension to play with
            auto [totalPositive, totalNegative] = ue.compute(); // Get the plus and minus energy

            // Turn dimension (1-9) into a spot on the x-axis
            int x = xOffset + (d - 1) * (width / 8);
            // Turn the energy values into spots on the y-axis (scale by 25 to see them)
            int yPositive = yOffset + height - (static_cast<int>(totalPositive * 25));
            int yNegative = yOffset + height - (static_cast<int>(totalNegative * 25));

            // Draw dots for the plus and minus energy
            SDL_RenderPoint(renderer, x, yPositive);
            SDL_RenderPoint(renderer, x, yNegative);

            // Connect the dots with lines to make a path
            if (d > 1) {
                int prevX = xOffset + (d - 2) * (width / 8);
                ue.setCurrentDimension(d - 1); // Go back to the last dimension
                auto [prevPos, prevNeg] = ue.compute();
                int prevYPos = yOffset + height - (static_cast<int>(prevPos * 25));
                int prevYNeg = yOffset + height - (static_cast<int>(prevNeg * 25));
                SDL_RenderLine(renderer, prevX, prevYPos, x, yPositive);
                SDL_RenderLine(renderer, prevX, prevYNeg, x, yNegative);
            }
        }

        // Draw the playground grid (x and y axes)!
        SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255); // Gray lines
        SDL_RenderLine(renderer, xOffset, yOffset + height, xOffset + width, yOffset + height); // X-axis
        SDL_RenderLine(renderer, xOffset, yOffset, xOffset, yOffset + height); // Y-axis

        // Add number labels for each dimension (1 to 9)!
        for (int d = 1; d <= 9; ++d) {
            char label[4];
            snprintf(label, sizeof(label), "%d", d);
            SDL_Color white = {255, 255, 255, 255}; // White color for text
            SDL_Surface* surface = TTF_RenderText_Solid(font, label, strlen(label), white);
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            int x = xOffset + (d - 1) * (width / 8) - 10; // Center the number
            int y = yOffset + height + 10;
            SDL_FRect rect = {static_cast<float>(x), static_cast<float>(y), static_cast<float>(surface->w), static_cast<float>(surface->h)};
            SDL_RenderTexture(renderer, texture, nullptr, &rect);
            SDL_DestroySurface(surface);
            SDL_DestroyTexture(texture);
        }

        // Add the current time (our playground clock)!
        time_t now = time(0);
        tm* ltm = localtime(&now);
        char timeStr[50];
        strftime(timeStr, sizeof(timeStr), "Time: %I:%M %p EDT, %B %d, %Y", ltm); // e.g., 07:34 PM EDT, September 04, 2025
        SDL_Color white = {255, 255, 255, 255}; // White color for text
        SDL_Surface* timeSurface = TTF_RenderText_Solid(font, timeStr, strlen(timeStr), white);
        SDL_Texture* timeTexture = SDL_CreateTextureFromSurface(renderer, timeSurface);
        SDL_FRect timeRect = {static_cast<float>(xOffset), static_cast<float>(yOffset - 20), static_cast<float>(timeSurface->w), static_cast<float>(timeSurface->h)};
        SDL_RenderTexture(renderer, timeTexture, nullptr, &timeRect);
        SDL_DestroySurface(timeSurface);
        SDL_DestroyTexture(timeTexture);

        // Show the drawing to everyone!
        SDL_RenderPresent(renderer);

        // Take a little break so we can enjoy the show!
        SDL_Delay(100);
    }

    // Put the toys away!
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    vkDestroyInstance(vulkanInstance, nullptr);
    delete[] names;
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}