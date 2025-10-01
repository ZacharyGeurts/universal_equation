// SDL3_init.hpp
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
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <fstream>
#include <iostream>
#include <sstream>
#include <filesystem>
#include "engine/SDL3/SDL3_window.hpp"
#include "engine/SDL3/SDL3_vulkan.hpp"
#include "engine/SDL3/SDL3_audio.hpp"
#include "engine/SDL3/SDL3_font.hpp"
#include "engine/SDL3/SDL3_input.hpp"

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

    // Constructor: Initializes logging and prepares for SDL initialization
    SDL3Initializer();

    // Destructor: Cleans up SDL resources and render thread
    ~SDL3Initializer();

    // Delete copy constructor and assignment operator to prevent copying
    SDL3Initializer(const SDL3Initializer&) = delete;
    SDL3Initializer& operator=(const SDL3Initializer&) = delete;

    // Allow move constructor and assignment for resource transfer
    SDL3Initializer(SDL3Initializer&&) noexcept = default;
    SDL3Initializer& operator=(SDL3Initializer&&) noexcept = default;

    // Initializes SDL subsystems, window, Vulkan, audio, font, and input
    // @param title: Window title
    // @param w: Window width
    // @param h: Window height
    // @param flags: SDL window flags (default: SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE)
    // @param rt: Enable ray tracing (default: true)
    // @param fontPath: Path to TTF font (default: "assets/fonts/sf-plasmatica-open.ttf")
    // @param enableValidation: Enable Vulkan validation layers (default: true)
    // @param preferNvidia: Prefer NVIDIA GPU for Vulkan (default: false)
    void initialize(const char* title, int w, int h, Uint32 flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE,
                    bool rt = true, const std::string& fontPath = "assets/fonts/sf-plasmatica-open.ttf",
                    bool enableValidation = true, bool preferNvidia = false);

    // Runs the main event loop, processing input and rendering
    // @param render: Render callback
    // @param onResize: Window resize callback
    // @param exitOnClose: Exit on window close or quit events (default: true)
    // @param kb: Keyboard callback
    // @param mb: Mouse button callback
    // @param mm: Mouse motion callback
    // @param mw: Mouse wheel callback
    // @param ti: Text input callback
    // @param tc: Touch callback
    // @param gb: Gamepad button callback
    // @param ga: Gamepad axis callback
    // @param gc: Gamepad connect callback
    void eventLoop(std::function<void()> render, ResizeCallback onResize = nullptr, bool exitOnClose = true,
                   KeyboardCallback kb = nullptr, MouseButtonCallback mb = nullptr, MouseMotionCallback mm = nullptr,
                   MouseWheelCallback mw = nullptr, TextInputCallback ti = nullptr, TouchCallback tc = nullptr,
                   GamepadButtonCallback gb = nullptr, GamepadAxisCallback ga = nullptr, GamepadConnectCallback gc = nullptr);

    // Cleans up SDL resources
    void cleanup();

    // Returns the SDL window
    SDL_Window* getWindow() const { return m_window.get(); }

    // Returns the Vulkan instance
    VkInstance getVkInstance() const { return m_useVulkan ? m_instance.get() : VK_NULL_HANDLE; }

    // Returns the Vulkan surface
    VkSurfaceKHR getVkSurface() const { return m_useVulkan ? m_surface.get() : VK_NULL_HANDLE; }

    // Returns the audio device ID
    SDL_AudioDeviceID getAudioDevice() const { return m_audioDevice; }

    // Returns the map of connected gamepads
    const std::map<SDL_JoystickID, SDL_Gamepad*>& getGamepads() const { return m_inputManager.getGamepads(); }

    // Returns the loaded TTF font
    TTF_Font* getFont() const { return m_fontManager.getFont(); }

    // Exports logs to a file
    // @param filename: Path to export the log
    void exportLog(const std::string& filename) const;

    // Returns the required Vulkan extensions
    std::vector<std::string> getVulkanExtensions() const;

    // Returns the console open state
    bool isConsoleOpen() const { return m_consoleOpen; }

private:
    // Logs a message to console and file
    // @param message: Message to log
    void logMessage(const std::string& message) const;

    std::unique_ptr<SDL_Window, SDLWindowDeleter> m_window; // SDL window
    std::unique_ptr<std::remove_pointer_t<VkInstance>, VulkanInstanceDeleter> m_instance; // Vulkan instance
    std::unique_ptr<std::remove_pointer_t<VkSurfaceKHR>, VulkanSurfaceDeleter> m_surface; // Vulkan surface
    SDL_AudioDeviceID m_audioDevice = 0; // Audio device ID
    SDL_AudioStream* m_audioStream = nullptr; // Audio stream
    SDL3Font m_fontManager; // Font manager
    SDL3Input m_inputManager; // Input manager
    std::mutex renderMutex; // Mutex for render thread
    std::condition_variable renderCond; // Condition variable for render thread
    std::atomic<bool> renderReady{false}; // Flag for render readiness
    std::atomic<bool> stopRender{false}; // Flag to stop render thread
    std::thread renderThread; // Render thread
    mutable std::ofstream logFile; // Log file stream
    mutable std::stringstream logStream; // In-memory log stream
    bool m_useVulkan; // Whether Vulkan is used
    bool m_consoleOpen = false; // Console open state
};

} // namespace SDL3Initializer

#endif // SDL3_INIT_HPP