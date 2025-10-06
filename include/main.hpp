#ifndef MAIN_HPP
#define MAIN_HPP

#include "engine/SDL3_init.hpp"
#include "engine/Vulkan_init.hpp"
#include "engine/core.hpp"
#include "engine/logging.hpp" // Ensure logger is included
#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>
#include <memory>

class Application {
public:
    Application(const char* name, int width, int height)
        : logger_(Logging::LogLevel::Info, "application.log"), // Initialize logger with file
          simulator_(nullptr),
          sdlInitializer_(logger_),
          window_(nullptr),
          vulkanInstance_(VK_NULL_HANDLE),
          width_(width),
          height_(height) {
        logger_.log(Logging::LogLevel::Info, "Initializing Application with name={}, width={}, height={}",
                    std::source_location::current(), name, width, height);

        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) { // Fix comparison
            logger_.log(Logging::LogLevel::Error, "SDL initialization failed: {}", 
                        std::source_location::current(), SDL_GetError());
            throw std::runtime_error("SDL initialization failed");
        }

        window_ = SDL_CreateWindow(name, width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
        if (!window_) {
            logger_.log(Logging::LogLevel::Error, "Window creation failed: {}", 
                        std::source_location::current(), SDL_GetError());
            throw std::runtime_error("Window creation failed");
        }

        simulator_ = new DimensionalNavigator("Dimensional Navigator", width_, height_, logger_);
        amouranth_ = new AMOURANTH(simulator_, logger_);
    }

    ~Application() {
        delete amouranth_;
        delete simulator_;
        if (window_) {
            SDL_DestroyWindow(window_);
        }
        SDL_Quit();
    }

    void run() {
        // Implementation of run method (assuming it exists)
    }

private:
    Logging::Logger logger_; // Logger instance for the application
    DimensionalNavigator* simulator_;
    SDL3Initializer::SDL3Initializer sdlInitializer_; // Use fully qualified namespace
    SDL_Window* window_;
    VkInstance vulkanInstance_;
    AMOURANTH* amouranth_;
    int width_;
    int height_;
};

#endif // MAIN_HPP