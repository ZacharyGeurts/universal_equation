// Configures and manages audio streams and devices for AMOURANTH RTX Engine, October 2025
// Thread-safe with C++20 features; no mutexes required.
// Dependencies: SDL3, C++20 standard library, logging.hpp.
// Zachary Geurts 2025

#include "engine/SDL3/SDL3_audio.hpp"
#include <stdexcept>
#include <vector>
#include <format>

namespace SDL3Initializer {

void initAudio(const AudioConfig& c, SDL_AudioDeviceID& audioDevice, SDL_AudioStream*& audioStream, const Logging::Logger& logger) {
    logger.log(Logging::LogLevel::Debug, "Initializing audio with frequency: {}, channels: {}, format: {}",
               std::source_location::current(), c.frequency, c.channels, c.format);

    SDL_AudioSpec desired{};
    desired.freq = c.frequency;
    desired.format = c.format;
    desired.channels = c.channels;

    logger.log(Logging::LogLevel::Debug, "Opening audio device", std::source_location::current());
    audioDevice = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired);
    if (audioDevice == 0) {
        logger.log(Logging::LogLevel::Error, "SDL_OpenAudioDevice failed: {}", std::source_location::current(), SDL_GetError());
        throw std::runtime_error(std::format("SDL_OpenAudioDevice failed: {}", SDL_GetError()));
    }

    SDL_AudioSpec obtained{};
    SDL_GetAudioDeviceFormat(audioDevice, &obtained, nullptr);
    const char* deviceName = SDL_GetAudioDeviceName(audioDevice);
    logger.log(Logging::LogLevel::Info, "Audio device opened: {}", std::source_location::current(),
               deviceName ? deviceName : "Unknown device");
    logger.log(Logging::LogLevel::Info, "Audio device status: {}", std::source_location::current(),
               SDL_AudioDevicePaused(audioDevice) ? "Paused" : "Active");
    logger.log(Logging::LogLevel::Info, "Audio device opened with obtained format: freq={}, channels={}, format={}",
               std::source_location::current(), obtained.freq, obtained.channels, obtained.format);

    if (c.callback) {
        logger.log(Logging::LogLevel::Debug, "Creating audio stream", std::source_location::current());
        audioStream = SDL_CreateAudioStream(&desired, &obtained);
        if (!audioStream) {
            logger.log(Logging::LogLevel::Error, "SDL_CreateAudioStream failed: {}", std::source_location::current(), SDL_GetError());
            SDL_CloseAudioDevice(audioDevice);
            audioDevice = 0;
            throw std::runtime_error(std::format("SDL_CreateAudioStream failed: {}", SDL_GetError()));
        }

        logger.log(Logging::LogLevel::Debug, "Pausing audio device before binding", std::source_location::current());
        SDL_PauseAudioDevice(audioDevice);

        logger.log(Logging::LogLevel::Debug, "Binding audio stream to device", std::source_location::current());
        if (SDL_BindAudioStream(audioDevice, audioStream) != 0) {
            logger.log(Logging::LogLevel::Error, "SDL_BindAudioStream failed: {}", std::source_location::current(), SDL_GetError());
            if (audioStream) {
                SDL_DestroyAudioStream(audioStream);
                audioStream = nullptr;
            }
            SDL_CloseAudioDevice(audioDevice);
            audioDevice = 0;
            throw std::runtime_error(std::format("SDL_BindAudioStream failed: {}", SDL_GetError()));
        }

        logger.log(Logging::LogLevel::Debug, "Setting audio stream callback (single-threaded to avoid overhead)", std::source_location::current());
        SDL_SetAudioStreamPutCallback(audioStream, [](void* userdata, SDL_AudioStream* s, int n, int) {
            auto* callback = static_cast<std::function<void(Uint8*, int)>*>(userdata);
            std::vector<Uint8> buf(n);
            (*callback)(buf.data(), n);
            SDL_PutAudioStreamData(s, buf.data(), n);
        }, &const_cast<std::function<void(Uint8*, int)>&>(c.callback));
    }

    logger.log(Logging::LogLevel::Info, "Resuming audio device", std::source_location::current());
    SDL_ResumeAudioDevice(audioDevice);
}

SDL_AudioDeviceID getAudioDevice(const SDL_AudioDeviceID& audioDevice, const Logging::Logger& logger) {
    logger.log(Logging::LogLevel::Debug, "Retrieving audio device ID", std::source_location::current());
    return audioDevice;
}

} // namespace SDL3Initializer