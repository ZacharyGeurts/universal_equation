// handleinput.hpp
#ifndef HANDLEINPUT_HPP
#define HANDLEINPUT_HPP

#include <SDL3/SDL.h>
#include <functional>
#include <stdexcept>
#include <mutex>

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

    HandleInput(AMOURANTH* amouranth, DimensionalNavigator* navigator);
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
    void defaultKeyboardHandler(const SDL_KeyboardEvent& key);
    void defaultMouseButtonHandler(const SDL_MouseButtonEvent& mb);
    void defaultMouseMotionHandler(const SDL_MouseMotionEvent& mm);
    void defaultMouseWheelHandler(const SDL_MouseWheelEvent& mw);
    void defaultTextInputHandler(const SDL_TextInputEvent& ti);
    void defaultTouchHandler(const SDL_TouchFingerEvent& tf);
    void defaultGamepadButtonHandler(const SDL_GamepadButtonEvent& gb);
    void defaultGamepadAxisHandler(const SDL_GamepadAxisEvent& ga);
    void defaultGamepadConnectHandler(bool connected, SDL_JoystickID id, SDL_Gamepad* pad);

    AMOURANTH* amouranth_;
    DimensionalNavigator* navigator_;
    mutable std::mutex inputMutex_;
    KeyboardCallback keyboardCallback_;
    MouseButtonCallback mouseButtonCallback_;
    MouseMotionCallback mouseMotionCallback_;
    MouseWheelCallback mouseWheelCallback_;
    TextInputCallback textInputCallback_;
    TouchCallback touchCallback_;
    GamepadButtonCallback gamepadButtonCallback_;
    GamepadAxisCallback gamepadAxisCallback_;
    GamepadConnectCallback gamepadConnectCallback_;
};

#endif // HANDLEINPUT_HPP