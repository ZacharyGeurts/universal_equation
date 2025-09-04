#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <iostream>
#include "universal_equation.hpp"

int main(int argc, char* argv[]) {
    // Initialize SDL and SDL_ttf (like turning on our playground lights!)
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Oops! Couldn't start SDL: " << SDL_GetError() << std::endl;
        return 1;
    }
    if (TTF_Init() < 0) {
        std::cerr << "Oops! Couldn't start text: " << TTF_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    // Create a window (our playground canvas!)
    SDL_Window* window = SDL_CreateWindow("Dimension Dance", 800, 600, 0);
    if (!window) {
        std::cerr << "Oops! Couldn't make a window: " << SDL_GetError() << std::endl;
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // Create a renderer (our paintbrush!)
    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "Oops! Couldn't get a paintbrush: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // Load the font (our letter magic from arial.ttf!)
    TTF_Font* font = TTF_OpenFont("arial.ttf", 16); // Size 16 text
    if (!font) {
        std::cerr << "Oops! Couldn't load the font: " << TTF_GetError() << std::endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // Set up our UniversalEquation toy (the dimension dance calculator!)
    UniversalEquation ue;

    // Game loop (keep playing until we close the window!)
    bool running = true;
    SDL_Event event;
    while (running) {
        // Check for quit (like closing the playground!)
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        // Clear the canvas (wipe it clean!)
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Black background
        SDL_RenderClear(renderer);

        // Draw the graph (our dimension dance!)
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // White lines
        int width = 700; // Graph width
        int height = 500; // Graph height
        int xOffset = 50; // Left margin
        int yOffset = 50; // Top margin
        for (int d = 1; d <= 9; ++d) {
            ue.setCurrentDimension(d); // Pick a dimension
            auto [totalPositive, totalNegative] = ue.compute(); // Get the plus and minus answers

            // Map dimension (1-9) to x-axis (0 to width)
            int x = xOffset + (d - 1) * (width / 8);
            // Map values (-10 to 10, adjust as needed) to y-axis (height to 0)
            int yPositive = yOffset + height - (totalPositive * 25); // Scale factor 25 for visibility
            int yNegative = yOffset + height - (totalNegative * 25);

            // Draw points for positive and negative values
            SDL_RenderDrawPoint(renderer, x, yPositive);
            SDL_RenderDrawPoint(renderer, x, yNegative);

            // Draw lines connecting points (simple graph)
            if (d > 1) {
                int prevX = xOffset + (d - 2) * (width / 8);
                auto [prevPos, prevNeg] = ue.compute(); // Recompute for prev dimension
                int prevYPos = yOffset + height - (prevPos * 25);
                int prevYNeg = yOffset + height - (prevNeg * 25);
                SDL_RenderDrawLine(renderer, prevX, prevYPos, x, yPositive);
                SDL_RenderDrawLine(renderer, prevX, prevYNeg, x, yNegative);
            }
        }

        // Add axes (like a playground grid!)
        SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255); // Gray lines
        SDL_RenderDrawLine(renderer, xOffset, yOffset + height, xOffset + width, yOffset + height); // X-axis
        SDL_RenderDrawLine(renderer, xOffset, yOffset, xOffset, yOffset + height); // Y-axis

        // Add labels for dimensions (1 to 9)
        for (int d = 1; d <= 9; ++d) {
            char label[4];
            snprintf(label, sizeof(label), "%d", d);
            SDL_Surface* surface = TTF_RenderText_Solid(font, label, {255, 255, 255, 255});
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            int x = xOffset + (d - 1) * (width / 8) - 10; // Center label
            int y = yOffset + height + 10;
            SDL_Rect rect = {x, y, surface->w, surface->h};
            SDL_RenderCopy(renderer, texture, NULL, &rect);
            SDL_FreeSurface(surface);
            SDL_DestroyTexture(texture);
        }

        // Add current date and time (like a playground clock!)
        time_t now = time(0);
        tm* ltm = localtime(&now);
        char timeStr[50];
        strftime(timeStr, sizeof(timeStr), "Time: %I:%M %p EDT, %B %d, %Y", ltm);
        SDL_Surface* timeSurface = TTF_RenderText_Solid(font, timeStr, {255, 255, 255, 255});
        SDL_Texture* timeTexture = SDL_CreateTextureFromSurface(renderer, timeSurface);
        SDL_Rect timeRect = {xOffset, yOffset - 20, timeSurface->w, timeSurface->h};
        SDL_RenderCopy(renderer, timeTexture, NULL, &timeRect);
        SDL_FreeSurface(timeSurface);
        SDL_DestroyTexture(timeTexture);

        // Show the canvas (let everyone see the drawing!)
        SDL_RenderPresent(renderer);

        // Slow down a bit so we can see (like taking a playground break!)
        SDL_Delay(100);
    }

    // Clean up (put the toys away!)
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}