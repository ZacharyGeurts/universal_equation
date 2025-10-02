// AMOURANTH RTX October 2025
// Zachary Geurts 2025

// src/engine/SDL3/SDL3_audio.cpp
#include "engine/SDL3/SDL3_audio.hpp"
#include "engine/SDL3_init.hpp"
#include <SDL3/SDL_audio.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <stdexcept>
#include <vector>
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

    class ThreadSafeAudioStream {
    private:
        SDL_AudioStream* stream;
        omp_lock_t streamLock;

    public:
        ThreadSafeAudioStream(SDL_AudioStream* s) : stream(s) {
            omp_init_lock(&streamLock);
        }

        ~ThreadSafeAudioStream() {
            omp_destroy_lock(&streamLock);
        }

        SDL_AudioStream* get() const { return stream; }

        void putData(const void* data, int len) {
            omp_set_lock(&streamLock);
            SDL_PutAudioStreamData(stream, data, len);
            omp_unset_lock(&streamLock);
        }

        void destroy() {
            omp_set_lock(&streamLock);
            if (stream) {
                SDL_DestroyAudioStream(stream);
                stream = nullptr;
            }
            omp_unset_lock(&streamLock);
        }
    };

    static ThreadSafeAudioStream* g_threadSafeStream = nullptr;

    void initAudio(const AudioConfig& c, SDL_AudioDeviceID& audioDevice, SDL_AudioStream*& audioStream,
                   std::function<void(const std::string&)> logMessage) {
        ThreadSafeLogger logger(logMessage);
        logger.log("Initializing SDL3 audio subsystem");

        #pragma omp critical
        {
            if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
                std::string error = "SDL_InitSubSystem(SDL_INIT_AUDIO) failed: " + std::string(SDL_GetError());
                logger.log(error);
                throw std::runtime_error(error);
            }
        }

        logger.log("Initializing SDL3_mixer with frequency: " + std::to_string(c.frequency) +
                   ", format: " + std::to_string(c.format) + ", channels: " + std::to_string(c.channels));

        logger.log("Configuring SDL3_mixer with " + std::to_string(c.mixerChannels) + " potential channels");

        logger.log("Initializing audio with frequency: " + std::to_string(c.frequency) +
                   ", channels: " + std::to_string(c.channels));
        SDL_AudioSpec desired{};
        desired.freq = c.frequency;
        desired.format = c.format;
        desired.channels = c.channels;

        logger.log("Opening audio device");
        #pragma omp critical
        {
            audioDevice = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired);
            if (audioDevice == 0) {
                std::string error = "SDL_OpenAudioDevice failed: " + std::string(SDL_GetError());
                logger.log(error);
                SDL_QuitSubSystem(SDL_INIT_AUDIO);
                throw std::runtime_error(error);
            }
        }

        SDL_AudioSpec obtained{};
        #pragma omp critical
        {
            SDL_GetAudioDeviceFormat(audioDevice, &obtained, nullptr);
        }
        const char* deviceName = SDL_GetAudioDeviceName(audioDevice);
        logger.log("Audio device opened: " + std::string(deviceName ? deviceName : "Unknown device"));
        logger.log("Audio device status: " + std::string(SDL_AudioDevicePaused(audioDevice) ? "Paused" : "Active"));
        logger.log("Audio device opened with obtained format: freq=" + std::to_string(obtained.freq) +
                   ", channels=" + std::to_string(obtained.channels) +
                   ", format=" + std::to_string(obtained.format));

        if (c.callback) {
            logger.log("Creating audio stream");
            #pragma omp critical
            {
                audioStream = SDL_CreateAudioStream(&desired, &obtained);
                if (!audioStream) {
                    std::string error = "SDL_CreateAudioStream failed: " + std::string(SDL_GetError());
                    logger.log(error);
                    SDL_CloseAudioDevice(audioDevice);
                    audioDevice = 0;
                    SDL_QuitSubSystem(SDL_INIT_AUDIO);
                    throw std::runtime_error(error);
                }
            }

            g_threadSafeStream = new ThreadSafeAudioStream(audioStream);
            logger.log("Pausing audio device before binding");
            #pragma omp critical
            {
                SDL_PauseAudioDevice(audioDevice);
            }

            logger.log("Binding audio stream to device");
            #pragma omp critical
            {
                if (SDL_BindAudioStream(audioDevice, audioStream) != 0) {
                    std::string error = "SDL_BindAudioStream failed: " + std::string(SDL_GetError());
                    logger.log(error);
                    g_threadSafeStream->destroy();
                    delete g_threadSafeStream;
                    g_threadSafeStream = nullptr;
                    audioStream = nullptr;
                    SDL_CloseAudioDevice(audioDevice);
                    audioDevice = 0;
                    SDL_QuitSubSystem(SDL_INIT_AUDIO);
                    throw std::runtime_error(error);
                }
            }

            logger.log("Setting audio stream callback (thread-safe)");
            SDL_SetAudioStreamPutCallback(audioStream, [](void* u, SDL_AudioStream* s, int n, int) {
                (void)s;
                auto* callback = static_cast<SDL3Initializer::AudioCallback*>(u);
                std::vector<Uint8> buf(n);
                #pragma omp critical
                {
                    (*callback)(buf.data(), n);
                }
                if (g_threadSafeStream) {
                    g_threadSafeStream->putData(buf.data(), n);
                }
            }, &const_cast<SDL3Initializer::AudioCallback&>(c.callback));
        }

        logger.log("Resuming audio device");
        #pragma omp critical
        {
            SDL_ResumeAudioDevice(audioDevice);
        }
    }

    SDL_AudioDeviceID getAudioDevice(const SDL_AudioDeviceID& audioDevice) {
        return audioDevice;
    }

    void cleanupAudio(SDL_AudioDeviceID& audioDevice, SDL_AudioStream*& audioStream,
                     std::function<void(const std::string&)> logMessage) {
        ThreadSafeLogger logger(logMessage);
        logger.log("Cleaning up audio resources");

        #pragma omp critical
        {
            if (audioStream) {
                if (g_threadSafeStream) {
                    g_threadSafeStream->destroy();
                    delete g_threadSafeStream;
                    g_threadSafeStream = nullptr;
                }
                audioStream = nullptr;
                logger.log("Audio stream destroyed");
            }
            if (audioDevice) {
                SDL_CloseAudioDevice(audioDevice);
                audioDevice = 0;
                logger.log("Audio device closed");
            }
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            logger.log("SDL audio subsystem quit");
        }
    }

} // namespace SDL3Initializer