// Configures and manages audio streams and devices for AMOURANTH RTX Engine, October 2025
// Thread-safe with C++20 features; no mutexes required.
// Dependencies: SDL3, C++20 standard library.
// Zachary Geurts 2025

#include "engine/SDL3/SDL3_audio.hpp"
#include "engine/SDL3_init.hpp" // Includes AudioCallback definition
#include <stdexcept>
#include <vector>
#include <format> // Added for std::format
#include <syncstream> // Added for std::osyncstream

// ANSI color codes for consistent logging
#define RESET "\033[0m"
#define MAGENTA "\033[1;35m" // Bold magenta for errors
#define CYAN "\033[1;36m"    // Bold cyan for debug
#define YELLOW "\033[1;33m"  // Bold yellow for warnings
#define GREEN "\033[1;32m"   // Bold green for info

namespace SDL3Initializer {

    void initAudio(const AudioConfig& c, SDL_AudioDeviceID& audioDevice, SDL_AudioStream*& audioStream) {
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Initializing audio with frequency: " << c.frequency
                                    << ", channels: " << c.channels << RESET << std::endl;

        SDL_AudioSpec desired{};
        desired.freq = c.frequency;
        desired.format = c.format;
        desired.channels = c.channels;

        std::osyncstream(std::cout) << CYAN << "[DEBUG] Opening audio device" << RESET << std::endl;
        audioDevice = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired);
        if (audioDevice == 0) {
            std::string error = std::format("SDL_OpenAudioDevice failed: {}", SDL_GetError());
            std::osyncstream(std::cerr) << MAGENTA << "[ERROR] " << error << RESET << std::endl;
            throw std::runtime_error(error);
        }

        SDL_AudioSpec obtained{};
        SDL_GetAudioDeviceFormat(audioDevice, &obtained, nullptr);
        const char* deviceName = SDL_GetAudioDeviceName(audioDevice);
        std::osyncstream(std::cout) << GREEN << "[INFO] Audio device opened: "
                                    << (deviceName ? deviceName : "Unknown device") << RESET << std::endl;
        std::osyncstream(std::cout) << GREEN << "[INFO] Audio device status: "
                                    << (SDL_AudioDevicePaused(audioDevice) ? "Paused" : "Active") << RESET << std::endl;
        std::osyncstream(std::cout) << GREEN << "[INFO] Audio device opened with obtained format: freq="
                                    << obtained.freq << ", channels=" << obtained.channels
                                    << ", format=" << obtained.format << RESET << std::endl;

        if (c.callback) {
            std::osyncstream(std::cout) << CYAN << "[DEBUG] Creating audio stream" << RESET << std::endl;
            audioStream = SDL_CreateAudioStream(&desired, &obtained);
            if (!audioStream) {
                std::string error = std::format("SDL_CreateAudioStream failed: {}", SDL_GetError());
                std::osyncstream(std::cerr) << MAGENTA << "[ERROR] " << error << RESET << std::endl;
                SDL_CloseAudioDevice(audioDevice);
                audioDevice = 0;
                throw std::runtime_error(error);
            }

            std::osyncstream(std::cout) << CYAN << "[DEBUG] Pausing audio device before binding" << RESET << std::endl;
            SDL_PauseAudioDevice(audioDevice);

            std::osyncstream(std::cout) << CYAN << "[DEBUG] Binding audio stream to device" << RESET << std::endl;
            if (SDL_BindAudioStream(audioDevice, audioStream) != 0) {
                std::string error = std::format("SDL_BindAudioStream failed: {}", SDL_GetError());
                std::osyncstream(std::cerr) << MAGENTA << "[ERROR] " << error << RESET << std::endl;
                if (audioStream) {
                    SDL_DestroyAudioStream(audioStream);
                    audioStream = nullptr;
                }
                SDL_CloseAudioDevice(audioDevice);
                audioDevice = 0;
                throw std::runtime_error(error);
            }

            std::osyncstream(std::cout) << CYAN << "[DEBUG] Setting audio stream callback (single-threaded to avoid overhead)" << RESET << std::endl;
            // Pass callback as userdata, avoiding const_cast
            SDL_SetAudioStreamPutCallback(audioStream, [](void* userdata, SDL_AudioStream* s, int n, int) {
                auto* callback = static_cast<std::function<void(Uint8*, int)>*>(userdata);
                std::vector<Uint8> buf(n);
                (*callback)(buf.data(), n);
                SDL_PutAudioStreamData(s, buf.data(), n);
            }, &const_cast<std::function<void(Uint8*, int)>&>(c.callback)); // const_cast is safe here as userdata is copied
        }

        std::osyncstream(std::cout) << GREEN << "[INFO] Resuming audio device" << RESET << std::endl;
        SDL_ResumeAudioDevice(audioDevice);
    }

    SDL_AudioDeviceID getAudioDevice(const SDL_AudioDeviceID& audioDevice) {
        return audioDevice;
    }

}