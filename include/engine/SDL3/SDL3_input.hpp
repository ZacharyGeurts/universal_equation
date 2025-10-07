#ifndef SDL3_INPUT_HPP
#define SDL3_INPUT_HPP

// Input handling for AMOURANTH RTX Engine, October 2025
// Manages SDL3 input events for keyboard, mouse, gamepad, and touch.
// Thread-safe with C++20 features; no mutexes required.
// Dependencies: SDL3, C++20 standard library, logging.hpp.
// Usage: Initialize with log callback, set event callbacks, and poll events in main loop.
// Zachary Geurts 2025

#include <SDL3/SDL.h>
#include <functional>
#include <map>
#include <string>
#include "engine/logging.hpp"
#include <format>

namespace SDL3Initializer {

class SDL3Input {
public:
    using KeyboardCallback = std::function<void(const SDL_KeyboardEvent&)>;
    using MouseButtonCallback = std::function<void(const SDL_MouseButtonEvent&)>;
    using MouseMotionCallback = std::function<void(const SDL_MouseMotionEvent&)>;
    using MouseWheelCallback = std::function<void(const SDL_MouseWheelEvent&)>;
    using TextInputCallback = std::function<void(const SDL_TextInputEvent&)>;
    using TouchCallback = std::function<void(const SDL_TouchFingerEvent&)>;
    using GamepadButtonCallback = std::function<void(const SDL_GamepadButtonEvent&)>;
    using GamepadAxisCallback = std::function<void(const SDL_GamepadAxisEvent&)>;
    using GamepadConnectCallback = std::function<void(bool, SDL_JoystickID, SDL_Gamepad*)>;
    using ResizeCallback = std::function<void(int, int)>;
    using LogCallback = std::function<void(const std::string&)>;

    SDL3Input(const Logging::Logger& logger) : logger_(logger) {}
    ~SDL3Input(); // Added destructor declaration

    void initialize(LogCallback logCallback);
    bool pollEvents(SDL_Window* window, SDL_AudioDeviceID audioDevice, bool& consoleOpen, bool exitOnClose);
    void setCallbacks(KeyboardCallback kb, MouseButtonCallback mb, MouseMotionCallback mm,
                      MouseWheelCallback mw, TextInputCallback ti, TouchCallback tc,
                      GamepadButtonCallback gb, GamepadAxisCallback ga, GamepadConnectCallback gc,
                      ResizeCallback onResize);
    void enableTextInput(SDL_Window* window, bool enable);
    const std::map<SDL_JoystickID, SDL_Gamepad*>& getGamepads() const { return m_gamepads; }
    void exportLog(const std::string& filename) const;

private:
    void handleKeyboard(const SDL_KeyboardEvent& k, SDL_Window* window, SDL_AudioDeviceID audioDevice, bool& consoleOpen);
    void handleMouseButton(const SDL_MouseButtonEvent& b, SDL_Window* window);
    void handleTouch(const SDL_TouchFingerEvent& t);
    void handleGamepadButton(const SDL_GamepadButtonEvent& g, SDL_AudioDeviceID audioDevice);
    void handleGamepadConnection(const SDL_GamepadDeviceEvent& e);

    std::map<SDL_JoystickID, SDL_Gamepad*> m_gamepads;
    KeyboardCallback m_keyboardCallback;
    MouseButtonCallback m_mouseButtonCallback;
    MouseMotionCallback m_mouseMotionCallback;
    MouseWheelCallback m_mouseWheelCallback;
    TextInputCallback m_textInputCallback;
    TouchCallback m_touchCallback;
    GamepadButtonCallback m_gamepadButtonCallback;
    GamepadAxisCallback m_gamepadAxisCallback;
    GamepadConnectCallback m_gamepadConnectCallback;
    ResizeCallback m_resizeCallback;
    LogCallback m_logCallback;
    const Logging::Logger& logger_;
};

} // namespace SDL3Initializer

// Custom formatter for SDL_EventType
namespace std {
template<>
struct formatter<SDL_EventType, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template<typename FormatContext>
    auto format(SDL_EventType type, FormatContext& ctx) const {
        switch (type) {
            case SDL_EVENT_QUIT: return format_to(ctx.out(), "SDL_EVENT_QUIT");
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED: return format_to(ctx.out(), "SDL_EVENT_WINDOW_CLOSE_REQUESTED");
            case SDL_EVENT_WINDOW_RESIZED: return format_to(ctx.out(), "SDL_EVENT_WINDOW_RESIZED");
            case SDL_EVENT_KEY_DOWN: return format_to(ctx.out(), "SDL_EVENT_KEY_DOWN");
            case SDL_EVENT_KEY_UP: return format_to(ctx.out(), "SDL_EVENT_KEY_UP");
            case SDL_EVENT_MOUSE_BUTTON_DOWN: return format_to(ctx.out(), "SDL_EVENT_MOUSE_BUTTON_DOWN");
            case SDL_EVENT_MOUSE_BUTTON_UP: return format_to(ctx.out(), "SDL_EVENT_MOUSE_BUTTON_UP");
            case SDL_EVENT_MOUSE_MOTION: return format_to(ctx.out(), "SDL_EVENT_MOUSE_MOTION");
            case SDL_EVENT_MOUSE_WHEEL: return format_to(ctx.out(), "SDL_EVENT_MOUSE_WHEEL");
            case SDL_EVENT_TEXT_INPUT: return format_to(ctx.out(), "SDL_EVENT_TEXT_INPUT");
            case SDL_EVENT_FINGER_DOWN: return format_to(ctx.out(), "SDL_EVENT_FINGER_DOWN");
            case SDL_EVENT_FINGER_UP: return format_to(ctx.out(), "SDL_EVENT_FINGER_UP");
            case SDL_EVENT_FINGER_MOTION: return format_to(ctx.out(), "SDL_EVENT_FINGER_MOTION");
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN: return format_to(ctx.out(), "SDL_EVENT_GAMEPAD_BUTTON_DOWN");
            case SDL_EVENT_GAMEPAD_BUTTON_UP: return format_to(ctx.out(), "SDL_EVENT_GAMEPAD_BUTTON_UP");
            case SDL_EVENT_GAMEPAD_AXIS_MOTION: return format_to(ctx.out(), "SDL_EVENT_GAMEPAD_AXIS_MOTION");
            case SDL_EVENT_GAMEPAD_ADDED: return format_to(ctx.out(), "SDL_EVENT_GAMEPAD_ADDED");
            case SDL_EVENT_GAMEPAD_REMOVED: return format_to(ctx.out(), "SDL_EVENT_GAMEPAD_REMOVED");
            default: return format_to(ctx.out(), "Unknown SDL_EventType({})", static_cast<uint32_t>(type));
        }
    }
};
} // namespace std

#endif // SDL3_INPUT_HPP