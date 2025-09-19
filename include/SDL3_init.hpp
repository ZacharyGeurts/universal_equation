// SDL3_init.hpp
#ifndef SDL3_INIT_HPP
#define SDL3_INIT_HPP

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_mouse.h>
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <vector>
#include <string>
#include <algorithm>
#include <functional>
#include <memory>
#include <iostream>
#include <cstring>

struct SDLWindowDeleter {
    void operator()(SDL_Window* window) const { if (window) SDL_DestroyWindow(window); }
};

struct VulkanInstanceDeleter {
    void operator()(VkInstance instance) const { if (instance) vkDestroyInstance(instance, nullptr); }
};

struct VulkanSurfaceDeleter {
    void operator()(VkSurfaceKHR surface) const { if (surface) vkDestroySurfaceKHR(m_instance, surface, nullptr); }
    VkInstance m_instance;
};

class SDL3Initializer {
public:
    using ResizeCallback = std::function<void(int, int)>;
    using AudioCallback = std::function<void(Uint8*, int)>;
    using InputCallback = std::function<void(const SDL_KeyboardEvent&)>;
    using MouseCallback = std::function<void(const SDL_MouseButtonEvent&)>;
    using GamepadCallback = std::function<void(const SDL_GamepadButtonEvent&)>;

    struct AudioConfig {
        int frequency;
        SDL_AudioFormat format;
        int channels;
        AudioCallback callback;
        AudioConfig() : frequency(44100), format(SDL_AUDIO_S16LE), channels(2), callback(nullptr) {}
    };

    SDL3Initializer() = default;
    ~SDL3Initializer() { cleanup(); }

    // Delete copy constructor and assignment
    SDL3Initializer(const SDL3Initializer&) = delete;
    SDL3Initializer& operator=(const SDL3Initializer&) = delete;

    // Move constructor and assignment
    SDL3Initializer(SDL3Initializer&& other) noexcept = default;
    SDL3Initializer& operator=(SDL3Initializer&& other) noexcept = default;

    void initialize(
        const char* title,
        int width,
        int height,
        Uint32 windowFlags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE,
        bool enableRayTracing = true,
        const AudioConfig& audioConfig = AudioConfig()
    ) {
        initializeSDL(title, width, height, windowFlags, enableRayTracing);
        if (audioConfig.callback) {
            initializeAudio(audioConfig);
        }
        initializeInput();
    }

    void eventLoop(
        std::function<void()> renderCallback,
        ResizeCallback onResize = nullptr,
        bool exitOnClose = true,
        InputCallback inputCallback = nullptr,
        MouseCallback mouseCallback = nullptr,
        GamepadCallback gamepadCallback = nullptr
    ) {
        bool running = true;
        while (running) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                switch (event.type) {
                case SDL_EVENT_QUIT:
                case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                    if (exitOnClose) running = false;
                    break;
                case SDL_EVENT_WINDOW_RESIZED:
                    if (onResize) {
                        onResize(event.window.data1, event.window.data2);
                    } else {
                        std::cout << "Window resized to: " << event.window.data1 << "x" << event.window.data2 << std::endl;
                    }
                    break;
                case SDL_EVENT_KEY_DOWN:
                    handleKeyboardInput(event.key);
                    if (inputCallback) {
                        inputCallback(event.key);
                    }
                    break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                case SDL_EVENT_MOUSE_BUTTON_UP:
                    handleMouseInput(event.button);
                    if (mouseCallback) {
                        mouseCallback(event.button);
                    }
                    break;
                case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                case SDL_EVENT_GAMEPAD_BUTTON_UP:
                    handleGamepadInput(event.gbutton);
                    if (gamepadCallback) {
                        gamepadCallback(event.gbutton);
                    }
                    break;
                default:
                    break;
                }
            }
            if (renderCallback && running) {
                renderCallback();
            }
        }
    }

    void cleanup() {
        if (m_audioStream) {
            SDL_DestroyAudioStream(m_audioStream);
            m_audioStream = nullptr;
        }
        if (m_audioDevice) {
            SDL_CloseAudioDevice(m_audioDevice);
            m_audioDevice = 0;
        }
        m_surface.reset();
        m_instance.reset();
        m_window.reset();
        SDL_Quit();
    }

    // Getters for resources
    SDL_Window* getWindow() const { return m_window.get(); }
    VkInstance getVkInstance() const { return m_instance.get(); }
    VkSurfaceKHR getVkSurface() const { return m_surface.get(); }
    SDL_AudioDeviceID getAudioDevice() const { return m_audioDevice; }

private:
    std::unique_ptr<SDL_Window, SDLWindowDeleter> m_window;
    std::unique_ptr<std::remove_pointer_t<VkInstance>, VulkanInstanceDeleter> m_instance;
    std::unique_ptr<std::remove_pointer_t<VkSurfaceKHR>, VulkanSurfaceDeleter> m_surface;
    SDL_AudioDeviceID m_audioDevice = 0;
    SDL_AudioStream* m_audioStream = nullptr;

    void initializeSDL(
        const char* title,
        int width,
        int height,
        Uint32 windowFlags,
        bool enableRayTracing
    ) {
        // Initialize SDL with required subsystems
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD) == 0) {
            throw std::runtime_error("SDL_Init failed: " + std::string(SDL_GetError()));
        }

        // Create the window
        m_window = std::unique_ptr<SDL_Window, SDLWindowDeleter>(
            SDL_CreateWindow(title, width, height, windowFlags)
        );
        if (!m_window) {
            SDL_Quit();
            throw std::runtime_error("SDL_CreateWindow failed: " + std::string(SDL_GetError()));
        }

        // Get required Vulkan instance extensions for SDL
        Uint32 extCount = 0;
        const char* const* exts = SDL_Vulkan_GetInstanceExtensions(&extCount);
        if (!exts || extCount == 0) {
            throw std::runtime_error("SDL_Vulkan_GetInstanceExtensions failed: " + std::string(SDL_GetError()));
        }

        // Add SDL Vulkan extensions
        std::vector<const char*> extensions(exts, exts + extCount);

        // Ensure surface extension is included
        if (std::find_if(extensions.begin(), extensions.end(),
                [](const char* ext){ return strcmp(ext, VK_KHR_SURFACE_EXTENSION_NAME) == 0; }) == extensions.end()) {
            extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
        }

        // Add ray tracing extensions if enabled
        if (enableRayTracing) {
            extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
            extensions.push_back(VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME);
            extensions.push_back(VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME);
        }

        // Validation layers (disabled in release builds)
        std::vector<const char*> layers = { "VK_LAYER_KHRONOS_validation" };
#ifdef NDEBUG
        layers.clear();
#endif

        // Application info
        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = title;
        appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3;

        // Instance create info
        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
        createInfo.ppEnabledLayerNames = layers.data();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        // Create Vulkan instance
        VkInstance instance;
        VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("vkCreateInstance failed: " + std::to_string(result));
        }
        m_instance = std::unique_ptr<std::remove_pointer_t<VkInstance>, VulkanInstanceDeleter>(instance);

        // Create Vulkan surface
        VkSurfaceKHR surface;
        if (!SDL_Vulkan_CreateSurface(m_window.get(), m_instance.get(), nullptr, &surface)) {
            throw std::runtime_error("SDL_Vulkan_CreateSurface failed: " + std::string(SDL_GetError()));
        }
        m_surface = std::unique_ptr<std::remove_pointer_t<VkSurfaceKHR>, VulkanSurfaceDeleter>(
            surface, VulkanSurfaceDeleter{m_instance.get()}
        );
    }

    void initializeAudio(const AudioConfig& config) {
        // Desired audio spec
        SDL_AudioSpec spec;
        SDL_zero(spec);
        spec.freq = config.frequency;
        spec.format = config.format;
        spec.channels = config.channels;

        // Open default audio device (use 0 for default)
        m_audioDevice = SDL_OpenAudioDevice(0, &spec);
        if (m_audioDevice == 0) {
            throw std::runtime_error("SDL_OpenAudioDevice failed: " + std::string(SDL_GetError()));
        }

        // Create audio stream for buffering
        m_audioStream = SDL_CreateAudioStream(&spec, &spec);
        if (!m_audioStream) {
            SDL_CloseAudioDevice(m_audioDevice);
            throw std::runtime_error("SDL_CreateAudioStream failed: " + std::string(SDL_GetError()));
        }

        // Bind stream to device
        if (SDL_BindAudioStream(m_audioDevice, m_audioStream) != 0) {
            SDL_DestroyAudioStream(m_audioStream);
            SDL_CloseAudioDevice(m_audioDevice);
            throw std::runtime_error("SDL_BindAudioStream failed: " + std::string(SDL_GetError()));
        }

        // Set callback for audio generation if provided
        if (config.callback) {
            SDL_SetAudioStreamPutCallback(m_audioStream, [](void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount) {
                auto* cb = static_cast<AudioCallback*>(userdata);
                std::vector<Uint8> buffer(additional_amount);
                (*cb)(buffer.data(), additional_amount);
                SDL_PutAudioStreamData(stream, buffer.data(), additional_amount);
            }, &const_cast<AudioCallback&>(config.callback));
        }

        // Resume playback
        SDL_ResumeAudioDevice(m_audioDevice);
    }

    void initializeInput() {
        // Enable HIDAPI for better gamepad support
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");
        // SDL_INIT_GAMEPAD is already called in SDL_Init
    }

    void handleKeyboardInput(const SDL_KeyboardEvent& key) {
        if (key.type == SDL_EVENT_KEY_DOWN) {
            switch (key.key) {
                case SDLK_F: {
                    // Toggle fullscreen
                    Uint32 flags = SDL_GetWindowFlags(m_window.get());
                    SDL_SetWindowFullscreen(m_window.get(), (flags & SDL_WINDOW_FULLSCREEN) ? 0 : SDL_WINDOW_FULLSCREEN);
                    break;
                }
                case SDLK_ESCAPE: {
                    // Trigger quit event
                    SDL_Event quitEvent;
                    quitEvent.type = SDL_EVENT_QUIT;
                    SDL_PushEvent(&quitEvent);
                    break;
                }
                case SDLK_SPACE: {
                    // Toggle audio pause/resume
                    if (m_audioDevice) {
                        if (SDL_AudioDevicePaused(m_audioDevice)) {
                            SDL_ResumeAudioDevice(m_audioDevice);
                        } else {
                            SDL_PauseAudioDevice(m_audioDevice);
                        }
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

    void handleMouseInput(const SDL_MouseButtonEvent& button) {
        if (button.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            switch (button.button) {
                case SDL_BUTTON_LEFT:
                    std::cout << "Left mouse button pressed at (" << button.x << ", " << button.y << ")" << std::endl;
                    break;
                case SDL_BUTTON_RIGHT: {
                    std::cout << "Right mouse button pressed at (" << button.x << ", " << button.y << ")" << std::endl;
                    // Toggle relative mouse mode (SDL3 API)
                    bool relativeMode = static_cast<bool>(SDL_GetWindowRelativeMouseMode(m_window.get()));
                    SDL_SetWindowRelativeMouseMode(m_window.get(), !relativeMode);
                    break;
                }
                case SDL_BUTTON_MIDDLE:
                    std::cout << "Middle mouse button pressed at (" << button.x << ", " << button.y << ")" << std::endl;
                    break;
                default:
                    break;
            }
        } else if (button.type == SDL_EVENT_MOUSE_BUTTON_UP) {
            switch (button.button) {
                case SDL_BUTTON_LEFT:
                    std::cout << "Left mouse button released at (" << button.x << ", " << button.y << ")" << std::endl;
                    break;
                case SDL_BUTTON_RIGHT:
                    std::cout << "Right mouse button released at (" << button.x << ", " << button.y << ")" << std::endl;
                    break;
                case SDL_BUTTON_MIDDLE:
                    std::cout << "Middle mouse button released at (" << button.x << ", " << button.y << ")" << std::endl;
                    break;
                default:
                    break;
            }
        }
    }

    void handleGamepadInput(const SDL_GamepadButtonEvent& gbutton) {
        if (gbutton.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
            switch (gbutton.button) {
                case SDL_GAMEPAD_BUTTON_SOUTH: // A/Cross
                    std::cout << "Gamepad South button (A/Cross) pressed" << std::endl;
                    break;
                case SDL_GAMEPAD_BUTTON_EAST: // B/Circle
                    std::cout << "Gamepad East button (B/Circle) pressed" << std::endl;
                    // Trigger quit
                    SDL_Event quitEvent;
                    quitEvent.type = SDL_EVENT_QUIT;
                    SDL_PushEvent(&quitEvent);
                    break;
                case SDL_GAMEPAD_BUTTON_WEST: // X/Square
                    std::cout << "Gamepad West button (X/Square) pressed" << std::endl;
                    break;
                case SDL_GAMEPAD_BUTTON_NORTH: // Y/Triangle
                    std::cout << "Gamepad North button (Y/Triangle) pressed" << std::endl;
                    break;
                case SDL_GAMEPAD_BUTTON_START:
                    std::cout << "Gamepad Start button pressed" << std::endl;
                    // Toggle audio
                    if (m_audioDevice) {
                        if (SDL_AudioDevicePaused(m_audioDevice)) {
                            SDL_ResumeAudioDevice(m_audioDevice);
                        } else {
                            SDL_PauseAudioDevice(m_audioDevice);
                        }
                    }
                    break;
                default:
                    std::cout << "Gamepad button " << static_cast<int>(gbutton.button) << " pressed" << std::endl;
                    break;
            }
        } else if (gbutton.type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
            std::cout << "Gamepad button " << static_cast<int>(gbutton.button) << " released" << std::endl;
        }
    }
};

#endif // SDL3_INIT_HPP