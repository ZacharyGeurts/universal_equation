// AMOURANTH RTX Engine, October 2025 - Configures and manages audio streams and devices implementation.
// Thread-safe with C++20 features; no mutexes required.
// Dependencies: SDL3, C++20 standard library, logging.hpp.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#include <SDL3/SDL.h>
#include "engine/SDL3/SDL3_audio.hpp"
#include <stdexcept>
#include <vector>
#include <format>

namespace SDL3Audio {

void initAudio(const AudioConfig& c, SDL_AudioDeviceID& audioDevice, SDL_AudioStream*& audioStream, const Logging::Logger& logger) {
    logger.log(Logging::LogLevel::Info, "Initializing audio with frequency: {}, channels: {}, format: {:#x}",
               std::source_location::current(), c.frequency, c.channels, c.format);

    // Verify platform support
    std::string_view platform = SDL_GetPlatform();
    if (platform != "Linux" && platform != "Windows") {
        std::string error = std::format("Unsupported platform for audio: {}", platform);
        logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }

    SDL_AudioSpec desired{};
    desired.freq = c.frequency;
    desired.format = c.format;
    desired.channels = c.channels;

    logger.log(Logging::LogLevel::Debug, "Opening audio device", std::source_location::current());
    audioDevice = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired);
    if (audioDevice == 0) {
        std::string error = std::format("SDL_OpenAudioDevice failed: {}", SDL_GetError());
        logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }

    SDL_AudioSpec obtained{};
    int obtainedSamples = 0;
    SDL_GetAudioDeviceFormat(audioDevice, &obtained, &obtainedSamples);
    const char* deviceName = SDL_GetAudioDeviceName(audioDevice);
    logger.log(Logging::LogLevel::Info, "Audio device opened: {}", std::source_location::current(),
               deviceName ? deviceName : "Unknown device");
    logger.log(Logging::LogLevel::Info, "Audio device status: {}", std::source_location::current(),
               SDL_AudioDevicePaused(audioDevice) ? "Paused" : "Active");
    logger.log(Logging::LogLevel::Info, "Audio device format: frequency={}, channels={}, format={:#x}, samples={}",
               std::source_location::current(), obtained.freq, obtained.channels, obtained.format, obtainedSamples);

    if (c.callback) {
        logger.log(Logging::LogLevel::Debug, "Creating audio stream with desired format: frequency={}, channels={}, format={:#x}",
                   std::source_location::current(), desired.freq, desired.channels, desired.format);
        audioStream = SDL_CreateAudioStream(&desired, &obtained);
        if (!audioStream) {
            std::string error = std::format("SDL_CreateAudioStream failed: {}", SDL_GetError());
            logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
            SDL_CloseAudioDevice(audioDevice);
            audioDevice = 0;
            throw std::runtime_error(error);
        }

        logger.log(Logging::LogLevel::Debug, "Pausing audio device before binding stream", std::source_location::current());
        SDL_PauseAudioDevice(audioDevice);

        logger.log(Logging::LogLevel::Debug, "Binding audio stream to device ID: {}", std::source_location::current(), audioDevice);
        if (SDL_BindAudioStream(audioDevice, audioStream) == 0) {
            std::string error = std::format("SDL_BindAudioStream failed: {}", SDL_GetError());
            logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
            if (audioStream) {
                SDL_DestroyAudioStream(audioStream);
                audioStream = nullptr;
            }
            SDL_CloseAudioDevice(audioDevice);
            audioDevice = 0;
            throw std::runtime_error(error);
        }

        logger.log(Logging::LogLevel::Debug, "Setting audio stream callback", std::source_location::current());
        SDL_SetAudioStreamPutCallback(audioStream, [](void* userdata, SDL_AudioStream* s, int n, int) {
            auto* callback = static_cast<std::function<void(Uint8*, int)>*>(userdata);
            std::vector<Uint8> buf(n);
            (*callback)(buf.data(), n);
            SDL_PutAudioStreamData(s, buf.data(), n);
        }, &const_cast<std::function<void(Uint8*, int)>&>(c.callback));
    }

    logger.log(Logging::LogLevel::Info, "Resuming audio device ID: {}", std::source_location::current(), audioDevice);
    SDL_ResumeAudioDevice(audioDevice);
}

SDL_AudioDeviceID getAudioDevice(const SDL_AudioDeviceID& audioDevice, const Logging::Logger& logger) {
    logger.log(Logging::LogLevel::Debug, "Retrieving audio device ID: {}", std::source_location::current(), audioDevice);
    return audioDevice;
}

} // namespace SDL3Audio