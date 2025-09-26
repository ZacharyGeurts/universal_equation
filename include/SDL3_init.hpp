#ifndef SDL3_INIT_HPP
#define SDL3_INIT_HPP

// AMOURANTH RTX Engine September 2025 - SDL3Initializer for window, input, audio, and Vulkan integration.
// Manages SDL3 and Vulkan initialization, including window creation, input handling, audio setup,
// and Vulkan instance/surface creation with Linux compatibility (X11, Wayland).
// Uses RAII for resource management, thread-safe event processing, and asynchronous font loading.
// Key features:
// - Vulkan window with SDL_WINDOW_VULKAN and Linux-specific surface extensions (VK_KHR_xlib_surface, VK_KHR_wayland_surface).
// - Thread-safe input handling for keyboard, mouse, touch, and gamepad events.
// - Parallel audio processing with customizable callbacks, supporting PulseAudio/ALSA.
// - Asynchronous TTF font loading to reduce startup latency.
// - Configurable Vulkan validation layers and explicit NVIDIA GPU selection for hybrid systems.
// - Fallback to AMD GPU if NVIDIA fails, then to non-Vulkan rendering if both fail.
// Dependencies: SDL3, SDL_ttf, Vulkan 1.3+, libX11-dev, libwayland-dev, mesa-vulkan-drivers, nvidia-driver.
// Best Practices:
// - Use SDL_Vulkan_GetInstanceExtensions and SDL_Vulkan_CreateSurface for Vulkan setup.
// - Handle window resize by recreating swapchain in callback.
// - Select NVIDIA GPU with SDL hints and environment variables for Linux hybrid graphics.
// - Enable validation layers in debug builds for error detection.
// Potential Issues:
// - Ensure NVIDIA GPU is used in hybrid systems (PRIME offload) to avoid Vulkan errors.
// - Verify VK_KHR_surface and platform-specific extensions via vulkaninfo.
// - Ensure libX11-dev and libwayland-dev are installed for vulkan_xlib.h and vulkan_wayland.h.
// Usage example:
//   SDL3Initializer sdl;
//   sdl.initialize("Dimensional Navigator", 1280, 720, true, "assets/fonts/custom.ttf", true, true);
//   sdl.eventLoop([] { render(); }, [](int w, int h) { resize(w, h); });
// Zachary Geurts 2025

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <vulkan/vulkan.h>
#include <X11/Xlib.h>  // Required for Display, Window, VisualID in vulkan_xlib.h
#include <vulkan/vulkan_xlib.h>
#include <vulkan/vulkan_wayland.h>
#include <stdexcept>
#include <vector>
#include <string>
#include <functional>
#include <memory>
#include <map>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <future>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <fstream>
#include <sstream>
#include <cstdlib>  // For setenv
#include <filesystem>  // For ICD path validation
#include "Vulkan_func.hpp"

// Configuration for audio initialization.
// Notes: Defaults to stereo (2 channels) for better hardware compatibility.
struct AudioConfig {
    int frequency = 44100;                        // Audio frequency in Hz (default: 44100).
    SDL_AudioFormat format = SDL_AUDIO_S16LE;    // Audio format (default: 16-bit little-endian).
    int channels = 2;                            // Number of audio channels (default: 2 for stereo).
    std::function<void(Uint8*, int)> callback;   // Callback for audio buffer processing.
};

// Custom deleter for SDL_Window to ensure proper cleanup with logging.
struct SDLWindowDeleter {
    void operator()(SDL_Window* w) const {
        std::cout << "Destroying SDL window\n";
        if (w) SDL_DestroyWindow(w);
    }
};

// Custom deleter for VkInstance to ensure proper cleanup with logging.
struct VulkanInstanceDeleter {
    void operator()(VkInstance i) const {
        std::cout << "Destroying Vulkan instance\n";
        if (i) vkDestroyInstance(i, nullptr);
    }
};

// Custom deleter for VkSurfaceKHR to ensure proper cleanup with logging.
struct VulkanSurfaceDeleter {
    void operator()(VkSurfaceKHR s) const {
        std::cout << "Destroying Vulkan surface\n";
        if (s && m_instance) {
            vkDestroySurfaceKHR(m_instance, s, nullptr);
        } else if (s) {
            std::cout << "Warning: Cannot destroy VkSurfaceKHR because VkInstance is invalid\n";
        }
    }
    VkInstance m_instance; // Vulkan instance for surface destruction.
};

// Manages SDL3 and Vulkan initialization with thread-safe event handling and RAII resource management.
// Non-copyable, movable, with support for custom input and render callbacks.
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

    SDL3Initializer() : logFile("sdl3_initializer.log"), m_useVulkan(true) {
        logMessage("Constructing SDL3Initializer");
        startWorkerThreads(std::min(4, static_cast<int>(std::thread::hardware_concurrency())));
    }

    ~SDL3Initializer() {
        logMessage("Destructing SDL3Initializer");
        {
            std::unique_lock<std::mutex> lock(taskMutex);
            stopWorkers = true;
            taskCond.notify_all();
        }
        for (auto& thread : workerThreads) {
            if (thread.joinable()) thread.join();
        }
        if (renderThread.joinable()) {
            {
                std::unique_lock<std::mutex> lock(renderMutex);
                renderReady = false;
                stopRender = true;
                renderCond.notify_one();
            }
            renderThread.join();
        }
        cleanup();
        logFile.close();
    }

    SDL3Initializer(const SDL3Initializer&) = delete;
    SDL3Initializer& operator=(const SDL3Initializer&) = delete;
    SDL3Initializer(SDL3Initializer&&) noexcept = default;
    SDL3Initializer& operator=(SDL3Initializer&&) noexcept = default;

    void initialize(const char* title, int w, int h, Uint32 flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE,
                    bool rt = true, const std::string& fontPath = "assets/fonts/sf-plasmatica-open.ttf",
                    bool enableValidation = true, bool preferNvidia = false) {
        logMessage("Initializing SDL3Initializer with title: " + std::string(title) + ", width: " + std::to_string(w) + ", height: " + std::to_string(h));
        initSDL(title, w, h, flags, rt, fontPath, enableValidation, preferNvidia);
        logMessage("Initializing audio");
        initAudio({});
        logMessage("Initializing input");
        initInput();
    }

    void eventLoop(std::function<void()> render, ResizeCallback onResize = nullptr, bool exitOnClose = true,
                   KeyboardCallback kb = nullptr, MouseButtonCallback mb = nullptr, MouseMotionCallback mm = nullptr,
                   MouseWheelCallback mw = nullptr, TextInputCallback ti = nullptr, TouchCallback tc = nullptr,
                   GamepadButtonCallback gb = nullptr, GamepadAxisCallback ga = nullptr, GamepadConnectCallback gc = nullptr) {
        logMessage("Starting event loop");
        if (ti) {
            logMessage("Enabling text input");
            SDL_StartTextInput(m_window.get());
        }

        if (render) {
            renderThread = std::thread([this, render]() {
                while (true) {
                    std::unique_lock<std::mutex> lock(renderMutex);
                    renderCond.wait(lock, [this] { return renderReady || stopRender; });
                    if (stopRender && !renderReady) break;
                    if (renderReady) {
                        logMessage("Render thread executing render callback");
                        render();
                        renderReady = false;
                        lock.unlock();
                        renderCond.notify_one();
                    }
                }
            });
        }

        for (bool running = true; running;) {
            SDL_Event e;
            std::vector<std::function<void()>> gamepadTasks;
            while (SDL_PollEvent(&e)) {
                logMessage("Processing SDL event type: " + std::to_string(e.type));
                switch (e.type) {
                    case SDL_EVENT_QUIT:
                        logMessage("Quit event received");
                        running = !exitOnClose;
                        break;
                    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                        logMessage("Window close requested");
                        running = !exitOnClose;
                        break;
                    case SDL_EVENT_WINDOW_RESIZED:
                        logMessage("Window resized to " + std::to_string(e.window.data1) + "x" + std::to_string(e.window.data2));
                        if (onResize) onResize(e.window.data1, e.window.data2);
                        break;
                    case SDL_EVENT_KEY_DOWN:
                        logMessage("Key down event: " + std::string(SDL_GetKeyName(e.key.key)));
                        handleKeyboard(e.key);
                        if (kb) kb(e.key);
                        break;
                    case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    case SDL_EVENT_MOUSE_BUTTON_UP:
                        logMessage("Mouse button event: " + std::string(e.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? "Down" : "Up"));
                        handleMouseButton(e.button);
                        if (mb) mb(e.button);
                        break;
                    case SDL_EVENT_MOUSE_MOTION:
                        logMessage("Mouse motion event at (" + std::to_string(e.motion.x) + ", " + std::to_string(e.motion.y) + ")");
                        if (mm) mm(e.motion);
                        break;
                    case SDL_EVENT_MOUSE_WHEEL:
                        logMessage("Mouse wheel event");
                        if (mw) mw(e.wheel);
                        break;
                    case SDL_EVENT_TEXT_INPUT:
                        logMessage("Text input event: " + std::string(e.text.text));
                        if (ti) ti(e.text);
                        break;
                    case SDL_EVENT_FINGER_DOWN:
                    case SDL_EVENT_FINGER_UP:
                    case SDL_EVENT_FINGER_MOTION:
                        logMessage("Touch event: " + std::string(e.type == SDL_EVENT_FINGER_DOWN ? "Down" : e.type == SDL_EVENT_FINGER_UP ? "Up" : "Motion"));
                        handleTouch(e.tfinger);
                        if (tc) tc(e.tfinger);
                        break;
                    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                    case SDL_EVENT_GAMEPAD_BUTTON_UP:
                        logMessage("Gamepad button event: " + std::string(e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ? "Down" : "Up"));
                        handleGamepadButton(e.gbutton);
                        if (gb) {
                            SDL_GamepadButtonEvent eventCopy = e.gbutton;
                            gamepadTasks.push_back([gb, eventCopy] { gb(eventCopy); });
                        }
                        break;
                    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                        logMessage("Gamepad axis motion event");
                        if (ga) {
                            SDL_GamepadAxisEvent eventCopy = e.gaxis;
                            gamepadTasks.push_back([ga, eventCopy] { ga(eventCopy); });
                        }
                        break;
                    case SDL_EVENT_GAMEPAD_ADDED:
                        logMessage("Gamepad added: ID " + std::to_string(e.gdevice.which));
                        {
                            std::unique_lock<std::mutex> lock(gamepadMutex);
                            if (SDL_Gamepad* gp = SDL_OpenGamepad(e.gdevice.which)) {
                                logMessage("Opened gamepad: ID " + std::to_string(e.gdevice.which));
                                m_gamepads[e.gdevice.which] = gp;
                                if (gc) gc(true, e.gdevice.which, gp);
                            }
                        }
                        break;
                    case SDL_EVENT_GAMEPAD_REMOVED:
                        logMessage("Gamepad removed: ID " + std::to_string(e.gdevice.which));
                        {
                            std::unique_lock<std::mutex> lock(gamepadMutex);
                            if (auto it = m_gamepads.find(e.gdevice.which); it != m_gamepads.end()) {
                                logMessage("Closing gamepad: ID " + std::to_string(e.gdevice.which));
                                SDL_CloseGamepad(it->second);
                                if (gc) gc(false, e.gdevice.which, nullptr);
                                m_gamepads.erase(it);
                            }
                        }
                        break;
                }
            }
            if (!gamepadTasks.empty()) {
                std::unique_lock<std::mutex> lock(taskMutex);
                for (auto& task : gamepadTasks) {
                    taskQueue.push(std::move(task));
                }
                taskCond.notify_all();
            }
            if (render && running) {
                std::unique_lock<std::mutex> lock(renderMutex);
                renderReady = true;
                renderCond.notify_one();
                renderCond.wait(lock, [this] { return !renderReady || stopRender; });
            }
        }

        {
            std::unique_lock<std::mutex> lock(renderMutex);
            stopRender = true;
            renderCond.notify_one();
        }

        if (ti) {
            logMessage("Disabling text input");
            SDL_StopTextInput(m_window.get());
        }
        logMessage("Exiting event loop");
    }

    void cleanup() {
        logMessage("Starting cleanup");
        {
            std::unique_lock<std::mutex> lock(gamepadMutex);
            for (auto& [id, gp] : m_gamepads) {
                logMessage("Closing gamepad: ID " + std::to_string(id));
                SDL_CloseGamepad(gp);
            }
            m_gamepads.clear();
        }
        logMessage("Cleared gamepads");
        if (m_audioStream) {
            logMessage("Pausing audio device before cleanup");
            if (m_audioDevice) {
                SDL_PauseAudioDevice(m_audioDevice);
            }
            logMessage("Destroying audio stream");
            SDL_DestroyAudioStream(m_audioStream);
            m_audioStream = nullptr;
        }
        if (m_audioDevice) {
            logMessage("Closing audio device: ID " + std::to_string(m_audioDevice));
            SDL_CloseAudioDevice(m_audioDevice);
            m_audioDevice = 0;
        }
        if (m_font) {
            logMessage("Closing TTF font");
            TTF_CloseFont(m_font);
            m_font = nullptr;
        }
        logMessage("Quitting TTF");
        TTF_Quit();
        if (m_surface) {
            logMessage("Destroying Vulkan surface");
            m_surface.reset();
        }
        if (m_instance) {
            logMessage("Destroying Vulkan instance");
            m_instance.reset();
        }
        if (m_window) {
            logMessage("Destroying SDL window");
            m_window.reset();
        }
        logMessage("Quitting SDL");
        SDL_Quit();
        logMessage("Cleanup completed");
    }

    SDL_Window* getWindow() const {
        logMessage("Getting SDL window");
        return m_window.get();
    }

    VkInstance getVkInstance() const {
        logMessage("Getting Vulkan instance");
        if (!m_useVulkan) {
            logMessage("Vulkan not available, returning VK_NULL_HANDLE");
            return VK_NULL_HANDLE;
        }
        return m_instance.get();
    }

    VkSurfaceKHR getVkSurface() const {
        logMessage("Getting Vulkan surface");
        if (!m_useVulkan) {
            logMessage("Vulkan not available, returning VK_NULL_HANDLE");
            return VK_NULL_HANDLE;
        }
        return m_surface.get();
    }

    SDL_AudioDeviceID getAudioDevice() const {
        logMessage("Getting audio device ID: " + std::to_string(m_audioDevice));
        return m_audioDevice;
    }

    const auto& getGamepads() const {
        logMessage("Getting gamepads");
        return m_gamepads;
    }

    TTF_Font* getFont() const {
        logMessage("Getting TTF font");
        return m_font;
    }

    std::vector<std::string> getVulkanExtensions() const {
        logMessage("Querying Vulkan instance extensions");
        uint32_t count;
        vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
        std::vector<VkExtensionProperties> props(count);
        vkEnumerateInstanceExtensionProperties(nullptr, &count, props.data());
        std::vector<std::string> extensions;
        for (const auto& prop : props) {
            extensions.push_back(prop.extensionName);
        }
        return extensions;
    }

    void exportLog(const std::string& filename) const {
        logMessage("Exporting log to " + filename);
        std::ofstream out(filename, std::ios::app);
        if (out.is_open()) {
            out << logStream.str();
            out.close();
            std::cout << "Exported log to " << filename << "\n";
        } else {
            std::cerr << "Failed to export log to " << filename << "\n";
        }
    }

private:
    std::unique_ptr<SDL_Window, SDLWindowDeleter> m_window;
    std::unique_ptr<std::remove_pointer_t<VkInstance>, VulkanInstanceDeleter> m_instance;
    std::unique_ptr<std::remove_pointer_t<VkSurfaceKHR>, VulkanSurfaceDeleter> m_surface;
    SDL_AudioDeviceID m_audioDevice = 0;
    SDL_AudioStream* m_audioStream = nullptr;
    std::map<SDL_JoystickID, SDL_Gamepad*> m_gamepads;
    std::mutex gamepadMutex;
    TTF_Font* m_font = nullptr;
    std::mutex renderMutex;
    std::condition_variable renderCond;
    std::atomic<bool> renderReady{false};
    std::atomic<bool> stopRender{false};
    std::thread renderThread;
    std::queue<std::function<void()>> taskQueue;
    std::mutex taskMutex;
    std::condition_variable taskCond;
    std::vector<std::thread> workerThreads;
    std::atomic<bool> stopWorkers{false};
    mutable std::ofstream logFile;
    mutable std::stringstream logStream;
    bool m_useVulkan;

    void logMessage(const std::string& message) const {
        std::string timestamp = "[" + std::to_string(SDL_GetTicks()) + "ms] " + message;
        std::cout << timestamp << "\n";
        logStream << timestamp << "\n";
        if (logFile.is_open()) {
            logFile << timestamp << "\n";
            logFile.flush();
        }
    }

    void startWorkerThreads(int numThreads) {
        logMessage("Starting " + std::to_string(numThreads) + " worker threads for gamepad events");
        for (int i = 0; i < numThreads; ++i) {
            workerThreads.emplace_back([this] {
                while (true) {
                    std::vector<std::function<void()>> tasks;
                    {
                        std::unique_lock<std::mutex> lock(taskMutex);
                        taskCond.wait(lock, [this] { return !taskQueue.empty() || stopWorkers; });
                        if (stopWorkers && taskQueue.empty()) break;
                        while (!taskQueue.empty() && tasks.size() < 10) {
                            tasks.push_back(std::move(taskQueue.front()));
                            taskQueue.pop();
                        }
                    }
                    for (auto& task : tasks) {
                        logMessage("Worker thread processing task");
                        task();
                    }
                }
            });
        }
    }

    void initSDL(const char* title, int w, int h, Uint32 flags, bool rt, const std::string& fontPath, bool enableValidation, bool preferNvidia) {
        logMessage("Initializing SDL subsystems");
        std::string platform = SDL_GetPlatform();
        logMessage("Detected platform: " + platform);

        bool pulseAvailable = false;
        int driverCount = SDL_GetNumAudioDrivers();
        for (int i = 0; i < driverCount; ++i) {
            const char* driver = SDL_GetAudioDriver(i);
            if (driver && std::string(driver) == "pulse") {
                pulseAvailable = true;
                break;
            }
        }
        const char* audioDriver = pulseAvailable ? "pulse" : "alsa";
        logMessage("Selecting audio driver: " + std::string(audioDriver));
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, audioDriver);

        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS) == 0) {
            std::string error = "SDL_Init failed: " + std::string(SDL_GetError());
            logMessage(error);
            throw std::runtime_error(error);
        }

        logMessage("Initializing TTF");
        if (TTF_Init() == 0) {
            std::string error = "TTF_Init failed: " + std::string(SDL_GetError());
            logMessage(error);
            SDL_Quit();
            throw std::runtime_error(error);
        }

        m_useVulkan = (flags & SDL_WINDOW_VULKAN) != 0;

        logMessage("Creating SDL window with flags: " + std::to_string(flags));
        m_window = std::unique_ptr<SDL_Window, SDLWindowDeleter>(SDL_CreateWindow(title, w, h, flags));
        if (!m_window) {
            std::string error = "SDL_CreateWindow failed: " + std::string(SDL_GetError());
            logMessage(error);
            throw std::runtime_error(error);
        }

        if (!m_useVulkan) {
            logMessage("Vulkan not requested; proceeding with non-Vulkan window");
            return;
        }

        logMessage("Getting Vulkan instance extensions from SDL");
        Uint32 extCount;
        auto exts = SDL_Vulkan_GetInstanceExtensions(&extCount);
        if (!exts || !extCount) {
            std::string error = "SDL_Vulkan_GetInstanceExtensions failed: " + std::string(SDL_GetError());
            logMessage(error);
            m_useVulkan = false;
            return;
        }
        std::vector<const char*> extensions(exts, exts + extCount);
        logMessage("Required SDL Vulkan extensions: " + std::to_string(extCount));
        for (uint32_t i = 0; i < extCount; ++i) {
            logMessage("Extension " + std::to_string(i) + ": " + extensions[i]);
        }

        // Check for Vulkan instance extensions
        logMessage("Checking Vulkan instance extensions");
        uint32_t instanceExtCount;
        vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtCount, nullptr);
        std::vector<VkExtensionProperties> instanceExtensions(instanceExtCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtCount, instanceExtensions.data());
        bool hasSurfaceExtension = false;
        std::vector<std::string> platformSurfaceExtensions;
        for (const auto& ext : instanceExtensions) {
            logMessage("Available instance extension: " + std::string(ext.extensionName));
            if (strcmp(ext.extensionName, VK_KHR_SURFACE_EXTENSION_NAME) == 0) {
                hasSurfaceExtension = true;
                logMessage("VK_KHR_surface extension found");
            }
            if (strcmp(ext.extensionName, VK_KHR_XLIB_SURFACE_EXTENSION_NAME) == 0 ||
                strcmp(ext.extensionName, VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME) == 0) {
                platformSurfaceExtensions.push_back(ext.extensionName);
                logMessage("Platform surface extension found: " + std::string(ext.extensionName));
            }
        }

        // Add ray tracing extensions if enabled
        if (rt) {
            logMessage("Enumerating Vulkan instance extension properties for ray tracing");
            const char* rtExts[] = {
                VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
                VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
                VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
                VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
            };
            for (auto ext : rtExts) {
                if (std::any_of(instanceExtensions.begin(), instanceExtensions.end(), [ext](auto& p) { return strcmp(p.extensionName, ext) == 0; })) {
                    logMessage("Adding Vulkan extension: " + std::string(ext));
                    extensions.push_back(ext);
                }
            }
        }

        std::vector<const char*> layers;
#ifndef NDEBUG
        if (enableValidation) {
            logMessage("Adding Vulkan validation layer");
            layers.push_back("VK_LAYER_KHRONOS_validation");
        }
#endif

        // Ensure VK_KHR_surface is included if available
        if (hasSurfaceExtension && std::find(extensions.begin(), extensions.end(), VK_KHR_SURFACE_EXTENSION_NAME) == extensions.end()) {
            logMessage("Adding VK_KHR_SURFACE_EXTENSION_NAME to extensions");
            extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
        }

        logMessage("Creating Vulkan instance with " + std::to_string(extensions.size()) + " extensions and " + std::to_string(layers.size()) + " layers");
        VkApplicationInfo appInfo = {
            VK_STRUCTURE_TYPE_APPLICATION_INFO,
            nullptr,
            title,
            VK_MAKE_API_VERSION(0, 1, 0, 0),
            "AMOURANTH RTX",
            VK_MAKE_API_VERSION(0, 1, 0, 0),
            VK_API_VERSION_1_3
        };
        VkInstanceCreateInfo ci = {
            VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            nullptr,
            0,
            &appInfo,
            static_cast<uint32_t>(layers.size()),
            layers.data(),
            static_cast<uint32_t>(extensions.size()),
            extensions.data()
        };
        VkInstance instance;
        VkResult result = vkCreateInstance(&ci, nullptr, &instance);
        if (result != VK_SUCCESS) {
            std::string error = "vkCreateInstance failed: " + std::to_string(result);
            logMessage(error);
            m_useVulkan = false;
            return;
        }
        m_instance = std::unique_ptr<std::remove_pointer_t<VkInstance>, VulkanInstanceDeleter>(instance);
        logMessage("Vulkan instance created");

        VkSurfaceKHR surface = VK_NULL_HANDLE;
        logMessage("Creating Vulkan surface");
        if (!SDL_Vulkan_CreateSurface(m_window.get(), m_instance.get(), nullptr, &surface)) {
            std::string error = "SDL_Vulkan_CreateSurface failed: " + std::string(SDL_GetError());
            logMessage(error);
            m_instance.reset();
            m_useVulkan = false;
            return;
        }
        m_surface = std::unique_ptr<std::remove_pointer_t<VkSurfaceKHR>, VulkanSurfaceDeleter>(surface, VulkanSurfaceDeleter{m_instance.get()});
        logMessage("Vulkan surface created successfully");

        logMessage("Selecting physical device");
        VkPhysicalDevice physicalDevice;
        uint32_t graphicsFamily, presentFamily;
        try {
            VulkanInitializer::createPhysicalDevice(
                m_instance.get(), physicalDevice, graphicsFamily, presentFamily, m_surface.get(), preferNvidia,
                [this](const std::string& msg) { logMessage(msg); }
            );
        } catch (const std::runtime_error& e) {
            logMessage("Physical device selection failed: " + std::string(e.what()));
            logMessage("Falling back to non-Vulkan rendering");
            m_surface.reset();
            m_instance.reset();
            m_useVulkan = false;
            flags &= ~SDL_WINDOW_VULKAN;
            m_window.reset();
            logMessage("Creating SDL window with flags (non-Vulkan): " + std::to_string(flags));
            m_window = std::unique_ptr<SDL_Window, SDLWindowDeleter>(SDL_CreateWindow(title, w, h, flags));
            if (!m_window) {
                std::string error = "SDL_CreateWindow failed for fallback: " + std::string(SDL_GetError());
                logMessage(error);
                throw std::runtime_error(error);
            }
            logMessage("Non-Vulkan window created successfully");
        }

        std::string resolvedFontPath = fontPath;
        logMessage("Loading TTF font in background thread: " + resolvedFontPath);
        std::future<TTF_Font*> fontFuture = std::async(std::launch::async, [resolvedFontPath] {
            TTF_Font* font = TTF_OpenFont(resolvedFontPath.c_str(), 24);
            if (!font) {
                std::string error = "TTF_OpenFont failed for " + resolvedFontPath + ": " + std::string(SDL_GetError());
                std::cerr << error << "\n";
                throw std::runtime_error(error);
            }
            return font;
        });

        m_font = fontFuture.get();
        logMessage("Font loaded successfully");
    }

    void initAudio(const AudioConfig& c) {
        logMessage("Initializing audio with frequency: " + std::to_string(c.frequency) + ", channels: " + std::to_string(c.channels));
        SDL_AudioSpec desired{};
        desired.freq = c.frequency;
        desired.format = c.format;
        desired.channels = c.channels;

        logMessage("Opening audio device");
        m_audioDevice = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired);
        if (m_audioDevice == 0) {
            std::string error = "SDL_OpenAudioDevice failed: " + std::string(SDL_GetError());
            logMessage(error);
            throw std::runtime_error(error);
        }

        SDL_AudioSpec obtained{};
        SDL_GetAudioDeviceFormat(m_audioDevice, &obtained, nullptr);
        const char* deviceName = SDL_GetAudioDeviceName(m_audioDevice);
        logMessage("Audio device opened: " + std::string(deviceName ? deviceName : "Unknown device"));
        logMessage("Audio device status: " + std::string(SDL_AudioDevicePaused(m_audioDevice) ? "Paused" : "Active"));
        logMessage("Audio device opened with obtained format: freq=" + std::to_string(obtained.freq) + 
                   ", channels=" + std::to_string(obtained.channels) + 
                   ", format=" + std::to_string(obtained.format));

        if (c.callback) {
            logMessage("Creating audio stream");
            m_audioStream = SDL_CreateAudioStream(&desired, &obtained);
            if (!m_audioStream) {
                std::string error = "SDL_CreateAudioStream failed: " + std::string(SDL_GetError());
                logMessage(error);
                SDL_CloseAudioDevice(m_audioDevice);
                m_audioDevice = 0;
                throw std::runtime_error(error);
            }

            logMessage("Pausing audio device before binding");
            SDL_PauseAudioDevice(m_audioDevice);

            logMessage("Binding audio stream to device");
            if (SDL_BindAudioStream(m_audioDevice, m_audioStream) != 0) {
                std::string error = "SDL_BindAudioStream failed: " + std::string(SDL_GetError());
                logMessage(error);
                if (m_audioStream) {
                    SDL_DestroyAudioStream(m_audioStream);
                    m_audioStream = nullptr;
                }
                SDL_CloseAudioDevice(m_audioDevice);
                m_audioDevice = 0;
                throw std::runtime_error(error);
            }

            logMessage("Setting audio stream callback with parallel processing");
            SDL_SetAudioStreamPutCallback(m_audioStream, [](void* u, SDL_AudioStream* s, int n, int) {
                auto* callback = static_cast<AudioCallback*>(u);
                std::vector<Uint8> buf(n);
                const int numThreads = std::min(4, static_cast<int>(std::thread::hardware_concurrency()));
                const int chunkSize = n / numThreads;
                std::vector<std::future<void>> futures;

                for (int i = 0; i < numThreads; ++i) {
                    int start = i * chunkSize;
                    int size = (i == numThreads - 1) ? (n - start) : chunkSize;
                    futures.push_back(std::async(std::launch::async, [callback, &buf, start, size]() {
                        (*callback)(buf.data() + start, size);
                    }));
                }

                for (auto& f : futures) {
                    f.wait();
                }

                SDL_PutAudioStreamData(s, buf.data(), n);
            }, &const_cast<AudioCallback&>(c.callback));
        }

        logMessage("Resuming audio device");
        SDL_ResumeAudioDevice(m_audioDevice);
    }

    void initInput() {
        logMessage("Initializing input system");
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");
        logMessage("Getting connected joysticks");
        int numJoysticks;
        SDL_JoystickID* joysticks = SDL_GetJoysticks(&numJoysticks);
        if (joysticks) {
            std::unique_lock<std::mutex> lock(gamepadMutex);
            for (int i = 0; i < numJoysticks; ++i) {
                if (SDL_IsGamepad(joysticks[i])) {
                    logMessage("Found gamepad: ID " + std::to_string(joysticks[i]));
                    if (auto gp = SDL_OpenGamepad(joysticks[i])) {
                        logMessage("Opened gamepad: ID " + std::to_string(joysticks[i]));
                        m_gamepads[joysticks[i]] = gp;
                    }
                }
            }
            SDL_free(joysticks);
        }
    }

    void handleKeyboard(const SDL_KeyboardEvent& k) {
        if (k.type != SDL_EVENT_KEY_DOWN) {
            logMessage("Ignoring non-key-down event");
            return;
        }
        logMessage("Handling key: " + std::string(SDL_GetKeyName(k.key)));
        switch (k.key) {
            case SDLK_F:
                logMessage("Toggling fullscreen mode");
                {
                    Uint32 flags = SDL_GetWindowFlags(m_window.get());
                    bool isFullscreen = flags & SDL_WINDOW_FULLSCREEN;
                    SDL_SetWindowFullscreen(m_window.get(), isFullscreen ? 0 : SDL_WINDOW_FULLSCREEN);
                }
                break;
            case SDLK_ESCAPE:
                logMessage("Pushing quit event");
                {
                    SDL_Event evt{.type = SDL_EVENT_QUIT};
                    SDL_PushEvent(&evt);
                }
                break;
            case SDLK_SPACE:
                logMessage("Toggling audio pause/resume");
                if (m_audioDevice) {
                    if (SDL_AudioDevicePaused(m_audioDevice)) {
                        SDL_ResumeAudioDevice(m_audioDevice);
                    } else {
                        SDL_PauseAudioDevice(m_audioDevice);
                    }
                }
                break;
            case SDLK_M:
                logMessage("Toggling audio mute");
                if (m_audioDevice) {
                    float gain = SDL_GetAudioDeviceGain(m_audioDevice);
                    SDL_SetAudioDeviceGain(m_audioDevice, gain == 0.0f ? 1.0f : 0.0f);
                    logMessage("Audio " + std::string(gain == 0.0f ? "unmuted" : "muted"));
                }
                break;
        }
    }

    void handleMouseButton(const SDL_MouseButtonEvent& b) {
        logMessage((b.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? "Pressed" : "Released") + std::string(" mouse button ") +
            (b.button == SDL_BUTTON_LEFT ? "Left" : b.button == SDL_BUTTON_RIGHT ? "Right" : "Middle") +
            " at (" + std::to_string(b.x) + ", " + std::to_string(b.y) + ")");
        if (b.type == SDL_EVENT_MOUSE_BUTTON_DOWN && b.button == SDL_BUTTON_RIGHT) {
            logMessage("Toggling relative mouse mode");
            bool relative = SDL_GetWindowRelativeMouseMode(m_window.get());
            SDL_SetWindowRelativeMouseMode(m_window.get(), !relative);
        }
    }

    void handleTouch(const SDL_TouchFingerEvent& t) {
        logMessage("Touch " + std::string(t.type == SDL_EVENT_FINGER_DOWN ? "DOWN" : t.type == SDL_EVENT_FINGER_UP ? "UP" : "MOTION") +
            " fingerID: " + std::to_string(t.fingerID) + " at (" + std::to_string(t.x) + ", " + std::to_string(t.y) +
            ") pressure: " + std::to_string(t.pressure));
    }

    void handleGamepadButton(const SDL_GamepadButtonEvent& g) {
        if (g.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
            logMessage("Gamepad button down: ");
            switch (g.button) {
                case SDL_GAMEPAD_BUTTON_SOUTH:
                    logMessage("South (A/Cross) pressed");
                    break;
                case SDL_GAMEPAD_BUTTON_EAST:
                    logMessage("East (B/Circle) pressed");
                    {
                        logMessage("Pushing quit event");
                        SDL_Event evt{.type = SDL_EVENT_QUIT};
                        SDL_PushEvent(&evt);
                    }
                    break;
                case SDL_GAMEPAD_BUTTON_WEST:
                    logMessage("West (X/Square) pressed");
                    break;
                case SDL_GAMEPAD_BUTTON_NORTH:
                    logMessage("North (Y/Triangle) pressed");
                    break;
                case SDL_GAMEPAD_BUTTON_START:
                    logMessage("Start pressed");
                    if (m_audioDevice) {
                        logMessage("Toggling audio pause/resume");
                        if (SDL_AudioDevicePaused(m_audioDevice)) {
                            SDL_ResumeAudioDevice(m_audioDevice);
                        } else {
                            SDL_PauseAudioDevice(m_audioDevice);
                        }
                    }
                    break;
                default:
                    logMessage("Button " + std::to_string(static_cast<int>(g.button)) + " pressed");
                    break;
            }
        } else {
            logMessage("Gamepad button " + std::to_string(static_cast<int>(g.button)) + " released");
        }
    }
};

#endif // SDL3_INIT_HPP