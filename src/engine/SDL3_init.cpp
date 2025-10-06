#include "engine/SDL3_init.hpp"
#include <format>
#include <algorithm>

namespace SDL3Initializer {

SDL3Initializer::SDL3Initializer() : logFile("sdl3_initializer.log"), m_useVulkan(true) {
    logMessage("Constructing SDL3Initializer");
}

SDL3Initializer::~SDL3Initializer() {
    logMessage("Destructing SDL3Initializer");
    cleanup();
    if (logFile.is_open()) {
        logFile.close();
    }
}

void SDL3Initializer::initialize(const char* title, int w, int h, Uint32 flags,
                                 bool rt, const std::string& fontPath, bool enableValidation, bool preferNvidia) {
    logMessage(std::format("Initializing SDL3Initializer with title: {}, width: {}, height: {}", title, w, h));
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS) == 0) {
        std::string error = std::format("SDL_Init failed: {}", SDL_GetError());
        logMessage(error);
        throw std::runtime_error(error);
    }

    m_useVulkan = (flags & SDL_WINDOW_VULKAN) != 0;
    try {
        m_window = createWindow(title, w, h, flags, [this](const std::string& msg) { logMessage(msg); });
        if (m_useVulkan) {
            initVulkan(m_window.get(), m_instance, m_surface, enableValidation, preferNvidia, rt, title,
                       [this](const std::string& msg) { logMessage(msg); });
        }
    } catch (const std::runtime_error& e) {
        if (m_useVulkan) {
            logMessage(std::format("Vulkan initialization failed: {}. Falling back to non-Vulkan rendering", e.what()));
            m_surface.reset();
            m_instance.reset();
            m_useVulkan = false;
            flags &= ~SDL_WINDOW_VULKAN;
            m_window = createWindow(title, w, h, flags, [this](const std::string& msg) { logMessage(msg); });
        } else {
            throw;
        }
    }

    logMessage("Initializing audio");
    try {
        AudioConfig config{44100, SDL_AUDIO_S16LE, 8, nullptr};
        initAudio(config, m_audioDevice, m_audioStream, [this](const std::string& msg) { logMessage(msg); });
    } catch (const std::runtime_error& e) {
        logMessage(std::format("Audio initialization failed: {}. Continuing without audio", e.what()));
        m_audioDevice = 0;
        m_audioStream = nullptr;
    }

    if (!fontPath.empty()) {
        m_fontManager.initialize(fontPath);
    }

    logMessage("Initializing input");
    m_inputManager.initialize(nullptr);
}

void SDL3Initializer::eventLoop(std::function<void()> render, ResizeCallback onResize, bool exitOnClose,
                                KeyboardCallback kb, MouseButtonCallback mb, MouseMotionCallback mm,
                                MouseWheelCallback mw, TextInputCallback ti, TouchCallback tc,
                                GamepadButtonCallback gb, GamepadAxisCallback ga, GamepadConnectCallback gc) {
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
    logMessage("Exiting event loop");
}

void SDL3Initializer::cleanup() {
    logMessage("Starting cleanup");
    if (m_audioStream) {
        logMessage("Pausing audio device before cleanup");
        if (m_audioDevice) {
            SDL_PauseAudioDevice(m_audioDevice);
        }
        logMessage("Destroying audio stream");
        SDL_DestroyAudioStream(m_audioStream);
        m_audioStream = nullptr;
    }
    if (m_audioDevice) {
        logMessage(std::format("Closing audio device: ID {}", m_audioDevice));
        SDL_CloseAudioDevice(m_audioDevice);
        m_audioDevice = 0;
    }
    if (m_surface) {
        logMessage("Destroying Vulkan surface");
        m_surface.reset();
    }
    if (m_instance) {
        logMessage("Destroying Vulkan instance");
        m_instance.reset();
    }
    if (m_window) {
        logMessage("Destroying SDL window");
        m_window.reset();
    }
    logMessage("Quitting SDL");
    SDL_Quit();
    logMessage("Cleanup completed");
}

void SDL3Initializer::exportLog(const std::string& filename) const {
    logMessage(std::format("Exporting log to {}", filename));
    std::ofstream out(filename, std::ios::app);
    if (out.is_open()) {
        out << logStream.str();
        out.close();
        std::cout << std::format("Exported log to {}\n", filename);
    } else {
        std::cerr << std::format("Failed to export log to {}\n", filename);
    }
    m_fontManager.exportLog(filename + "_font");
    m_inputManager.exportLog(filename + "_input");
}

std::vector<std::string> SDL3Initializer::getVulkanExtensions() const {
    return ::SDL3Initializer::getVulkanExtensions([this](const std::string& msg) { logMessage(msg); });
}

void SDL3Initializer::logMessage(const std::string& message) const {
    std::string timestamp = std::format("[{}ms] {}", SDL_GetTicks(), message);
    std::cout << timestamp << "\n";
    logStream << timestamp << "\n";
    if (logFile.is_open()) {
        logFile << timestamp << "\n";
        logFile.flush();
    }
}

} // namespace SDL3Initializer