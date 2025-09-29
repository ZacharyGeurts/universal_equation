#ifndef SDL3_INIT_HPP
#define SDL3_INIT_HPP

// AMOURANTH RTX Engine September 2025 - SDL3Initializer for window, input, audio, and Vulkan integration.
// Manages SDL3 and Vulkan initialization, including window creation, input handling, audio setup,
// and Vulkan instance/surface creation with multi-platform compatibility (Linux/X11/Wayland, Windows, macOS/MoltenVK, Android).
// Uses RAII for resource management, thread-safe event processing, and asynchronous font loading.
// Key features:
// - Vulkan window with SDL_WINDOW_VULKAN and platform-specific surface extensions (e.g., VK_KHR_win32_surface, VK_EXT_metal_surface, VK_KHR_android_surface).
// - Thread-safe input handling for keyboard, mouse, touch, and gamepad events.
// - Parallel audio processing with customizable callbacks, supporting platform-specific backends (WASAPI, CoreAudio, AAudio/OpenSL ES, PulseAudio/ALSA).
// - Asynchronous TTF font loading to reduce startup latency.
// - Configurable Vulkan validation layers and explicit NVIDIA GPU selection for hybrid systems.
// - Fallback to AMD GPU if NVIDIA fails, then to non-Vulkan rendering if both fail.
// Dependencies: SDL3, SDL_ttf, Vulkan 1.3+, platform-specific libs (libX11-dev/libwayland-dev for Linux, MoltenVK for macOS).
// Best Practices:
// - Use SDL_Vulkan_GetInstanceExtensions and SDL_Vulkan_CreateSurface for Vulkan setup.
// - Handle window resize by recreating swapchain in callback.
// - Select NVIDIA GPU with SDL hints and environment variables for hybrid graphics.
// - Enable validation layers in debug builds for error detection.
// Potential Issues:
// - Ensure NVIDIA GPU is used in hybrid systems (e.g., PRIME offload on Linux, D3D on Windows) to avoid Vulkan errors.
// - Verify VK_KHR_surface and platform-specific extensions via vulkaninfo.
// - On Android: Use 0x0 for fullscreen window; font paths relative to assets.
// Usage example:
//   SDL3Initializer sdl;
//   sdl.initialize("Dimensional Navigator", 1280, 720, true, "assets/fonts/custom.ttf", true, true);
//   sdl.eventLoop([] { render(); }, [](int w, int h) { resize(w, h); });
// Zachary Geurts 2025

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <vector>
#include <string>
#include <functional>
#include <memory>
#include <map>
#include <future>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <fstream>
#include <sstream>
#include <filesystem>
#include "SDL3_window.hpp"
#include "SDL3_vulkan.hpp"
#include "SDL3_input.hpp"
#include "SDL3_audio.hpp"

namespace SDL3Initializer {

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

        SDL3Initializer();
        ~SDL3Initializer();

        SDL3Initializer(const SDL3Initializer&) = delete;
        SDL3Initializer& operator=(const SDL3Initializer&) = delete;
        SDL3Initializer(SDL3Initializer&&) noexcept = default;
        SDL3Initializer& operator=(SDL3Initializer&&) noexcept = default;

        void initialize(const char* title, int w, int h, Uint32 flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE,
                        bool rt = true, const std::string& fontPath = "assets/fonts/sf-plasmatica-open.ttf",
                        bool enableValidation = true, bool preferNvidia = false);

        void eventLoop(std::function<void()> render, ResizeCallback onResize = nullptr, bool exitOnClose = true,
                       KeyboardCallback kb = nullptr, MouseButtonCallback mb = nullptr, MouseMotionCallback mm = nullptr,
                       MouseWheelCallback mw = nullptr, TextInputCallback ti = nullptr, TouchCallback tc = nullptr,
                       GamepadButtonCallback gb = nullptr, GamepadAxisCallback ga = nullptr, GamepadConnectCallback gc = nullptr);

        void cleanup();

        SDL_Window* getWindow() const { return m_window.get(); }
        VkInstance getVkInstance() const { return m_useVulkan ? m_instance.get() : VK_NULL_HANDLE; }
        VkSurfaceKHR getVkSurface() const { return m_useVulkan ? m_surface.get() : VK_NULL_HANDLE; }
        SDL_AudioDeviceID getAudioDevice() const { return m_audioDevice; }
        const auto& getGamepads() const { std::lock_guard<std::mutex> lock(gamepadMutex); return m_gamepads; }
        TTF_Font* getFont() const;
        void exportLog(const std::string& filename) const;
        std::vector<std::string> getVulkanExtensions() const;
        bool isConsoleOpen() const { return m_consoleOpen; }

    private:
        std::unique_ptr<SDL_Window, SDLWindowDeleter> m_window;
        std::unique_ptr<std::remove_pointer_t<VkInstance>, VulkanInstanceDeleter> m_instance;
        std::unique_ptr<std::remove_pointer_t<VkSurfaceKHR>, VulkanSurfaceDeleter> m_surface;
        SDL_AudioDeviceID m_audioDevice = 0;
        SDL_AudioStream* m_audioStream = nullptr;
        std::map<SDL_JoystickID, SDL_Gamepad*> m_gamepads;
        mutable std::mutex gamepadMutex;
        mutable TTF_Font* m_font = nullptr;
        mutable std::future<TTF_Font*> m_fontFuture;
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
        ResizeCallback m_onResize;
        KeyboardCallback m_kb;
        MouseButtonCallback m_mb;
        MouseMotionCallback m_mm;
        MouseWheelCallback m_mw;
        TextInputCallback m_ti;
        TouchCallback m_tc;
        GamepadButtonCallback m_gb;
        GamepadAxisCallback m_ga;
        GamepadConnectCallback m_gc;
        bool m_consoleOpen = false;

        void logMessage(const std::string& message) const;
    };

}

#endif // SDL3_INIT_HPP