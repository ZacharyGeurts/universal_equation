#ifndef SDL3_AUDIO_HPP
#define SDL3_AUDIO_HPP

// Configures and manages audio streams and devices for AMOURANTH RTX Engine, October 2025
// Thread-safe with C++20 features; no mutexes required.
// Dependencies: SDL3, C++20 standard library, logging.hpp.
// Usage: Call initAudio with AudioConfig to set up audio device and stream.
// Zachary Geurts 2025

#include <SDL3/SDL_audio.h>
#include <functional>
#include <string>
#include <vector>
#include "engine/logging.hpp"
#include <format>

namespace SDL3Audio {

struct AudioConfig {
    int frequency = 44100;
    SDL_AudioFormat format = SDL_AUDIO_S16LE;
    int channels = 8; // 8-channel audio
    std::function<void(Uint8*, int)> callback;
};

void initAudio(const AudioConfig& c, SDL_AudioDeviceID& audioDevice, SDL_AudioStream*& audioStream, const Logging::Logger& logger);

SDL_AudioDeviceID getAudioDevice(const SDL_AudioDeviceID& audioDevice, const Logging::Logger& logger);

} // namespace SDL3Audio

// Custom formatter for SDL_AudioFormat
namespace std {
template<>
struct formatter<SDL_AudioFormat, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template<typename FormatContext>
    auto format(SDL_AudioFormat format, FormatContext& ctx) const {
        switch (format) {
            case SDL_AUDIO_U8: return format_to(ctx.out(), "SDL_AUDIO_U8");
            case SDL_AUDIO_S8: return format_to(ctx.out(), "SDL_AUDIO_S8");
            case SDL_AUDIO_S16LE: return format_to(ctx.out(), "SDL_AUDIO_S16LE");
            case SDL_AUDIO_S16BE: return format_to(ctx.out(), "SDL_AUDIO_S16BE");
            case SDL_AUDIO_S32LE: return format_to(ctx.out(), "SDL_AUDIO_S32LE");
            case SDL_AUDIO_S32BE: return format_to(ctx.out(), "SDL_AUDIO_S32BE");
            case SDL_AUDIO_F32LE: return format_to(ctx.out(), "SDL_AUDIO_F32LE");
            case SDL_AUDIO_F32BE: return format_to(ctx.out(), "SDL_AUDIO_F32BE");
            default: return format_to(ctx.out(), "Unknown SDL_AudioFormat({})", static_cast<int>(format));
        }
    }
};
} // namespace std

#endif // SDL3_AUDIO_HPP