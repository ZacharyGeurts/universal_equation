// Initializes SDL3 and Vulkan surface for AMOURANTH RTX Engine, October 2025
// Thread-safe with C++20 features; no mutexes required.
// Dependencies: SDL3, SDL3_ttf, Vulkan 1.3+, logging.hpp.
// Zachary Geurts 2025

#include "engine/SDL3_init.hpp"
#include <stdexcept>
#include <format>

namespace SDL3Initializer {

SDL3Initializer::SDL3Initializer(const Logging::Logger& logger)
    : m_window(nullptr, SDLWindowDeleter{}), m_instance(nullptr), m_surface(nullptr, VulkanSurfaceDeleter{nullptr}),
      m_audioDevice(0), m_audioStream(nullptr), m_fontManager(logger), m_inputManager(logger),
      logger_(logger), m_useVulkan(true), m_consoleOpen(false) {
    logger_.log(Logging::LogLevel::Info, "Constructing SDL3Initializer", std::source_location::current());
}

SDL3Initializer::~SDL3Initializer() {
    logger_.log(Logging::LogLevel::Info, "Destructing SDL3Initializer", std::source_location::current());
    cleanup();
}

void SDL3Initializer::initialize(const char* title, int w, int h, Uint32 flags,
                                 bool rt, const std::string& fontPath, bool enableValidation, bool preferNvidia) {
    logger_.log(Logging::LogLevel::Info, "Initializing SDL3Initializer with title={}, width={}, height={}",
                std::source_location::current(), title, w, h);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS) != 0) {
        logger_.log(Logging::LogLevel::Error, "SDL_Init failed: {}", std::source_location::current(), SDL_GetError());
        throw std::runtime_error(std::format("SDL_Init failed: {}", SDL_GetError()));
    }

    m_useVulkan = (flags & SDL_WINDOW_VULKAN) != 0;
    try {
        m_window = createWindow(title, w, h, flags, [this](const std::string& msg) {
            logger_.log(Logging::LogLevel::Info, "{}", std::source_location::current(), msg);
        });
        if (m_useVulkan) {
            initVulkan(m_window.get(), m_instance, m_surface, enableValidation, preferNvidia, rt, title);
        }
    } catch (const std::runtime_error& e) {
        if (m_useVulkan) {
            logger_.log(Logging::LogLevel::Warning, "Vulkan initialization failed: {}. Falling back to non-Vulkan rendering",
                        std::source_location::current(), e.what());
            m_surface.reset(nullptr);
            m_instance.reset(nullptr);
            m_useVulkan = false;
            flags &= ~SDL_WINDOW_VULKAN;
            m_window = createWindow(title, w, h, flags, [this](const std::string& msg) {
                logger_.log(Logging::LogLevel::Info, "{}", std::source_location::current(), msg);
            });
        } else {
            throw;
        }
    }

    logger_.log(Logging::LogLevel::Info, "Initializing audio", std::source_location::current());
    try {
        AudioConfig config{44100, SDL_AUDIO_S16LE, 8, nullptr};
        initAudio(config, m_audioDevice, m_audioStream, logger_);
    } catch (const std::runtime_error& e) {
        logger_.log(Logging::LogLevel::Warning, "Audio initialization failed: {}. Continuing without audio",
                    std::source_location::current(), e.what());
        m_audioDevice = 0;
        m_audioStream = nullptr;
    }

    if (!fontPath.empty()) {
        m_fontManager.initialize(fontPath);
    }

    logger_.log(Logging::LogLevel::Info, "Initializing input", std::source_location::current());
    m_inputManager.initialize([this](const std::string& msg) {
        logger_.log(Logging::LogLevel::Info, "{}", std::source_location::current(), msg);
    });
}

void SDL3Initializer::eventLoop(std::function<void()> render, ResizeCallback onResize, bool exitOnClose,
                                KeyboardCallback kb, MouseButtonCallback mb, MouseMotionCallback mm,
                                MouseWheelCallback mw, TextInputCallback ti, TouchCallback tc,
                                GamepadButtonCallback gb, GamepadAxisCallback ga, GamepadConnectCallback gc) {
    logger_.log(Logging::LogLevel::Info, "Starting event loop", std::source_location::current());
    m_inputManager.setCallbacks(kb, mb, mm, mw, ti, tc, gb, ga, gc, onResize);
    if (ti) {
        m_inputManager.enableTextInput(m_window.get(), true);
    }

    for (bool running = true; running;) {
        running = m_inputManager.pollEvents(m_window.get(), m_audioDevice, m_consoleOpen, exitOnClose);
        if (render && running) {
            render();
        }
    }

    if (ti) {
        m_inputManager.enableTextInput(m_window.get(), false);
    }
    logger_.log(Logging::LogLevel::Info, "Exiting event loop", std::source_location::current());
}

void SDL3Initializer::cleanup() {
    logger_.log(Logging::LogLevel::Info, "Starting cleanup", std::source_location::current());
    if (m_audioStream) {
        logger_.log(Logging::LogLevel::Debug, "Pausing audio device before cleanup", std::source_location::current());
        if (m_audioDevice) {
            SDL_PauseAudioDevice(m_audioDevice);
        }
        logger_.log(Logging::LogLevel::Info, "Destroying audio stream", std::source_location::current());
        SDL_DestroyAudioStream(m_audioStream);
        m_audioStream = nullptr;
    }
    if (m_audioDevice) {
        logger_.log(Logging::LogLevel::Info, "Closing audio device: ID {}", std::source_location::current(), m_audioDevice);
        SDL_CloseAudioDevice(m_audioDevice);
        m_audioDevice = 0;
    }
    if (m_surface) {
        logger_.log(Logging::LogLevel::Info, "Destroying Vulkan surface", std::source_location::current());
        m_surface.reset(nullptr);
    }
    if (m_instance) {
        logger_.log(Logging::LogLevel::Info, "Destroying Vulkan instance", std::source_location::current());
        m_instance.reset(nullptr);
    }
    if (m_window) {
        logger_.log(Logging::LogLevel::Info, "Destroying SDL window", std::source_location::current());
        m_window.reset(nullptr);
    }
    logger_.log(Logging::LogLevel::Info, "Quitting SDL", std::source_location::current());
    SDL_Quit();
    logger_.log(Logging::LogLevel::Info, "Cleanup completed", std::source_location::current());
}

void SDL3Initializer::exportLog(const std::string& filename) const {
    logger_.log(Logging::LogLevel::Info, "Exporting log to {}", std::source_location::current(), filename);
    m_fontManager.exportLog(filename + "_font");
    m_inputManager.exportLog(filename + "_input");
}

std::vector<std::string> SDL3Initializer::getVulkanExtensions() const {
    logger_.log(Logging::LogLevel::Debug, "Retrieving Vulkan extensions", std::source_location::current());
    return ::SDL3Initializer::getVulkanExtensions();
}

} // namespace SDL3Initializer