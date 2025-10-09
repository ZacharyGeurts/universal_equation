// AMOURANTH RTX Engine, October 2025 - Configures and manages audio streams and devices implementation.
// Thread-safe with C++20 features; no mutexes required.
// Dependencies: SDL3, C++20 standard library, logging.hpp.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#include "engine/SDL3/SDL3_audio.hpp"
#include "engine/logging.hpp"
#include <SDL3/SDL.h>
#include <stdexcept>
#include <vector>
#include <source_location>

namespace SDL3Audio {

void initAudio(const AudioConfig& c, SDL_AudioDeviceID& audioDevice, SDL_AudioStream*& audioStream) {
    LOG_INFO_CAT("Audio", "Initializing audio with frequency: {}, channels: {}, format: {:#x}", 
                 std::source_location::current(), c.frequency, c.channels, static_cast<uint16_t>(c.format));

    // Verify platform support
    std::string_view platform = SDL_GetPlatform();
    if (platform != "Linux" && platform != "Windows") {
        LOG_ERROR_CAT("Audio", "Unsupported platform for audio: {}", std::source_location::current(), platform);
        throw std::runtime_error(std::string("Unsupported platform for audio: ") + std::string(platform));
    }

    SDL_AudioSpec desired{};
    desired.freq = c.frequency;
    desired.format = c.format;
    desired.channels = c.channels;

    LOG_DEBUG_CAT("Audio", "Opening audio device", std::source_location::current());
    audioDevice = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired);
    if (audioDevice == 0) {
        LOG_ERROR_CAT("Audio", "SDL_OpenAudioDevice failed: {}", std::source_location::current(), SDL_GetError());
        throw std::runtime_error(std::string("SDL_OpenAudioDevice failed: ") + SDL_GetError());
    }

    SDL_AudioSpec obtained{};
    int obtainedSamples = 0;
    SDL_GetAudioDeviceFormat(audioDevice, &obtained, &obtainedSamples);
    const char* deviceName = SDL_GetAudioDeviceName(audioDevice);
    LOG_INFO_CAT("Audio", "Audio device opened: {}", std::source_location::current(), 
                 deviceName ? deviceName : "Unknown device");

    LOG_INFO_CAT("Audio", "Audio device status: {}", std::source_location::current(), 
                 SDL_AudioDevicePaused(audioDevice) ? "Paused" : "Active");

    LOG_INFO_CAT("Audio", "Audio device format: frequency={}, channels={}, format={:#x}, samples={}", 
                 std::source_location::current(), obtained.freq, static_cast<int>(obtained.channels), 
                 static_cast<uint16_t>(obtained.format), obtainedSamples);

    if (c.callback) {
        LOG_DEBUG_CAT("Audio", "Creating audio stream with desired format: frequency={}, channels={}, format={:#x}", 
                      std::source_location::current(), desired.freq, static_cast<int>(desired.channels), 
                      static_cast<uint16_t>(desired.format));
        
        audioStream = SDL_CreateAudioStream(&desired, &obtained);
        if (!audioStream) {
            LOG_ERROR_CAT("Audio", "SDL_CreateAudioStream failed: {}", std::source_location::current(), SDL_GetError());
            SDL_CloseAudioDevice(audioDevice);
            audioDevice = 0;
            throw std::runtime_error(std::string("SDL_CreateAudioStream failed: ") + SDL_GetError());
        }

        LOG_DEBUG_CAT("Audio", "Pausing audio device before binding stream", std::source_location::current());
        SDL_PauseAudioDevice(audioDevice);

        LOG_DEBUG_CAT("Audio", "Binding audio stream to device ID: {}", std::source_location::current(), audioDevice);
        if (SDL_BindAudioStream(audioDevice, audioStream) == 0) {
            LOG_ERROR_CAT("Audio", "SDL_BindAudioStream failed: {}", std::source_location::current(), SDL_GetError());
            if (audioStream) {
                SDL_DestroyAudioStream(audioStream);
                audioStream = nullptr;
            }
            SDL_CloseAudioDevice(audioDevice);
            audioDevice = 0;
            throw std::runtime_error(std::string("SDL_BindAudioStream failed: ") + SDL_GetError());
        }

        LOG_DEBUG_CAT("Audio", "Setting audio stream callback", std::source_location::current());
        SDL_SetAudioStreamPutCallback(audioStream, [](void* userdata, SDL_AudioStream* s, int n, int) {
            auto* callback = static_cast<std::function<void(Uint8*, int)>*>(userdata);
            std::vector<Uint8> buf(n);
            (*callback)(buf.data(), n);
            SDL_PutAudioStreamData(s, buf.data(), n);
        }, &const_cast<std::function<void(Uint8*, int)>&>(c.callback));
    }

    LOG_INFO_CAT("Audio", "Resuming audio device ID: {}", std::source_location::current(), audioDevice);
    SDL_ResumeAudioDevice(audioDevice);
}

SDL_AudioDeviceID getAudioDevice(const SDL_AudioDeviceID& audioDevice) {
    LOG_DEBUG_CAT("Audio", "Retrieving audio device ID: {}", std::source_location::current(), audioDevice);
    return audioDevice;
}

} // namespace SDL3Audio