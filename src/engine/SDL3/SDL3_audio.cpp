// AMOURANTH RTX Engine, October 2025 - Configures and manages audio streams and devices implementation.
// Thread-safe with C++20 features; no mutexes required.
// Dependencies: SDL3, C++20 standard library, logging.hpp.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#include "engine/SDL3/SDL3_audio.hpp"
#include <SDL3/SDL.h>
#include <stdexcept>
#include <vector>
#include <sstream>
#include <source_location>

namespace SDL3Audio {

void initAudio(const AudioConfig& c, SDL_AudioDeviceID& audioDevice, SDL_AudioStream*& audioStream, const Logging::Logger& logger) {
    std::stringstream ss;
    ss << "Initializing audio with frequency: " << c.frequency << ", channels: " << c.channels << ", format: 0x" << std::hex << c.format;
    logger.log(Logging::LogLevel::Info, ss.str(), std::source_location::current());

    // Verify platform support
    std::string_view platform = SDL_GetPlatform();
    if (platform != "Linux" && platform != "Windows") {
        std::stringstream error_ss;
        error_ss << "Unsupported platform for audio: " << platform;
        logger.log(Logging::LogLevel::Error, error_ss.str(), std::source_location::current());
        throw std::runtime_error(error_ss.str());
    }

    SDL_AudioSpec desired{};
    desired.freq = c.frequency;
    desired.format = c.format;
    desired.channels = c.channels;

    logger.log(Logging::LogLevel::Debug, "Opening audio device", std::source_location::current());
    audioDevice = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired);
    if (audioDevice == 0) {
        std::stringstream error_ss;
        error_ss << "SDL_OpenAudioDevice failed: " << SDL_GetError();
        logger.log(Logging::LogLevel::Error, error_ss.str(), std::source_location::current());
        throw std::runtime_error(error_ss.str());
    }

    SDL_AudioSpec obtained{};
    int obtainedSamples = 0;
    SDL_GetAudioDeviceFormat(audioDevice, &obtained, &obtainedSamples);
    const char* deviceName = SDL_GetAudioDeviceName(audioDevice);
    std::stringstream device_ss;
    device_ss << "Audio device opened: " << (deviceName ? deviceName : "Unknown device");
    logger.log(Logging::LogLevel::Info, device_ss.str(), std::source_location::current());

    std::stringstream status_ss;
    status_ss << "Audio device status: " << (SDL_AudioDevicePaused(audioDevice) ? "Paused" : "Active");
    logger.log(Logging::LogLevel::Info, status_ss.str(), std::source_location::current());

    std::stringstream format_ss;
    format_ss << "Audio device format: frequency=" << obtained.freq << ", channels=" << static_cast<int>(obtained.channels) 
              << ", format=0x" << std::hex << obtained.format << ", samples=" << obtainedSamples;
    logger.log(Logging::LogLevel::Info, format_ss.str(), std::source_location::current());

    if (c.callback) {
        std::stringstream callback_ss;
        callback_ss << "Creating audio stream with desired format: frequency=" << desired.freq 
                    << ", channels=" << static_cast<int>(desired.channels) << ", format=0x" << std::hex << desired.format;
        logger.log(Logging::LogLevel::Debug, callback_ss.str(), std::source_location::current());
        
        audioStream = SDL_CreateAudioStream(&desired, &obtained);
        if (!audioStream) {
            std::stringstream error_ss;
            error_ss << "SDL_CreateAudioStream failed: " << SDL_GetError();
            logger.log(Logging::LogLevel::Error, error_ss.str(), std::source_location::current());
            SDL_CloseAudioDevice(audioDevice);
            audioDevice = 0;
            throw std::runtime_error(error_ss.str());
        }

        logger.log(Logging::LogLevel::Debug, "Pausing audio device before binding stream", std::source_location::current());
        SDL_PauseAudioDevice(audioDevice);

        std::stringstream bind_ss;
        bind_ss << "Binding audio stream to device ID: " << audioDevice;
        logger.log(Logging::LogLevel::Debug, bind_ss.str(), std::source_location::current());
        if (SDL_BindAudioStream(audioDevice, audioStream) == 0) {
            std::stringstream error_ss;
            error_ss << "SDL_BindAudioStream failed: " << SDL_GetError();
            logger.log(Logging::LogLevel::Error, error_ss.str(), std::source_location::current());
            if (audioStream) {
                SDL_DestroyAudioStream(audioStream);
                audioStream = nullptr;
            }
            SDL_CloseAudioDevice(audioDevice);
            audioDevice = 0;
            throw std::runtime_error(error_ss.str());
        }

        logger.log(Logging::LogLevel::Debug, "Setting audio stream callback", std::source_location::current());
        SDL_SetAudioStreamPutCallback(audioStream, [](void* userdata, SDL_AudioStream* s, int n, int) {
            auto* callback = static_cast<std::function<void(Uint8*, int)>*>(userdata);
            std::vector<Uint8> buf(n);
            (*callback)(buf.data(), n);
            SDL_PutAudioStreamData(s, buf.data(), n);
        }, &const_cast<std::function<void(Uint8*, int)>&>(c.callback));
    }

    std::stringstream resume_ss;
    resume_ss << "Resuming audio device ID: " << audioDevice;
    logger.log(Logging::LogLevel::Info, resume_ss.str(), std::source_location::current());
    SDL_ResumeAudioDevice(audioDevice);
}

SDL_AudioDeviceID getAudioDevice(const SDL_AudioDeviceID& audioDevice, const Logging::Logger& logger) {
    std::stringstream ss;
    ss << "Retrieving audio device ID: " << audioDevice;
    logger.log(Logging::LogLevel::Debug, ss.str(), std::source_location::current());
    return audioDevice;
}

} // namespace SDL3Audio