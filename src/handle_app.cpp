// AMOURANTH RTX Engine, October 2025 - Application and input handling implementation.
// Manages SDL3 input events, Vulkan rendering, audio, and application lifecycle.
// Dependencies: SDL3, Vulkan, GLM, C++20 standard library.
// Zachary Geurts 2025

#include "handle_app.hpp"
#include <stdexcept>
#include <format>

Application::Application(const char* title, int width, int height)
    : title_(title), width_(width), height_(height), mode_(1), logger_(Logging::LogLevel::Info, "amouranth.log"),
      audioDevice_(0), audioStream_(nullptr), lastFrameTime_(std::chrono::steady_clock::now()), amouranth_{} {
    logger_.log(Logging::LogLevel::Info, "Initializing Application with title={}, width={}, height={}",
                std::source_location::current(), title, width, height);

    sdl_ = std::make_unique<SDL3Initializer>(title, width, height, logger_);

    // Populate sample geometry data
    vertices_ = {
        glm::vec3(-0.5f, -0.5f, 0.0f),
        glm::vec3(0.5f, -0.5f, 0.0f),
        glm::vec3(0.0f, 0.5f, 0.0f)
    };
    indices_ = {0, 1, 2};

    renderer_ = std::make_unique<VulkanRenderer>(sdl_->getInstance(), sdl_->getSurface(),
                                                 std::span<const glm::vec3>(vertices_),
                                                 std::span<const uint32_t>(indices_),
                                                 width_, height_, logger_);
    navigator_ = std::make_unique<DimensionalNavigator>("AMOURANTH Navigator", width_, height_, *renderer_, logger_);
    amouranth_.emplace(navigator_.get(), logger_, renderer_->getDevice(),
                       renderer_->getVertexBufferMemory(), renderer_->getGraphicsPipeline());
    inputHandler_ = std::make_unique<HandleInput>(amouranth_.value(), navigator_.get(), logger_);

    initializeInput();
    initializeAudio();
    logger_.log(Logging::LogLevel::Info, "Application initialized successfully", std::source_location::current());
}

Application::~Application() {
    logger_.log(Logging::LogLevel::Info, "Destroying Application", std::source_location::current());
    if (audioStream_) {
        SDL_DestroyAudioStream(audioStream_);
        audioStream_ = nullptr;
    }
    if (audioDevice_) {
        SDL_CloseAudioDevice(audioDevice_);
        audioDevice_ = 0;
    }
    inputHandler_.reset();
    amouranth_.reset();
    navigator_.reset();
    renderer_.reset();
    sdl_.reset();
}

void Application::initializeInput() {
    inputHandler_->setCallbacks();
    logger_.log(Logging::LogLevel::Info, "Input callbacks initialized", std::source_location::current());
}

void Application::initializeAudio() {
    // Note: Ensure SDL3_audio.hpp uses 'namespace SDL3Audio' instead of 'namespace SDL3Initializer'
    SDL3Audio::AudioConfig config;
    config.callback = [](Uint8* stream, int len) {
        // Example callback: fill with silence (extend for actual audio)
        std::fill(stream, stream + len, 0);
    };
    try {
        SDL3Audio::initAudio(config, audioDevice_, audioStream_, logger_);
        logger_.log(Logging::LogLevel::Info, "Audio initialized successfully with device ID: {}", std::source_location::current(), audioDevice_);
    } catch (const std::exception& e) {
        logger_.log(Logging::LogLevel::Error, "Audio initialization failed: {}", std::source_location::current(), e.what());
        throw;
    }
}

void Application::run() {
    logger_.log(Logging::LogLevel::Info, "Starting application loop", std::source_location::current());
    while (mode_ != 0 && !sdl_->shouldQuit()) {
        sdl_->pollEvents();
        inputHandler_->handleInput(*this);
        render();
    }
    logger_.log(Logging::LogLevel::Info, "Application loop terminated", std::source_location::current());
}

void Application::render() {
    if (!amouranth_.has_value()) {
        logger_.log(Logging::LogLevel::Warning, "AMOURANTH not initialized, skipping render", std::source_location::current());
        return;
    }

    auto currentTime = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(currentTime - lastFrameTime_).count();
    lastFrameTime_ = currentTime;

    if (vertices_.empty() || indices_.empty()) {
        logger_.log(Logging::LogLevel::Warning, "No valid geometry data for rendering", std::source_location::current());
        return;
    }

    try {
        renderer_->beginFrame();
        amouranth_.value().render(
            renderer_->getCurrentImageIndex(),
            renderer_->getVertexBuffer(),
            renderer_->getCommandBuffer(),
            renderer_->getIndexBuffer(),
            renderer_->getPipelineLayout(),
            renderer_->getDescriptorSet(),
            renderer_->getRenderPass(),
            renderer_->getFramebuffer(),
            deltaTime
        );
        renderer_->endFrame();
        logger_.log(Logging::LogLevel::Debug, "Frame rendered successfully with deltaTime={:.3f}", std::source_location::current(), deltaTime);
    } catch (const std::exception& e) {
        logger_.log(Logging::LogLevel::Error, "Render failed: {}", std::source_location::current(), e.what());
        mode_ = 0; // Exit on render failure
    }
}

void Application::handleResize(int width, int height) {
    width_ = width;
    height_ = height;
    renderer_->handleResize(width, height);
    navigator_->setWidth(width);
    navigator_->setHeight(height);
    if (amouranth_.has_value()) {
        amouranth_.value().setWidth(width);
        amouranth_.value().setHeight(height);
    }
    logger_.log(Logging::LogLevel::Info, "Application resized to width={}, height={}", std::source_location::current(), width, height);
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
                logger_.log(Logging::LogLevel::Debug, "Unhandled SDL event: type={}", std::source_location::current(), static_cast<int>(event.type));
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
                amouranth_.setMode(key.scancode - SDL_SCANCODE_1 + 1);
                navigator_->setMode(key.scancode - SDL_SCANCODE_1 + 1);
                break;
            case SDL_SCANCODE_KP_PLUS: case SDL_SCANCODE_EQUALS:
                amouranth_.updateZoom(true);
                break;
            case SDL_SCANCODE_KP_MINUS: case SDL_SCANCODE_MINUS:
                amouranth_.updateZoom(false);
                break;
            case SDL_SCANCODE_I:
                amouranth_.adjustInfluence(0.1f);
                break;
            case SDL_SCANCODE_O:
                amouranth_.adjustInfluence(-0.1f);
                break;
            case SDL_SCANCODE_J:
                amouranth_.adjustNurbMatter(0.1f);
                break;
            case SDL_SCANCODE_K:
                amouranth_.adjustNurbMatter(-0.1f);
                break;
            case SDL_SCANCODE_N:
                amouranth_.adjustNurbEnergy(0.1f);
                break;
            case SDL_SCANCODE_M:
                amouranth_.adjustNurbEnergy(-0.1f);
                break;
            case SDL_SCANCODE_P:
                amouranth_.togglePause();
                break;
            case SDL_SCANCODE_C:
                amouranth_.toggleUserCam();
                break;
            case SDL_SCANCODE_W:
                if (amouranth_.isUserCamActive()) amouranth_.moveUserCam(0.0f, 0.0f, -0.1f);
                break;
            case SDL_SCANCODE_S:
                if (amouranth_.isUserCamActive()) amouranth_.moveUserCam(0.0f, 0.0f, 0.1f);
                break;
            case SDL_SCANCODE_A:
                if (amouranth_.isUserCamActive()) amouranth_.moveUserCam(-0.1f, 0.0f, 0.0f);
                break;
            case SDL_SCANCODE_D:
                if (amouranth_.isUserCamActive()) amouranth_.moveUserCam(0.1f, 0.0f, 0.0f);
                break;
            case SDL_SCANCODE_Q:
                if (amouranth_.isUserCamActive()) amouranth_.moveUserCam(0.0f, 0.1f, 0.0f);
                break;
            case SDL_SCANCODE_E:
                if (amouranth_.isUserCamActive()) amouranth_.moveUserCam(0.0f, -0.1f, 0.0f);
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
            amouranth_.toggleUserCam();
        } else if (mb.button == SDL_BUTTON_RIGHT) {
            amouranth_.togglePause();
        }
    }
}

void HandleInput::defaultMouseMotionHandler(const SDL_MouseMotionEvent& mm) {
    if (amouranth_.isUserCamActive()) {
        float dx = mm.xrel * 0.005f;
        float dy = mm.yrel * 0.005f;
        logger_.log(Logging::LogLevel::Debug, "Handling mouse motion: dx={}, dy={}", std::source_location::current(), dx, dy);
        amouranth_.moveUserCam(-dx, -dy, 0.0f);
    }
}

void HandleInput::defaultMouseWheelHandler(const SDL_MouseWheelEvent& mw) {
    logger_.log(Logging::LogLevel::Debug, "Handling mouse wheel: y={}", std::source_location::current(), mw.y);
    if (mw.y > 0) {
        amouranth_.updateZoom(true);
    } else if (mw.y < 0) {
        amouranth_.updateZoom(false);
    }
}

void HandleInput::defaultTextInputHandler(const SDL_TextInputEvent& ti) {
    logger_.log(Logging::LogLevel::Debug, "Handling text input: text={}", std::source_location::current(), ti.text);
}

void HandleInput::defaultTouchHandler(const SDL_TouchFingerEvent& tf) {
    if (tf.type == SDL_EVENT_FINGER_DOWN) {
        logger_.log(Logging::LogLevel::Debug, "Handling touch down", std::source_location::current());
    }
}

void HandleInput::defaultGamepadButtonHandler(const SDL_GamepadButtonEvent& gb) {
    logger_.log(Logging::LogLevel::Debug, "Handling gamepad button: button={}", std::source_location::current(), gb.button);
}

void HandleInput::defaultGamepadAxisHandler(const SDL_GamepadAxisEvent& ga) {
    logger_.log(Logging::LogLevel::Debug, "Handling gamepad axis: axis={}, value={}", std::source_location::current(), ga.axis, ga.value);
}

void HandleInput::defaultGamepadConnectHandler(bool connected, SDL_JoystickID id, SDL_Gamepad* pad) {
    (void)pad; // Suppress unused parameter warning
    logger_.log(Logging::LogLevel::Info, "Gamepad {}: id={}", std::source_location::current(),
                connected ? "connected" : "disconnected", id);
}