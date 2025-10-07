// AMOURANTH RTX Engine, October 2025 - Image loading and texture management implementation.
// Thread-safe with C++20 features; no mutexes required.
// Dependencies: SDL3, SDL_image, C++20 standard library, logging.hpp.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#include "engine/SDL3/SDL3_image.hpp"
#include <SDL3_image/SDL_image.h>
#include <stdexcept>
#include <format>

namespace SDL3Initializer {

void initImage(const ImageConfig& config) {
    Logging::Logger logger;
    // Note: config is currently unused but retained for future extensions (e.g., format selection)
    (void)config; // Suppress unused parameter warning

    logger.log(Logging::LogLevel::Info, "Initializing SDL_image", std::source_location::current());

    // Verify platform support
    std::string_view platform = SDL_GetPlatform();
    if (platform != "Linux" && platform != "Windows") {
        std::string error = std::format("Unsupported platform for image: {}", platform);
        logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }

    // SDL3's SDL_image does not require explicit initialization (IMG_Init is removed)
    // Image format support (PNG, JPG) is handled automatically by IMG_LoadTexture
    logger.log(Logging::LogLevel::Info, "SDL_image ready for PNG and JPG loading", std::source_location::current());
}

void cleanupImage() {
    Logging::Logger logger;
    logger.log(Logging::LogLevel::Info, "Cleaning up SDL_image", std::source_location::current());
    // SDL3's SDL_image does not require IMG_Quit; cleanup is handled by SDL core if needed
}

SDL_Texture* loadTexture(SDL_Renderer* renderer, const std::string& file) {
    Logging::Logger logger;
    logger.log(Logging::LogLevel::Debug, "Loading texture from file: {}", std::source_location::current(), file);

    SDL_Texture* texture = IMG_LoadTexture(renderer, file.c_str());
    if (!texture) {
        std::string error = std::format("IMG_LoadTexture failed for {}: {}", file, SDL_GetError());
        logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }

    logger.log(Logging::LogLevel::Info, "Texture loaded successfully: {}", std::source_location::current(), file);
    return texture;
}

} // namespace SDL3Initializer