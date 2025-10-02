// AMOURANTH RTX October 2025
// Zachary Geurts 2025

// src/engine/SDL3/SDL3_image.cpp
#include "engine/SDL3/SDL3_image.hpp"
#include <functional>
#include <string>
#include <stdexcept>
#include <omp.h>

namespace SDL3Initializer {

    class ThreadSafeLogger {
    private:
        std::function<void(const std::string&)> logCallback;
        omp_lock_t logLock;

    public:
        ThreadSafeLogger(std::function<void(const std::string&)> callback) : logCallback(callback) {
            omp_init_lock(&logLock);
        }

        ~ThreadSafeLogger() {
            omp_destroy_lock(&logLock);
        }

        void log(const std::string& message) {
            omp_set_lock(&logLock);
            logCallback(message);
            omp_unset_lock(&logLock);
        }
    };

    void initImage(const ImageConfig& c, std::function<void(const std::string&)> logMessage) {
        (void)c;
        ThreadSafeLogger logger(logMessage);
        logger.log("Initializing SDL3_image (automatic, no explicit init required)");
    }

    void cleanupImage(std::function<void(const std::string&)> logMessage) {
        ThreadSafeLogger logger(logMessage);
        logger.log("Cleaning up SDL3_image (automatic, no explicit quit required)");
    }

    SDL_Texture* loadTexture(SDL_Renderer* renderer, const std::string& file, std::function<void(const std::string&)> logMessage) {
        ThreadSafeLogger logger(logMessage);
        logger.log("Loading texture from: " + file);

        SDL_Texture* texture = nullptr;
        #pragma omp critical
        {
            texture = IMG_LoadTexture(renderer, file.c_str());
            if (!texture) {
                std::string error = "IMG_LoadTexture failed: " + std::string(SDL_GetError());
                logger.log(error);
                throw std::runtime_error(error);
            }
        }
        logger.log("Texture loaded successfully");
        return texture;
    }

    void freeTexture(SDL_Texture* texture, std::function<void(const std::string&)> logMessage) {
        if (!texture) return;

        ThreadSafeLogger logger(logMessage);
        logger.log("Freeing texture");

        #pragma omp critical
        {
            SDL_DestroyTexture(texture);
            logger.log("Texture freed");
        }
    }

} // namespace SDL3Initializer