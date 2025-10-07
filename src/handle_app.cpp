// AMOURANTH RTX Engine, October 2025 - Application and input handling implementation.
// Manages SDL3 input events, Vulkan initialization, and application lifecycle.
// Dependencies: SDL3, Vulkan, GLM, C++20 standard library.
// Zachary Geurts 2025

#include "handle_app.hpp"
#include <stdexcept>
#include <format>

HandleInput::HandleInput(AMOURANTH* amouranth, DimensionalNavigator* navigator, const Logging::Logger& logger)
    : amouranth_(amouranth), navigator_(navigator), logger_(logger) {
    if (!amouranth || !navigator) {
        logger_.log(Logging::LogLevel::Error, "HandleInput: Null AMOURANTH or DimensionalNavigator provided", std::source_location::current());
        throw std::runtime_error("HandleInput: Null AMOURANTH or DimensionalNavigator provided.");
    }
    keyboardCallback_ = [this](const SDL_KeyboardEvent& key) { defaultKeyboardHandler(key); };
    mouseButtonCallback_ = [this](const SDL_MouseButtonEvent& mb) { defaultMouseButtonHandler(mb); };
    mouseMotionCallback_ = [this](const SDL_MouseMotionEvent& mm) { defaultMouseMotionHandler(mm); };
    mouseWheelCallback_ = [this](const SDL_MouseWheelEvent& mw) { defaultMouseWheelHandler(mw); };
    textInputCallback_ = [this](const SDL_TextInputEvent& ti) { defaultTextInputHandler(ti); };
    touchCallback_ = [this](const SDL_TouchFingerEvent& tf) { defaultTouchHandler(tf); };
    gamepadButtonCallback_ = [this](const SDL_GamepadButtonEvent& gb) { defaultGamepadButtonHandler(gb); };
    gamepadAxisCallback_ = [this](const SDL_GamepadAxisEvent& ga) { defaultGamepadAxisHandler(ga); };
    gamepadConnectCallback_ = [this](bool connected, SDL_JoystickID id, SDL_Gamepad* pad) { defaultGamepadConnectHandler(connected, id, pad); };
    logger_.log(Logging::LogLevel::Info, "HandleInput initialized successfully", std::source_location::current());
}

void HandleInput::handleInput(Application& app) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                logger_.log(Logging::LogLevel::Info, "Quit event received", std::source_location::current());
                app.setRenderMode(0);
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                int newWidth, newHeight;
                SDL_GetWindowSize(SDL_GetWindowFromID(event.window.windowID), &newWidth, &newHeight);
                logger_.log(Logging::LogLevel::Info, "Window resized to {}x{}", std::source_location::current(), newWidth, newHeight);
                app.handleResize(newWidth, newHeight);
                break;
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
                if (keyboardCallback_) keyboardCallback_(event.key);
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (mouseButtonCallback_) mouseButtonCallback_(event.button);
                break;
            case SDL_EVENT_MOUSE_MOTION:
                if (mouseMotionCallback_) mouseMotionCallback_(event.motion);
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                if (mouseWheelCallback_) mouseWheelCallback_(event.wheel);
                break;
            case SDL_EVENT_TEXT_INPUT:
                if (textInputCallback_) textInputCallback_(event.text);
                break;
            case SDL_EVENT_FINGER_DOWN:
            case SDL_EVENT_FINGER_UP:
            case SDL_EVENT_FINGER_MOTION:
                if (touchCallback_) touchCallback_(event.tfinger);
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                if (gamepadButtonCallback_) gamepadButtonCallback_(event.gbutton);
                break;
            case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                if (gamepadAxisCallback_) gamepadAxisCallback_(event.gaxis);
                break;
            case SDL_EVENT_GAMEPAD_ADDED:
            case SDL_EVENT_GAMEPAD_REMOVED:
                if (gamepadConnectCallback_) {
                    gamepadConnectCallback_(
                        event.type == SDL_EVENT_GAMEPAD_ADDED,
                        event.gdevice.which,
                        event.type == SDL_EVENT_GAMEPAD_ADDED ? SDL_OpenGamepad(event.gdevice.which) : nullptr
                    );
                }
                break;
            default:
                logger_.log(Logging::LogLevel::Debug, "Unhandled SDL event: type={}", std::source_location::current(), event.type);
                break;
        }
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
    logger_.log(Logging::LogLevel::Info, "Input callbacks set successfully", std::source_location::current());
}

void HandleInput::defaultKeyboardHandler(const SDL_KeyboardEvent& key) {
    if (key.type == SDL_EVENT_KEY_DOWN) {
        logger_.log(Logging::LogLevel::Debug, "Handling key down: scancode={}", std::source_location::current(), SDL_GetScancodeName(key.scancode));
        switch (key.scancode) {
            case SDL_SCANCODE_1: case SDL_SCANCODE_2: case SDL_SCANCODE_3: case SDL_SCANCODE_4:
            case SDL_SCANCODE_5: case SDL_SCANCODE_6: case SDL_SCANCODE_7: case SDL_SCANCODE_8: case SDL_SCANCODE_9:
                amouranth_->setMode(key.scancode - SDL_SCANCODE_1 + 1);
                navigator_->setMode(key.scancode - SDL_SCANCODE_1 + 1);
                break;
            case SDL_SCANCODE_KP_PLUS: case SDL_SCANCODE_EQUALS:
                amouranth_->updateZoom(true);
                break;
            case SDL_SCANCODE_KP_MINUS: case SDL_SCANCODE_MINUS:
                amouranth_->updateZoom(false);
                break;
            case SDL_SCANCODE_I:
                amouranth_->adjustInfluence(0.1f);
                break;
            case SDL_SCANCODE_O:
                amouranth_->adjustInfluence(-0.1f);
                break;
            case SDL_SCANCODE_J:
                amouranth_->adjustNurbMatter(0.1f);
                break;
            case SDL_SCANCODE_K:
                amouranth_->adjustNurbMatter(-0.1f);
                break;
            case SDL_SCANCODE_N:
                amouranth_->adjustNurbEnergy(0.1f);
                break;
            case SDL_SCANCODE_M:
                amouranth_->adjustNurbEnergy(-0.1f);
                break;
            case SDL_SCANCODE_P:
                amouranth_->togglePause();
                break;
            case SDL_SCANCODE_C:
                amouranth_->toggleUserCam();
                break;
            case SDL_SCANCODE_W:
                if (amouranth_->isUserCamActive()) amouranth_->moveUserCam(0.0f, 0.0f, -0.1f);
                break;
            case SDL_SCANCODE_S:
                if (amouranth_->isUserCamActive()) amouranth_->moveUserCam(0.0f, 0.0f, 0.1f);
                break;
            case SDL_SCANCODE_A:
                if (amouranth_->isUserCamActive()) amouranth_->moveUserCam(-0.1f, 0.0f, 0.0f);
                break;
            case SDL_SCANCODE_D:
                if (amouranth_->isUserCamActive()) amouranth_->moveUserCam(0.1f, 0.0f, 0.0f);
                break;
            case SDL_SCANCODE_Q:
                if (amouranth_->isUserCamActive()) amouranth_->moveUserCam(0.0f, 0.1f, 0.0f);
                break;
            case SDL_SCANCODE_E:
                if (amouranth_->isUserCamActive()) amouranth_->moveUserCam(0.0f, -0.1f, 0.0f);
                break;
            default:
                logger_.log(Logging::LogLevel::Debug, "Unhandled key scancode: {}", std::source_location::current(), SDL_GetScancodeName(key.scancode));
                break;
        }
    }
}

void HandleInput::defaultMouseButtonHandler(const SDL_MouseButtonEvent& mb) {
    if (mb.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        logger_.log(Logging::LogLevel::Debug, "Handling mouse button down: button={}", std::source_location::current(), mb.button);
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
        logger_.log(Logging::LogLevel::Debug, "Handling mouse motion: dx={}, dy={}", std::source_location::current(), dx, dy);
        amouranth_->moveUserCam(-dx, -dy, 0.0f);
    }
}

void HandleInput::defaultMouseWheelHandler(const SDL_MouseWheelEvent& mw) {
    logger_.log(Logging::LogLevel::Debug, "Handling mouse wheel: y={}", std::source_location::current(), mw.y);
    if (mw.y > 0) {
        amouranth_->updateZoom(true);
    } else if (mw.y < 0) {
        amouranth_->updateZoom(false);
    }
}

void HandleInput::defaultTextInputHandler(const SDL_TextInputEvent& ti) {
    logger_.log(Logging::LogLevel::Debug, "Handling text input: text={}", std::source_location::current(), ti.text);
}

void HandleInput::defaultTouchHandler(const SDL_TouchFingerEvent& tf) {
    if (tf.type == SDL_EVENT_FINGER_DOWN) {
        logger_.log(Logging::LogLevel::Debug, "Handling touch down", std::source_location::current());
        amouranth_->toggleUserCam();
    } else if (tf.type == SDL_EVENT_FINGER_MOTION) {
        if (amouranth_->isUserCamActive()) {
            float dx = tf.dx * 0.1f;
            float dy = tf.dy * 0.1f;
            logger_.log(Logging::LogLevel::Debug, "Handling touch motion: dx={}, dy={}", std::source_location::current(), dx, dy);
            amouranth_->moveUserCam(-dx, -dy, 0.0f);
        }
    }
}

void HandleInput::defaultGamepadButtonHandler(const SDL_GamepadButtonEvent& gb) {
    if (gb.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        logger_.log(Logging::LogLevel::Debug, "Handling gamepad button down: button={}", std::source_location::current(), gb.button);
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
                amouranth_->adjustInfluence(-0.1f);
                break;
            case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
                amouranth_->adjustInfluence(0.1f);
                break;
        }
    }
}

void HandleInput::defaultGamepadAxisHandler(const SDL_GamepadAxisEvent& ga) {
    if (amouranth_->isUserCamActive()) {
        float value = ga.value / 32768.0f;
        logger_.log(Logging::LogLevel::Debug, "Handling gamepad axis: axis={}, value={}", std::source_location::current(), ga.axis, value);
        if (ga.axis == SDL_GAMEPAD_AXIS_LEFTX) {
            amouranth_->moveUserCam(value * 0.1f, 0.0f, 0.0f);
        } else if (ga.axis == SDL_GAMEPAD_AXIS_LEFTY) {
            amouranth_->moveUserCam(0.0f, -value * 0.1f, 0.0f);
        } else if (ga.axis == SDL_GAMEPAD_AXIS_RIGHTX) {
            amouranth_->moveUserCam(0.0f, 0.0f, value * 0.1f);
        }
    }
}

void HandleInput::defaultGamepadConnectHandler(bool connected, SDL_JoystickID id, SDL_Gamepad* pad) {
    const char* gamepadName = pad ? SDL_GetGamepadName(pad) : "Unknown";
    if (connected) {
        logger_.log(Logging::LogLevel::Info, "Gamepad connected: ID {}, Name {}", std::source_location::current(), id, gamepadName);
    } else {
        logger_.log(Logging::LogLevel::Warning, "Gamepad disconnected: ID {}, Name {}", std::source_location::current(), id, gamepadName);
    }
}

Application::Application(const char* title, int width, int height)
    : title_(title),
      width_(width),
      height_(height),
      mode_(1),
      vertices_({
          glm::vec3(-0.5f, -0.5f, 0.0f),
          glm::vec3(0.5f, -0.5f, 0.0f),
          glm::vec3(0.0f, 0.5f, 0.0f)
      }),
      indices_({0, 1, 2}),
      sdl_(std::make_unique<SDL3Initializer>(std::string(title), width, height)),
      renderer_(std::make_unique<VulkanRenderer>(
          sdl_->getInstance(), sdl_->getSurface(), vertices_, indices_, width, height)),
      logger_(),
      navigator_(std::make_unique<DimensionalNavigator>(title, width, height, *renderer_, logger_)),
      amouranth_(navigator_.get(), logger_, renderer_->getContext().device,
                 renderer_->getContext().vertexBufferMemory, renderer_->getContext().pipeline),
      inputHandler_(nullptr) {
    try {
        renderer_->initializeVulkan(vertices_, indices_, width, height);
        initializeInput();
        logger_.log(Logging::LogLevel::Info, "Application initialized successfully", std::source_location::current());
    } catch (const std::exception& e) {
        logger_.log(Logging::LogLevel::Error, "Failed to initialize Application: {}", std::source_location::current(), e.what());
        throw;
    }
}

Application::~Application() {
    logger_.log(Logging::LogLevel::Info, "Cleaning up Application", std::source_location::current());
    // Explicit destruction order
    inputHandler_.reset();
    // amouranth_ is not a unique_ptr, so it will be destroyed automatically
    navigator_.reset();
    renderer_.reset();
    sdl_.reset();
}

void Application::initializeInput() {
    inputHandler_ = std::make_unique<HandleInput>(&amouranth_, navigator_.get(), logger_);
}

void Application::run() {
    float deltaTime = 0.016f; // Assume 60 FPS
    while (!sdl_->shouldQuit()) {
        sdl_->pollEvents();
        inputHandler_->handleInput(*this);
        amouranth_.update(deltaTime);
        render();
    }
}

void Application::render() {
    try {
        renderer_->renderFrame(&amouranth_);
    } catch (const std::exception& e) {
        logger_.log(Logging::LogLevel::Error, "Rendering failed: {}", std::source_location::current(), e.what());
        throw;
    }
}

void Application::handleResize(int newWidth, int newHeight) {
    width_ = newWidth;
    height_ = newHeight;
    renderer_->handleResize(newWidth, newHeight);
    navigator_->setWidth(newWidth);
    navigator_->setHeight(newHeight);
    amouranth_.setWidth(newWidth);
    amouranth_.setHeight(newHeight);
    logger_.log(Logging::LogLevel::Info, "Application resized to {}x{}", std::source_location::current(), newWidth, newHeight);
}