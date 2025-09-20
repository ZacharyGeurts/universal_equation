#ifndef SDL3_INIT_HPP
#define SDL3_INIT_HPP

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

struct AudioConfig {
    int frequency = 44100;
    SDL_AudioFormat format = SDL_AUDIO_S16LE;
    int channels = 8;
    std::function<void(Uint8*, int)> callback = nullptr;
};

struct SDLWindowDeleter { void operator()(SDL_Window* w) const { if (w) SDL_DestroyWindow(w); } };
struct VulkanInstanceDeleter { void operator()(VkInstance i) const { if (i) vkDestroyInstance(i, nullptr); } };
struct VulkanSurfaceDeleter { void operator()(VkSurfaceKHR s) const { if (s) vkDestroySurfaceKHR(m_instance, s, nullptr); } VkInstance m_instance; };

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

    SDL3Initializer() = default;
    ~SDL3Initializer() { cleanup(); }
    SDL3Initializer(const SDL3Initializer&) = delete;
    SDL3Initializer& operator=(const SDL3Initializer&) = delete;
    SDL3Initializer(SDL3Initializer&&) noexcept = default;
    SDL3Initializer& operator=(SDL3Initializer&&) noexcept = default;

    void initialize(const char* title, int w, int h, Uint32 flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE, bool rt = true, const AudioConfig& ac = AudioConfig()) {
        initSDL(title, w, h, flags, rt);
        if (ac.callback) initAudio(ac);
        initInput();
    }

    void eventLoop(std::function<void()> render, ResizeCallback onResize = nullptr, bool exitOnClose = true,
                   KeyboardCallback kb = nullptr, MouseButtonCallback mb = nullptr, MouseMotionCallback mm = nullptr,
                   MouseWheelCallback mw = nullptr, TextInputCallback ti = nullptr, TouchCallback tc = nullptr,
                   GamepadButtonCallback gb = nullptr, GamepadAxisCallback ga = nullptr, GamepadConnectCallback gc = nullptr) {
        if (ti) SDL_StartTextInput(m_window.get());
        for (bool running = true; running;) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                switch (e.type) {
                    case SDL_EVENT_QUIT: case SDL_EVENT_WINDOW_CLOSE_REQUESTED: running = !exitOnClose; break;
                    case SDL_EVENT_WINDOW_RESIZED:
                        if (onResize) onResize(e.window.data1, e.window.data2);
                        else std::cout << "Resized: " << e.window.data1 << "x" << e.window.data2 << '\n';
                        break;
                    case SDL_EVENT_KEY_DOWN: handleKeyboard(e.key); if (kb) kb(e.key); break;
                    case SDL_EVENT_MOUSE_BUTTON_DOWN: case SDL_EVENT_MOUSE_BUTTON_UP: handleMouseButton(e.button); if (mb) mb(e.button); break;
                    case SDL_EVENT_MOUSE_MOTION: if (mm) mm(e.motion); break;
                    case SDL_EVENT_MOUSE_WHEEL: if (mw) mw(e.wheel); break;
                    case SDL_EVENT_TEXT_INPUT: if (ti) ti(e.text); break;
                    case SDL_EVENT_FINGER_DOWN: case SDL_EVENT_FINGER_UP: case SDL_EVENT_FINGER_MOTION: handleTouch(e.tfinger); if (tc) tc(e.tfinger); break;
                    case SDL_EVENT_GAMEPAD_BUTTON_DOWN: case SDL_EVENT_GAMEPAD_BUTTON_UP: handleGamepadButton(e.gbutton); if (gb) gb(e.gbutton); break;
                    case SDL_EVENT_GAMEPAD_AXIS_MOTION: if (ga) ga(e.gaxis); break;
                    case SDL_EVENT_GAMEPAD_ADDED:
                        if (SDL_Gamepad* gp = SDL_OpenGamepad(e.gdevice.which)) {
                            m_gamepads[e.gdevice.which] = gp;
                            if (gc) gc(true, e.gdevice.which, gp);
                        }
                        break;
                    case SDL_EVENT_GAMEPAD_REMOVED:
                        if (auto it = m_gamepads.find(e.gdevice.which); it != m_gamepads.end()) {
                            SDL_CloseGamepad(it->second);
                            if (gc) gc(false, e.gdevice.which, nullptr);
                            m_gamepads.erase(it);
                        }
                        break;
                }
            }
            if (render && running) render();
        }
        if (ti) SDL_StopTextInput(m_window.get());
    }

    void cleanup() {
        for (auto& [id, gp] : m_gamepads) SDL_CloseGamepad(gp);
        m_gamepads.clear();
        if (m_audioStream) SDL_DestroyAudioStream(m_audioStream);
        if (m_audioDevice) SDL_CloseAudioDevice(m_audioDevice);
        if (m_font) TTF_CloseFont(m_font);
        TTF_Quit();
        m_surface.reset();
        m_instance.reset();
        m_window.reset();
        SDL_Quit();
    }

    SDL_Window* getWindow() const { return m_window.get(); }
    VkInstance getVkInstance() const { return m_instance.get(); }
    VkSurfaceKHR getVkSurface() const { return m_surface.get(); }
    SDL_AudioDeviceID getAudioDevice() const { return m_audioDevice; }
    const auto& getGamepads() const { return m_gamepads; }
    TTF_Font* getFont() const { return m_font; }

private:
    std::unique_ptr<SDL_Window, SDLWindowDeleter> m_window;
    std::unique_ptr<std::remove_pointer_t<VkInstance>, VulkanInstanceDeleter> m_instance;
    std::unique_ptr<std::remove_pointer_t<VkSurfaceKHR>, VulkanSurfaceDeleter> m_surface;
    SDL_AudioDeviceID m_audioDevice = 0;
    SDL_AudioStream* m_audioStream = nullptr;
    std::map<SDL_JoystickID, SDL_Gamepad*> m_gamepads;
    TTF_Font* m_font = nullptr;

    void initSDL(const char* title, int w, int h, Uint32 flags, bool rt) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS) == 0)
            throw std::runtime_error("SDL_Init failed: " + std::string(SDL_GetError()));
        if (TTF_Init() == 0)
            throw std::runtime_error("TTF_Init failed: " + std::string(SDL_GetError()));
        m_window = std::unique_ptr<SDL_Window, SDLWindowDeleter>(SDL_CreateWindow(title, w, h, flags));
        if (!m_window) throw std::runtime_error("SDL_CreateWindow failed: " + std::string(SDL_GetError()));
        m_font = TTF_OpenFont("sf-plasmatica-open.ttf", 24);
        if (!m_font) throw std::runtime_error("TTF_OpenFont failed: " + std::string(SDL_GetError()));
        Uint32 extCount;
        auto exts = SDL_Vulkan_GetInstanceExtensions(&extCount);
        if (!exts || !extCount) throw std::runtime_error("SDL_Vulkan_GetInstanceExtensions failed: " + std::string(SDL_GetError()));
        std::vector<const char*> extensions(exts, exts + extCount);
        if (std::find(extensions.begin(), extensions.end(), VK_KHR_SURFACE_EXTENSION_NAME) == extensions.end())
            extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
        if (rt) {
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
            for (auto ext : rtExts)
                if (std::any_of(props.begin(), props.end(), [ext](auto& p) { return !strcmp(p.extensionName, ext); }))
                    extensions.push_back(ext);
        }
        std::vector<const char*> layers;
#ifndef NDEBUG
        layers.push_back("VK_LAYER_KHRONOS_validation");
#endif
        VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO, nullptr, title, VK_MAKE_API_VERSION(0, 1, 0, 0), "AMOURANTH", VK_MAKE_API_VERSION(0, 1, 0, 0), VK_API_VERSION_1_3};
        VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, nullptr, 0, &appInfo, (uint32_t)layers.size(), layers.data(), (uint32_t)extensions.size(), extensions.data()};
        VkInstance instance;
        if (vkCreateInstance(&ci, nullptr, &instance) != VK_SUCCESS)
            throw std::runtime_error("vkCreateInstance failed");
        m_instance = std::unique_ptr<std::remove_pointer_t<VkInstance>, VulkanInstanceDeleter>(instance);
        VkSurfaceKHR surface;
        if (!SDL_Vulkan_CreateSurface(m_window.get(), m_instance.get(), nullptr, &surface))
            throw std::runtime_error("SDL_Vulkan_CreateSurface failed: " + std::string(SDL_GetError()));
        m_surface = std::unique_ptr<std::remove_pointer_t<VkSurfaceKHR>, VulkanSurfaceDeleter>(surface, VulkanSurfaceDeleter{m_instance.get()});
    }

    void initAudio(const AudioConfig& c) {
        SDL_AudioSpec spec{};
        spec.freq = c.frequency;
        spec.format = c.format;
        spec.channels = c.channels;
        m_audioDevice = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
        if (!m_audioDevice) throw std::runtime_error("SDL_OpenAudioDevice failed: " + std::string(SDL_GetError()));
        SDL_AudioSpec obtained;
        if (SDL_GetAudioDeviceFormat(m_audioDevice, &obtained, nullptr) != 0) {
            SDL_CloseAudioDevice(m_audioDevice);
            throw std::runtime_error("SDL_GetAudioDeviceFormat failed: " + std::string(SDL_GetError()));
        }
        m_audioStream = SDL_CreateAudioStream(&spec, &obtained);
        if (!m_audioStream) throw std::runtime_error("SDL_CreateAudioStream failed: " + std::string(SDL_GetError()));
        if (SDL_BindAudioStream(m_audioDevice, m_audioStream) != 0) {
            SDL_DestroyAudioStream(m_audioStream);
            SDL_CloseAudioDevice(m_audioDevice);
            throw std::runtime_error("SDL_BindAudioStream failed: " + std::string(SDL_GetError()));
        }
        if (c.callback)
            SDL_SetAudioStreamPutCallback(m_audioStream, [](void* u, SDL_AudioStream* s, int n, int) {
                std::vector<Uint8> buf(n);
                (*static_cast<AudioCallback*>(u))(buf.data(), n);
                SDL_PutAudioStreamData(s, buf.data(), n);
            }, &const_cast<AudioCallback&>(c.callback));
        SDL_ResumeAudioDevice(m_audioDevice);
    }

    void initInput() {
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");
        auto* joysticks = SDL_GetJoysticks(nullptr);
        if (joysticks) {
            for (int i = 0; joysticks[i]; ++i)
                if (SDL_IsGamepad(joysticks[i]))
                    if (auto gp = SDL_OpenGamepad(joysticks[i]))
                        m_gamepads[joysticks[i]] = gp;
            SDL_free(joysticks);
        }
    }

    void handleKeyboard(const SDL_KeyboardEvent& k) {
        if (k.type != SDL_EVENT_KEY_DOWN) return;
        switch (k.key) {
            case SDLK_F: SDL_SetWindowFullscreen(m_window.get(), SDL_GetWindowFlags(m_window.get()) & SDL_WINDOW_FULLSCREEN ? 0 : SDL_WINDOW_FULLSCREEN); break;
            case SDLK_ESCAPE: {
                SDL_Event evt{.type = SDL_EVENT_QUIT};
                SDL_PushEvent(&evt);
                break;
            }
            case SDLK_SPACE: if (m_audioDevice) SDL_AudioDevicePaused(m_audioDevice) ? SDL_ResumeAudioDevice(m_audioDevice) : SDL_PauseAudioDevice(m_audioDevice); break;
        }
    }

    void handleMouseButton(const SDL_MouseButtonEvent& b) {
        std::cout << (b.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? "Pressed" : "Released") << " mouse button "
                  << (b.button == SDL_BUTTON_LEFT ? "Left" : b.button == SDL_BUTTON_RIGHT ? "Right" : "Middle")
                  << " at (" << b.x << ", " << b.y << ")\n";
        if (b.type == SDL_EVENT_MOUSE_BUTTON_DOWN && b.button == SDL_BUTTON_RIGHT)
            SDL_SetWindowRelativeMouseMode(m_window.get(), !SDL_GetWindowRelativeMouseMode(m_window.get()));
    }

    void handleTouch(const SDL_TouchFingerEvent& t) {
        std::cout << "Touch " << (t.type == SDL_EVENT_FINGER_DOWN ? "DOWN" : t.type == SDL_EVENT_FINGER_UP ? "UP" : "MOTION")
                  << " fingerId: " << t.fingerID << " at (" << t.x << ", " << t.y << ") pressure: " << t.pressure << '\n';
    }

    void handleGamepadButton(const SDL_GamepadButtonEvent& g) {
        if (g.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
            switch (g.button) {
                case SDL_GAMEPAD_BUTTON_SOUTH: std::cout << "Gamepad South (A/Cross) pressed\n"; break;
                case SDL_GAMEPAD_BUTTON_EAST: {
                    std::cout << "Gamepad East (B/Circle) pressed\n";
                    SDL_Event evt{.type = SDL_EVENT_QUIT};
                    SDL_PushEvent(&evt);
                    break;
                }
                case SDL_GAMEPAD_BUTTON_WEST: std::cout << "Gamepad West (X/Square) pressed\n"; break;
                case SDL_GAMEPAD_BUTTON_NORTH: std::cout << "Gamepad North (Y/Triangle) pressed\n"; break;
                case SDL_GAMEPAD_BUTTON_START:
                    std::cout << "Gamepad Start pressed\n";
                    if (m_audioDevice)
                        SDL_AudioDevicePaused(m_audioDevice) ? SDL_ResumeAudioDevice(m_audioDevice) : SDL_PauseAudioDevice(m_audioDevice);
                    break;
                default: std::cout << "Gamepad button " << (int)g.button << " pressed\n"; break;
            }
        } else
            std::cout << "Gamepad button " << (int)g.button << " released\n";
    }
};

#endif