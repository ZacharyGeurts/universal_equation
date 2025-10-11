// handle_app.cpp
// AMOURANTH RTX Engine, October 2025 - Application handling.
// Dependencies: VulkanRenderer, AMOURANTH, DimensionalNavigator, SDL3Initializer, SDL3Audio, etc.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#include "handle_app.hpp"
#include "engine/SDL3/SDL3_audio.hpp"
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <source_location>

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
      audioDevice_(0),
      audioStream_(nullptr),
      lastFrameTime_(std::chrono::steady_clock::now()) {
    LOG_INFO_CAT("Application", "Initializing Application with title: {}, width: {}, height: {}", 
                 std::source_location::current(), title, width, height);

    sdl_ = std::make_unique<SDL3Initializer::SDL3Initializer>(title, width, height);
    LOG_DEBUG_CAT("Application", "SDL3Initializer created successfully", std::source_location::current());

    // Populate sample geometry data
    vertices_ = {
        glm::vec3(-0.5f, -0.5f, 0.0f),
        glm::vec3(0.5f, -0.5f, 0.0f),
        glm::vec3(0.0f, 0.5f, 0.0f)
    };
    indices_ = {0, 1, 2};
    LOG_DEBUG_CAT("Application", "Sample geometry data populated: {} vertices, {} indices", 
                  std::source_location::current(), vertices_.size(), indices_.size());

    renderer_ = std::make_unique<VulkanRenderer>(sdl_->getInstance(), sdl_->getSurface(),
                                                 std::span<const glm::vec3>(vertices_),
                                                 std::span<const uint32_t>(indices_),
                                                 width_, height_);
    LOG_DEBUG_CAT("Application", "VulkanRenderer created successfully", std::source_location::current());

    navigator_ = std::make_unique<DimensionalNavigator>("AMOURANTH Navigator", width_, height_, *renderer_);
    LOG_DEBUG_CAT("Application", "DimensionalNavigator created successfully", std::source_location::current());

    amouranth_.emplace(navigator_.get(), renderer_->getDevice(),
                       renderer_->getVertexBufferMemory(), renderer_->getIndexBufferMemory(),
                       renderer_->getRayTracingPipeline());
    LOG_DEBUG_CAT("Application", "AMOURANTH instance created successfully", std::source_location::current());

    inputHandler_ = std::make_unique<HandleInput>(amouranth_.value(), navigator_.get());
    LOG_DEBUG_CAT("Application", "HandleInput created successfully", std::source_location::current());

    initializeInput();
    LOG_DEBUG_CAT("Application", "Input initialized", std::source_location::current());
    initializeAudio();
    LOG_DEBUG_CAT("Application", "Audio initialized", std::source_location::current());
    LOG_INFO_CAT("Application", "Application initialized successfully", std::source_location::current());
}

Application::~Application() {
    LOG_INFO_CAT("Application", "Destroying Application", std::source_location::current());
    if (audioStream_) {
        SDL_DestroyAudioStream(audioStream_);
        audioStream_ = nullptr;
        LOG_DEBUG_CAT("Application", "Audio stream destroyed", std::source_location::current());
    }
    if (audioDevice_) {
        SDL_CloseAudioDevice(audioDevice_);
        audioDevice_ = 0;
        LOG_DEBUG_CAT("Application", "Audio device closed", std::source_location::current());
    }
    inputHandler_.reset();
    LOG_DEBUG_CAT("Application", "HandleInput destroyed", std::source_location::current());
    amouranth_.reset();
    LOG_DEBUG_CAT("Application", "AMOURANTH instance destroyed", std::source_location::current());
    navigator_.reset();
    LOG_DEBUG_CAT("Application", "DimensionalNavigator destroyed", std::source_location::current());
    renderer_.reset();
    LOG_DEBUG_CAT("Application", "VulkanRenderer destroyed", std::source_location::current());
    sdl_.reset();
    LOG_DEBUG_CAT("Application", "SDL3Initializer destroyed", std::source_location::current());
}

void Application::initializeInput() {
    inputHandler_->setCallbacks();
    LOG_INFO_CAT("Application", "Input callbacks initialized", std::source_location::current());
}

void Application::initializeAudio() {
    SDL3Audio::AudioConfig config;
    config.callback = [](Uint8* stream, int len) {
        std::fill(stream, stream + len, 0);
    };
    try {
        SDL3Audio::initAudio(config, audioDevice_, audioStream_);
        LOG_INFO_CAT("Application", "Audio initialized successfully with device ID: {}", 
                     std::source_location::current(), audioDevice_);
    } catch (const std::exception& e) {
        LOG_ERROR_CAT("Application", "Audio initialization failed: {}", std::source_location::current(), e.what());
        throw;
    }
}

void Application::run() {
    LOG_INFO_CAT("Application", "Starting application loop", std::source_location::current());
    while (mode_ != 0 && !sdl_->shouldQuit()) {
        LOG_DEBUG_CAT("Application", "Starting new frame iteration", std::source_location::current());
        sdl_->pollEvents();
        LOG_DEBUG_CAT("Application", "SDL events polled", std::source_location::current());
        inputHandler_->handleInput(*this);
        LOG_DEBUG_CAT("Application", "Input processed", std::source_location::current());
        render();
        LOG_DEBUG_CAT("Application", "Frame rendered", std::source_location::current());
    }
    LOG_INFO_CAT("Application", "Application loop terminated", std::source_location::current());
}

void Application::render() {
    LOG_DEBUG_CAT("Application", "Starting render", std::source_location::current());
    if (!amouranth_.has_value()) {
        LOG_WARNING_CAT("Application", "AMOURANTH not initialized, skipping render", std::source_location::current());
        return;
    }

    auto currentTime = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(currentTime - lastFrameTime_).count();
    lastFrameTime_ = currentTime;
    LOG_DEBUG_CAT("Application", "Calculated deltaTime: {:.3f}", std::source_location::current(), deltaTime);

    if (vertices_.empty() || indices_.empty()) {
        LOG_WARNING_CAT("Application", "No valid geometry data for rendering", std::source_location::current());
        return;
    }

    try {
        LOG_DEBUG_CAT("Application", "Calling renderer_->renderFrame()", std::source_location::current());
        renderer_->renderFrame(amouranth_.value());
        LOG_DEBUG_CAT("Application", "Frame rendered successfully with deltaTime: {:.3f}", std::source_location::current(), deltaTime);
    } catch (const std::exception& e) {
        LOG_ERROR_CAT("Application", "Render failed: {}", std::source_location::current(), e.what());
        mode_ = 0;
    }
}

void Application::handleResize(int width, int height) {
    LOG_INFO_CAT("Application", "Handling resize to width: {}, height: {}", std::source_location::current(), width, height);
    width_ = width;
    height_ = height;
    renderer_->handleResize(width, height);
    LOG_DEBUG_CAT("Application", "Renderer resized", std::source_location::current());
    navigator_->setWidth(width);
    navigator_->setHeight(height);
    LOG_DEBUG_CAT("Application", "Navigator dimensions updated", std::source_location::current());
    if (amouranth_.has_value()) {
        amouranth_.value().getUniversalEquation().getNavigator()->setWidth(width);
        amouranth_.value().getUniversalEquation().getNavigator()->setHeight(height);
        LOG_DEBUG_CAT("Application", "AMOURANTH dimensions updated", std::source_location::current());
    }
    LOG_INFO_CAT("Application", "Application resized to width: {}, height: {}", std::source_location::current(), width, height);
}

HandleInput::HandleInput(AMOURANTH& amouranth, DimensionalNavigator* navigator)
    : amouranth_(amouranth), navigator_(navigator) {
    if (!navigator) {
        LOG_ERROR_CAT("Application", "HandleInput: Null DimensionalNavigator provided", std::source_location::current());
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
    gamepadConnectCallback_ = [this](bool connected, SDL_JoystickID id, SDL_Gamepad* pad) { 
        defaultGamepadConnectHandler(connected, id, pad); 
    };
    LOG_INFO_CAT("Application", "HandleInput initialized successfully", std::source_location::current());
}

void HandleInput::handleInput(Application& app) {
    LOG_DEBUG_CAT("Application", "Processing input events", std::source_location::current());
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        LOG_DEBUG_CAT("Application", "Handling SDL event: type={}", std::source_location::current(), event.type);
        switch (event.type) {
            case SDL_EVENT_QUIT:
                LOG_INFO_CAT("Application", "Quit event received", std::source_location::current());
                app.setRenderMode(0);
                break;
            case SDL_EVENT_WINDOW_RESIZED: {
                int newWidth, newHeight;
                SDL_GetWindowSize(SDL_GetWindowFromID(event.window.windowID), &newWidth, &newHeight);
                LOG_INFO_CAT("Application", "Window resized to {}x{}", std::source_location::current(), newWidth, newHeight);
                app.handleResize(newWidth, newHeight);
                break;
            }
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
                if (keyboardCallback_) keyboardCallback_(event.key);
                LOG_DEBUG_CAT("Application", "Keyboard event processed", std::source_location::current());
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (mouseButtonCallback_) mouseButtonCallback_(event.button);
                LOG_DEBUG_CAT("Application", "Mouse button event processed", std::source_location::current());
                break;
            case SDL_EVENT_MOUSE_MOTION:
                if (mouseMotionCallback_) mouseMotionCallback_(event.motion);
                LOG_DEBUG_CAT("Application", "Mouse motion event processed", std::source_location::current());
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                if (mouseWheelCallback_) mouseWheelCallback_(event.wheel);
                LOG_DEBUG_CAT("Application", "Mouse wheel event processed", std::source_location::current());
                break;
            case SDL_EVENT_TEXT_INPUT:
                if (textInputCallback_) textInputCallback_(event.text);
                LOG_DEBUG_CAT("Application", "Text input event processed", std::source_location::current());
                break;
            case SDL_EVENT_FINGER_DOWN:
            case SDL_EVENT_FINGER_UP:
            case SDL_EVENT_FINGER_MOTION:
                if (touchCallback_) touchCallback_(event.tfinger);
                LOG_DEBUG_CAT("Application", "Touch event processed", std::source_location::current());
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                if (gamepadButtonCallback_) gamepadButtonCallback_(event.gbutton);
                LOG_DEBUG_CAT("Application", "Gamepad button event processed", std::source_location::current());
                break;
            case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                if (gamepadAxisCallback_) gamepadAxisCallback_(event.gaxis);
                LOG_DEBUG_CAT("Application", "Gamepad axis event processed", std::source_location::current());
                break;
            case SDL_EVENT_GAMEPAD_ADDED:
            case SDL_EVENT_GAMEPAD_REMOVED:
                if (gamepadConnectCallback_) {
                    gamepadConnectCallback_(
                        event.type == SDL_EVENT_GAMEPAD_ADDED,
                        event.gdevice.which,
                        event.type == SDL_EVENT_GAMEPAD_ADDED ? SDL_OpenGamepad(event.gdevice.which) : nullptr
                    );
                    LOG_DEBUG_CAT("Application", "Gamepad connect event processed", std::source_location::current());
                }
                break;
            default:
                LOG_DEBUG_CAT("Application", "Unhandled SDL event: type={}", std::source_location::current(), event.type);
                break;
        }
    }
    LOG_DEBUG_CAT("Application", "Input event processing completed", std::source_location::current());
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
    gamepadConnectCallback_ = gc ? gc : [this](bool connected, SDL_JoystickID id, SDL_Gamepad* pad) { 
        defaultGamepadConnectHandler(connected, id, pad); 
    };
    LOG_INFO_CAT("Application", "Input callbacks set successfully", std::source_location::current());
}

void HandleInput::defaultKeyboardHandler(const SDL_KeyboardEvent& key) {
    if (key.type == SDL_EVENT_KEY_DOWN) {
        LOG_DEBUG_CAT("Application", "Handling key down: scancode={}", 
                      std::source_location::current(), SDL_GetScancodeName(key.scancode));
        switch (key.scancode) {
            case SDL_SCANCODE_1: case SDL_SCANCODE_2: case SDL_SCANCODE_3: case SDL_SCANCODE_4:
            case SDL_SCANCODE_5: case SDL_SCANCODE_6: case SDL_SCANCODE_7: case SDL_SCANCODE_8: case SDL_SCANCODE_9:
                {
                    int mode = key.scancode - SDL_SCANCODE_1 + 1;
                    LOG_DEBUG_CAT("Application", "Setting mode to {}", std::source_location::current(), mode);
                    amouranth_.setMode(mode);
                    navigator_->setMode(mode);
                }
                break;
            case SDL_SCANCODE_KP_PLUS: case SDL_SCANCODE_EQUALS:
                LOG_DEBUG_CAT("Application", "Zooming in", std::source_location::current());
                amouranth_.updateZoom(true);
                break;
            case SDL_SCANCODE_KP_MINUS: case SDL_SCANCODE_MINUS:
                LOG_DEBUG_CAT("Application", "Zooming out", std::source_location::current());
                amouranth_.updateZoom(false);
                break;
            case SDL_SCANCODE_I:
                LOG_DEBUG_CAT("Application", "Increasing influence by 0.1", std::source_location::current());
                amouranth_.adjustInfluence(0.1f);
                break;
            case SDL_SCANCODE_O:
                LOG_DEBUG_CAT("Application", "Decreasing influence by 0.1", std::source_location::current());
                amouranth_.adjustInfluence(-0.1f);
                break;
            case SDL_SCANCODE_J:
                LOG_DEBUG_CAT("Application", "Increasing nurb matter by 0.1", std::source_location::current());
                amouranth_.adjustNurbMatter(0.1f);
                break;
            case SDL_SCANCODE_K:
                LOG_DEBUG_CAT("Application", "Decreasing nurb matter by 0.1", std::source_location::current());
                amouranth_.adjustNurbMatter(-0.1f);
                break;
            case SDL_SCANCODE_N:
                LOG_DEBUG_CAT("Application", "Increasing nurb energy by 0.1", std::source_location::current());
                amouranth_.adjustNurbEnergy(0.1f);
                break;
            case SDL_SCANCODE_M:
                LOG_DEBUG_CAT("Application", "Decreasing nurb energy by 0.1", std::source_location::current());
                amouranth_.adjustNurbEnergy(-0.1f);
                break;
            case SDL_SCANCODE_P:
                LOG_DEBUG_CAT("Application", "Toggling pause", std::source_location::current());
                amouranth_.togglePause();
                break;
            case SDL_SCANCODE_C:
                LOG_DEBUG_CAT("Application", "Toggling user camera", std::source_location::current());
                amouranth_.toggleUserCam();
                break;
            case SDL_SCANCODE_W:
                if (amouranth_.isUserCamActive()) {
                    LOG_DEBUG_CAT("Application", "Moving user camera forward", std::source_location::current());
                    amouranth_.moveUserCam(0.0f, 0.0f, -0.1f);
                }
                break;
            case SDL_SCANCODE_S:
                if (amouranth_.isUserCamActive()) {
                    LOG_DEBUG_CAT("Application", "Moving user camera backward", std::source_location::current());
                    amouranth_.moveUserCam(0.0f, 0.0f, 0.1f);
                }
                break;
            case SDL_SCANCODE_A:
                if (amouranth_.isUserCamActive()) {
                    LOG_DEBUG_CAT("Application", "Moving user camera left", std::source_location::current());
                    amouranth_.moveUserCam(-0.1f, 0.0f, 0.0f);
                }
                break;
            case SDL_SCANCODE_D:
                if (amouranth_.isUserCamActive()) {
                    LOG_DEBUG_CAT("Application", "Moving user camera right", std::source_location::current());
                    amouranth_.moveUserCam(0.1f, 0.0f, 0.0f);
                }
                break;
            case SDL_SCANCODE_Q:
                if (amouranth_.isUserCamActive()) {
                    LOG_DEBUG_CAT("Application", "Moving user camera up", std::source_location::current());
                    amouranth_.moveUserCam(0.0f, 0.1f, 0.0f);
                }
                break;
            case SDL_SCANCODE_E:
                if (amouranth_.isUserCamActive()) {
                    LOG_DEBUG_CAT("Application", "Moving user camera down", std::source_location::current());
                    amouranth_.moveUserCam(0.0f, -0.1f, 0.0f);
                }
                break;
            default:
                LOG_DEBUG_CAT("Application", "Unhandled key scancode: {}", 
                              std::source_location::current(), SDL_GetScancodeName(key.scancode));
                break;
        }
    }
}

void HandleInput::defaultMouseButtonHandler(const SDL_MouseButtonEvent& mb) {
    if (mb.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        LOG_DEBUG_CAT("Application", "Handling mouse button down: button={}", 
                      std::source_location::current(), static_cast<int>(mb.button));
        if (mb.button == SDL_BUTTON_LEFT) {
            LOG_DEBUG_CAT("Application", "Toggling user camera via left mouse button", std::source_location::current());
            amouranth_.toggleUserCam();
        } else if (mb.button == SDL_BUTTON_RIGHT) {
            LOG_DEBUG_CAT("Application", "Toggling pause via right mouse button", std::source_location::current());
            amouranth_.togglePause();
        }
    }
}

void HandleInput::defaultMouseMotionHandler(const SDL_MouseMotionEvent& mm) {
    if (amouranth_.isUserCamActive()) {
        float dx = mm.xrel * 0.005f;
        float dy = mm.yrel * 0.005f;
        LOG_DEBUG_CAT("Application", "Handling mouse motion: dx={:.3f}, dy={:.3f}", 
                      std::source_location::current(), dx, dy);
        amouranth_.moveUserCam(-dx, -dy, 0.0f);
    }
}

void HandleInput::defaultMouseWheelHandler(const SDL_MouseWheelEvent& mw) {
    LOG_DEBUG_CAT("Application", "Handling mouse wheel: y={}", std::source_location::current(), mw.y);
    if (mw.y > 0) {
        LOG_DEBUG_CAT("Application", "Zooming in via mouse wheel", std::source_location::current());
        amouranth_.updateZoom(true);
    } else if (mw.y < 0) {
        LOG_DEBUG_CAT("Application", "Zooming out via mouse wheel", std::source_location::current());
        amouranth_.updateZoom(false);
    }
}

void HandleInput::defaultTextInputHandler(const SDL_TextInputEvent& ti) {
    LOG_DEBUG_CAT("Application", "Handling text input: text={}", std::source_location::current(), ti.text);
}

void HandleInput::defaultTouchHandler(const SDL_TouchFingerEvent& tf) {
    if (tf.type == SDL_EVENT_FINGER_DOWN) {
        LOG_DEBUG_CAT("Application", "Handling touch down", std::source_location::current());
    }
}

void HandleInput::defaultGamepadButtonHandler(const SDL_GamepadButtonEvent& gb) {
    LOG_DEBUG_CAT("Application", "Handling gamepad button: button={}", 
                  std::source_location::current(), static_cast<int>(gb.button));
}

void HandleInput::defaultGamepadAxisHandler(const SDL_GamepadAxisEvent& ga) {
    LOG_DEBUG_CAT("Application", "Handling gamepad axis: axis={}, value={}", 
                  std::source_location::current(), static_cast<int>(ga.axis), ga.value);
}

void HandleInput::defaultGamepadConnectHandler(bool connected, SDL_JoystickID id, SDL_Gamepad* pad) {
    (void)pad;
    LOG_INFO_CAT("Application", "Gamepad {}: id={}", std::source_location::current(), 
                 connected ? "connected" : "disconnected", id);
}