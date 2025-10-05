// src/engine/SDL3/SDL3_audio.cpp
#include "engine/SDL3/SDL3_audio.hpp"
#include "engine/SDL3_init.hpp"
#include <SDL3/SDL_audio.h>
#include <stdexcept>
#include <vector>
#include <mutex>
#include <format>

// Custom formatter for SDL_AudioFormat
template <>
struct std::formatter<SDL_AudioFormat, char> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(SDL_AudioFormat format, FormatContext& ctx) const {
        return std::format_to(ctx.out(), "{}", static_cast<uint16_t>(format));
    }
};

namespace SDL3Initializer {

    class ThreadSafeLogger {
    private:
        std::function<void(const std::string&)> logCallback;
        std::mutex logMutex;

    public:
        ThreadSafeLogger(std::function<void(const std::string&)> callback) : logCallback(callback) {}

        void log(const std::string& message) {
            std::lock_guard<std::mutex> lock(logMutex);
            logCallback(message);
        }
    };

    class ThreadSafeAudioStream {
    private:
        SDL_AudioStream* stream;
        std::mutex streamMutex;

    public:
        ThreadSafeAudioStream(SDL_AudioStream* s) : stream(s) {}

        ~ThreadSafeAudioStream() {
            std::lock_guard<std::mutex> lock(streamMutex);
            if (stream) {
                SDL_DestroyAudioStream(stream);
                stream = nullptr;
            }
        }

        SDL_AudioStream* get() const { return stream; }

        void putData(const void* data, int len) {
            std::lock_guard<std::mutex> lock(streamMutex);
            if (stream) {
                SDL_PutAudioStreamData(stream, data, len);
            }
        }
    };

    static ThreadSafeAudioStream* g_threadSafeStream = nullptr;

    void initAudio(const AudioConfig& c, SDL_AudioDeviceID& audioDevice, SDL_AudioStream*& audioStream,
                   std::function<void(const std::string&)> logMessage) {
        ThreadSafeLogger logger(logMessage);
        logger.log("Initializing SDL3 audio subsystem");

        if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            std::string error = std::format("SDL_InitSubSystem(SDL_INIT_AUDIO) failed: {}", SDL_GetError());
            logger.log(error);
            throw std::runtime_error(error);
        }

        logger.log(std::format("Initializing audio with frequency: {}, format: {}, channels: {}",
                               c.frequency, c.format, c.channels));
        SDL_AudioSpec desired{};
        desired.freq = c.frequency;
        desired.format = c.format;
        desired.channels = c.channels;

        logger.log("Opening audio device");
        audioDevice = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired);
        if (audioDevice == 0) {
            std::string error = std::format("SDL_OpenAudioDevice failed: {}", SDL_GetError());
            logger.log(error);
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            throw std::runtime_error(error);
        }

        SDL_AudioSpec obtained{};
        SDL_GetAudioDeviceFormat(audioDevice, &obtained, nullptr);
        const char* deviceName = SDL_GetAudioDeviceName(audioDevice);
        logger.log(std::format("Audio device opened: {}", deviceName ? deviceName : "Unknown device"));
        logger.log(std::format("Audio device status: {}", SDL_AudioDevicePaused(audioDevice) ? "Paused" : "Active"));
        logger.log(std::format("Audio device opened with obtained format: freq={}, channels={}, format={}",
                               obtained.freq, obtained.channels, obtained.format));

        if (c.callback) {
            logger.log("Creating audio stream");
            audioStream = SDL_CreateAudioStream(&desired, &obtained);
            if (!audioStream) {
                std::string error = std::format("SDL_CreateAudioStream failed: {}", SDL_GetError());
                logger.log(error);
                SDL_CloseAudioDevice(audioDevice);
                audioDevice = 0;
                SDL_QuitSubSystem(SDL_INIT_AUDIO);
                throw std::runtime_error(error);
            }

            g_threadSafeStream = new ThreadSafeAudioStream(audioStream);
            logger.log("Pausing audio device before binding");
            SDL_PauseAudioDevice(audioDevice);

            logger.log("Binding audio stream to device");
            if (SDL_BindAudioStream(audioDevice, audioStream) != 0) {
                std::string error = std::format("SDL_BindAudioStream failed: {}", SDL_GetError());
                logger.log(error);
                delete g_threadSafeStream;
                g_threadSafeStream = nullptr;
                audioStream = nullptr;
                SDL_CloseAudioDevice(audioDevice);
                audioDevice = 0;
                SDL_QuitSubSystem(SDL_INIT_AUDIO);
                throw std::runtime_error(error);
            }

            logger.log("Setting audio stream callback (thread-safe)");
            SDL_SetAudioStreamPutCallback(audioStream, [](void* u, SDL_AudioStream*, int n, int) {
                auto* callback = static_cast<SDL3Initializer::AudioCallback*>(u);
                std::vector<Uint8> buf(n);
                (*callback)(buf.data(), n);
                if (g_threadSafeStream && g_threadSafeStream->get()) {
                    g_threadSafeStream->putData(buf.data(), n);
                }
            }, &const_cast<SDL3Initializer::AudioCallback&>(c.callback));
        }

        logger.log("Resuming audio device");
        SDL_ResumeAudioDevice(audioDevice);
    }

    SDL_AudioDeviceID getAudioDevice(const SDL_AudioDeviceID& audioDevice) {
        return audioDevice;
    }

    void cleanupAudio(SDL_AudioDeviceID& audioDevice, SDL_AudioStream*& audioStream,
                     std::function<void(const std::string&)> logMessage) {
        ThreadSafeLogger logger(logMessage);
        logger.log("Cleaning up audio resources");

        if (audioStream) {
            if (g_threadSafeStream) {
                delete g_threadSafeStream; // Destructor handles SDL_DestroyAudioStream
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

} // namespace SDL3Initializer