#ifndef SDL3_AUDIO_HPP
#define SDL3_AUDIO_HPP

#include <SDL3/SDL_audio.h>
#include <functional>
#include <string>
// Configures and manages audio streams and devices.
// AMOURANTH RTX Engine September 2025
// Zachary Geurts 2025
namespace SDL3Initializer {

    // Configuration for audio initialization
    struct AudioConfig {
        int frequency = 44100;
        SDL_AudioFormat format = SDL_AUDIO_S16LE;
        int channels = 2;
        std::function<void(Uint8*, int)> callback;
    };

    // Initializes audio device and stream
    void initAudio(const AudioConfig& c, SDL_AudioDeviceID& audioDevice, SDL_AudioStream*& audioStream,
                   std::function<void(const std::string&)> logMessage);

    // Gets audio device ID
    SDL_AudioDeviceID getAudioDevice(const SDL_AudioDeviceID& audioDevice);

}

#endif // SDL3_AUDIO_HPP