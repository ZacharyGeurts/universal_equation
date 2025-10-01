// handleinput.cpp
#include <iostream>
#include "handleinput.hpp"
#include "core.hpp"

HandleInput::HandleInput(AMOURANTH* amouranth, DimensionalNavigator* navigator)
    : amouranth_(amouranth), navigator_(navigator) {
    if (!amouranth || !navigator) {
        throw std::runtime_error("HandleInput: Null AMOURANTH or DimensionalNavigator provided.");
    }
}

void HandleInput::setCallbacks(
    KeyboardCallback kb,
    MouseButtonCallback mb,
    MouseMotionCallback mm,
    MouseWheelCallback mw,
    TextInputCallback ti,
    TouchCallback tc,
    GamepadButtonCallback gb,
    GamepadAxisCallback ga,
    GamepadConnectCallback gc
) {
    keyboardCallback_ = kb ? kb : [this](const SDL_KeyboardEvent& key) { defaultKeyboardHandler(key); };
    mouseButtonCallback_ = mb ? mb : [this](const SDL_MouseButtonEvent& mb) { defaultMouseButtonHandler(mb); };
    mouseMotionCallback_ = mm ? mm : [this](const SDL_MouseMotionEvent& mm) { defaultMouseMotionHandler(mm); };
    mouseWheelCallback_ = mw ? mw : [this](const SDL_MouseWheelEvent& mw) { defaultMouseWheelHandler(mw); };
    textInputCallback_ = ti ? ti : [this](const SDL_TextInputEvent& ti) { defaultTextInputHandler(ti); };
    touchCallback_ = tc ? tc : [this](const SDL_TouchFingerEvent& tf) { defaultTouchHandler(tf); };
    gamepadButtonCallback_ = gb ? gb : [this](const SDL_GamepadButtonEvent& gb) { defaultGamepadButtonHandler(gb); };
    gamepadAxisCallback_ = ga ? ga : [this](const SDL_GamepadAxisEvent& ga) { defaultGamepadAxisHandler(ga); };
    gamepadConnectCallback_ = gc ? gc : [this](bool connected, SDL_JoystickID id, SDL_Gamepad* pad) { defaultGamepadConnectHandler(connected, id, pad); };
}

void HandleInput::defaultKeyboardHandler(const SDL_KeyboardEvent& key) {
    if (key.type == SDL_EVENT_KEY_DOWN) {
        switch (key.key) {
            case SDLK_PLUS: case SDLK_KP_PLUS: amouranth_->updateZoom(true); break;
            case SDLK_MINUS: case SDLK_KP_MINUS: amouranth_->updateZoom(false); break;
            case SDLK_I: amouranth_->adjustInfluence(0.1); break;
            case SDLK_O: amouranth_->adjustInfluence(-0.1); break;
            case SDLK_J: amouranth_->adjustDarkMatter(0.1); break;
            case SDLK_K: amouranth_->adjustDarkMatter(-0.1); break;
            case SDLK_N: amouranth_->adjustDarkEnergy(0.1); break;
            case SDLK_M: amouranth_->adjustDarkEnergy(-0.1); break;
            case SDLK_P: amouranth_->togglePause(); break;
            case SDLK_C: amouranth_->toggleUserCam(); break;
            case SDLK_W: if (amouranth_->isUserCamActive()) amouranth_->moveUserCam(0.0f, 0.0f, -0.1f); break;
            case SDLK_S: if (amouranth_->isUserCamActive()) amouranth_->moveUserCam(0.0f, 0.0f, 0.1f); break;
            case SDLK_A: if (amouranth_->isUserCamActive()) amouranth_->moveUserCam(-0.1f, 0.0f, 0.0f); break;
            case SDLK_D: if (amouranth_->isUserCamActive()) amouranth_->moveUserCam(0.1f, 0.0f, 0.0f); break;
            case SDLK_Q: if (amouranth_->isUserCamActive()) amouranth_->moveUserCam(0.0f, 0.1f, 0.0f); break;
            case SDLK_E: if (amouranth_->isUserCamActive()) amouranth_->moveUserCam(0.0f, -0.1f, 0.0f); break;
            case SDLK_1: case SDLK_2: case SDLK_3: case SDLK_4: case SDLK_5:
            case SDLK_6: case SDLK_7: case SDLK_8: case SDLK_9:
                amouranth_->setMode(key.key - SDLK_0);
                navigator_->setMode(key.key - SDLK_0);
                break;
        }
    }
}

void HandleInput::defaultMouseButtonHandler(const SDL_MouseButtonEvent& mb) {
    if (mb.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        if (mb.button == SDL_BUTTON_LEFT) {
            amouranth_->toggleUserCam();
        } else if (mb.button == SDL_BUTTON_RIGHT) {
            amouranth_->togglePause();
        }
    }
}

void HandleInput::defaultMouseMotionHandler(const SDL_MouseMotionEvent& mm) {
    if (amouranth_->isUserCamActive()) {
        float dx = mm.xrel * 0.005f;
        float dy = mm.yrel * 0.005f;
        amouranth_->moveUserCam(-dx, -dy, 0.0f);
    }
}

void HandleInput::defaultMouseWheelHandler(const SDL_MouseWheelEvent& mw) {
    if (mw.y > 0) {
        amouranth_->updateZoom(true);
    } else if (mw.y < 0) {
        amouranth_->updateZoom(false);
    }
}

void HandleInput::defaultTextInputHandler([[maybe_unused]] const SDL_TextInputEvent& ti) {
    // Placeholder for future text-based UI
}

void HandleInput::defaultTouchHandler(const SDL_TouchFingerEvent& tf) {
    if (tf.type == SDL_EVENT_FINGER_DOWN) {
        amouranth_->toggleUserCam();
    } else if (tf.type == SDL_EVENT_FINGER_MOTION) {
        if (amouranth_->isUserCamActive()) {
            float dx = tf.dx * 0.1f;
            float dy = tf.dy * 0.1f;
            amouranth_->moveUserCam(-dx, -dy, 0.0f);
        }
    }
}

void HandleInput::defaultGamepadButtonHandler(const SDL_GamepadButtonEvent& gb) {
    if (gb.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        switch (gb.button) {
            case SDL_GAMEPAD_BUTTON_SOUTH: amouranth_->togglePause(); break;
            case SDL_GAMEPAD_BUTTON_EAST: amouranth_->toggleUserCam(); break;
            case SDL_GAMEPAD_BUTTON_DPAD_UP: amouranth_->updateZoom(true); break;
            case SDL_GAMEPAD_BUTTON_DPAD_DOWN: amouranth_->updateZoom(false); break;
            case SDL_GAMEPAD_BUTTON_DPAD_LEFT: amouranth_->adjustInfluence(-0.1); break;
            case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: amouranth_->adjustInfluence(0.1); break;
        }
    }
}

void HandleInput::defaultGamepadAxisHandler(const SDL_GamepadAxisEvent& ga) {
    if (amouranth_->isUserCamActive()) {
        float value = ga.value / 32768.0f; // Normalize to [-1, 1]
        if (ga.axis == SDL_GAMEPAD_AXIS_LEFTX) {
            amouranth_->moveUserCam(value * 0.1f, 0.0f, 0.0f);
        } else if (ga.axis == SDL_GAMEPAD_AXIS_LEFTY) {
            amouranth_->moveUserCam(0.0f, -value * 0.1f, 0.0f);
        } else if (ga.axis == SDL_GAMEPAD_AXIS_RIGHTX) {
            amouranth_->moveUserCam(0.0f, 0.0f, value * 0.1f);
        }
    }
}

void HandleInput::defaultGamepadConnectHandler(bool connected, SDL_JoystickID id, [[maybe_unused]] SDL_Gamepad* pad) {
    if (connected) {
        std::cerr << "Gamepad connected: ID " << id << "\n";
    } else {
        std::cerr << "Gamepad disconnected: ID " << id << "\n";
    }
}