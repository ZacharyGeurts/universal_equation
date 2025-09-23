#ifndef SDL3_INIT_HPP
#define SDL3_INIT_HPP
// SDL3 AMOURANTH RTX engine
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_ttf.h>
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <vector>
#include <string>
#include <functional>
#include <memory>
#include <map>
#include <algorithm>
#include <cstring>
#include <iostream>

struct AudioConfig {
    int frequency = 44100;
    SDL_AudioFormat format = SDL_AUDIO_S16LE;
    int channels = 8;
    std::function<void(Uint8*, int)> callback = nullptr;
};

struct SDLWindowDeleter {
    void operator()(SDL_Window* w) const {
        std::cout << "Destroying SDL Window" << std::endl;
        if (w) SDL_DestroyWindow(w);
    }
};

struct VulkanInstanceDeleter {
    void operator()(VkInstance i) const {
        std::cout << "Destroying Vulkan Instance" << std::endl;
        if (i) vkDestroyInstance(i, nullptr);
    }
};

struct VulkanSurfaceDeleter {
    void operator()(VkSurfaceKHR s) const {
        std::cout << "Destroying Vulkan Surface" << std::endl;
        if (s) vkDestroySurfaceKHR(m_instance, s, nullptr);
    }
    VkInstance m_instance;
};

class SDL3Initializer {
public:
    using ResizeCallback = std::function<void(int, int)>;
    using AudioCallback = std::function<void(Uint8*, int)>;
    using KeyboardCallback = std::function<void(const SDL_KeyboardEvent&)>;
    using MouseButtonCallback = std::function<void(const SDL_MouseButtonEvent&)>;
    using MouseMotionCallback = std::function<void(const SDL_MouseMotionEvent&)>;
    using MouseWheelCallback = std::function<void(const SDL_MouseWheelEvent&)>;
    using TextInputCallback = std::function<void(const SDL_TextInputEvent&)>;
    using TouchCallback = std::function<void(const SDL_TouchFingerEvent&)>;
    using GamepadButtonCallback = std::function<void(const SDL_GamepadButtonEvent&)>;
    using GamepadAxisCallback = std::function<void(const SDL_GamepadAxisEvent&)>;
    using GamepadConnectCallback = std::function<void(bool, SDL_JoystickID, SDL_Gamepad*)>;

    SDL3Initializer() {
        std::cout << "Constructing SDL3Initializer" << std::endl;
    }

    ~SDL3Initializer() {
        std::cout << "Destructing SDL3Initializer" << std::endl;
        cleanup();
    }

    SDL3Initializer(const SDL3Initializer&) = delete;
    SDL3Initializer& operator=(const SDL3Initializer&) = delete;
    SDL3Initializer(SDL3Initializer&&) noexcept = default;
    SDL3Initializer& operator=(SDL3Initializer&&) noexcept = default;

    void initialize(const char* title, int w, int h, Uint32 flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE, bool rt = true, const AudioConfig& ac = AudioConfig()) {
        std::cout << "Initializing SDL3Initializer with title: " << title << ", width: " << w << ", height: " << h << std::endl;
        initSDL(title, w, h, flags, rt);
        if (ac.callback) {
            std::cout << "Initializing audio" << std::endl;
            initAudio(ac);
        }
        std::cout << "Initializing input" << std::endl;
        initInput();
    }

    void eventLoop(std::function<void()> render, ResizeCallback onResize = nullptr, bool exitOnClose = true,
                   KeyboardCallback kb = nullptr, MouseButtonCallback mb = nullptr, MouseMotionCallback mm = nullptr,
                   MouseWheelCallback mw = nullptr, TextInputCallback ti = nullptr, TouchCallback tc = nullptr,
                   GamepadButtonCallback gb = nullptr, GamepadAxisCallback ga = nullptr, GamepadConnectCallback gc = nullptr) {
        std::cout << "Starting event loop" << std::endl;
        if (ti) {
            std::cout << "Enabling text input" << std::endl;
            SDL_StartTextInput(m_window.get());
        }
        for (bool running = true; running;) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                std::cout << "Processing SDL event type: " << e.type << std::endl;
                switch (e.type) {
                    case SDL_EVENT_QUIT:
                        std::cout << "Quit event received" << std::endl;
                        running = !exitOnClose;
                        break;
                    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                        std::cout << "Window close requested" << std::endl;
                        running = !exitOnClose;
                        break;
                    case SDL_EVENT_WINDOW_RESIZED:
                        std::cout << "Window resized to " << e.window.data1 << "x" << e.window.data2 << std::endl;
                        if (onResize) onResize(e.window.data1, e.window.data2);
                        else std::cout << "Resized: " << e.window.data1 << "x" << e.window.data2 << '\n';
                        break;
                    case SDL_EVENT_KEY_DOWN:
                        std::cout << "Key down event: " << SDL_GetKeyName(e.key.key) << std::endl;
                        handleKeyboard(e.key);
                        if (kb) kb(e.key);
                        break;
                    case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    case SDL_EVENT_MOUSE_BUTTON_UP:
                        std::cout << "Mouse button event: " << (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? "Down" : "Up") << std::endl;
                        handleMouseButton(e.button);
                        if (mb) mb(e.button);
                        break;
                    case SDL_EVENT_MOUSE_MOTION:
                        std::cout << "Mouse motion event at (" << e.motion.x << ", " << e.motion.y << ")" << std::endl;
                        if (mm) mm(e.motion);
                        break;
                    case SDL_EVENT_MOUSE_WHEEL:
                        std::cout << "Mouse wheel event" << std::endl;
                        if (mw) mw(e.wheel);
                        break;
                    case SDL_EVENT_TEXT_INPUT:
                        std::cout << "Text input event: " << e.text.text << std::endl;
                        if (ti) ti(e.text);
                        break;
                    case SDL_EVENT_FINGER_DOWN:
                    case SDL_EVENT_FINGER_UP:
                    case SDL_EVENT_FINGER_MOTION:
                        std::cout << "Touch event: " << (e.type == SDL_EVENT_FINGER_DOWN ? "Down" : e.type == SDL_EVENT_FINGER_UP ? "Up" : "Motion") << std::endl;
                        handleTouch(e.tfinger);
                        if (tc) tc(e.tfinger);
                        break;
                    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                    case SDL_EVENT_GAMEPAD_BUTTON_UP:
                        std::cout << "Gamepad button event: " << (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ? "Down" : "Up") << std::endl;
                        handleGamepadButton(e.gbutton);
                        if (gb) gb(e.gbutton);
                        break;
                    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                        std::cout << "Gamepad axis motion event" << std::endl;
                        if (ga) ga(e.gaxis);
                        break;
                    case SDL_EVENT_GAMEPAD_ADDED:
                        std::cout << "Gamepad added: ID " << e.gdevice.which << std::endl;
                        if (SDL_Gamepad* gp = SDL_OpenGamepad(e.gdevice.which)) {
                            std::cout << "Opened gamepad: ID " << e.gdevice.which << std::endl;
                            m_gamepads[e.gdevice.which] = gp;
                            if (gc) gc(true, e.gdevice.which, gp);
                        }
                        break;
                    case SDL_EVENT_GAMEPAD_REMOVED:
                        std::cout << "Gamepad removed: ID " << e.gdevice.which << std::endl;
                        if (auto it = m_gamepads.find(e.gdevice.which); it != m_gamepads.end()) {
                            std::cout << "Closing gamepad: ID " << e.gdevice.which << std::endl;
                            SDL_CloseGamepad(it->second);
                            if (gc) gc(false, e.gdevice.which, nullptr);
                            m_gamepads.erase(it);
                        }
                        break;
                }
            }
            if (render && running) {
                std::cout << "Calling render function" << std::endl;
                render();
            }
        }
        if (ti) {
            std::cout << "Disabling text input" << std::endl;
            SDL_StopTextInput(m_window.get());
        }
        std::cout << "Exiting event loop" << std::endl;
    }

    void cleanup() {
        std::cout << "Starting cleanup" << std::endl;
        for (auto& [id, gp] : m_gamepads) {
            std::cout << "Closing gamepad: ID " << id << std::endl;
            SDL_CloseGamepad(gp);
        }
        m_gamepads.clear();
        std::cout << "Cleared gamepads" << std::endl;
        if (m_audioStream) {
            std::cout << "Destroying audio stream" << std::endl;
            SDL_DestroyAudioStream(m_audioStream);
        }
        if (m_audioDevice) {
            std::cout << "Closing audio device: ID " << m_audioDevice << std::endl;
            SDL_CloseAudioDevice(m_audioDevice);
        }
        if (m_font) {
            std::cout << "Closing TTF font" << std::endl;
            TTF_CloseFont(m_font);
            m_font = nullptr; // Set to nullptr after closing
        }
        std::cout << "Quitting TTF" << std::endl;
        TTF_Quit();
        std::cout << "Resetting Vulkan surface" << std::endl;
        m_surface.reset();
        std::cout << "Resetting Vulkan instance" << std::endl;
        m_instance.reset();
        std::cout << "Resetting SDL window" << std::endl;
        m_window.reset();
        std::cout << "Quitting SDL" << std::endl;
        SDL_Quit();
        std::cout << "Cleanup completed" << std::endl;
    }

    SDL_Window* getWindow() const {
        std::cout << "Getting SDL window" << std::endl;
        return m_window.get();
    }

    VkInstance getVkInstance() const {
        std::cout << "Getting Vulkan instance" << std::endl;
        return m_instance.get();
    }

    VkSurfaceKHR getVkSurface() const {
        std::cout << "Getting Vulkan surface" << std::endl;
        return m_surface.get();
    }

    SDL_AudioDeviceID getAudioDevice() const {
        std::cout << "Getting audio device ID: " << m_audioDevice << std::endl;
        return m_audioDevice;
    }

    const auto& getGamepads() const {
        std::cout << "Getting gamepads" << std::endl;
        return m_gamepads;
    }

    TTF_Font* getFont() const {
        std::cout << "Getting TTF font" << std::endl;
        return m_font;
    }

private:
    std::unique_ptr<SDL_Window, SDLWindowDeleter> m_window;
    std::unique_ptr<std::remove_pointer_t<VkInstance>, VulkanInstanceDeleter> m_instance;
    std::unique_ptr<std::remove_pointer_t<VkSurfaceKHR>, VulkanSurfaceDeleter> m_surface;
    SDL_AudioDeviceID m_audioDevice = 0;
    SDL_AudioStream* m_audioStream = nullptr;
    std::map<SDL_JoystickID, SDL_Gamepad*> m_gamepads;
    TTF_Font* m_font = nullptr;

    void initSDL(const char* title, int w, int h, Uint32 flags, bool rt) {
        std::cout << "Initializing SDL subsystems" << std::endl;
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS) == 0) {
            std::cout << "SDL_Init failed: " << SDL_GetError() << std::endl;
            throw std::runtime_error("SDL_Init failed: " + std::string(SDL_GetError()));
        }
        std::cout << "Initializing TTF" << std::endl;
        if (TTF_Init() == 0) {
            std::cout << "TTF_Init failed: " << SDL_GetError() << std::endl;
            throw std::runtime_error("TTF_Init failed: " + std::string(SDL_GetError()));
        }
        std::cout << "Creating SDL window" << std::endl;
        m_window = std::unique_ptr<SDL_Window, SDLWindowDeleter>(SDL_CreateWindow(title, w, h, flags));
        if (!m_window) {
            std::cout << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
            throw std::runtime_error("SDL_CreateWindow failed: " + std::string(SDL_GetError()));
        }
        std::cout << "Opening TTF font" << std::endl;
        m_font = TTF_OpenFont("assets/fonts/sf-plasmatica-open.ttf", 24);
        if (!m_font) {
            std::cout << "TTF_OpenFont failed: " << SDL_GetError() << std::endl;
            throw std::runtime_error("TTF_OpenFont failed: " + std::string(SDL_GetError()));
        }
        std::cout << "Getting Vulkan instance extensions" << std::endl;
        Uint32 extCount;
        auto exts = SDL_Vulkan_GetInstanceExtensions(&extCount);
        if (!exts || !extCount) {
            std::cout << "SDL_Vulkan_GetInstanceExtensions failed: " << SDL_GetError() << std::endl;
            throw std::runtime_error("SDL_Vulkan_GetInstanceExtensions failed: " + std::string(SDL_GetError()));
        }
        std::vector<const char*> extensions(exts, exts + extCount);
        std::cout << "Checking for VK_KHR_SURFACE_EXTENSION_NAME" << std::endl;
        if (std::find(extensions.begin(), extensions.end(), VK_KHR_SURFACE_EXTENSION_NAME) == extensions.end()) {
            std::cout << "Adding VK_KHR_SURFACE_EXTENSION_NAME to extensions" << std::endl;
            extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
        }
        if (rt) {
            std::cout << "Enumerating Vulkan instance extension properties" << std::endl;
            uint32_t count;
            vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
            std::vector<VkExtensionProperties> props(count);
            vkEnumerateInstanceExtensionProperties(nullptr, &count, props.data());
            const char* rtExts[] = {
                VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
                VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
                VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
                VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
            };
            for (auto ext : rtExts) {
                if (std::any_of(props.begin(), props.end(), [ext](auto& p) { return !strcmp(p.extensionName, ext); })) {
                    std::cout << "Adding Vulkan extension: " << ext << std::endl;
                    extensions.push_back(ext);
                }
            }
        }
        std::vector<const char*> layers;
#ifndef NDEBUG
        std::cout << "Adding Vulkan validation layer" << std::endl;
        layers.push_back("VK_LAYER_KHRONOS_validation");
#endif
        std::cout << "Creating Vulkan instance" << std::endl;
        VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO, nullptr, title, VK_MAKE_API_VERSION(0, 1, 0, 0), "AMOURANTH RTX", VK_MAKE_API_VERSION(0, 1, 0, 0), VK_API_VERSION_1_3};
        VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, nullptr, 0, &appInfo, (uint32_t)layers.size(), layers.data(), (uint32_t)extensions.size(), extensions.data()};
        VkInstance instance;
        if (vkCreateInstance(&ci, nullptr, &instance) != VK_SUCCESS) {
            std::cout << "vkCreateInstance failed" << std::endl;
            throw std::runtime_error("vkCreateInstance failed");
        }
        m_instance = std::unique_ptr<std::remove_pointer_t<VkInstance>, VulkanInstanceDeleter>(instance);
        std::cout << "Vulkan instance created" << std::endl;
        VkSurfaceKHR surface;
        std::cout << "Creating Vulkan surface" << std::endl;
        if (!SDL_Vulkan_CreateSurface(m_window.get(), m_instance.get(), nullptr, &surface)) {
            std::cout << "SDL_Vulkan_CreateSurface failed: " << SDL_GetError() << std::endl;
            throw std::runtime_error("SDL_Vulkan_CreateSurface failed: " + std::string(SDL_GetError()));
        }
        m_surface = std::unique_ptr<std::remove_pointer_t<VkSurfaceKHR>, VulkanSurfaceDeleter>(surface, VulkanSurfaceDeleter{m_instance.get()});
        std::cout << "Vulkan surface created" << std::endl;
    }

    void initAudio(const AudioConfig& c) {
        std::cout << "Initializing audio with frequency: " << c.frequency << ", channels: " << c.channels << std::endl;
        SDL_AudioSpec spec{};
        spec.freq = c.frequency;
        spec.format = c.format;
        spec.channels = c.channels;
        std::cout << "Opening audio device" << std::endl;
        m_audioDevice = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
        if (!m_audioDevice) {
            std::cout << "SDL_OpenAudioDevice failed: " << SDL_GetError() << std::endl;
            throw std::runtime_error("SDL_OpenAudioDevice failed: " + std::string(SDL_GetError()));
        }
        std::cout << "Getting audio device format" << std::endl;
        SDL_AudioSpec obtained;
        if (SDL_GetAudioDeviceFormat(m_audioDevice, &obtained, nullptr) != 0) {
            std::cout << "SDL_GetAudioDeviceFormat failed: " << SDL_GetError() << std::endl;
            SDL_CloseAudioDevice(m_audioDevice);
            throw std::runtime_error("SDL_GetAudioDeviceFormat failed: " + std::string(SDL_GetError()));
        }
        std::cout << "Creating audio stream" << std::endl;
        m_audioStream = SDL_CreateAudioStream(&spec, &obtained);
        if (!m_audioStream) {
            std::cout << "SDL_CreateAudioStream failed: " << SDL_GetError() << std::endl;
            throw std::runtime_error("SDL_CreateAudioStream failed: " + std::string(SDL_GetError()));
        }
        std::cout << "Binding audio stream to device" << std::endl;
        if (SDL_BindAudioStream(m_audioDevice, m_audioStream) != 0) {
            std::cout << "SDL_BindAudioStream failed: " << SDL_GetError() << std::endl;
            SDL_DestroyAudioStream(m_audioStream);
            SDL_CloseAudioDevice(m_audioDevice);
            throw std::runtime_error("SDL_BindAudioStream failed: " + std::string(SDL_GetError()));
        }
        if (c.callback) {
            std::cout << "Setting audio stream callback" << std::endl;
            SDL_SetAudioStreamPutCallback(m_audioStream, [](void* u, SDL_AudioStream* s, int n, int) {
                std::cout << "Audio callback triggered, processing " << n << " bytes" << std::endl;
                std::vector<Uint8> buf(n);
                (*static_cast<AudioCallback*>(u))(buf.data(), n);
                SDL_PutAudioStreamData(s, buf.data(), n);
            }, &const_cast<AudioCallback&>(c.callback));
        }
        std::cout << "Resuming audio device" << std::endl;
        SDL_ResumeAudioDevice(m_audioDevice);
    }

    void initInput() {
        std::cout << "Initializing input system" << std::endl;
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");
        std::cout << "Getting connected joysticks" << std::endl;
        auto* joysticks = SDL_GetJoysticks(nullptr);
        if (joysticks) {
            for (int i = 0; joysticks[i]; ++i) {
                if (SDL_IsGamepad(joysticks[i])) {
                    std::cout << "Found gamepad: ID " << joysticks[i] << std::endl;
                    if (auto gp = SDL_OpenGamepad(joysticks[i])) {
                        std::cout << "Opened gamepad: ID " << joysticks[i] << std::endl;
                        m_gamepads[joysticks[i]] = gp;
                    }
                }
            }
            std::cout << "Freeing joystick list" << std::endl;
            SDL_free(joysticks);
        }
    }

    void handleKeyboard(const SDL_KeyboardEvent& k) {
        if (k.type != SDL_EVENT_KEY_DOWN) {
            std::cout << "Ignoring non-key-down event" << std::endl;
            return;
        }
        std::cout << "Handling key: " << SDL_GetKeyName(k.key) << std::endl;
        switch (k.key) {
            case SDLK_F:
                std::cout << "Toggling fullscreen mode" << std::endl;
                SDL_SetWindowFullscreen(m_window.get(), SDL_GetWindowFlags(m_window.get()) & SDL_WINDOW_FULLSCREEN ? 0 : SDL_WINDOW_FULLSCREEN);
                break;
            case SDLK_ESCAPE:
                std::cout << "Pushing quit event" << std::endl;
                {
                    SDL_Event evt{.type = SDL_EVENT_QUIT};
                    SDL_PushEvent(&evt);
                }
                break;
            case SDLK_SPACE:
                std::cout << "Toggling audio pause/resume" << std::endl;
                if (m_audioDevice)
                    SDL_AudioDevicePaused(m_audioDevice) ? SDL_ResumeAudioDevice(m_audioDevice) : SDL_PauseAudioDevice(m_audioDevice);
                break;
            case SDLK_M:
                std::cout << "Toggling audio mute" << std::endl;
                if (m_audioDevice) {
                    bool isMuted = SDL_GetAudioDeviceGain(m_audioDevice) == 0.0f;
                    SDL_SetAudioDeviceGain(m_audioDevice, isMuted ? 1.0f : 0.0f);
                    std::cout << "Audio " << (isMuted ? "unmuted" : "muted") << std::endl;
                }
                break;
        }
    }

    void handleMouseButton(const SDL_MouseButtonEvent& b) {
        std::cout << (b.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? "Pressed" : "Released") << " mouse button "
                  << (b.button == SDL_BUTTON_LEFT ? "Left" : b.button == SDL_BUTTON_RIGHT ? "Right" : "Middle")
                  << " at (" << b.x << ", " << b.y << ")" << std::endl;
        if (b.type == SDL_EVENT_MOUSE_BUTTON_DOWN && b.button == SDL_BUTTON_RIGHT) {
            std::cout << "Toggling relative mouse mode" << std::endl;
            SDL_SetWindowRelativeMouseMode(m_window.get(), !SDL_GetWindowRelativeMouseMode(m_window.get()));
        }
    }

    void handleTouch(const SDL_TouchFingerEvent& t) {
        std::cout << "Touch " << (t.type == SDL_EVENT_FINGER_DOWN ? "DOWN" : t.type == SDL_EVENT_FINGER_UP ? "UP" : "MOTION")
                  << " fingerId: " << t.fingerID << " at (" << t.x << ", " << t.y << ") pressure: " << t.pressure << std::endl;
    }

    void handleGamepadButton(const SDL_GamepadButtonEvent& g) {
        if (g.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
            std::cout << "Gamepad button down: ";
            switch (g.button) {
                case SDL_GAMEPAD_BUTTON_SOUTH:
                    std::cout << "South (A/Cross) pressed" << std::endl;
                    break;
                case SDL_GAMEPAD_BUTTON_EAST:
                    std::cout << "East (B/Circle) pressed" << std::endl;
                    {
                        std::cout << "Pushing quit event" << std::endl;
                        SDL_Event evt{.type = SDL_EVENT_QUIT};
                        SDL_PushEvent(&evt);
                    }
                    break;
                case SDL_GAMEPAD_BUTTON_WEST:
                    std::cout << "West (X/Square) pressed" << std::endl;
                    break;
                case SDL_GAMEPAD_BUTTON_NORTH:
                    std::cout << "North (Y/Triangle) pressed" << std::endl;
                    break;
                case SDL_GAMEPAD_BUTTON_START:
                    std::cout << "Start pressed" << std::endl;
                    if (m_audioDevice) {
                        std::cout << "Toggling audio pause/resume" << std::endl;
                        SDL_AudioDevicePaused(m_audioDevice) ? SDL_ResumeAudioDevice(m_audioDevice) : SDL_PauseAudioDevice(m_audioDevice);
                    }
                    break;
                default:
                    std::cout << "Button " << (int)g.button << " pressed" << std::endl;
                    break;
            }
        } else {
            std::cout << "Gamepad button " << (int)g.button << " released" << std::endl;
        }
    }
};

#endif