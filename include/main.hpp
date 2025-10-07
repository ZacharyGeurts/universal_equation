#ifndef MAIN_HPP
#define MAIN_HPP

#include "engine/SDL3_init.hpp"
#include "engine/Vulkan_init.hpp"
#include "engine/core.hpp"
#include "engine/logging.hpp"
#include <stdexcept>
#include <format>

class Application {
public:
    Application(const char* name, int width, int height)
        : logger_(Logging::LogLevel::Info, "application.log"),
          simulator_(nullptr),
          sdlInitializer_(logger_),
          width_(width),
          height_(height) {
        logger_.log(Logging::LogLevel::Info, "Initializing Application with name={}, width={}, height={}",
                    std::source_location::current(), name, width, height);

        // Use SDL3Initializer for SDL and window initialization
        try {
            sdlInitializer_.initialize(name, width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE,
                                       true, "assets/fonts/sf-plasmatica-open.ttf");
        } catch (const std::runtime_error& e) {
            logger_.log(Logging::LogLevel::Error, "SDL3Initializer failed: {}", 
                        std::source_location::current(), e.what());
            throw;
        }

        window_ = sdlInitializer_.getWindow();
        if (!window_) {
            logger_.log(Logging::LogLevel::Error, "Failed to retrieve window from SDL3Initializer",
                        std::source_location::current());
            throw std::runtime_error("Failed to retrieve window");
        }

        simulator_ = new DimensionalNavigator("Dimensional Navigator", width_, height_, logger_);
        amouranth_ = new AMOURANTH(simulator_, logger_);
    }

    ~Application() {
        logger_.log(Logging::LogLevel::Info, "Destroying Application", std::source_location::current());
        delete amouranth_;
        delete simulator_;
        // Window and SDL cleanup handled by sdlInitializer_
        sdlInitializer_.cleanup();
    }

    void run() {
        // Assuming run uses sdlInitializer_.eventLoop
        auto renderCallback = []() {
            // Placeholder for rendering logic
            // Add Vulkan or other rendering here
        };
        sdlInitializer_.eventLoop(renderCallback, [this](int w, int h) {
            width_ = w;
            height_ = h;
            logger_.log(Logging::LogLevel::Info, "Window resized to width={}, height={}",
                        std::source_location::current(), w, h);
        });
    }

private:
    Logging::Logger logger_;
    DimensionalNavigator* simulator_;
    SDL3Initializer::SDL3Initializer sdlInitializer_;
    SDL_Window* window_;
    AMOURANTH* amouranth_;
    int width_;
    int height_;
};

#endif // MAIN_HPP