// AMOURANTH RTX Engine, October 2025 - SDL3Initializer for SDL3 and Vulkan integration.
// Initializes SDL subsystems, window, Vulkan, audio, font, and input with thread-safe event processing.
// Supports Windows and Linux (X11/Wayland); no mutexes or threads for rendering.
// Dependencies: SDL3, SDL3_ttf, Vulkan 1.3+, C++20 standard library.
// Usage: Initialize with title, size, and flags; run eventLoop with render and resize callbacks.
// Potential Issues: Ensure Vulkan extensions (VK_KHR_surface, platform-specific) and NVIDIA GPU selection for hybrid systems.
// Zachary Geurts 2025

#ifndef SDL3_INIT_HPP
#define SDL3_INIT_HPP

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <vulkan/vulkan.h>
#include <functional>
#include <vector>
#include <string>
#include <memory>
#include <map>
#include <fstream>
#include <sstream>
#include <format>
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
    const std::map<SDL_JoystickID, SDL_Gamepad*>& getGamepads() const { return m_inputManager.getGamepads(); }
    TTF_Font* getFont() const { return m_fontManager.getFont(); }
    void exportLog(const std::string& filename) const;
    std::vector<std::string> getVulkanExtensions() const;
    bool isConsoleOpen() const { return m_consoleOpen; }

private:
    void logMessage(const std::string& message) const;

    std::unique_ptr<SDL_Window, SDLWindowDeleter> m_window;
    std::unique_ptr<std::remove_pointer_t<VkInstance>, VulkanInstanceDeleter> m_instance;
    std::unique_ptr<std::remove_pointer_t<VkSurfaceKHR>, VulkanSurfaceDeleter> m_surface;
    SDL_AudioDeviceID m_audioDevice = 0;
    SDL_AudioStream* m_audioStream = nullptr;
    SDL3Font m_fontManager;
    SDL3Input m_inputManager;
    mutable std::ofstream logFile;
    mutable std::stringstream logStream;
    bool m_useVulkan;
    bool m_consoleOpen = false;
};

} // namespace SDL3Initializer

#endif // SDL3_INIT_HPP