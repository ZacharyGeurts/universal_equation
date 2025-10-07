// AMOURANTH RTX Engine, October 2025 - Input handling implementation for keyboard, mouse, and gamepad.
// Thread-safe with C++20 features; no mutexes required.
// Dependencies: SDL3, C++20 standard library, logging.hpp.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#include "engine/SDL3/SDL3_input.hpp"
#include <stdexcept>
#include <format>
#include <fstream>

namespace SDL3Initializer {

SDL3Input::~SDL3Input() {
    Logging::Logger logger;
    logger.log(Logging::LogLevel::Info, "Destroying SDL3Input, closing all gamepads", std::source_location::current());
    for (auto& [id, gp] : m_gamepads) {
        SDL_CloseGamepad(gp);
        logger.log(Logging::LogLevel::Info, "Closed gamepad: ID {}", std::source_location::current(), id);
    }
    m_gamepads.clear();
}

void SDL3Input::initialize(LogCallback logCallback) {
    m_logCallback = logCallback; // Store the callback
    // Verify platform support
    std::string_view platform = SDL_GetPlatform();
    if (platform != "Linux" && platform != "Windows") {
        std::string error = std::format("Unsupported platform for input: {}", platform);
        if (m_logCallback) m_logCallback(error);
        throw std::runtime_error(error);
    }

    if (m_logCallback) m_logCallback("Initializing SDL3Input");

    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");
    int numJoysticks;
    SDL_JoystickID* joysticks = SDL_GetJoysticks(&numJoysticks);
    if (joysticks) {
        if (m_logCallback) m_logCallback(std::format("Found {} joysticks", numJoysticks));
        for (int i = 0; i < numJoysticks; ++i) {
            if (SDL_IsGamepad(joysticks[i])) {
                if (m_logCallback) m_logCallback(std::format("Found gamepad: ID {}", joysticks[i]));
                if (auto gp = SDL_OpenGamepad(joysticks[i])) {
                    if (m_logCallback) m_logCallback(std::format("Opened gamepad: ID {}", joysticks[i]));
                    m_gamepads[joysticks[i]] = gp;
                    if (m_gamepadConnectCallback) {
                        m_gamepadConnectCallback(true, joysticks[i], gp);
                    }
                } else {
                    if (m_logCallback) m_logCallback(std::format("Failed to open gamepad: ID {}, error: {}", joysticks[i], SDL_GetError()));
                }
            }
        }
        SDL_free(joysticks);
    } else {
        if (m_logCallback) m_logCallback(std::format("No joysticks found: {}", SDL_GetError()));
    }
}

bool SDL3Input::pollEvents(SDL_Window* window, SDL_AudioDeviceID audioDevice, bool& consoleOpen, bool exitOnClose) {
    Logging::Logger logger;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        logger.log(Logging::LogLevel::Debug, "Processing SDL event type: {:#x}", std::source_location::current(), event.type);
        switch (event.type) {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                logger.log(Logging::LogLevel::Info, "Quit or window close event received", std::source_location::current());
                return !exitOnClose;
            case SDL_EVENT_WINDOW_RESIZED:
                logger.log(Logging::LogLevel::Info, "Window resized: width={}, height={}", 
                           std::source_location::current(), event.window.data1, event.window.data2);
                if (m_resizeCallback) m_resizeCallback(event.window.data1, event.window.data2);
                break;
            case SDL_EVENT_KEY_DOWN:
                handleKeyboard(event.key, window, audioDevice, consoleOpen);
                if (m_keyboardCallback) m_keyboardCallback(event.key);
                logger.log(Logging::LogLevel::Debug, "Key down event: {}", std::source_location::current(), SDL_GetKeyName(event.key.key));
                break;
            case SDL_EVENT_KEY_UP:
                if (m_keyboardCallback) m_keyboardCallback(event.key);
                logger.log(Logging::LogLevel::Debug, "Key up event: {}", std::source_location::current(), SDL_GetKeyName(event.key.key));
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                handleMouseButton(event.button, window);
                if (m_mouseButtonCallback) m_mouseButtonCallback(event.button);
                logger.log(Logging::LogLevel::Debug, "Mouse button event: button {}", std::source_location::current(), event.button.button);
                break;
            case SDL_EVENT_MOUSE_MOTION:
                if (m_mouseMotionCallback) m_mouseMotionCallback(event.motion);
                logger.log(Logging::LogLevel::Debug, "Mouse motion event: xrel={}, yrel={}", 
                           std::source_location::current(), event.motion.xrel, event.motion.yrel);
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                if (m_mouseWheelCallback) m_mouseWheelCallback(event.wheel);
                logger.log(Logging::LogLevel::Debug, "Mouse wheel event: x={}, y={}", 
                           std::source_location::current(), event.wheel.x, event.wheel.y);
                break;
            case SDL_EVENT_TEXT_INPUT:
                if (m_textInputCallback) m_textInputCallback(event.text);
                logger.log(Logging::LogLevel::Debug, "Text input event: {}", std::source_location::current(), event.text.text);
                break;
            case SDL_EVENT_FINGER_DOWN:
            case SDL_EVENT_FINGER_UP:
            case SDL_EVENT_FINGER_MOTION:
                handleTouch(event.tfinger);
                if (m_touchCallback) m_touchCallback(event.tfinger);
                logger.log(Logging::LogLevel::Debug, "Touch event: finger {}, x={}, y={}", 
                           std::source_location::current(), event.tfinger.fingerID, event.tfinger.x, event.tfinger.y);
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                handleGamepadButton(event.gbutton, audioDevice);
                if (m_gamepadButtonCallback) m_gamepadButtonCallback(event.gbutton);
                logger.log(Logging::LogLevel::Debug, "Gamepad button event: button {}", std::source_location::current(), event.gbutton.button);
                break;
            case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                if (m_gamepadAxisCallback) m_gamepadAxisCallback(event.gaxis);
                logger.log(Logging::LogLevel::Debug, "Gamepad axis event: axis {}, value {}", 
                           std::source_location::current(), event.gaxis.axis, event.gaxis.value);
                break;
            case SDL_EVENT_GAMEPAD_ADDED:
            case SDL_EVENT_GAMEPAD_REMOVED:
                handleGamepadConnection(event.gdevice);
                logger.log(Logging::LogLevel::Info, "Gamepad event: type={:#x}, which={}", 
                           std::source_location::current(), event.gdevice.type, event.gdevice.which);
                break;
        }
    }
    return true;
}

void SDL3Input::setCallbacks(KeyboardCallback kb, MouseButtonCallback mb, MouseMotionCallback mm,
                             MouseWheelCallback mw, TextInputCallback ti, TouchCallback tc,
                             GamepadButtonCallback gb, GamepadAxisCallback ga, GamepadConnectCallback gc,
                             ResizeCallback onResize) {
    Logging::Logger logger;
    m_keyboardCallback = kb;
    m_mouseButtonCallback = mb;
    m_mouseMotionCallback = mm;
    m_mouseWheelCallback = mw;
    m_textInputCallback = ti;
    m_touchCallback = tc; // Added touch callback assignment
    m_gamepadButtonCallback = gb;
    m_gamepadAxisCallback = ga;
    m_gamepadConnectCallback = gc;
    m_resizeCallback = onResize;
    logger.log(Logging::LogLevel::Info, "Input callbacks set", std::source_location::current());
}

void SDL3Input::enableTextInput(SDL_Window* window, bool enable) {
    Logging::Logger logger;
    if (enable) {
        SDL_StartTextInput(window);
        logger.log(Logging::LogLevel::Info, "Text input enabled", std::source_location::current());
    } else {
        SDL_StopTextInput(window);
        logger.log(Logging::LogLevel::Info, "Text input disabled", std::source_location::current());
    }
}

void SDL3Input::handleTouch(const SDL_TouchFingerEvent& t) {
    Logging::Logger logger;
    logger.log(Logging::LogLevel::Debug, "Touch event: finger {}, x={}, y={}", 
               std::source_location::current(), t.fingerID, t.x, t.y);
    if (m_touchCallback) m_touchCallback(t);
}

void SDL3Input::handleKeyboard(const SDL_KeyboardEvent& k, SDL_Window* window, SDL_AudioDeviceID audioDevice, bool& consoleOpen) {
    Logging::Logger logger;
    if (k.type != SDL_EVENT_KEY_DOWN) return;
    logger.log(Logging::LogLevel::Debug, "Handling key: {}", std::source_location::current(), SDL_GetKeyName(k.key));
    switch (k.key) {
        case SDLK_F:
            {
                Uint32 flags = SDL_GetWindowFlags(window);
                bool isFullscreen = flags & SDL_WINDOW_FULLSCREEN;
                SDL_SetWindowFullscreen(window, isFullscreen ? 0 : SDL_WINDOW_FULLSCREEN);
                logger.log(Logging::LogLevel::Info, "Toggling fullscreen mode", std::source_location::current());
            }
            break;
        case SDLK_ESCAPE:
            {
                SDL_Event evt{.type = SDL_EVENT_QUIT};
                SDL_PushEvent(&evt);
                logger.log(Logging::LogLevel::Info, "Pushing quit event", std::source_location::current());
            }
            break;
        case SDLK_SPACE:
            if (audioDevice) {
                if (SDL_AudioDevicePaused(audioDevice)) {
                    SDL_ResumeAudioDevice(audioDevice);
                    logger.log(Logging::LogLevel::Info, "Resuming audio device", std::source_location::current());
                } else {
                    SDL_PauseAudioDevice(audioDevice);
                    logger.log(Logging::LogLevel::Info, "Pausing audio device", std::source_location::current());
                }
            } else {
                logger.log(Logging::LogLevel::Warning, "No audio device available for pause/resume", std::source_location::current());
            }
            break;
        case SDLK_M:
            if (audioDevice) {
                float gain = SDL_GetAudioDeviceGain(audioDevice);
                SDL_SetAudioDeviceGain(audioDevice, gain == 0.0f ? 1.0f : 0.0f);
                logger.log(Logging::LogLevel::Info, "Toggling audio {}", std::source_location::current(), gain == 0.0f ? "unmuted" : "muted");
            } else {
                logger.log(Logging::LogLevel::Warning, "No audio device available for mute/unmute", std::source_location::current());
            }
            break;
        case SDLK_GRAVE:
            consoleOpen = !consoleOpen;
            logger.log(Logging::LogLevel::Info, "Toggling console: {}", std::source_location::current(), consoleOpen ? "open" : "closed");
            break;
    }
}

void SDL3Input::handleMouseButton(const SDL_MouseButtonEvent& b, SDL_Window* window) {
    Logging::Logger logger;
    logger.log(Logging::LogLevel::Debug, "{} mouse button {} at ({}, {})",
               std::source_location::current(),
               b.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? "Pressed" : "Released",
               b.button == SDL_BUTTON_LEFT ? "Left" : b.button == SDL_BUTTON_RIGHT ? "Right" : "Middle",
               b.x, b.y);
    if (b.type == SDL_EVENT_MOUSE_BUTTON_DOWN && b.button == SDL_BUTTON_RIGHT) {
        bool relative = SDL_GetWindowRelativeMouseMode(window);
        SDL_SetWindowRelativeMouseMode(window, !relative);
        logger.log(Logging::LogLevel::Info, "Toggling relative mouse mode: {}", std::source_location::current(), !relative ? "enabled" : "disabled");
    }
}

void SDL3Input::handleGamepadButton(const SDL_GamepadButtonEvent& g, SDL_AudioDeviceID audioDevice) {
    Logging::Logger logger;
    if (g.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        logger.log(Logging::LogLevel::Debug, "Gamepad button down: {}", std::source_location::current(), g.button);
        switch (g.button) {
            case SDL_GAMEPAD_BUTTON_SOUTH:
                logger.log(Logging::LogLevel::Info, "South (A/Cross) pressed", std::source_location::current());
                break;
            case SDL_GAMEPAD_BUTTON_EAST:
                {
                    SDL_Event evt{.type = SDL_EVENT_QUIT};
                    SDL_PushEvent(&evt);
                    logger.log(Logging::LogLevel::Info, "East (B/Circle) pressed, pushing quit event", std::source_location::current());
                }
                break;
            case SDL_GAMEPAD_BUTTON_WEST:
                logger.log(Logging::LogLevel::Info, "West (X/Square) pressed", std::source_location::current());
                break;
            case SDL_GAMEPAD_BUTTON_NORTH:
                logger.log(Logging::LogLevel::Info, "North (Y/Triangle) pressed", std::source_location::current());
                break;
            case SDL_GAMEPAD_BUTTON_START:
                if (audioDevice) {
                    if (SDL_AudioDevicePaused(audioDevice)) {
                        SDL_ResumeAudioDevice(audioDevice);
                        logger.log(Logging::LogLevel::Info, "Start pressed, resuming audio device", std::source_location::current());
                    } else {
                        SDL_PauseAudioDevice(audioDevice);
                        logger.log(Logging::LogLevel::Info, "Start pressed, pausing audio device", std::source_location::current());
                    }
                } else {
                    logger.log(Logging::LogLevel::Warning, "Start pressed, no audio device available for pause/resume", std::source_location::current());
                }
                break;
            default:
                logger.log(Logging::LogLevel::Info, "Button {} pressed", std::source_location::current(), static_cast<int>(g.button));
                break;
        }
    } else {
        logger.log(Logging::LogLevel::Debug, "Gamepad button {} released", std::source_location::current(), static_cast<int>(g.button));
    }
}

void SDL3Input::handleGamepadConnection(const SDL_GamepadDeviceEvent& e) {
    Logging::Logger logger;
    if (e.type == SDL_EVENT_GAMEPAD_ADDED) {
        if (auto gp = SDL_OpenGamepad(e.which)) {
            m_gamepads[e.which] = gp;
            if (m_gamepadConnectCallback) m_gamepadConnectCallback(true, e.which, gp);
            logger.log(Logging::LogLevel::Info, "Gamepad connected: ID {}", std::source_location::current(), e.which);
        } else {
            logger.log(Logging::LogLevel::Warning, "Failed to open gamepad: ID {}, error: {}", 
                       std::source_location::current(), e.which, SDL_GetError());
        }
    } else if (e.type == SDL_EVENT_GAMEPAD_REMOVED) {
        auto it = m_gamepads.find(e.which);
        if (it != m_gamepads.end()) {
            if (m_gamepadConnectCallback) m_gamepadConnectCallback(false, e.which, it->second);
            SDL_CloseGamepad(it->second);
            m_gamepads.erase(it);
            logger.log(Logging::LogLevel::Info, "Gamepad disconnected: ID {}", std::source_location::current(), e.which);
        } else {
            logger.log(Logging::LogLevel::Warning, "Gamepad disconnect event for unknown ID {}", std::source_location::current(), e.which);
        }
    }
}

void SDL3Input::exportLog(const std::string& filename) const {
    Logging::Logger logger;
    logger.log(Logging::LogLevel::Info, "Exporting log to {}", std::source_location::current(), filename);
    if (m_logCallback) m_logCallback(std::format("Log exported to {}", filename));
    // Basic implementation: write a placeholder log message to the file
    std::ofstream outFile(filename, std::ios::app);
    if (outFile.is_open()) {
        outFile << "SDL3Input log exported at " << std::time(nullptr) << "\n";
        outFile.close();
    } else {
        logger.log(Logging::LogLevel::Error, "Failed to open log file {}", std::source_location::current(), filename);
    }
}

} // namespace SDL3Initializer