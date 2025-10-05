// AMOURANTH RTX Engine September 2025 - Implementation of SDL3Input class for input handling.
// Provides methods for initializing input, polling events, handling keyboard, mouse, touch, and gamepad inputs,
// and managing worker threads for asynchronous gamepad event processing.
// Dependencies: SDL3 (SDL.h, SDL_mouse.h, SDL_gamepad.h), C++20 standard library.
// Best Practices:
// - Call initialize() before polling events to set up gamepad support.
// - Use setCallbacks() to register event handlers before eventLoop.
// - Call exportLog() to save diagnostic logs for debugging.
// Potential Issues:
// - Ensure SDL_INIT_GAMEPAD is set before initializing.
// - Verify gamepad callbacks are set to avoid null callback crashes.
// - Handle gamepad disconnection gracefully to prevent invalid memory access.
// Usage example:
//   SDL3Input input("input.log");
//   input.initialize([](bool connected, SDL_JoystickID id, SDL_Gamepad* gp) { /* handle connection */ });
//   input.setCallbacks(kb, mb, mm, mw, ti, tc, gb, ga, gc, resize);
//   input.pollEvents(window, audioDevice, consoleOpen, true);
//   input.exportLog("input_log.txt");
// Zachary Geurts 2025

#include "engine/SDL3/SDL3_input.hpp"
#include <algorithm>
#include <span>
#include <format>

namespace SDL3Initializer {

SDL3Input::SDL3Input(const std::string& logFilePath)
    : logFile(logFilePath, std::ios::app) {
    logMessage(std::format("Constructing SDL3Input with log file: {}", logFilePath));
}

SDL3Input::~SDL3Input() {
    logMessage("Destructing SDL3Input");
    {
        std::unique_lock lock(taskMutex);
        stopWorkers = true;
        taskCond.notify_all();
    }
    for (auto& thread : workerThreads) {
        if (thread.joinable()) thread.join();
    }
    cleanup();
    if (logFile.is_open()) {
        logFile.close();
    }
}

void SDL3Input::initialize(GamepadConnectCallback gc) {
    logMessage("Initializing input system");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");

    logMessage("Getting connected joysticks");
    int numJoysticks;
    if (SDL_JoystickID* joysticks = SDL_GetJoysticks(&numJoysticks)) {
        std::unique_lock lock(gamepadMutex);
        for (int i = 0; i < numJoysticks; ++i) {
            auto id = joysticks[i];
            if (SDL_IsGamepad(id)) {
                logMessage(std::format("Found gamepad: ID {}", id));
                if (SDL_Gamepad* gp = SDL_OpenGamepad(id)) {
                    logMessage(std::format("Opened gamepad: ID {}", id));
                    gamepads.emplace(id, gp);
                    if (gc) gc(true, id, gp);
                }
            }
        }
        SDL_free(joysticks);
    }

    startWorkerThreads(std::min(4, static_cast<int>(std::thread::hardware_concurrency())));
}

void SDL3Input::setCallbacks(KeyboardCallback kb, MouseButtonCallback mb, MouseMotionCallback mm,
                             MouseWheelCallback mw, TextInputCallback ti, TouchCallback tc,
                             GamepadButtonCallback gb, GamepadAxisCallback ga, GamepadConnectCallback gc,
                             ResizeCallback onResize) {
    logMessage("Setting input callbacks");
    std::lock_guard lock(callbackMutex);
    m_kb = std::move(kb);
    m_mb = std::move(mb);
    m_mm = std::move(mm);
    m_mw = std::move(mw);
    m_ti = std::move(ti);
    m_tc = std::move(tc);
    m_gb = std::move(gb);
    m_ga = std::move(ga);
    m_gc = std::move(gc);
    m_onResize = std::move(onResize);
}

bool SDL3Input::pollEvents(SDL_Window* window, SDL_AudioDeviceID audioDevice, bool& consoleOpen, bool exitOnClose) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        logMessage(std::format("Processing SDL event type: {}", e.type));
        switch (e.type) {
            case SDL_EVENT_QUIT:
                logMessage("Quit event received");
                return !exitOnClose;
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                logMessage("Window close requested");
                return !exitOnClose;
            case SDL_EVENT_WINDOW_RESIZED:
                logMessage(std::format("Window resized to {}x{}", e.window.data1, e.window.data2));
                {
                    std::lock_guard lock(callbackMutex);
                    if (m_onResize) m_onResize(e.window.data1, e.window.data2);
                }
                break;
            case SDL_EVENT_KEY_DOWN:
                logMessage(std::format("Key down event: {}", SDL_GetKeyName(e.key.key)));
                handleKeyboard(e.key, window, audioDevice, consoleOpen);
                {
                    std::lock_guard lock(callbackMutex);
                    if (m_kb) m_kb(e.key);
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                logMessage(std::format("Mouse button event: {}", e.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? "Down" : "Up"));
                handleMouseButton(e.button, window);
                {
                    std::lock_guard lock(callbackMutex);
                    if (m_mb) m_mb(e.button);
                }
                break;
            case SDL_EVENT_MOUSE_MOTION:
                logMessage(std::format("Mouse motion event at ({}, {})", e.motion.x, e.motion.y));
                {
                    std::lock_guard lock(callbackMutex);
                    if (m_mm) m_mm(e.motion);
                }
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                logMessage("Mouse wheel event");
                {
                    std::lock_guard lock(callbackMutex);
                    if (m_mw) m_mw(e.wheel);
                }
                break;
            case SDL_EVENT_TEXT_INPUT:
                logMessage(std::format("Text input event: {}", e.text.text));
                {
                    std::lock_guard lock(callbackMutex);
                    if (m_ti) m_ti(e.text);
                }
                break;
            case SDL_EVENT_FINGER_DOWN:
            case SDL_EVENT_FINGER_UP:
            case SDL_EVENT_FINGER_MOTION:
                logMessage(std::format("Touch event: {}", e.type == SDL_EVENT_FINGER_DOWN ? "Down" : e.type == SDL_EVENT_FINGER_UP ? "Up" : "Motion"));
                handleTouch(e.tfinger);
                {
                    std::lock_guard lock(callbackMutex);
                    if (m_tc) m_tc(e.tfinger);
                }
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                logMessage(std::format("Gamepad button event: {}", e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ? "Down" : "Up"));
                {
                    std::lock_guard lock(gamepadMutex);
                    if (gamepads.find(e.gbutton.which) != gamepads.end()) {
                        handleGamepadButton(e.gbutton, audioDevice);
                        std::lock_guard cbLock(callbackMutex);
                        if (m_gb) {
                            SDL_GamepadButtonEvent eventCopy = e.gbutton;
                            std::unique_lock taskLock(taskMutex);
                            taskQueue.push([this, eventCopy] {
                                std::lock_guard cbLockInner(callbackMutex);
                                if (m_gb) m_gb(eventCopy);
                                else logMessage("Skipping gamepad button callback (null)");
                            });
                            taskCond.notify_one();
                        } else {
                            logMessage("Skipping gamepad button callback (not set)");
                        }
                    } else {
                        logMessage(std::format("Ignoring gamepad button event for unknown gamepad ID: {}", e.gbutton.which));
                    }
                }
                break;
            case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                logMessage("Gamepad axis motion event");
                {
                    std::lock_guard lock(gamepadMutex);
                    if (gamepads.find(e.gaxis.which) != gamepads.end()) {
                        std::lock_guard cbLock(callbackMutex);
                        if (m_ga) {
                            SDL_GamepadAxisEvent eventCopy = e.gaxis;
                            std::unique_lock taskLock(taskMutex);
                            taskQueue.push([this, eventCopy] {
                                std::lock_guard cbLockInner(callbackMutex);
                                if (m_ga) m_ga(eventCopy);
                                else logMessage("Skipping gamepad axis callback (null)");
                            });
                            taskCond.notify_one();
                        } else {
                            logMessage("Skipping gamepad axis callback (not set)");
                        }
                    } else {
                        logMessage(std::format("Ignoring gamepad axis event for unknown gamepad ID: {}", e.gaxis.which));
                    }
                }
                break;
            case SDL_EVENT_GAMEPAD_ADDED:
                logMessage(std::format("Gamepad added: ID {}", e.gdevice.which));
                if (SDL_Gamepad* gp = SDL_OpenGamepad(e.gdevice.which)) {
                    std::unique_lock lock(gamepadMutex);
                    logMessage(std::format("Opened gamepad: ID {}", e.gdevice.which));
                    gamepads.emplace(e.gdevice.which, gp);
                    std::lock_guard cbLock(callbackMutex);
                    if (m_gc) m_gc(true, e.gdevice.which, gp);
                }
                break;
            case SDL_EVENT_GAMEPAD_REMOVED:
                logMessage(std::format("Gamepad removed: ID {}", e.gdevice.which));
                {
                    std::unique_lock lock(gamepadMutex);
                    if (auto it = gamepads.find(e.gdevice.which); it != gamepads.end()) {
                        logMessage(std::format("Closing gamepad: ID {}", e.gdevice.which));
                        SDL_CloseGamepad(it->second);
                        std::lock_guard cbLock(callbackMutex);
                        if (m_gc) m_gc(false, e.gdevice.which, nullptr);
                        gamepads.erase(it);
                    }
                }
                break;
        }
    }
    return true;
}

void SDL3Input::enableTextInput(SDL_Window* window, bool enable) {
    logMessage(enable ? "Enabling text input" : "Disabling text input");
    enable ? SDL_StartTextInput(window) : SDL_StopTextInput(window);
}

const std::map<SDL_JoystickID, SDL_Gamepad*>& SDL3Input::getGamepads() const {
    std::lock_guard lock(gamepadMutex);
    return gamepads;
}

void SDL3Input::exportLog(const std::string& filename) const {
    logMessage(std::format("Exporting log to {}", filename));
    std::ofstream out(filename, std::ios::app);
    if (out.is_open()) {
        out << logStream.str();
        out.close();
        std::cout << std::format("Exported log to {}\n", filename);
    } else {
        std::cerr << std::format("Failed to export log to {}\n", filename);
    }
}

void SDL3Input::handleKeyboard(const SDL_KeyboardEvent& k, SDL_Window* window, SDL_AudioDeviceID audioDevice, bool& consoleOpen) {
    if (k.type != SDL_EVENT_KEY_DOWN) {
        logMessage("Ignoring non-key-down event");
        return;
    }
    logMessage(std::format("Handling key: {}", SDL_GetKeyName(k.key)));
    switch (k.key) {
        case SDLK_F:
            logMessage("Toggling fullscreen mode");
            {
                Uint32 flags = SDL_GetWindowFlags(window);
                bool isFullscreen = flags & SDL_WINDOW_FULLSCREEN;
                SDL_SetWindowFullscreen(window, isFullscreen ? 0 : SDL_WINDOW_FULLSCREEN);
            }
            break;
        case SDLK_ESCAPE:
            logMessage("Pushing quit event");
            {
                SDL_Event evt{.type = SDL_EVENT_QUIT};
                SDL_PushEvent(&evt);
            }
            break;
        case SDLK_SPACE:
            logMessage("Toggling audio pause/resume");
            if (audioDevice) {
                SDL_AudioDevicePaused(audioDevice) ? SDL_ResumeAudioDevice(audioDevice) : SDL_PauseAudioDevice(audioDevice);
            }
            break;
        case SDLK_M:
            logMessage("Toggling audio mute");
            if (audioDevice) {
                float gain = SDL_GetAudioDeviceGain(audioDevice);
                SDL_SetAudioDeviceGain(audioDevice, gain == 0.0f ? 1.0f : 0.0f);
                logMessage(std::format("Audio {}", gain == 0.0f ? "unmuted" : "muted"));
            }
            break;
        case SDLK_GRAVE:
            logMessage("Toggling console");
            consoleOpen = !consoleOpen;
            break;
    }
}

void SDL3Input::handleMouseButton(const SDL_MouseButtonEvent& b, SDL_Window* window) {
    const char* button = b.button == SDL_BUTTON_LEFT ? "Left" : b.button == SDL_BUTTON_RIGHT ? "Right" : "Middle";
    logMessage(std::format("{} mouse button {} at ({}, {})", 
                           b.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? "Pressed" : "Released", button, b.x, b.y));
    if (b.type == SDL_EVENT_MOUSE_BUTTON_DOWN && b.button == SDL_BUTTON_RIGHT) {
        logMessage("Toggling relative mouse mode");
        bool relative = SDL_GetWindowRelativeMouseMode(window);
        SDL_SetWindowRelativeMouseMode(window, !relative);
    }
}

void SDL3Input::handleTouch(const SDL_TouchFingerEvent& t) {
    const char* type = t.type == SDL_EVENT_FINGER_DOWN ? "DOWN" : t.type == SDL_EVENT_FINGER_UP ? "UP" : "MOTION";
    logMessage(std::format("Touch {} fingerID: {} at ({}, {}) pressure: {}", type, t.fingerID, t.x, t.y, t.pressure));
}

void SDL3Input::handleGamepadButton(const SDL_GamepadButtonEvent& g, SDL_AudioDeviceID audioDevice) {
    if (g.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        logMessage("Gamepad button down: ");
        switch (g.button) {
            case SDL_GAMEPAD_BUTTON_SOUTH:
                logMessage("South (A/Cross) pressed");
                break;
            case SDL_GAMEPAD_BUTTON_EAST:
                logMessage("East (B/Circle) pressed");
                {
                    logMessage("Pushing quit event");
                    SDL_Event evt{.type = SDL_EVENT_QUIT};
                    SDL_PushEvent(&evt);
                }
                break;
            case SDL_GAMEPAD_BUTTON_WEST:
                logMessage("West (X/Square) pressed");
                break;
            case SDL_GAMEPAD_BUTTON_NORTH:
                logMessage("North (Y/Triangle) pressed");
                break;
            case SDL_GAMEPAD_BUTTON_START:
                logMessage("Start pressed");
                if (audioDevice) {
                    logMessage("Toggling audio pause/resume");
                    SDL_AudioDevicePaused(audioDevice) ? SDL_ResumeAudioDevice(audioDevice) : SDL_PauseAudioDevice(audioDevice);
                }
                break;
            default:
                logMessage(std::format("Button {} pressed", static_cast<int>(g.button)));
                break;
        }
    } else {
        logMessage(std::format("Gamepad button {} released", static_cast<int>(g.button)));
    }
}

void SDL3Input::startWorkerThreads(int numThreads) {
    logMessage(std::format("Starting {} worker threads for gamepad events", numThreads));
    workerThreads.reserve(numThreads);
    for (int i = 0; i < numThreads; ++i) {
        workerThreads.emplace_back([this] {
            while (true) {
                std::vector<std::function<void()>> tasks;
                {
                    std::unique_lock lock(taskMutex);
                    taskCond.wait(lock, [this] { return !taskQueue.empty() || stopWorkers; });
                    if (stopWorkers && taskQueue.empty()) break;
                    while (!taskQueue.empty() && tasks.size() < 10) {
                        tasks.push_back(std::move(taskQueue.front()));
                        taskQueue.pop();
                    }
                }
                for (auto& task : tasks) {
                    logMessage("Worker thread processing task");
                    task();
                }
            }
            logMessage("Worker thread exiting");
        });
    }
}

void SDL3Input::cleanup() {
    logMessage("Cleaning up input system");
    std::unique_lock lock(gamepadMutex);
    for (auto [id, gp] : gamepads) {
        logMessage(std::format("Closing gamepad: ID {}", id));
        SDL_CloseGamepad(gp);
    }
    gamepads.clear();
}

void SDL3Input::logMessage(const std::string& message) const {
    std::string timestamp = std::format("[{}ms] {}", SDL_GetTicks(), message);
    std::cout << timestamp << "\n";
    logStream << timestamp << "\n";
    if (logFile.is_open()) {
        logFile << timestamp << "\n";
        logFile.flush();
    }
}

} // namespace SDL3Initializer