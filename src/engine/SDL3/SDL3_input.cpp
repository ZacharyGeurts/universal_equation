// SDL3_input.cpp
// AMOURANTH RTX Engine September 2025 - Implementation of SDL3Input class for input handling.
// Provides methods for initializing input, polling events, handling keyboard, mouse, touch, and gamepad inputs,
// and managing worker threads for asynchronous gamepad event processing.
// Dependencies: SDL3 (SDL.h, SDL_mouse.h, SDL_gamepad.h), C++17 standard library.
// Zachary Geurts 2025

#include "engine/SDL3/SDL3_input.hpp"
#include <algorithm>
#include <sched.h>

namespace SDL3Initializer {

SDL3Input::SDL3Input(const std::string& logFilePath)
    : logFile(logFilePath, std::ios::app) {
    logMessage("Constructing SDL3Input");
}

SDL3Input::~SDL3Input() {
    logMessage("Destructing SDL3Input");
    {
        std::unique_lock<std::mutex> lock(taskMutex);
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
    // Set SDL thread priority to default to avoid glibc priority errors
    SDL_SetHint(SDL_HINT_THREAD_PRIORITY_POLICY, "default");
    logMessage("Initializing input system");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");

    // Enumerate and open connected gamepads
    logMessage("Getting connected joysticks");
    int numJoysticks;
    SDL_JoystickID* joysticks = SDL_GetJoysticks(&numJoysticks);
    if (joysticks) {
        std::unique_lock<std::mutex> lock(gamepadMutex);
        for (int i = 0; i < numJoysticks; ++i) {
            if (SDL_IsGamepad(joysticks[i])) {
                logMessage("Found gamepad: ID " + std::to_string(joysticks[i]));
                if (auto gp = SDL_OpenGamepad(joysticks[i])) {
                    logMessage("Opened gamepad: ID " + std::to_string(joysticks[i]));
                    gamepads[joysticks[i]] = gp;
                    if (gc) gc(true, joysticks[i], gp);
                }
            }
        }
        SDL_free(joysticks);
    }

    // Start worker threads for gamepad event processing
    startWorkerThreads(std::min(4, static_cast<int>(std::thread::hardware_concurrency())));
}

void SDL3Input::setCallbacks(KeyboardCallback kb, MouseButtonCallback mb, MouseMotionCallback mm,
                             MouseWheelCallback mw, TextInputCallback ti, TouchCallback tc,
                             GamepadButtonCallback gb, GamepadAxisCallback ga, GamepadConnectCallback gc,
                             ResizeCallback onResize) {
    logMessage("Setting input callbacks");
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
        logMessage("Processing SDL event type: " + std::to_string(e.type));
        switch (e.type) {
            case SDL_EVENT_QUIT:
                logMessage("Quit event received");
                return !exitOnClose;
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                logMessage("Window close requested");
                return !exitOnClose;
            case SDL_EVENT_WINDOW_RESIZED:
                logMessage("Window resized to " + std::to_string(e.window.data1) + "x" + std::to_string(e.window.data2));
                if (m_onResize) m_onResize(e.window.data1, e.window.data2);
                break;
            case SDL_EVENT_KEY_DOWN:
                logMessage("Key down event: " + std::string(SDL_GetKeyName(e.key.key)));
                handleKeyboard(e.key, window, audioDevice, consoleOpen);
                if (m_kb) m_kb(e.key);
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                logMessage("Mouse button event: " + std::string(e.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? "Down" : "Up"));
                handleMouseButton(e.button, window);
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
                handleTouch(e.tfinger);
                if (m_tc) m_tc(e.tfinger);
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                logMessage("Gamepad button event: " + std::string(e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ? "Down" : "Up"));
                handleGamepadButton(e.gbutton, audioDevice);
                if (m_gb) {
                    SDL_GamepadButtonEvent eventCopy = e.gbutton;
                    std::unique_lock<std::mutex> lock(taskMutex);
                    taskQueue.push([eventCopy, this]() { m_gb(eventCopy); });
                    taskCond.notify_one();
                }
                break;
            case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                logMessage("Gamepad axis motion event");
                if (m_ga) {
                    SDL_GamepadAxisEvent eventCopy = e.gaxis;
                    std::unique_lock<std::mutex> lock(taskMutex);
                    taskQueue.push([eventCopy, this]() { m_ga(eventCopy); });
                    taskCond.notify_one();
                }
                break;
            case SDL_EVENT_GAMEPAD_ADDED:
                logMessage("Gamepad added: ID " + std::to_string(e.gdevice.which));
                if (auto gp = SDL_OpenGamepad(e.gdevice.which)) {
                    std::unique_lock<std::mutex> lock(gamepadMutex);
                    logMessage("Opened gamepad: ID " + std::to_string(e.gdevice.which));
                    gamepads[e.gdevice.which] = gp;
                    if (m_gc) m_gc(true, e.gdevice.which, gp);
                }
                break;
            case SDL_EVENT_GAMEPAD_REMOVED:
                logMessage("Gamepad removed: ID " + std::to_string(e.gdevice.which));
                {
                    std::unique_lock<std::mutex> lock(gamepadMutex);
                    if (auto it = gamepads.find(e.gdevice.which); it != gamepads.end()) {
                        logMessage("Closing gamepad: ID " + std::to_string(e.gdevice.which));
                        SDL_CloseGamepad(it->second);
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
    if (enable) {
        SDL_StartTextInput(window);
    } else {
        SDL_StopTextInput(window);
    }
}

const std::map<SDL_JoystickID, SDL_Gamepad*>& SDL3Input::getGamepads() const {
    std::lock_guard<std::mutex> lock(gamepadMutex);
    return gamepads;
}

void SDL3Input::exportLog(const std::string& filename) const {
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

void SDL3Input::handleKeyboard(const SDL_KeyboardEvent& k, SDL_Window* window, SDL_AudioDeviceID audioDevice, bool& consoleOpen) {
    if (k.type != SDL_EVENT_KEY_DOWN) {
        logMessage("Ignoring non-key-down event");
        return;
    }
    logMessage("Handling key: " + std::string(SDL_GetKeyName(k.key)));
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
                if (SDL_AudioDevicePaused(audioDevice)) {
                    SDL_ResumeAudioDevice(audioDevice);
                } else {
                    SDL_PauseAudioDevice(audioDevice);
                }
            }
            break;
        case SDLK_M:
            logMessage("Toggling audio mute");
            if (audioDevice) {
                float gain = SDL_GetAudioDeviceGain(audioDevice);
                SDL_SetAudioDeviceGain(audioDevice, gain == 0.0f ? 1.0f : 0.0f);
                logMessage("Audio " + std::string(gain == 0.0f ? "unmuted" : "muted"));
            }
            break;
        case SDLK_GRAVE:
            logMessage("Toggling console");
            consoleOpen = !consoleOpen;
            break;
    }
}

void SDL3Input::handleMouseButton(const SDL_MouseButtonEvent& b, SDL_Window* window) {
    logMessage((b.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? "Pressed" : "Released") + std::string(" mouse button ") +
               (b.button == SDL_BUTTON_LEFT ? "Left" : b.button == SDL_BUTTON_RIGHT ? "Right" : "Middle") +
               " at (" + std::to_string(b.x) + ", " + std::to_string(b.y) + ")");
    if (b.type == SDL_EVENT_MOUSE_BUTTON_DOWN && b.button == SDL_BUTTON_RIGHT) {
        logMessage("Toggling relative mouse mode");
        bool relative = SDL_GetWindowRelativeMouseMode(window);
        SDL_SetWindowRelativeMouseMode(window, !relative);
    }
}

void SDL3Input::handleTouch(const SDL_TouchFingerEvent& t) {
    logMessage("Touch " + std::string(t.type == SDL_EVENT_FINGER_DOWN ? "DOWN" : t.type == SDL_EVENT_FINGER_UP ? "UP" : "MOTION") +
               " fingerID: " + std::to_string(t.fingerID) + " at (" + std::to_string(t.x) + ", " + std::to_string(t.y) +
               ") pressure: " + std::to_string(t.pressure));
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
                    if (SDL_AudioDevicePaused(audioDevice)) {
                        SDL_ResumeAudioDevice(audioDevice);
                    } else {
                        SDL_PauseAudioDevice(audioDevice);
                    }
                }
                break;
            default:
                logMessage("Button " + std::to_string(static_cast<int>(g.button)) + " pressed");
                break;
        }
    } else {
        logMessage("Gamepad button " + std::to_string(static_cast<int>(g.button)) + " released");
    }
}

void SDL3Input::startWorkerThreads(int numThreads) {
    logMessage("Starting " + std::to_string(numThreads) + " worker threads for gamepad events");
    for (int i = 0; i < numThreads; ++i) {
        workerThreads.emplace_back([this] {
            // Set thread to use default scheduling policy to avoid glibc error
            sched_param param = {};
            param.sched_priority = 0; // Default for SCHED_OTHER
            pthread_setschedparam(pthread_self(), SCHED_OTHER, &param);
            
            while (true) {
                std::vector<std::function<void()>> tasks;
                {
                    std::unique_lock<std::mutex> lock(taskMutex);
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
        });
    }
}

void SDL3Input::cleanup() {
    logMessage("Cleaning up input system");
    std::unique_lock<std::mutex> lock(gamepadMutex);
    for (auto& [id, gp] : gamepads) {
        logMessage("Closing gamepad: ID " + std::to_string(id));
        SDL_CloseGamepad(gp);
    }
    gamepads.clear();
}

void SDL3Input::logMessage(const std::string& message) const {
    std::string timestamp = "[" + std::to_string(SDL_GetTicks()) + "ms] " + message;
    std::cout << timestamp << "\n";
    logStream << timestamp << "\n";
    if (logFile.is_open()) {
        logFile << timestamp << "\n";
        logFile.flush();
    }
}

} // namespace SDL3Initializer