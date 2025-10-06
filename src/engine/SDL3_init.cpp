// AMOURANTH RTX Engine, October 2025 - SDL3Initializer for SDL3 and Vulkan integration.
// Dependencies: SDL3, SDL3_ttf, Vulkan 1.3+, C++20 standard library.
// Zachary Geurts 2025

#include "engine/SDL3_init.hpp"
#include <stdexcept>
#include <format>
#include <syncstream>
#include <fstream>
#include <sstream>

// ANSI color codes for consistent logging
#define RESET "\033[0m"
#define MAGENTA "\033[1;35m" // Bold magenta for errors
#define CYAN "\033[1;36m"    // Bold cyan for debug
#define YELLOW "\033[1;33m"  // Bold yellow for warnings
#define GREEN "\033[1;32m"   // Bold green for info

namespace SDL3Initializer {

SDL3Initializer::SDL3Initializer() : m_instance(nullptr), m_surface(nullptr, VulkanSurfaceDeleter{nullptr}), logFile("sdl3_initializer.log"), m_useVulkan(true) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Constructing SDL3Initializer" << RESET << std::endl;
    logMessage("Constructing SDL3Initializer");
}

SDL3Initializer::~SDL3Initializer() {
    std::osyncstream(std::cout) << GREEN << "[INFO] Destructing SDL3Initializer" << RESET << std::endl;
    logMessage("Destructing SDL3Initializer");
    cleanup();
    if (logFile.is_open()) {
        logFile.close();
    }
}

void SDL3Initializer::initialize(const char* title, int w, int h, Uint32 flags,
                                 bool rt, const std::string& fontPath, bool enableValidation, bool preferNvidia) {
    auto msg = std::format("Initializing SDL3Initializer with title: {}, width: {}, height: {}", title, w, h);
    std::osyncstream(std::cout) << GREEN << "[INFO] " << msg << RESET << std::endl;
    logMessage(msg);

    auto log = [this](const std::string& message) {
        std::osyncstream(std::cout) << GREEN << "[INFO] " << message << RESET << std::endl;
        logMessage(message);
    };

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS) != 0) {
        auto error = std::format("SDL_Init failed: {}", SDL_GetError());
        std::osyncstream(std::cout) << MAGENTA << "[ERROR] " << error << RESET << std::endl;
        logMessage(error);
        throw std::runtime_error(error);
    }

    m_useVulkan = (flags & SDL_WINDOW_VULKAN) != 0;
    try {
        m_window = createWindow(title, w, h, flags, log);
        if (m_useVulkan) {
            initVulkan(m_window.get(), m_instance, m_surface, enableValidation, preferNvidia, rt, title);
        }
    } catch (const std::runtime_error& e) {
        if (m_useVulkan) {
            auto msg = std::format("Vulkan initialization failed: {}. Falling back to non-Vulkan rendering", e.what());
            std::osyncstream(std::cout) << YELLOW << "[WARNING] " << msg << RESET << std::endl;
            logMessage(msg);
            m_surface.reset(nullptr);
            m_instance.reset(nullptr);
            m_useVulkan = false;
            flags &= ~SDL_WINDOW_VULKAN;
            m_window = createWindow(title, w, h, flags, log);
        } else {
            throw;
        }
    }

    std::osyncstream(std::cout) << GREEN << "[INFO] Initializing audio" << RESET << std::endl;
    logMessage("Initializing audio");
    try {
        AudioConfig config{44100, SDL_AUDIO_S16LE, 8, nullptr};
        initAudio(config, m_audioDevice, m_audioStream);
    } catch (const std::runtime_error& e) {
        auto msg = std::format("Audio initialization failed: {}. Continuing without audio", e.what());
        std::osyncstream(std::cout) << YELLOW << "[WARNING] " << msg << RESET << std::endl;
        logMessage(msg);
        m_audioDevice = 0;
        m_audioStream = nullptr;
    }

    if (!fontPath.empty()) {
        m_fontManager.initialize(fontPath);
    }

    std::osyncstream(std::cout) << GREEN << "[INFO] Initializing input" << RESET << std::endl;
    logMessage("Initializing input");
    m_inputManager.initialize(nullptr);
}

void SDL3Initializer::eventLoop(std::function<void()> render, ResizeCallback onResize, bool exitOnClose,
                                KeyboardCallback kb, MouseButtonCallback mb, MouseMotionCallback mm,
                                MouseWheelCallback mw, TextInputCallback ti, TouchCallback tc,
                                GamepadButtonCallback gb, GamepadAxisCallback ga, GamepadConnectCallback gc) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Starting event loop" << RESET << std::endl;
    logMessage("Starting event loop");
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
    std::osyncstream(std::cout) << GREEN << "[INFO] Exiting event loop" << RESET << std::endl;
    logMessage("Exiting event loop");
}

void SDL3Initializer::cleanup() {
    std::osyncstream(std::cout) << GREEN << "[INFO] Starting cleanup" << RESET << std::endl;
    logMessage("Starting cleanup");
    if (m_audioStream) {
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Pausing audio device before cleanup" << RESET << std::endl;
        logMessage("Pausing audio device before cleanup");
        if (m_audioDevice) {
            SDL_PauseAudioDevice(m_audioDevice);
        }
        std::osyncstream(std::cout) << GREEN << "[INFO] Destroying audio stream" << RESET << std::endl;
        logMessage("Destroying audio stream");
        SDL_DestroyAudioStream(m_audioStream);
        m_audioStream = nullptr;
    }
    if (m_audioDevice) {
        auto msg = std::format("Closing audio device: ID {}", m_audioDevice);
        std::osyncstream(std::cout) << GREEN << "[INFO] " << msg << RESET << std::endl;
        logMessage(msg);
        SDL_CloseAudioDevice(m_audioDevice);
        m_audioDevice = 0;
    }
    if (m_surface) {
        std::osyncstream(std::cout) << GREEN << "[INFO] Destroying Vulkan surface" << RESET << std::endl;
        logMessage("Destroying Vulkan surface");
        m_surface.reset(nullptr);
    }
    if (m_instance) {
        std::osyncstream(std::cout) << GREEN << "[INFO] Destroying Vulkan instance" << RESET << std::endl;
        logMessage("Destroying Vulkan instance");
        m_instance.reset(nullptr);
    }
    if (m_window) {
        std::osyncstream(std::cout) << GREEN << "[INFO] Destroying SDL window" << RESET << std::endl;
        logMessage("Destroying SDL window");
        m_window.reset(nullptr);
    }
    std::osyncstream(std::cout) << GREEN << "[INFO] Quitting SDL" << RESET << std::endl;
    logMessage("Quitting SDL");
    SDL_Quit();
    std::osyncstream(std::cout) << GREEN << "[INFO] Cleanup completed" << RESET << std::endl;
    logMessage("Cleanup completed");
}

void SDL3Initializer::exportLog(const std::string& filename) const {
    auto msg = std::format("Exporting log to {}", filename);
    std::osyncstream(std::cout) << GREEN << "[INFO] " << msg << RESET << std::endl;
    logMessage(msg);
    std::ofstream out(filename, std::ios::app);
    if (out.is_open()) {
        out << logStream.str();
        out.close();
        std::osyncstream(std::cout) << GREEN << "[INFO] Exported log to " << filename << RESET << std::endl;
        logMessage("Exported log to " + filename);
    } else {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to export log to " << filename << RESET << std::endl;
        logMessage("Failed to export log to " + filename);
    }
    m_fontManager.exportLog(filename + "_font");
    m_inputManager.exportLog(filename + "_input");
}

std::vector<std::string> SDL3Initializer::getVulkanExtensions() const {
    return ::SDL3Initializer::getVulkanExtensions();
}

void SDL3Initializer::logMessage(const std::string& message) const {
    auto formattedMessage = std::format("[{}ms] {}", SDL_GetTicks(), message);
    if (logFile.is_open()) {
        logFile << formattedMessage << "\n";
        logFile.flush();
    }
    logStream << formattedMessage << "\n";
}

} // namespace SDL3Initializer