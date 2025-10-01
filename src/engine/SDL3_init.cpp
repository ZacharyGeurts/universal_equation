// SDL3_init.cpp
// AMOURANTH RTX Engine September 2025 - Implementation of SDL3Initializer class for SDL3 and Vulkan integration.
// Initializes SDL subsystems, manages window creation, input handling, audio setup, and Vulkan resources.
// Provides a main event loop for rendering and input processing.
// Dependencies: SDL3, SDL_ttf, Vulkan 1.3+, C++17 standard library.
// Best Practices:
// - Call initialize() with appropriate parameters before starting the event loop.
// - Use eventLoop() to process input and rendering, providing callbacks as needed.
// - Call exportLog() to save diagnostic logs for debugging.
// Potential Issues:
// - Ensure SDL_INIT_VIDEO, SDL_INIT_AUDIO, SDL_INIT_GAMEPAD, and SDL_INIT_EVENTS are set during initialization.
// - Handle Vulkan fallback gracefully in case of initialization failure.
// - Verify font and audio resources are accessible, especially on Android.
// Usage example:
//   SDL3Initializer sdl;
//   sdl.initialize("Dimensional Navigator", 1280, 720, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE,
//                  true, "assets/fonts/custom.ttf", true, true);
//   sdl.eventLoop([] { /* render */ }, [](int w, int h) { /* resize */ });
//   sdl.exportLog("game_log.txt");
// Zachary Geurts 2025

#include "SDL3_init.hpp"
#include <algorithm>

namespace SDL3Initializer {

SDL3Initializer::SDL3Initializer() : logFile("sdl3_initializer.log"), m_useVulkan(true) {
    logMessage("Constructing SDL3Initializer");
}

SDL3Initializer::~SDL3Initializer() {
    logMessage("Destructing SDL3Initializer");
    if (renderThread.joinable()) {
        {
            std::unique_lock<std::mutex> lock(renderMutex);
            renderReady = false;
            stopRender = true;
            renderCond.notify_one();
        }
        renderThread.join();
    }
    cleanup();
    if (logFile.is_open()) {
        logFile.close();
    }
}

void SDL3Initializer::initialize(const char* title, int w, int h, Uint32 flags, bool rt,
                                 const std::string& fontPath, bool enableValidation, bool preferNvidia) {
    // Initialize SDL subsystems
    logMessage("Initializing SDL3Initializer with title: " + std::string(title) + ", width: " + std::to_string(w) + ", height: " + std::to_string(h));
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS) == 0) {
        std::string error = "SDL_Init failed: " + std::string(SDL_GetError());
        logMessage(error);
        throw std::runtime_error(error);
    }

    // Create window and initialize Vulkan if needed
    m_useVulkan = (flags & SDL_WINDOW_VULKAN) != 0;
    try {
        m_window = createWindow(title, w, h, flags, [this](const std::string& msg) { logMessage(msg); });
        if (m_useVulkan) {
            initVulkan(m_window.get(), m_instance, m_surface, enableValidation, preferNvidia, rt, title,
                       [this](const std::string& msg) { logMessage(msg); });
        }
    } catch (const std::runtime_error& e) {
        if (m_useVulkan) {
            logMessage("Vulkan initialization failed: " + std::string(e.what()));
            logMessage("Falling back to non-Vulkan rendering");
            m_surface.reset();
            m_instance.reset();
            m_useVulkan = false;
            flags &= ~SDL_WINDOW_VULKAN;
            m_window = createWindow(title, w, h, flags, [this](const std::string& msg) { logMessage(msg); });
        } else {
            throw;
        }
    }

    // Initialize audio
    logMessage("Initializing audio");
    initAudio(AudioConfig{}, m_audioDevice, m_audioStream, [this](const std::string& msg) { logMessage(msg); });

    // Initialize font if provided
    if (!fontPath.empty()) {
        m_fontManager.initialize(fontPath);
    }

    // Initialize input system
    logMessage("Initializing input");
    m_inputManager.initialize(nullptr); // Gamepad connect callback set in eventLoop
}

void SDL3Initializer::eventLoop(std::function<void()> render, ResizeCallback onResize, bool exitOnClose,
                                KeyboardCallback kb, MouseButtonCallback mb, MouseMotionCallback mm,
                                MouseWheelCallback mw, TextInputCallback ti, TouchCallback tc,
                                GamepadButtonCallback gb, GamepadAxisCallback ga, GamepadConnectCallback gc) {
    // Start the main event loop
    logMessage("Starting event loop");
    m_inputManager.setCallbacks(kb, mb, mm, mw, ti, tc, gb, ga, gc, onResize);
    if (ti) {
        m_inputManager.enableTextInput(m_window.get(), true);
    }

    // Start render thread if render callback is provided
    if (render) {
        renderThread = std::thread([this, render]() {
            while (true) {
                std::unique_lock<std::mutex> lock(renderMutex);
                renderCond.wait(lock, [this] { return renderReady || stopRender; });
                if (stopRender && !renderReady) break;
                if (renderReady) {
                    render();
                    renderReady = false;
                    lock.unlock();
                    renderCond.notify_one();
                }
            }
        });
    }

    // Poll events and render until quit
    for (bool running = true; running;) {
        running = m_inputManager.pollEvents(m_window.get(), m_audioDevice, m_consoleOpen, exitOnClose);
        if (render && running) {
            std::unique_lock<std::mutex> lock(renderMutex);
            renderReady = true;
            renderCond.notify_one();
            renderCond.wait(lock, [this] { return !renderReady || stopRender; });
        }
    }

    // Stop render thread
    {
        std::unique_lock<std::mutex> lock(renderMutex);
        stopRender = true;
        renderCond.notify_one();
    }

    // Disable text input if enabled
    if (ti) {
        m_inputManager.enableTextInput(m_window.get(), false);
    }
    logMessage("Exiting event loop");
}

void SDL3Initializer::cleanup() {
    // Clean up SDL resources
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
        logMessage("Closing audio device: ID " + std::to_string(m_audioDevice));
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
    // Export logs for initializer, font, and input
    logMessage("Exporting log to " + filename);
    std::ofstream out(filename, std::ios::app);
    if (out.is_open()) {
        out << logStream.str();
        out.close();
        std::cout << "Exported log to " << filename << "\n";
    } else {
        std::cerr << "Failed to export log to " << filename << "\n";
    }
    m_fontManager.exportLog(filename + "_font");
    m_inputManager.exportLog(filename + "_input");
}

std::vector<std::string> SDL3Initializer::getVulkanExtensions() const {
    // Retrieve Vulkan extensions
    return ::SDL3Initializer::getVulkanExtensions([this](const std::string& msg) { logMessage(msg); });
}

void SDL3Initializer::logMessage(const std::string& message) const {
    // Log message to console and file with timestamp
    std::string timestamp = "[" + std::to_string(SDL_GetTicks()) + "ms] " + message;
    std::cout << timestamp << "\n";
    logStream << timestamp << "\n";
    if (logFile.is_open()) {
        logFile << timestamp << "\n";
        logFile.flush();
    }
}

} // namespace SDL3Initializer