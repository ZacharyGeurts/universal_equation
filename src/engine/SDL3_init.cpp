#include "SDL3_init.hpp"
#include <stdexcept>

// Main class integrating all components, event loop, and logging.
// AMOURANTH RTX Engine September 2025
// Zachary Geurts 2025
namespace SDL3Initializer {

    SDL3Initializer::SDL3Initializer() : logFile("sdl3_initializer.log"), m_useVulkan(true) {
        logMessage("Constructing SDL3Initializer");
        startWorkerThreads(std::min(4, static_cast<int>(std::thread::hardware_concurrency())),
                          taskQueue, taskMutex, taskCond, stopWorkers, workerThreads,
                          [this](const std::string& msg) { logMessage(msg); });
    }

    SDL3Initializer::~SDL3Initializer() {
        logMessage("Destructing SDL3Initializer");
        {
            std::unique_lock<std::mutex> lock(taskMutex);
            stopWorkers = true;
            taskCond.notify_all();
        }
        for (auto& thread : workerThreads) {
            if (thread.joinable()) thread.join();
        }
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
        logFile.close();
    }

    void SDL3Initializer::initialize(const char* title, int w, int h, Uint32 flags, bool rt,
                                    const std::string& fontPath, bool enableValidation, bool preferNvidia) {
        logMessage("Initializing SDL3Initializer with title: " + std::string(title) + ", width: " + std::to_string(w) + ", height: " + std::to_string(h));
        
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS) == 0) {
            std::string error = "SDL_Init failed: " + std::string(SDL_GetError());
            logMessage(error);
            throw std::runtime_error(error);
        }

        logMessage("Initializing TTF");
        if (TTF_Init() == 0) {
            std::string error = "TTF_Init failed: " + std::string(SDL_GetError());
            logMessage(error);
            SDL_Quit();
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

        logMessage("Initializing audio");
        initAudio(AudioConfig{}, m_audioDevice, m_audioStream, [this](const std::string& msg) { logMessage(msg); });

        std::string resolvedFontPath = fontPath;
#ifdef __ANDROID__
        if (!resolvedFontPath.empty() && resolvedFontPath[0] != '/') {
            resolvedFontPath = "assets/" + resolvedFontPath;
        }
#endif
        logMessage("Loading TTF font asynchronously: " + resolvedFontPath);
        m_fontFuture = std::async(std::launch::async, [resolvedFontPath]() -> TTF_Font* {
            TTF_Font* font = TTF_OpenFont(resolvedFontPath.c_str(), 24);
            if (!font) {
                std::string error = "TTF_OpenFont failed for " + resolvedFontPath + ": " + std::string(SDL_GetError());
                std::cerr << error << "\n";
                throw std::runtime_error(error);
            }
            return font;
        });
    }

    void SDL3Initializer::eventLoop(std::function<void()> render, ResizeCallback onResize, bool exitOnClose,
                                    KeyboardCallback kb, MouseButtonCallback mb, MouseMotionCallback mm,
                                    MouseWheelCallback mw, TextInputCallback ti, TouchCallback tc,
                                    GamepadButtonCallback gb, GamepadAxisCallback ga, GamepadConnectCallback gc) {
        logMessage("Starting event loop");
        m_onResize = std::move(onResize);
        m_kb = std::move(kb);
        m_mb = std::move(mb);
        m_mm = std::move(mm);
        m_mw = std::move(mw);
        m_ti = std::move(ti);
        m_tc = std::move(tc);
        m_gb = std::move(gb);
        m_ga = std::move(ga);
        m_gc = std::move(gc);

        if (m_ti) {
            logMessage("Enabling text input");
            SDL_StartTextInput(m_window.get());
        }

        initInput(m_gamepads, gamepadMutex, m_gc, [this](const std::string& msg) { logMessage(msg); });

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

        for (bool running = true; running;) {
            SDL_Event e;
            std::vector<std::function<void()>> gamepadTasks;
            while (SDL_PollEvent(&e)) {
                logMessage("Processing SDL event type: " + std::to_string(e.type));
                switch (e.type) {
                    case SDL_EVENT_QUIT:
                        logMessage("Quit event received");
                        running = !exitOnClose;
                        break;
                    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                        logMessage("Window close requested");
                        running = !exitOnClose;
                        break;
                    case SDL_EVENT_WINDOW_RESIZED:
                        logMessage("Window resized to " + std::to_string(e.window.data1) + "x" + std::to_string(e.window.data2));
                        if (m_onResize) m_onResize(e.window.data1, e.window.data2);
                        break;
                    case SDL_EVENT_KEY_DOWN:
                        logMessage("Key down event: " + std::string(SDL_GetKeyName(e.key.key)));
                        handleKeyboard(e.key, m_window.get(), m_audioDevice, m_consoleOpen,
                                       [this](const std::string& msg) { logMessage(msg); });
                        if (m_kb) m_kb(e.key);
                        break;
                    case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    case SDL_EVENT_MOUSE_BUTTON_UP:
                        logMessage("Mouse button event: " + std::string(e.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? "Down" : "Up"));
                        handleMouseButton(e.button, m_window.get(), [this](const std::string& msg) { logMessage(msg); });
                        if (m_mb) m_mb(e.button);
                        break;
                    case SDL_EVENT_MOUSE_MOTION:
                        logMessage("Mouse motion event at (" + std::to_string(e.motion.x) + ", " + std::to_string(e.motion.y) + ")");
                        if (m_mm) m_mm(e.motion);
                        break;
                    case SDL_EVENT_MOUSE_WHEEL:
                        logMessage("Mouse wheel event");
                        if (m_mw) m_mw(e.wheel);
                        break;
                    case SDL_EVENT_TEXT_INPUT:
                        logMessage("Text input event: " + std::string(e.text.text));
                        if (m_ti) m_ti(e.text);
                        break;
                    case SDL_EVENT_FINGER_DOWN:
                    case SDL_EVENT_FINGER_UP:
                    case SDL_EVENT_FINGER_MOTION:
                        logMessage("Touch event: " + std::string(e.type == SDL_EVENT_FINGER_DOWN ? "Down" : e.type == SDL_EVENT_FINGER_UP ? "Up" : "Motion"));
                        handleTouch(e.tfinger, [this](const std::string& msg) { logMessage(msg); });
                        if (m_tc) m_tc(e.tfinger);
                        break;
                    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                    case SDL_EVENT_GAMEPAD_BUTTON_UP:
                        logMessage("Gamepad button event: " + std::string(e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ? "Down" : "Up"));
                        handleGamepadButton(e.gbutton, m_audioDevice, [this](const std::string& msg) { logMessage(msg); });
                        if (m_gb) {
                            SDL_GamepadButtonEvent eventCopy = e.gbutton;
                            gamepadTasks.push_back([eventCopy, this] { m_gb(eventCopy); });
                        }
                        break;
                    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                        logMessage("Gamepad axis motion event");
                        if (m_ga) {
                            SDL_GamepadAxisEvent eventCopy = e.gaxis;
                            gamepadTasks.push_back([eventCopy, this] { m_ga(eventCopy); });
                        }
                        break;
                    case SDL_EVENT_GAMEPAD_ADDED:
                        logMessage("Gamepad added: ID " + std::to_string(e.gdevice.which));
                        {
                            std::unique_lock<std::mutex> lock(gamepadMutex);
                            if (SDL_Gamepad* gp = SDL_OpenGamepad(e.gdevice.which)) {
                                logMessage("Opened gamepad: ID " + std::to_string(e.gdevice.which));
                                m_gamepads[e.gdevice.which] = gp;
                                if (m_gc) m_gc(true, e.gdevice.which, gp);
                            }
                        }
                        break;
                    case SDL_EVENT_GAMEPAD_REMOVED:
                        logMessage("Gamepad removed: ID " + std::to_string(e.gdevice.which));
                        {
                            std::unique_lock<std::mutex> lock(gamepadMutex);
                            if (auto it = m_gamepads.find(e.gdevice.which); it != m_gamepads.end()) {
                                logMessage("Closing gamepad: ID " + std::to_string(e.gdevice.which));
                                SDL_CloseGamepad(it->second);
                                if (m_gc) m_gc(false, e.gdevice.which, nullptr);
                                m_gamepads.erase(it);
                            }
                        }
                        break;
                }
            }
            if (!gamepadTasks.empty()) {
                std::unique_lock<std::mutex> lock(taskMutex);
                for (auto& task : gamepadTasks) {
                    taskQueue.push(std::move(task));
                }
                taskCond.notify_all();
            }
            if (render && running) {
                std::unique_lock<std::mutex> lock(renderMutex);
                renderReady = true;
                renderCond.notify_one();
                renderCond.wait(lock, [this] { return !renderReady || stopRender; });
            }
        }

        {
            std::unique_lock<std::mutex> lock(renderMutex);
            stopRender = true;
            renderCond.notify_one();
        }

        if (m_ti) {
            logMessage("Disabling text input");
            SDL_StopTextInput(m_window.get());
        }
        logMessage("Exiting event loop");
    }

    void SDL3Initializer::cleanup() {
        logMessage("Starting cleanup");
        {
            std::unique_lock<std::mutex> lock(gamepadMutex);
            for (auto& [id, gp] : m_gamepads) {
                logMessage("Closing gamepad: ID " + std::to_string(id));
                SDL_CloseGamepad(gp);
            }
            m_gamepads.clear();
        }
        logMessage("Cleared gamepads");
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
        if (m_fontFuture.valid()) {
            try {
                TTF_Font* pending = m_fontFuture.get();
                if (pending) {
                    TTF_CloseFont(pending);
                    logMessage("Closed pending font in cleanup");
                }
            } catch (...) {
                logMessage("Error closing pending font");
            }
        }
        if (m_font) {
            logMessage("Closing TTF font");
            TTF_CloseFont(m_font);
            m_font = nullptr;
        }
        logMessage("Quitting TTF");
        TTF_Quit();
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

    TTF_Font* SDL3Initializer::getFont() const {
        if (m_font == nullptr && m_fontFuture.valid()) {
            try {
                m_font = m_fontFuture.get();
                logMessage("Font loaded successfully");
            } catch (const std::runtime_error& e) {
                logMessage("Font loading failed: " + std::string(e.what()));
                m_font = nullptr;
            }
        }
        logMessage("Getting TTF font");
        return m_font;
    }

    std::vector<std::string> SDL3Initializer::getVulkanExtensions() const {
        return ::SDL3Initializer::getVulkanExtensions([this](const std::string& msg) { logMessage(msg); });
    }

    void SDL3Initializer::exportLog(const std::string& filename) const {
        logMessage("Exporting log to " + filename);
        std::ofstream out(filename, std::ios::app);
        if (out.is_open()) {
            out << logStream.str();
            out.close();
            std::cout << "Exported log to " << filename << "\n";
        } else {
            std::cerr << "Failed to export log to " << filename << "\n";
        }
    }

    void SDL3Initializer::logMessage(const std::string& message) const {
        std::string timestamp = "[" + std::to_string(SDL_GetTicks()) + "ms] " + message;
        std::cout << timestamp << "\n";
        logStream << timestamp << "\n";
        if (logFile.is_open()) {
            logFile << timestamp << "\n";
            logFile.flush();
        }
    }

}