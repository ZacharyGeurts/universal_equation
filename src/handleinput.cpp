// handleinput.cpp implementation
// Zachary Geurts 2025 (enhanced by Grok for AMOURANTH RTX)
// Manages user input for the AMOURANTH RTX simulation and visualization pipeline.
// Integrates with AMOURANTH (core simulation, interfacing with UniversalEquation) and DimensionalNavigator (Vulkan-based rendering of n-dimensional hypercube lattices).
// Purpose: Provides real-time control over simulation parameters and 3D visualization for RTX-accelerated rendering.
// Features:
// - Handles keyboard, mouse, touch, and gamepad inputs via SDL3 for cross-platform compatibility.
// - Maps inputs to simulation controls (e.g., influence, dark matter/energy) and visualization (e.g., camera, zoom).
// - Thread-safe with a mutex to protect AMOURANTH and DimensionalNavigator calls in multi-threaded RTX environments.
// - Supports custom callbacks for advanced RTX applications (e.g., custom UI, automation).
// Why AMOURANTH RTX Developers Will Love It:
// - Intuitive controls for real-time simulation tweaking (e.g., I/O for influence, +/- for zoom).
// - Seamless integration with Vulkan rendering via DimensionalNavigator for high-performance RTX visualization.
// - Flexible callback system for extending input handling in complex RTX pipelines.
// Usage: Create HandleInput with AMOURANTH and DimensionalNavigator, set callbacks if needed, and process SDL events in the main loop.
// Example: HandleInput input(amouranth, navigator); input.setCallbacks(...); // Process events in Vulkan render loop
// Updated: Added thread-safety with mutex, optimized comments for RTX developers, suggested input debouncing.
// Note: Ensure AMOURANTH and DimensionalNavigator methods are thread-safe or protected by this mutex for concurrent access.

#include <iostream>
#include <mutex>
#include "handleinput.hpp"
#include "engine/core.hpp"

// Constructor: Initializes HandleInput with AMOURANTH and DimensionalNavigator pointers.
// AMOURANTH manages simulation state (e.g., UniversalEquation parameters), while DimensionalNavigator handles visualization.
// Throws std::runtime_error if either pointer is null.
// Example: HandleInput input(amouranth, navigator); to set up input handling for the simulation.
HandleInput::HandleInput(AMOURANTH* amouranth, DimensionalNavigator* navigator)
    : amouranth_(amouranth), navigator_(navigator) {
    if (!amouranth || !navigator) {
        throw std::runtime_error("HandleInput: Null AMOURANTH or DimensionalNavigator provided.");
    }
}

// Sets custom callback functions for input events, with defaults if none provided.
// Callbacks control simulation parameters (e.g., influence, dark matter) and visualization (e.g., camera, zoom).
// Default handlers provide intuitive controls for RTX visualization (e.g., WASD for camera, +/- for zoom).
// Example: input.setCallbacks(customKeyboard, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
// For RTX developers: Use custom callbacks to integrate with advanced Vulkan pipelines or UI systems.
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
    std::lock_guard<std::mutex> lock(inputMutex_);
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

// Default keyboard handler: Maps keys to AMOURANTH RTX simulation and visualization controls.
// Thread-safe with mutex to prevent concurrent access to AMOURANTH/Navigator.
// Key mappings:
// - +/-: Zoom in/out (adjusts visualization scale via AMOURANTH::updateZoom).
// - I/O: Increase/decrease influence (UniversalEquation::setInfluence).
// - J/K: Increase/decrease dark matter (UniversalEquation::setDarkMatterStrength).
// - N/M: Increase/decrease dark energy (UniversalEquation::setDarkEnergyStrength).
// - P: Toggle simulation pause (AMOURANTH::togglePause).
// - C: Toggle user-controlled camera (AMOURANTH::toggleUserCam).
// - WASD/QE: Move camera in 3D space (AMOURANTH::moveUserCam, when user camera is active).
// - 1-9: Set simulation and visualization mode/dimension (UniversalEquation::setMode, DimensionalNavigator::setMode).
void HandleInput::defaultKeyboardHandler(const SDL_KeyboardEvent& key) {
    std::lock_guard<std::mutex> lock(inputMutex_);
    if (key.type == SDL_EVENT_KEY_DOWN) {
        switch (key.key) {
            case SDLK_PLUS: case SDLK_KP_PLUS:
                amouranth_->updateZoom(true);
                break;
            case SDLK_MINUS: case SDLK_KP_MINUS:
                amouranth_->updateZoom(false);
                break;
            case SDLK_I:
                amouranth_->adjustInfluence(0.1);
                break;
            case SDLK_O:
                amouranth_->adjustInfluence(-0.1);
                break;
            case SDLK_J:
                amouranth_->adjustDarkMatter(0.1);
                break;
            case SDLK_K:
                amouranth_->adjustDarkMatter(-0.1);
                break;
            case SDLK_N:
                amouranth_->adjustDarkEnergy(0.1);
                break;
            case SDLK_M:
                amouranth_->adjustDarkEnergy(-0.1);
                break;
            case SDLK_P:
                amouranth_->togglePause();
                break;
            case SDLK_C:
                amouranth_->toggleUserCam();
                break;
            case SDLK_W:
                if (amouranth_->isUserCamActive()) amouranth_->moveUserCam(0.0f, 0.0f, -0.1f);
                break;
            case SDLK_S:
                if (amouranth_->isUserCamActive()) amouranth_->moveUserCam(0.0f, 0.0f, 0.1f);
                break;
            case SDLK_A:
                if (amouranth_->isUserCamActive()) amouranth_->moveUserCam(-0.1f, 0.0f, 0.0f);
                break;
            case SDLK_D:
                if (amouranth_->isUserCamActive()) amouranth_->moveUserCam(0.1f, 0.0f, 0.0f);
                break;
            case SDLK_Q:
                if (amouranth_->isUserCamActive()) amouranth_->moveUserCam(0.0f, 0.1f, 0.0f);
                break;
            case SDLK_E:
                if (amouranth_->isUserCamActive()) amouranth_->moveUserCam(0.0f, -0.1f, 0.0f);
                break;
            case SDLK_1: case SDLK_2: case SDLK_3: case SDLK_4: case SDLK_5:
            case SDLK_6: case SDLK_7: case SDLK_8: case SDLK_9:
                amouranth_->setMode(key.key - SDLK_0);
                navigator_->setMode(key.key - SDLK_0);
                break;
        }
    }
}

// Default mouse button handler: Maps mouse clicks to simulation controls.
// Thread-safe with mutex for RTX pipeline safety.
// Mappings:
// - Left click: Toggle user-controlled camera (AMOURANTH::toggleUserCam).
// - Right click: Toggle simulation pause (AMOURANTH::togglePause).
void HandleInput::defaultMouseButtonHandler(const SDL_MouseButtonEvent& mb) {
    std::lock_guard<std::mutex> lock(inputMutex_);
    if (mb.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        if (mb.button == SDL_BUTTON_LEFT) {
            amouranth_->toggleUserCam();
        } else if (mb.button == SDL_BUTTON_RIGHT) {
            amouranth_->togglePause();
        }
    }
}

// Default mouse motion handler: Controls camera rotation when user camera is active.
// Thread-safe with mutex to ensure safe AMOURANTH calls in RTX rendering loop.
// Example: Drag mouse to rotate the 3D visualization of hypercube vertices.
void HandleInput::defaultMouseMotionHandler(const SDL_MouseMotionEvent& mm) {
    std::lock_guard<std::mutex> lock(inputMutex_);
    if (amouranth_->isUserCamActive()) {
        float dx = mm.xrel * 0.005f;
        float dy = mm.yrel * 0.005f;
        amouranth_->moveUserCam(-dx, -dy, 0.0f);
    }
}

// Default mouse wheel handler: Adjusts visualization zoom level.
// Thread-safe with mutex for RTX rendering compatibility.
// Mappings:
// - Wheel up: Zoom in (AMOURANTH::updateZoom(true)).
// - Wheel down: Zoom out (AMOURANTH::updateZoom(false)).
void HandleInput::defaultMouseWheelHandler(const SDL_MouseWheelEvent& mw) {
    std::lock_guard<std::mutex> lock(inputMutex_);
    if (mw.y > 0) {
        amouranth_->updateZoom(true);
    } else if (mw.y < 0) {
        amouranth_->updateZoom(false);
    }
}

// Default text input handler: Placeholder for future text-based UI (e.g., parameter input).
// Currently unused, but ready for RTX developers to implement custom UI features.
void HandleInput::defaultTextInputHandler([[maybe_unused]] const SDL_TextInputEvent& ti) {
    // Placeholder for future text-based UI
}

// Default touch handler: Maps touch inputs to camera control for touch-enabled RTX devices.
// Thread-safe with mutex for safe AMOURANTH calls.
// Mappings:
// - Finger down: Toggle user camera (AMOURANTH::toggleUserCam).
// - Finger motion: Rotate camera when user camera is active (AMOURANTH::moveUserCam).
void HandleInput::defaultTouchHandler(const SDL_TouchFingerEvent& tf) {
    std::lock_guard<std::mutex> lock(inputMutex_);
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

// Default gamepad button handler: Maps gamepad buttons to simulation controls.
// Thread-safe with mutex for RTX pipeline safety.
// Mappings:
// - South (A): Toggle pause (AMOURANTH::togglePause).
// - East (B): Toggle user camera (AMOURANTH::toggleUserCam).
// - D-Pad Up/Down: Zoom in/out (AMOURANTH::updateZoom).
// - D-Pad Left/Right: Decrease/increase influence (AMOURANTH::adjustInfluence).
void HandleInput::defaultGamepadButtonHandler(const SDL_GamepadButtonEvent& gb) {
    std::lock_guard<std::mutex> lock(inputMutex_);
    if (gb.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        switch (gb.button) {
            case SDL_GAMEPAD_BUTTON_SOUTH:
                amouranth_->togglePause();
                break;
            case SDL_GAMEPAD_BUTTON_EAST:
                amouranth_->toggleUserCam();
                break;
            case SDL_GAMEPAD_BUTTON_DPAD_UP:
                amouranth_->updateZoom(true);
                break;
            case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
                amouranth_->updateZoom(false);
                break;
            case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
                amouranth_->adjustInfluence(-0.1);
                break;
            case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
                amouranth_->adjustInfluence(0.1);
                break;
        }
    }
}

// Default gamepad axis handler: Controls camera movement via analog sticks when user camera is active.
// Thread-safe with mutex for RTX rendering compatibility.
// Mappings:
// - Left Stick X: Move camera left/right (AMOURANTH::moveUserCam).
// - Left Stick Y: Move camera up/down (AMOURANTH::moveUserCam).
// - Right Stick X: Move camera forward/backward (AMOURANTH::moveUserCam).
void HandleInput::defaultGamepadAxisHandler(const SDL_GamepadAxisEvent& ga) {
    std::lock_guard<std::mutex> lock(inputMutex_);
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

// Default gamepad connect handler: Logs gamepad connection/disconnection events.
// Thread-safe with mutex for safe logging in RTX environments.
// Example: Useful for debugging input issues in multi-device RTX setups.
void HandleInput::defaultGamepadConnectHandler(bool connected, SDL_JoystickID id, [[maybe_unused]] SDL_Gamepad* pad) {
    std::lock_guard<std::mutex> lock(inputMutex_);
    if (connected) {
        std::cerr << "Gamepad connected: ID " << id << "\n";
    } else {
        std::cerr << "Gamepad disconnected: ID " << id << "\n";
    }
}