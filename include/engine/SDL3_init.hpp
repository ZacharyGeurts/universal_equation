#ifndef SDL3_INIT_HPP
#define SDL3_INIT_HPP

// Initializes SDL3 and Vulkan surface for AMOURANTH RTX Engine, October 2025
// Thread-safe with C++20 features; no mutexes required.
// Dependencies: SDL3, SDL3_ttf, Vulkan 1.3+, logging.hpp.
// Usage: Initialize with title, size, and flags; run eventLoop with render and resize callbacks.
// Zachary Geurts 2025

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <vulkan/vulkan.h>
#include <functional>
#include <vector>
#include <string>
#include <memory>
#include <map>
#include "engine/logging.hpp"
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

    SDL3Initializer(const Logging::Logger& logger);
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
    bool isConsoleOpen() const { return m_consoleOpen; }
    void exportLog(const std::string& filename) const;
    std::vector<std::string> getVulkanExtensions() const;

private:
    std::unique_ptr<SDL_Window, SDLWindowDeleter> m_window;
    std::unique_ptr<std::remove_pointer_t<VkInstance>, VulkanInstanceDeleter> m_instance;
    std::unique_ptr<std::remove_pointer_t<VkSurfaceKHR>, VulkanSurfaceDeleter> m_surface;
    SDL_AudioDeviceID m_audioDevice = 0;
    SDL_AudioStream* m_audioStream = nullptr;
    SDL3Font m_fontManager;
    SDL3Input m_inputManager;
    const Logging::Logger& logger_;
    bool m_useVulkan;
    bool m_consoleOpen = false;
};

} // namespace SDL3Initializer

#endif // SDL3_INIT_HPP