// AMOURANTH RTX Engine, October 2025 - Application and input handling implementation.
// Manages SDL3 input events, Vulkan rendering, audio, and application lifecycle.
// Dependencies: SDL3, Vulkan, GLM, C++20 standard library.
// Supported platforms: Windows, Linux.
// Zachary Geurts 2025

#include "handle_app.hpp"
#include <stdexcept>
#include <sstream>
#include <source_location>
#include <iomanip>

Application::Application(const char* title, int width, int height)
    : title_(title),
      width_(width),
      height_(height),
      mode_(1),
      vertices_{},
      indices_{},
      sdl_(nullptr),
      renderer_(nullptr),
      navigator_(nullptr),
      amouranth_{},
      inputHandler_(nullptr),
      logger_(Logging::LogLevel::Info, "amouranth.log"),
      audioDevice_(0),
      audioStream_(nullptr),
      lastFrameTime_(std::chrono::steady_clock::now()) {
    std::stringstream ss;
    ss << "Initializing Application with title=" << title << ", width=" << width << ", height=" << height;
    logger_.log(Logging::LogLevel::Info, ss.str(), std::source_location::current());

    sdl_ = std::make_unique<SDL3Initializer>(title, width, height, logger_);
    logger_.log(Logging::LogLevel::Debug, "SDL3Initializer created successfully", std::source_location::current());

    // Populate sample geometry data
    vertices_ = {
        glm::vec3(-0.5f, -0.5f, 0.0f),
        glm::vec3(0.5f, -0.5f, 0.0f),
        glm::vec3(0.0f, 0.5f, 0.0f)
    };
    indices_ = {0, 1, 2};
    std::stringstream geom_ss;
    geom_ss << "Sample geometry data populated: " << vertices_.size() << " vertices, " << indices_.size() << " indices";
    logger_.log(Logging::LogLevel::Debug, geom_ss.str(), std::source_location::current());

    renderer_ = std::make_unique<VulkanRenderer>(sdl_->getInstance(), sdl_->getSurface(),
                                                 std::span<const glm::vec3>(vertices_),
                                                 std::span<const uint32_t>(indices_),
                                                 width_, height_, logger_);
    logger_.log(Logging::LogLevel::Debug, "VulkanRenderer created successfully", std::source_location::current());

    navigator_ = std::make_unique<DimensionalNavigator>("AMOURANTH Navigator", width_, height_, *renderer_, logger_);
    logger_.log(Logging::LogLevel::Debug, "DimensionalNavigator created successfully", std::source_location::current());

    amouranth_.emplace(navigator_.get(), logger_, renderer_->getDevice(),
                       renderer_->getVertexBufferMemory(), renderer_->getGraphicsPipeline());
    logger_.log(Logging::LogLevel::Debug, "AMOURANTH instance created successfully", std::source_location::current());

    inputHandler_ = std::make_unique<HandleInput>(amouranth_.value(), navigator_.get(), logger_);
    logger_.log(Logging::LogLevel::Debug, "HandleInput created successfully", std::source_location::current());

    initializeInput();
    logger_.log(Logging::LogLevel::Debug, "Input initialized", std::source_location::current());
    initializeAudio();
    logger_.log(Logging::LogLevel::Debug, "Audio initialized", std::source_location::current());
    logger_.log(Logging::LogLevel::Info, "Application initialized successfully", std::source_location::current());
}

Application::~Application() {
    logger_.log(Logging::LogLevel::Info, "Destroying Application", std::source_location::current());
    if (audioStream_) {
        SDL_DestroyAudioStream(audioStream_);
        audioStream_ = nullptr;
        logger_.log(Logging::LogLevel::Debug, "Audio stream destroyed", std::source_location::current());
    }
    if (audioDevice_) {
        SDL_CloseAudioDevice(audioDevice_);
        audioDevice_ = 0;
        logger_.log(Logging::LogLevel::Debug, "Audio device closed", std::source_location::current());
    }
    inputHandler_.reset();
    logger_.log(Logging::LogLevel::Debug, "HandleInput destroyed", std::source_location::current());
    amouranth_.reset();
    logger_.log(Logging::LogLevel::Debug, "AMOURANTH instance destroyed", std::source_location::current());
    navigator_.reset();
    logger_.log(Logging::LogLevel::Debug, "DimensionalNavigator destroyed", std::source_location::current());
    renderer_.reset();
    logger_.log(Logging::LogLevel::Debug, "VulkanRenderer destroyed", std::source_location::current());
    sdl_.reset();
    logger_.log(Logging::LogLevel::Debug, "SDL3Initializer destroyed", std::source_location::current());
}

void Application::initializeInput() {
    inputHandler_->setCallbacks();
    logger_.log(Logging::LogLevel::Info, "Input callbacks initialized", std::source_location::current());
}

void Application::initializeAudio() {
    SDL3Audio::AudioConfig config;
    config.callback = [](Uint8* stream, int len) {
        std::fill(stream, stream + len, 0);
    };
    try {
        SDL3Audio::initAudio(config, audioDevice_, audioStream_, logger_);
        std::stringstream ss;
        ss << "Audio initialized successfully with device ID: " << audioDevice_;
        logger_.log(Logging::LogLevel::Info, ss.str(), std::source_location::current());
    } catch (const std::exception& e) {
        std::stringstream ss;
        ss << "Audio initialization failed: " << e.what();
        logger_.log(Logging::LogLevel::Error, ss.str(), std::source_location::current());
        throw;
    }
}

void Application::run() {
    logger_.log(Logging::LogLevel::Info, "Starting application loop", std::source_location::current());
    while (mode_ != 0 && !sdl_->shouldQuit()) {
        logger_.log(Logging::LogLevel::Debug, "Starting new frame iteration", std::source_location::current());
        sdl_->pollEvents();
        logger_.log(Logging::LogLevel::Debug, "SDL events polled", std::source_location::current());
        inputHandler_->handleInput(*this);
        logger_.log(Logging::LogLevel::Debug, "Input processed", std::source_location::current());
        render();
        logger_.log(Logging::LogLevel::Debug, "Frame rendered", std::source_location::current());
    }
    logger_.log(Logging::LogLevel::Info, "Application loop terminated", std::source_location::current());
}

void Application::render() {
    logger_.log(Logging::LogLevel::Debug, "Starting render", std::source_location::current());
    if (!amouranth_.has_value()) {
        logger_.log(Logging::LogLevel::Warning, "AMOURANTH not initialized, skipping render", std::source_location::current());
        return;
    }

    auto currentTime = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(currentTime - lastFrameTime_).count();
    lastFrameTime_ = currentTime;
    std::stringstream ss;
    ss << "Calculated deltaTime=" << std::fixed << std::setprecision(3) << deltaTime;
    logger_.log(Logging::LogLevel::Debug, ss.str(), std::source_location::current());

    if (vertices_.empty() || indices_.empty()) {
        logger_.log(Logging::LogLevel::Warning, "No valid geometry data for rendering", std::source_location::current());
        return;
    }

    try {
        logger_.log(Logging::LogLevel::Debug, "Calling renderer_->renderFrame()", std::source_location::current());
        renderer_->renderFrame(&amouranth_.value());
        std::stringstream render_ss;
        render_ss << "Frame rendered successfully with deltaTime=" << std::fixed << std::setprecision(3) << deltaTime;
        logger_.log(Logging::LogLevel::Debug, render_ss.str(), std::source_location::current());
    } catch (const std::exception& e) {
        std::stringstream error_ss;
        error_ss << "Render failed: " << e.what();
        logger_.log(Logging::LogLevel::Error, error_ss.str(), std::source_location::current());
        mode_ = 0;
    }
}

void Application::handleResize(int width, int height) {
    std::stringstream ss;
    ss << "Handling resize to width=" << width << ", height=" << height;
    logger_.log(Logging::LogLevel::Info, ss.str(), std::source_location::current());
    width_ = width;
    height_ = height;
    renderer_->handleResize(width, height);
    logger_.log(Logging::LogLevel::Debug, "Renderer resized", std::source_location::current());
    navigator_->setWidth(width);
    navigator_->setHeight(height);
    logger_.log(Logging::LogLevel::Debug, "Navigator dimensions updated", std::source_location::current());
    if (amouranth_.has_value()) {
        amouranth_.value().setWidth(width);
        amouranth_.value().setHeight(height);
        logger_.log(Logging::LogLevel::Debug, "AMOURANTH dimensions updated", std::source_location::current());
    }
    std::stringstream resize_ss;
    resize_ss << "Application resized to width=" << width << ", height=" << height;
    logger_.log(Logging::LogLevel::Info, resize_ss.str(), std::source_location::current());
}

HandleInput::HandleInput(AMOURANTH& amouranth, DimensionalNavigator* navigator, const Logging::Logger& logger)
    : amouranth_(amouranth), navigator_(navigator), logger_(logger) {
    if (!navigator) {
        logger_.log(Logging::LogLevel::Error, "HandleInput: Null DimensionalNavigator provided", std::source_location::current());
        throw std::runtime_error("HandleInput: Null DimensionalNavigator provided.");
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
    logger_.log(Logging::LogLevel::Debug, "Processing input events", std::source_location::current());
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        std::stringstream ss;
        ss << "Handling SDL event: type=" << event.type;
        logger_.log(Logging::LogLevel::Debug, ss.str(), std::source_location::current());
        switch (event.type) {
            case SDL_EVENT_QUIT:
                logger_.log(Logging::LogLevel::Info, "Quit event received", std::source_location::current());
                app.setRenderMode(0);
                break;
            case SDL_EVENT_WINDOW_RESIZED: {
                int newWidth, newHeight;
                SDL_GetWindowSize(SDL_GetWindowFromID(event.window.windowID), &newWidth, &newHeight);
                std::stringstream resize_ss;
                resize_ss << "Window resized to " << newWidth << "x" << newHeight;
                logger_.log(Logging::LogLevel::Info, resize_ss.str(), std::source_location::current());
                app.handleResize(newWidth, newHeight);
                break;
            }
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
                if (keyboardCallback_) keyboardCallback_(event.key);
                logger_.log(Logging::LogLevel::Debug, "Keyboard event processed", std::source_location::current());
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (mouseButtonCallback_) mouseButtonCallback_(event.button);
                logger_.log(Logging::LogLevel::Debug, "Mouse button event processed", std::source_location::current());
                break;
            case SDL_EVENT_MOUSE_MOTION:
                if (mouseMotionCallback_) mouseMotionCallback_(event.motion);
                logger_.log(Logging::LogLevel::Debug, "Mouse motion event processed", std::source_location::current());
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                if (mouseWheelCallback_) mouseWheelCallback_(event.wheel);
                logger_.log(Logging::LogLevel::Debug, "Mouse wheel event processed", std::source_location::current());
                break;
            case SDL_EVENT_TEXT_INPUT:
                if (textInputCallback_) textInputCallback_(event.text);
                logger_.log(Logging::LogLevel::Debug, "Text input event processed", std::source_location::current());
                break;
            case SDL_EVENT_FINGER_DOWN:
            case SDL_EVENT_FINGER_UP:
            case SDL_EVENT_FINGER_MOTION:
                if (touchCallback_) touchCallback_(event.tfinger);
                logger_.log(Logging::LogLevel::Debug, "Touch event processed", std::source_location::current());
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                if (gamepadButtonCallback_) gamepadButtonCallback_(event.gbutton);
                logger_.log(Logging::LogLevel::Debug, "Gamepad button event processed", std::source_location::current());
                break;
            case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                if (gamepadAxisCallback_) gamepadAxisCallback_(event.gaxis);
                logger_.log(Logging::LogLevel::Debug, "Gamepad axis event processed", std::source_location::current());
                break;
            case SDL_EVENT_GAMEPAD_ADDED:
            case SDL_EVENT_GAMEPAD_REMOVED:
                if (gamepadConnectCallback_) {
                    gamepadConnectCallback_(
                        event.type == SDL_EVENT_GAMEPAD_ADDED,
                        event.gdevice.which,
                        event.type == SDL_EVENT_GAMEPAD_ADDED ? SDL_OpenGamepad(event.gdevice.which) : nullptr
                    );
                    logger_.log(Logging::LogLevel::Debug, "Gamepad connect event processed", std::source_location::current());
                }
                break;
            default:
                std::stringstream default_ss;
                default_ss << "Unhandled SDL event: type=" << event.type;
                logger_.log(Logging::LogLevel::Debug, default_ss.str(), std::source_location::current());
                break;
        }
    }
    logger_.log(Logging::LogLevel::Debug, "Input event processing completed", std::source_location::current());
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
        std::stringstream ss;
        ss << "Handling key down: scancode=" << SDL_GetScancodeName(key.scancode);
        logger_.log(Logging::LogLevel::Debug, ss.str(), std::source_location::current());
        switch (key.scancode) {
            case SDL_SCANCODE_1: case SDL_SCANCODE_2: case SDL_SCANCODE_3: case SDL_SCANCODE_4:
            case SDL_SCANCODE_5: case SDL_SCANCODE_6: case SDL_SCANCODE_7: case SDL_SCANCODE_8: case SDL_SCANCODE_9:
                {
                    int mode = key.scancode - SDL_SCANCODE_1 + 1;
                    std::stringstream mode_ss;
                    mode_ss << "Setting mode to " << mode;
                    logger_.log(Logging::LogLevel::Debug, mode_ss.str(), std::source_location::current());
                    amouranth_.setMode(mode);
                    navigator_->setMode(mode);
                }
                break;
            case SDL_SCANCODE_KP_PLUS: case SDL_SCANCODE_EQUALS:
                logger_.log(Logging::LogLevel::Debug, "Zooming in", std::source_location::current());
                amouranth_.updateZoom(true);
                break;
            case SDL_SCANCODE_KP_MINUS: case SDL_SCANCODE_MINUS:
                logger_.log(Logging::LogLevel::Debug, "Zooming out", std::source_location::current());
                amouranth_.updateZoom(false);
                break;
            case SDL_SCANCODE_I:
                logger_.log(Logging::LogLevel::Debug, "Increasing influence by 0.1", std::source_location::current());
                amouranth_.adjustInfluence(0.1f);
                break;
            case SDL_SCANCODE_O:
                logger_.log(Logging::LogLevel::Debug, "Decreasing influence by 0.1", std::source_location::current());
                amouranth_.adjustInfluence(-0.1f);
                break;
            case SDL_SCANCODE_J:
                logger_.log(Logging::LogLevel::Debug, "Increasing nurb matter by 0.1", std::source_location::current());
                amouranth_.adjustNurbMatter(0.1f);
                break;
            case SDL_SCANCODE_K:
                logger_.log(Logging::LogLevel::Debug, "Decreasing nurb matter by 0.1", std::source_location::current());
                amouranth_.adjustNurbMatter(-0.1f);
                break;
            case SDL_SCANCODE_N:
                logger_.log(Logging::LogLevel::Debug, "Increasing nurb energy by 0.1", std::source_location::current());
                amouranth_.adjustNurbEnergy(0.1f);
                break;
            case SDL_SCANCODE_M:
                logger_.log(Logging::LogLevel::Debug, "Decreasing nurb energy by 0.1", std::source_location::current());
                amouranth_.adjustNurbEnergy(-0.1f);
                break;
            case SDL_SCANCODE_P:
                logger_.log(Logging::LogLevel::Debug, "Toggling pause", std::source_location::current());
                amouranth_.togglePause();
                break;
            case SDL_SCANCODE_C:
                logger_.log(Logging::LogLevel::Debug, "Toggling user camera", std::source_location::current());
                amouranth_.toggleUserCam();
                break;
            case SDL_SCANCODE_W:
                if (amouranth_.isUserCamActive()) {
                    logger_.log(Logging::LogLevel::Debug, "Moving user camera forward", std::source_location::current());
                    amouranth_.moveUserCam(0.0f, 0.0f, -0.1f);
                }
                break;
            case SDL_SCANCODE_S:
                if (amouranth_.isUserCamActive()) {
                    logger_.log(Logging::LogLevel::Debug, "Moving user camera backward", std::source_location::current());
                    amouranth_.moveUserCam(0.0f, 0.0f, 0.1f);
                }
                break;
            case SDL_SCANCODE_A:
                if (amouranth_.isUserCamActive()) {
                    logger_.log(Logging::LogLevel::Debug, "Moving user camera left", std::source_location::current());
                    amouranth_.moveUserCam(-0.1f, 0.0f, 0.0f);
                }
                break;
            case SDL_SCANCODE_D:
                if (amouranth_.isUserCamActive()) {
                    logger_.log(Logging::LogLevel::Debug, "Moving user camera right", std::source_location::current());
                    amouranth_.moveUserCam(0.1f, 0.0f, 0.0f);
                }
                break;
            case SDL_SCANCODE_Q:
                if (amouranth_.isUserCamActive()) {
                    logger_.log(Logging::LogLevel::Debug, "Moving user camera up", std::source_location::current());
                    amouranth_.moveUserCam(0.0f, 0.1f, 0.0f);
                }
                break;
            case SDL_SCANCODE_E:
                if (amouranth_.isUserCamActive()) {
                    logger_.log(Logging::LogLevel::Debug, "Moving user camera down", std::source_location::current());
                    amouranth_.moveUserCam(0.0f, -0.1f, 0.0f);
                }
                break;
            default:
                std::stringstream default_ss;
                default_ss << "Unhandled key scancode: " << SDL_GetScancodeName(key.scancode);
                logger_.log(Logging::LogLevel::Debug, default_ss.str(), std::source_location::current());
                break;
        }
    }
}

void HandleInput::defaultMouseButtonHandler(const SDL_MouseButtonEvent& mb) {
    if (mb.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        std::stringstream ss;
        ss << "Handling mouse button down: button=" << static_cast<int>(mb.button);
        logger_.log(Logging::LogLevel::Debug, ss.str(), std::source_location::current());
        if (mb.button == SDL_BUTTON_LEFT) {
            logger_.log(Logging::LogLevel::Debug, "Toggling user camera via left mouse button", std::source_location::current());
            amouranth_.toggleUserCam();
        } else if (mb.button == SDL_BUTTON_RIGHT) {
            logger_.log(Logging::LogLevel::Debug, "Toggling pause via right mouse button", std::source_location::current());
            amouranth_.togglePause();
        }
    }
}

void HandleInput::defaultMouseMotionHandler(const SDL_MouseMotionEvent& mm) {
    if (amouranth_.isUserCamActive()) {
        float dx = mm.xrel * 0.005f;
        float dy = mm.yrel * 0.005f;
        std::stringstream ss;
        ss << "Handling mouse motion: dx=" << dx << ", dy=" << dy;
        logger_.log(Logging::LogLevel::Debug, ss.str(), std::source_location::current());
        amouranth_.moveUserCam(-dx, -dy, 0.0f);
    }
}

void HandleInput::defaultMouseWheelHandler(const SDL_MouseWheelEvent& mw) {
    std::stringstream ss;
    ss << "Handling mouse wheel: y=" << mw.y;
    logger_.log(Logging::LogLevel::Debug, ss.str(), std::source_location::current());
    if (mw.y > 0) {
        logger_.log(Logging::LogLevel::Debug, "Zooming in via mouse wheel", std::source_location::current());
        amouranth_.updateZoom(true);
    } else if (mw.y < 0) {
        logger_.log(Logging::LogLevel::Debug, "Zooming out via mouse wheel", std::source_location::current());
        amouranth_.updateZoom(false);
    }
}

void HandleInput::defaultTextInputHandler(const SDL_TextInputEvent& ti) {
    std::stringstream ss;
    ss << "Handling text input: text=" << ti.text;
    logger_.log(Logging::LogLevel::Debug, ss.str(), std::source_location::current());
}

void HandleInput::defaultTouchHandler(const SDL_TouchFingerEvent& tf) {
    if (tf.type == SDL_EVENT_FINGER_DOWN) {
        logger_.log(Logging::LogLevel::Debug, "Handling touch down", std::source_location::current());
    }
}

void HandleInput::defaultGamepadButtonHandler(const SDL_GamepadButtonEvent& gb) {
    std::stringstream ss;
    ss << "Handling gamepad button: button=" << static_cast<int>(gb.button);
    logger_.log(Logging::LogLevel::Debug, ss.str(), std::source_location::current());
}

void HandleInput::defaultGamepadAxisHandler(const SDL_GamepadAxisEvent& ga) {
    std::stringstream ss;
    ss << "Handling gamepad axis: axis=" << static_cast<int>(ga.axis) << ", value=" << ga.value;
    logger_.log(Logging::LogLevel::Debug, ss.str(), std::source_location::current());
}

void HandleInput::defaultGamepadConnectHandler(bool connected, SDL_JoystickID id, SDL_Gamepad* pad) {
    (void)pad;
    std::stringstream ss;
    ss << "Gamepad " << (connected ? "connected" : "disconnected") << ": id=" << id;
    logger_.log(Logging::LogLevel::Info, ss.str(), std::source_location::current());
}