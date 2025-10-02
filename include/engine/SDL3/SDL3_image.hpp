// AMOURANTH RTX October 2025
// Zachary Geurts 2025

// src/engine/SDL3/SDL3_image.hpp
#pragma once
#include <SDL3/SDL.h>
#include <functional>
#include <string>

extern "C" {
#include <SDL3_image/SDL_image.h>
}

namespace SDL3Initializer {

    struct ImageConfig {
        // No specific config needed for SDL3_image as initialization is automatic
    };

    void initImage(const ImageConfig& config, std::function<void(const std::string&)> logMessage);
    void cleanupImage(std::function<void(const std::string&)> logMessage);
    SDL_Texture* loadTexture(SDL_Renderer* renderer, const std::string& file, std::function<void(const std::string&)> logMessage);
    void freeTexture(SDL_Texture* texture, std::function<void(const std::string&)> logMessage);

} // namespace SDL3Initializer