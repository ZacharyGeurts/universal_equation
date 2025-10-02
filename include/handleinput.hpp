// handleinput.hpp
// AMOURANTH RTX Engine, Sep 2025 - Input handling for real-time simulation and visualization.
// Provides a centralized class to manage SDL3 inputs (keyboard, mouse, gamepad, touch, text) for the AMOURANTH RTX application.
// Integrates with AMOURANTH (core simulation, interfacing with UniversalEquation) and DimensionalNavigator (Vulkan-based rendering of n-dimensional hypercube lattices).
// Features:
// - Thread-safe input handling with mutex for safe AMOURANTH and DimensionalNavigator access in RTX pipelines.
// - Cross-platform support (Linux, Windows, macOS, Android) via SDL3.
// - Extensible callback system for custom input mappings (e.g., advanced UI, automation).
// - Default handlers for intuitive controls (e.g., WASD for camera, +/- for zoom, I/O for influence).
// Why RTX Developers Will Love It:
// - Real-time control over simulation parameters (e.g., influence, dark matter/energy) for dynamic RTX rendering.
// - Seamless camera navigation for Vulkan-rendered 3D visualizations of hypercube vertices.
// - Flexible callbacks for integrating with custom RTX shaders or UI systems.
// Usage:
// - Instantiate with AMOURANTH and DimensionalNavigator pointers.
// - Set callbacks (optional) and process events via SDL3Initializer::eventLoop.
// - Example: HandleInput input(amouranth, navigator); input.setCallbacks(...); sdlInitializer.eventLoop(..., input.getKeyboardCallback(), ...);
// Dependencies: SDL3 (libSDL3-dev), C++17, engine/core.hpp (for AMOURANTH, DimensionalNavigator).
// Best Practices:
// - Ensure SDL3 subsystems (SDL_INIT_GAMEPAD, SDL_INIT_EVENTS) are initialized in SDL3Initializer.
// - Use inputMutex_ for thread-safe access in multi-threaded RTX environments.
// - On Android, prioritize touch inputs for mobile-friendly controls.
// - Handle gamepad connect/disconnect events to avoid crashes.
// Potential Optimizations:
// - Implement input debouncing to reduce redundant AMOURANTH calls (e.g., limit moveUserCam calls to 10ms intervals).
// - Synchronize input events with Vulkan frame updates to avoid tearing or stuttering.
// Potential Issues:
// - Verify AMOURANTH and DimensionalNavigator pointers are valid to prevent null pointer dereferences.
// - Ensure SDL3 is initialized before processing events to avoid undefined behavior.
// Updated: Added thread-safety with inputMutex_, enhanced comments for RTX developers, clarified Vulkan integration.
// Zachary Geurts, 2025 (enhanced by Grok for AMOURANTH RTX)

#ifndef HANDLEINPUT_HPP
#define HANDLEINPUT_HPP

#include <SDL3/SDL.h>
#include <functional>
#include <stdexcept>
#include <mutex>

// Forward declarations for AMOURANTH and DimensionalNavigator
class AMOURANTH;
class DimensionalNavigator;

// HandleInput manages user input for AMOURANTH RTX, controlling simulation and Vulkan-based visualization.
// Provides callbacks for keyboard, mouse, touch, and gamepad inputs, with defaults for common controls.
// Thread-safe with mutex to ensure safe AMOURANTH and DimensionalNavigator calls in RTX pipelines.
// Example: HandleInput input(amouranth, navigator); input.setCallbacks(...);
class HandleInput {
public:
    // Callback function types for SDL3 input events, used to customize simulation and visualization controls.
    using KeyboardCallback = std::function<void(const SDL_KeyboardEvent&)>;           // Keyboard events (e.g., WASD, 1-9)
    using MouseButtonCallback = std::function<void(const SDL_MouseButtonEvent&)>;     // Mouse clicks (e.g., toggle camera)
    using MouseMotionCallback = std::function<void(const SDL_MouseMotionEvent&)>;     // Mouse movement (e.g., camera rotation)
    using MouseWheelCallback = std::function<void(const SDL_MouseWheelEvent&)>;       // Mouse wheel (e.g., zoom)
    using TextInputCallback = std::function<void(const SDL_TextInputEvent&)>;         // Text input (e.g., future UI)
    using TouchCallback = std::function<void(const SDL_TouchFingerEvent&)>;           // Touch events (e.g., mobile controls)
    using GamepadButtonCallback = std::function<void(const SDL_GamepadButtonEvent&)>; // Gamepad buttons (e.g., pause)
    using GamepadAxisCallback = std::function<void(const SDL_GamepadAxisEvent&)>;     // Gamepad analog sticks (e.g., camera)
    using GamepadConnectCallback = std::function<void(bool, SDL_JoystickID, SDL_Gamepad*)>; // Gamepad connect/disconnect

    // Constructor: Initializes input handling with AMOURANTH (simulation) and DimensionalNavigator (Vulkan rendering).
    // Throws std::runtime_error if either pointer is null.
    // Example: HandleInput input(amouranth, navigator); for RTX simulation control.
    HandleInput(AMOURANTH* amouranth, DimensionalNavigator* navigator);

    // Sets custom callbacks for input events, using default handlers if nullptr is provided.
    // Thread-safe with mutex to protect AMOURANTH and DimensionalNavigator access.
    // Example: input.setCallbacks(customKeyboard, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    // For RTX developers: Use custom callbacks to integrate with Vulkan shaders or custom UI systems.
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

    // Getters for callbacks to pass to SDL3Initializer::eventLoop for event processing.
    // Example: sdlInitializer.eventLoop(..., input.getKeyboardCallback(), input.getMouseButtonCallback(), ...);
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
    // Default input handlers for common RTX controls (e.g., zoom, camera, parameter tweaks).
    // Thread-safe with inputMutex_ to ensure safe AMOURANTH and DimensionalNavigator calls.
    void defaultKeyboardHandler(const SDL_KeyboardEvent& key);           // Handles keys (e.g., WASD, 1-9)
    void defaultMouseButtonHandler(const SDL_MouseButtonEvent& mb);       // Handles mouse clicks (e.g., toggle camera)
    void defaultMouseMotionHandler(const SDL_MouseMotionEvent& mm);       // Handles mouse movement (e.g., camera rotation)
    void defaultMouseWheelHandler(const SDL_MouseWheelEvent& mw);         // Handles mouse wheel (e.g., zoom)
    void defaultTextInputHandler(const SDL_TextInputEvent& ti);           // Placeholder for text input
    void defaultTouchHandler(const SDL_TouchFingerEvent& tf);             // Handles touch (e.g., mobile camera control)
    void defaultGamepadButtonHandler(const SDL_GamepadButtonEvent& gb);   // Handles gamepad buttons (e.g., pause)
    void defaultGamepadAxisHandler(const SDL_GamepadAxisEvent& ga);       // Handles gamepad sticks (e.g., camera movement)
    void defaultGamepadConnectHandler(bool connected, SDL_JoystickID id, SDL_Gamepad* pad); // Handles gamepad connect/disconnect

    AMOURANTH* amouranth_;                     // Pointer to AMOURANTH simulation (interfaces with UniversalEquation)
    DimensionalNavigator* navigator_;           // Pointer to Vulkan-based visualization component
    mutable std::mutex inputMutex_;            // Mutex for thread-safe AMOURANTH and DimensionalNavigator access
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