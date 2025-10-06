#ifndef SDL3_AUDIO_HPP
#define SDL3_AUDIO_HPP

// Configures and manages audio streams and devices for AMOURANTH RTX Engine, October 2025
// Thread-safe with C++20 features; no mutexes required.
// Dependencies: SDL3, C++20 standard library.
// Usage: Call initAudio with AudioConfig to set up audio device and stream.
// Zachary Geurts 2025

#include <SDL3/SDL_audio.h>
#include <functional>
#include <string>
#include <syncstream> // Added for std::osyncstream
#include <vector>

// ANSI color codes for consistent logging
#define RESET "\033[0m"
#define MAGENTA "\033[1;35m" // Bold magenta for errors
#define CYAN "\033[1;36m"    // Bold cyan for debug
#define YELLOW "\033[1;33m"  // Bold yellow for warnings
#define GREEN "\033[1;32m"   // Bold green for info

namespace SDL3Initializer {

    // Configuration for audio initialization
    struct AudioConfig {
        int frequency = 44100;
        SDL_AudioFormat format = SDL_AUDIO_S16LE;
        int channels = 2;
        std::function<void(Uint8*, int)> callback;
    };

    // Initializes audio device and stream
    void initAudio(const AudioConfig& c, SDL_AudioDeviceID& audioDevice, SDL_AudioStream*& audioStream);

    // Gets audio device ID
    SDL_AudioDeviceID getAudioDevice(const SDL_AudioDeviceID& audioDevice);

}

#endif // SDL3_AUDIO_HPP