#ifndef HANDLEINPUT_HPP
#define HANDLEINPUT_HPP

// Input handling for AMOURANTH RTX Engine, October 2025
// Manages SDL3 input events for keyboard, mouse, gamepad, and touch.
// Thread-safe with C++20 features; no mutexes required.
// Dependencies: SDL3, C++20 standard library.
// Usage: Instantiate with AMOURANTH and DimensionalNavigator; set callbacks for input events.
// Zachary Geurts 2025

#include <SDL3/SDL.h>
#include <functional>
#include <stdexcept>

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

    HandleInput(AMOURANTH* amouranth, DimensionalNavigator* navigator) : amouranth_(amouranth), navigator_(navigator) {
        if (!amouranth || !navigator) {
            throw std::runtime_error("HandleInput: Null AMOURANTH or DimensionalNavigator provided.");
        }
        // Set default callbacks
        keyboardCallback_ = [this](const SDL_KeyboardEvent& key) { defaultKeyboardHandler(key); };
        mouseButtonCallback_ = [this](const SDL_MouseButtonEvent& mb) { defaultMouseButtonHandler(mb); };
        mouseMotionCallback_ = [this](const SDL_MouseMotionEvent& mm) { defaultMouseMotionHandler(mm); };
        mouseWheelCallback_ = [this](const SDL_MouseWheelEvent& mw) { defaultMouseWheelHandler(mw); };
        textInputCallback_ = [this](const SDL_TextInputEvent& ti) { defaultTextInputHandler(ti); };
        touchCallback_ = [this](const SDL_TouchFingerEvent& tf) { defaultTouchHandler(tf); };
        gamepadButtonCallback_ = [this](const SDL_GamepadButtonEvent& gb) { defaultGamepadButtonHandler(gb); };
        gamepadAxisCallback_ = [this](const SDL_GamepadAxisEvent& ga) { defaultGamepadAxisHandler(ga); };
        gamepadConnectCallback_ = [this](bool connected, SDL_JoystickID id, SDL_Gamepad* pad) { defaultGamepadConnectHandler(connected, id, pad); };
    }

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
    ) {
        if (kb) keyboardCallback_ = kb;
        if (mb) mouseButtonCallback_ = mb;
        if (mm) mouseMotionCallback_ = mm;
        if (mw) mouseWheelCallback_ = mw;
        if (ti) textInputCallback_ = ti;
        if (tc) touchCallback_ = tc;
        if (gb) gamepadButtonCallback_ = gb;
        if (ga) gamepadAxisCallback_ = ga;
        if (gc) gamepadConnectCallback_ = gc;
    }

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
    void defaultKeyboardHandler(const SDL_KeyboardEvent& key) {
        if (key.type == SDL_EVENT_KEY_DOWN) {
            switch (key.keysym.sym) {
                case SDLK_1: case SDLK_2: case SDLK_3: case SDLK_4:
                case SDLK_5: case SDLK_6: case SDLK_7: case SDLK_8: case SDLK_9:
                    amouranth_->setMode(key.keysym.sym - SDLK_0); break;
                case SDLK_p: amouranth_->togglePause(); break;
                case SDLK_c: amouranth_->toggleUserCam(); break;
                case SDLK_i: amouranth_->adjustInfluence(0.1); break;
                case SDLK_k: amouranth_->adjustInfluence(-0.1); break;
                case SDLK_m: amouranth_->adjustDarkMatter(0.1); break;
                case SDLK_n: amouranth_->adjustDarkMatter(-0.1); break;
                case SDLK_e: amouranth_->adjustDarkEnergy(0.1); break;
                case SDLK_r: amouranth_->adjustDarkEnergy(-0.1); break;
            }
        }
    }

    void defaultMouseButtonHandler(const SDL_MouseButtonEvent& mb) {
        if (mb.type == SDL_EVENT_MOUSE_BUTTON_DOWN && mb.button == SDL_BUTTON_RIGHT) {
            amouranth_->toggleUserCam();
        }
    }

    void defaultMouseMotionHandler(const SDL_MouseMotionEvent& mm) {
        if (amouranth_->isUserCamActive()) {
            amouranth_->moveUserCam(mm.xrel * 0.01f, mm.yrel * 0.01f, 0.0f);
        }
    }

    void defaultMouseWheelHandler(const SDL_MouseWheelEvent& mw) {
        amouranth_->updateZoom(mw.y > 0);
    }

    void defaultTextInputHandler(const SDL_TextInputEvent& /*ti*/) {
        // No-op by default
    }

    void defaultTouchHandler(const SDL_TouchFingerEvent& /*tf*/) {
        // No-op by default
    }

    void defaultGamepadButtonHandler(const SDL_GamepadButtonEvent& gb) {
        if (gb.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
            switch (gb.button) {
                case SDL_GAMEPAD_BUTTON_SOUTH: amouranth_->togglePause(); break;
                case SDL_GAMEPAD_BUTTON_EAST: amouranth_->toggleUserCam(); break;
            }
        }
    }

    void defaultGamepadAxisHandler(const SDL_GamepadAxisEvent& ga) {
        if (amouranth_->isUserCamActive()) {
            float value = ga.value / 32768.0f;
            if (ga.axis == SDL_GAMEPAD_AXIS_LEFTX) {
                amouranth_->moveUserCam(value * 0.1f, 0.0f, 0.0f);
            } else if (ga.axis == SDL_GAMEPAD_AXIS_LEFTY) {
                amouranth_->moveUserCam(0.0f, value * 0.1f, 0.0f);
            }
        }
    }

    void defaultGamepadConnectHandler(bool /*connected*/, SDL_JoystickID /*id*/, SDL_Gamepad* /*pad*/) {
        // No-op by default
    }

    AMOURANTH* amouranth_;
    DimensionalNavigator* navigator_;
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