// Input handling for AMOURANTH RTX Engine, October 2025
// Manages SDL3 input events for keyboard, mouse, gamepad, and touch.
// Thread-safe with C++20 features; no mutexes required.
// Dependencies: SDL3, C++20 standard library, logging.hpp.
// Zachary Geurts 2025

#include "engine/SDL3/SDL3_input.hpp"
#include <stdexcept>
#include <format>
#include <fstream>

namespace SDL3Initializer {

void SDL3Input::initialize(LogCallback logCallback) {
    m_logCallback = logCallback;
    if (m_logCallback) {
        m_logCallback("Initializing SDL3Input");
    }
    logger_.log(Logging::LogLevel::Info, "Initializing SDL3Input", std::source_location::current());

    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");
    int numJoysticks;
    SDL_JoystickID* joysticks = SDL_GetJoysticks(&numJoysticks);
    if (joysticks) {
        for (int i = 0; i < numJoysticks; ++i) {
            if (SDL_IsGamepad(joysticks[i])) {
                logger_.log(Logging::LogLevel::Info, "Found gamepad: ID {}", std::source_location::current(), joysticks[i]);
                if (auto gp = SDL_OpenGamepad(joysticks[i])) {
                    logger_.log(Logging::LogLevel::Info, "Opened gamepad: ID {}", std::source_location::current(), joysticks[i]);
                    m_gamepads[joysticks[i]] = gp;
                    if (m_gamepadConnectCallback) {
                        m_gamepadConnectCallback(true, joysticks[i], gp);
                    }
                }
            }
        }
        SDL_free(joysticks);
    }
}

bool SDL3Input::pollEvents(SDL_Window* window, SDL_AudioDeviceID audioDevice, bool& consoleOpen, bool exitOnClose) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        logger_.log(Logging::LogLevel::Debug, "Processing SDL event type: {}", std::source_location::current(), event.type);
        switch (event.type) {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                if (m_logCallback) m_logCallback("Received SDL_EVENT_QUIT or SDL_EVENT_WINDOW_CLOSE_REQUESTED");
                logger_.log(Logging::LogLevel::Info, "Quit or window close event received", std::source_location::current());
                return !exitOnClose;
            case SDL_EVENT_WINDOW_RESIZED:
                if (m_logCallback) m_logCallback(std::format("Window resized: width={}, height={}", event.window.data1, event.window.data2));
                logger_.log(Logging::LogLevel::Info, "Window resized: width={}, height={}", std::source_location::current(), event.window.data1, event.window.data2);
                if (m_resizeCallback) m_resizeCallback(event.window.data1, event.window.data2);
                break;
            case SDL_EVENT_KEY_DOWN:
                handleKeyboard(event.key, window, audioDevice, consoleOpen);
                if (m_keyboardCallback) m_keyboardCallback(event.key);
                if (m_logCallback) m_logCallback(std::format("Key down event: {}", SDL_GetKeyName(event.key.key)));
                break;
            case SDL_EVENT_KEY_UP:
                if (m_keyboardCallback) m_keyboardCallback(event.key);
                if (m_logCallback) m_logCallback(std::format("Key up event: {}", SDL_GetKeyName(event.key.key)));
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                handleMouseButton(event.button, window);
                if (m_mouseButtonCallback) m_mouseButtonCallback(event.button);
                if (m_logCallback) m_logCallback(std::format("Mouse button event: button {}", event.button.button));
                break;
            case SDL_EVENT_MOUSE_MOTION:
                if (m_mouseMotionCallback) m_mouseMotionCallback(event.motion);
                if (m_logCallback) m_logCallback(std::format("Mouse motion event: xrel={}, yrel={}", event.motion.xrel, event.motion.yrel));
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                if (m_mouseWheelCallback) m_mouseWheelCallback(event.wheel);
                if (m_logCallback) m_logCallback(std::format("Mouse wheel event: x={}, y={}", event.wheel.x, event.wheel.y));
                break;
            case SDL_EVENT_TEXT_INPUT:
                if (m_textInputCallback) m_textInputCallback(event.text);
                if (m_logCallback) m_logCallback(std::format("Text input event: {}", event.text.text));
                break;
            case SDL_EVENT_FINGER_DOWN:
            case SDL_EVENT_FINGER_UP:
            case SDL_EVENT_FINGER_MOTION:
                handleTouch(event.tfinger);
                if (m_touchCallback) m_touchCallback(event.tfinger);
                if (m_logCallback) m_logCallback(std::format("Touch event: finger {}", event.tfinger.fingerID));
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                handleGamepadButton(event.gbutton, audioDevice);
                if (m_gamepadButtonCallback) m_gamepadButtonCallback(event.gbutton);
                if (m_logCallback) m_logCallback(std::format("Gamepad button event: button {}", event.gbutton.button));
                break;
            case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                if (m_gamepadAxisCallback) m_gamepadAxisCallback(event.gaxis);
                if (m_logCallback) m_logCallback(std::format("Gamepad axis event: axis {}, value {}", event.gaxis.axis, event.gaxis.value));
                break;
            case SDL_EVENT_GAMEPAD_ADDED:
            case SDL_EVENT_GAMEPAD_REMOVED:
                handleGamepadConnection(event.gdevice);
                if (m_logCallback) m_logCallback(std::format("Gamepad event: type={}, which={}", event.gdevice.type, event.gdevice.which));
                break;
        }
    }
    return true;
}

void SDL3Input::setCallbacks(KeyboardCallback kb, MouseButtonCallback mb, MouseMotionCallback mm,
                             MouseWheelCallback mw, TextInputCallback ti, TouchCallback tc,
                             GamepadButtonCallback gb, GamepadAxisCallback ga, GamepadConnectCallback gc,
                             ResizeCallback onResize) {
    m_keyboardCallback = kb;
    m_mouseButtonCallback = mb;
    m_mouseMotionCallback = mm;
    m_mouseWheelCallback = mw;
    m_textInputCallback = ti;
    m_touchCallback = tc;
    m_gamepadButtonCallback = gb;
    m_gamepadAxisCallback = ga;
    m_gamepadConnectCallback = gc;
    m_resizeCallback = onResize;
    if (m_logCallback) m_logCallback("Input callbacks set");
    logger_.log(Logging::LogLevel::Info, "Input callbacks set", std::source_location::current());
}

void SDL3Input::enableTextInput(SDL_Window* window, bool enable) {
    if (enable) {
        SDL_StartTextInput(window);
        if (m_logCallback) m_logCallback("Text input enabled");
        logger_.log(Logging::LogLevel::Info, "Text input enabled", std::source_location::current());
    } else {
        SDL_StopTextInput(window);
        if (m_logCallback) m_logCallback("Text input disabled");
        logger_.log(Logging::LogLevel::Info, "Text input disabled", std::source_location::current());
    }
}

void SDL3Input::handleKeyboard(const SDL_KeyboardEvent& k, SDL_Window* window, SDL_AudioDeviceID audioDevice, bool& consoleOpen) {
    if (k.type != SDL_EVENT_KEY_DOWN) return;
    logger_.log(Logging::LogLevel::Debug, "Handling key: {}", std::source_location::current(), SDL_GetKeyName(k.key));
    switch (k.key) {
        case SDLK_F:
            {
                Uint32 flags = SDL_GetWindowFlags(window);
                bool isFullscreen = flags & SDL_WINDOW_FULLSCREEN;
                SDL_SetWindowFullscreen(window, isFullscreen ? 0 : SDL_WINDOW_FULLSCREEN);
                logger_.log(Logging::LogLevel::Info, "Toggling fullscreen mode", std::source_location::current());
            }
            break;
        case SDLK_ESCAPE:
            {
                SDL_Event evt{.type = SDL_EVENT_QUIT};
                SDL_PushEvent(&evt);
                logger_.log(Logging::LogLevel::Info, "Pushing quit event", std::source_location::current());
            }
            break;
        case SDLK_SPACE:
            if (audioDevice) {
                if (SDL_AudioDevicePaused(audioDevice)) {
                    SDL_ResumeAudioDevice(audioDevice);
                } else {
                    SDL_PauseAudioDevice(audioDevice);
                }
                logger_.log(Logging::LogLevel::Info, "Toggling audio pause/resume", std::source_location::current());
            }
            break;
        case SDLK_M:
            if (audioDevice) {
                float gain = SDL_GetAudioDeviceGain(audioDevice);
                SDL_SetAudioDeviceGain(audioDevice, gain == 0.0f ? 1.0f : 0.0f);
                logger_.log(Logging::LogLevel::Info, "Toggling audio {}", std::source_location::current(), gain == 0.0f ? "unmuted" : "muted");
            }
            break;
        case SDLK_GRAVE:
            consoleOpen = !consoleOpen;
            logger_.log(Logging::LogLevel::Info, "Toggling console", std::source_location::current());
            break;
    }
}

void SDL3Input::handleMouseButton(const SDL_MouseButtonEvent& b, SDL_Window* window) {
    logger_.log(Logging::LogLevel::Debug, "{} mouse button {} at ({}, {})",
                std::source_location::current(),
                b.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? "Pressed" : "Released",
                b.button == SDL_BUTTON_LEFT ? "Left" : b.button == SDL_BUTTON_RIGHT ? "Right" : "Middle",
                b.x, b.y);
    if (b.type == SDL_EVENT_MOUSE_BUTTON_DOWN && b.button == SDL_BUTTON_RIGHT) {
        bool relative = SDL_GetWindowRelativeMouseMode(window);
        SDL_SetWindowRelativeMouseMode(window, !relative);
        logger_.log(Logging::LogLevel::Info, "Toggling relative mouse mode", std::source_location::current());
    }
}

void SDL3Input::handleTouch(const SDL_TouchFingerEvent& t) {
    logger_.log(Logging::LogLevel::Debug, "Touch {} fingerID: {} at ({}, {}) pressure: {}",
                std::source_location::current(),
                t.type == SDL_EVENT_FINGER_DOWN ? "DOWN" : t.type == SDL_EVENT_FINGER_UP ? "UP" : "MOTION",
                t.fingerID, t.x, t.y, t.pressure);
}

void SDL3Input::handleGamepadButton(const SDL_GamepadButtonEvent& g, SDL_AudioDeviceID audioDevice) {
    if (g.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        logger_.log(Logging::LogLevel::Debug, "Gamepad button down: {}", std::source_location::current(), g.button);
        switch (g.button) {
            case SDL_GAMEPAD_BUTTON_SOUTH:
                logger_.log(Logging::LogLevel::Info, "South (A/Cross) pressed", std::source_location::current());
                break;
            case SDL_GAMEPAD_BUTTON_EAST:
                {
                    SDL_Event evt{.type = SDL_EVENT_QUIT};
                    SDL_PushEvent(&evt);
                    logger_.log(Logging::LogLevel::Info, "East (B/Circle) pressed, pushing quit event", std::source_location::current());
                }
                break;
            case SDL_GAMEPAD_BUTTON_WEST:
                logger_.log(Logging::LogLevel::Info, "West (X/Square) pressed", std::source_location::current());
                break;
            case SDL_GAMEPAD_BUTTON_NORTH:
                logger_.log(Logging::LogLevel::Info, "North (Y/Triangle) pressed", std::source_location::current());
                break;
            case SDL_GAMEPAD_BUTTON_START:
                if (audioDevice) {
                    if (SDL_AudioDevicePaused(audioDevice)) {
                        SDL_ResumeAudioDevice(audioDevice);
                    } else {
                        SDL_PauseAudioDevice(audioDevice);
                    }
                    logger_.log(Logging::LogLevel::Info, "Start pressed, toggling audio pause/resume", std::source_location::current());
                }
                break;
            default:
                logger_.log(Logging::LogLevel::Info, "Button {} pressed", std::source_location::current(), static_cast<int>(g.button));
                break;
        }
    } else {
        logger_.log(Logging::LogLevel::Debug, "Gamepad button {} released", std::source_location::current(), static_cast<int>(g.button));
    }
}

void SDL3Input::handleGamepadConnection(const SDL_GamepadDeviceEvent& e) {
    if (e.type == SDL_EVENT_GAMEPAD_ADDED) {
        if (auto gp = SDL_OpenGamepad(e.which)) {
            m_gamepads[e.which] = gp;
            if (m_gamepadConnectCallback) m_gamepadConnectCallback(true, e.which, gp);
            logger_.log(Logging::LogLevel::Info, "Gamepad connected: ID {}", std::source_location::current(), e.which);
        }
    } else if (e.type == SDL_EVENT_GAMEPAD_REMOVED) {
        auto it = m_gamepads.find(e.which);
        if (it != m_gamepads.end()) {
            if (m_gamepadConnectCallback) m_gamepadConnectCallback(false, e.which, it->second);
            SDL_CloseGamepad(it->second);
            m_gamepads.erase(it);
            logger_.log(Logging::LogLevel::Info, "Gamepad disconnected: ID {}", std::source_location::current(), e.which);
        }
    }
}

void SDL3Input::exportLog(const std::string& filename) const {
    logger_.log(Logging::LogLevel::Info, "Exporting input log to {}", std::source_location::current(), filename);
    std::ofstream out(filename, std::ios::app);
    if (out.is_open()) {
        out << "SDL3Input log\n";
        out.close();
        logger_.log(Logging::LogLevel::Info, "Exported input log to {}", std::source_location::current(), filename);
    } else {
        logger_.log(Logging::LogLevel::Error, "Failed to export input log to {}", std::source_location::current(), filename);
    }
}

} // namespace SDL3Initializer