// handleinput.hpp
#ifndef HANDLEINPUT_HPP
#define HANDLEINPUT_HPP

// AMOURANTH RTX Engine, Sep 2025 - Input handling for the Dimensional Navigator application.
// Provides a centralized class to manage all SDL3 input types: keyboard, mouse, gamepad, touch, and text input.
// Integrates with AMOURANTH and DimensionalNavigator to handle game-specific input actions (e.g., zoom, mode switching, camera control).
// Features: Thread-safe callbacks, cross-platform compatibility (Linux, Windows, macOS, Android), and extensible input mapping.
// Usage: Instantiate HandleInput with AMOURANTH and DimensionalNavigator pointers, then set callbacks and process events via SDL3Initializer.
// Dependencies: SDL3, C++17 standard library.
// Best Practices:
// - Use setCallbacks to configure input actions for specific game controls.
// - Call processEvent in the main event loop (via SDL3Initializer::eventLoop) to handle input events.
// - Ensure AMOURANTH and DimensionalNavigator are valid before processing inputs to avoid null pointer issues.
// Potential Issues:
// - Verify SDL3 input subsystems (SDL_INIT_GAMEPAD, SDL_INIT_EVENTS) are initialized in SDL3Initializer.
// - On Android, ensure touch input is prioritized for mobile-friendly controls.
// - Handle gamepad connection/disconnection gracefully to avoid crashes.
// Usage example:
//   HandleInput input(amouranth, navigator);
//   input.setCallbacks(...);
//   sdlInitializer.eventLoop(..., input.getKeyboardCallback(), input.getMouseButtonCallback(), ...);
// Zachary Geurts, 2025

#include <SDL3/SDL.h>
#include <functional>
#include <stdexcept>

// Forward declarations
class AMOURANTH;
class DimensionalNavigator;

class HandleInput {
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

    // Constructor: Initializes input handling with AMOURANTH and DimensionalNavigator
    HandleInput(AMOURANTH* amouranth, DimensionalNavigator* navigator);

    // Sets callbacks for all input types, defaulting to internal handlers
    void setCallbacks(
        KeyboardCallback kb = nullptr,
        MouseButtonCallback mb = nullptr,
        MouseMotionCallback mm = nullptr,
        MouseWheelCallback mw = nullptr,
        TextInputCallback ti = nullptr,
        TouchCallback tc = nullptr,
        GamepadButtonCallback gb = nullptr,
        GamepadAxisCallback ga = nullptr,
        GamepadConnectCallback gc = nullptr
    );

    // Getters for callbacks to pass to SDL3Initializer::eventLoop
    KeyboardCallback getKeyboardCallback() const { return keyboardCallback_; }
    MouseButtonCallback getMouseButtonCallback() const { return mouseButtonCallback_; }
    MouseMotionCallback getMouseMotionCallback() const { return mouseMotionCallback_; }
    MouseWheelCallback getMouseWheelCallback() const { return mouseWheelCallback_; }
    TextInputCallback getTextInputCallback() const { return textInputCallback_; }
    TouchCallback getTouchCallback() const { return touchCallback_; }
    GamepadButtonCallback getGamepadButtonCallback() const { return gamepadButtonCallback_; }
    GamepadAxisCallback getGamepadAxisCallback() const { return gamepadAxisCallback_; }
    GamepadConnectCallback getGamepadConnectCallback() const { return gamepadConnectCallback_; }

private:
    // Default input handlers
    void defaultKeyboardHandler(const SDL_KeyboardEvent& key);
    void defaultMouseButtonHandler(const SDL_MouseButtonEvent& mb);
    void defaultMouseMotionHandler(const SDL_MouseMotionEvent& mm);
    void defaultMouseWheelHandler(const SDL_MouseWheelEvent& mw);
    void defaultTextInputHandler(const SDL_TextInputEvent& ti);
    void defaultTouchHandler(const SDL_TouchFingerEvent& tf);
    void defaultGamepadButtonHandler(const SDL_GamepadButtonEvent& gb);
    void defaultGamepadAxisHandler(const SDL_GamepadAxisEvent& ga);
    void defaultGamepadConnectHandler(bool connected, SDL_JoystickID id, SDL_Gamepad* pad);

    AMOURANTH* amouranth_;                     // Pointer to AMOURANTH instance
    DimensionalNavigator* navigator_;           // Pointer to DimensionalNavigator instance
    KeyboardCallback keyboardCallback_;         // Keyboard event callback
    MouseButtonCallback mouseButtonCallback_;   // Mouse button event callback
    MouseMotionCallback mouseMotionCallback_;   // Mouse motion event callback
    MouseWheelCallback mouseWheelCallback_;     // Mouse wheel event callback
    TextInputCallback textInputCallback_;       // Text input event callback
    TouchCallback touchCallback_;               // Touch event callback
    GamepadButtonCallback gamepadButtonCallback_; // Gamepad button event callback
    GamepadAxisCallback gamepadAxisCallback_;   // Gamepad axis event callback
    GamepadConnectCallback gamepadConnectCallback_; // Gamepad connect/disconnect callback
};

#endif // HANDLEINPUT_HPP