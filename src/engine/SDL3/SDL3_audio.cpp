#include "engine/SDL3/SDL3_audio.hpp"
#include "engine/SDL3_init.hpp" // Includes AudioCallback definition
#include <stdexcept>
#include <vector>
// AMOURANTH RTX Engine September 2025
// Zachary Geurts 2025
namespace SDL3Initializer {

    void initAudio(const AudioConfig& c, SDL_AudioDeviceID& audioDevice, SDL_AudioStream*& audioStream,
                   std::function<void(const std::string&)> logMessage) {
        logMessage("Initializing audio with frequency: " + std::to_string(c.frequency) + ", channels: " + std::to_string(c.channels));
        SDL_AudioSpec desired{};
        desired.freq = c.frequency;
        desired.format = c.format;
        desired.channels = c.channels;

        logMessage("Opening audio device");
        audioDevice = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired);
        if (audioDevice == 0) {
            std::string error = "SDL_OpenAudioDevice failed: " + std::string(SDL_GetError());
            logMessage(error);
            throw std::runtime_error(error);
        }

        SDL_AudioSpec obtained{};
        SDL_GetAudioDeviceFormat(audioDevice, &obtained, nullptr);
        const char* deviceName = SDL_GetAudioDeviceName(audioDevice);
        logMessage("Audio device opened: " + std::string(deviceName ? deviceName : "Unknown device"));
        logMessage("Audio device status: " + std::string(SDL_AudioDevicePaused(audioDevice) ? "Paused" : "Active"));
        logMessage("Audio device opened with obtained format: freq=" + std::to_string(obtained.freq) + 
                   ", channels=" + std::to_string(obtained.channels) + 
                   ", format=" + std::to_string(obtained.format));

        if (c.callback) {
            logMessage("Creating audio stream");
            audioStream = SDL_CreateAudioStream(&desired, &obtained);
            if (!audioStream) {
                std::string error = "SDL_CreateAudioStream failed: " + std::string(SDL_GetError());
                logMessage(error);
                SDL_CloseAudioDevice(audioDevice);
                audioDevice = 0;
                throw std::runtime_error(error);
            }

            logMessage("Pausing audio device before binding");
            SDL_PauseAudioDevice(audioDevice);

            logMessage("Binding audio stream to device");
            if (SDL_BindAudioStream(audioDevice, audioStream) != 0) {
                std::string error = "SDL_BindAudioStream failed: " + std::string(SDL_GetError());
                logMessage(error);
                if (audioStream) {
                    SDL_DestroyAudioStream(audioStream);
                    audioStream = nullptr;
                }
                SDL_CloseAudioDevice(audioDevice);
                audioDevice = 0;
                throw std::runtime_error(error);
            }

            logMessage("Setting audio stream callback (single-threaded to avoid overhead)");
            SDL_SetAudioStreamPutCallback(audioStream, [](void* u, SDL_AudioStream* s, int n, int) {
                auto* callback = static_cast<SDL3Initializer::SDL3Initializer::AudioCallback*>(u);
                std::vector<Uint8> buf(n);
                (*callback)(buf.data(), n);
                SDL_PutAudioStreamData(s, buf.data(), n);
            }, &const_cast<SDL3Initializer::SDL3Initializer::AudioCallback&>(c.callback));
        }

        logMessage("Resuming audio device");
        SDL_ResumeAudioDevice(audioDevice);
    }

    SDL_AudioDeviceID getAudioDevice(const SDL_AudioDeviceID& audioDevice) {
        return audioDevice;
    }

}