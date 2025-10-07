// AMOURANTH RTX Engine, October 2025 - Application lifecycle management.
// Manages SDL3 input events, Vulkan rendering, and core application loop.
// Dependencies: SDL3, Vulkan, GLM, C++20 standard library.
// Zachary Geurts 2025

#ifndef HANDLE_APP_HPP
#define HANDLE_APP_HPP

#include "engine/SDL3_init.hpp"
#include "engine/Vulkan_init.hpp"
#include "engine/logging.hpp"
#include "engine/core.hpp"
#include <SDL3/SDL.h>
#include <functional>
#include <memory>
#include <vector>
#include <glm/glm.hpp>

class HandleInput;

class Application {
public:
    Application(const char* title, int width, int height);
    ~Application();

    void run();
    void render();
    void handleResize(int width, int height); // Added declaration
    void setRenderMode(int mode) { mode_ = mode; }

private:
    void initializeInput();

    std::string title_;
    int width_;
    int height_;
    int mode_;
    std::vector<glm::vec3> vertices_;
    std::vector<uint32_t> indices_;
    std::unique_ptr<SDL3Initializer> sdl_;
    std::unique_ptr<VulkanRenderer> renderer_;
    Logging::Logger logger_;
    std::unique_ptr<DimensionalNavigator> navigator_;
    AMOURANTH amouranth_;
    std::unique_ptr<HandleInput> inputHandler_;
};

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
    void handleInput(Application& app);
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

#endif // HANDLE_APP_HPP