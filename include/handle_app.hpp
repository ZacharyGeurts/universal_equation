#ifndef HANDLE_APP_HPP
#define HANDLE_APP_HPP

// Combined input handling and application management for AMOURANTH RTX Engine, October 2025
// Manages SDL3 input events and application lifecycle.
// Thread-safe with C++20 features; no mutexes required.
// Dependencies: SDL3, Vulkan, C++20 standard library.
// Zachary Geurts 2025

#include <SDL3/SDL.h>
#include <functional>
#include <stdexcept>
#include <vector>
#include <memory>
#include <string>
#include "engine/core.hpp" // For AMOURANTH, DimensionalNavigator, glm::vec3
#include "engine/Vulkan_init.hpp" // For VulkanRenderer
#include "engine/SDL3_init.hpp" // For SDL3Initializer
#include "engine/logging.hpp" // For Logging::Logger

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

    HandleInput(AMOURANTH* amouranth, DimensionalNavigator* navigator, const Logging::Logger& logger);

    void handleInput(class Application& app);

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
    const Logging::Logger& logger_;
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

class Application {
public:
    Application(const char* title, int width, int height);
    ~Application() = default;

    void initialize();
    void run();
    void render();
    void setRenderMode(int mode) { mode_ = mode; amouranth_.setMode(mode); }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

private:
    void initializeInput();

    std::string title_;
    int width_, height_;
    int mode_;
    std::unique_ptr<SDL3Initializer> sdl_;
    std::unique_ptr<VulkanRenderer> renderer_;
    std::unique_ptr<DimensionalNavigator> navigator_;
    AMOURANTH amouranth_;
    Logging::Logger logger_;
    std::unique_ptr<HandleInput> inputHandler_;
    std::vector<glm::vec3> vertices_;
    std::vector<uint32_t> indices_;
};

#endif // HANDLE_APP_HPP