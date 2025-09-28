#ifndef SDL3_CONSOLE_HPP
#define SDL3_CONSOLE_HPP

// AMOURANTH RTX Engine September 2025 - SDL3 Developer Console with FPS and system stats display.
// Toggled with grave key, shows FPS, system stats, command input for developers.
// Uses RAII, thread-safe.
// Commands: help, clear, export, mode <1-3>, quit.
// Dependencies: SDL3, SDL_ttf.
// Best Practices:
// - Call update() every frame.
// - Call handleEvent(e) for events when open to consume input.
// - Call render() after game render.
// Potential Issues:
// - Ensure renderer is created with appropriate backend (Vulkan if used).
// - On Android, font size may need adjustment.
// Usage example:
//   Console console(window, renderer, font);
//   if (console.isOpen() && console.handleEvent(e)) continue;
//   console.update();
//   console.render();
// Zachary Geurts 2025

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "SDL3_fps_counter.hpp"
#include <stdexcept>
#include <vector>
#include <string>
#include <functional>
#include <memory>
#include <deque>
#include <atomic>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <map>

struct ConsoleConfig {
    float updateInterval = 0.1f;      // FPS update interval (seconds)
    int frameWindow = 1000;           // Number of frames for stats
    bool logToFile = false;           // Enable file logging
    std::string logFilePath = "console_log.csv"; // Log file path
    SDL_Color textColor = {255, 255, 255, 255};  // Text color (RGBA)
    SDL_Color bgColor = {0, 0, 0, 128};          // Background color (RGBA)
    int maxHistory = 20;              // Max history lines
};

class Console {
public:
    using CommandCallback = std::function<void(const std::string&)>;

    Console(SDL_Window* window, SDL_Renderer* renderer, TTF_Font* font, const ConsoleConfig& config = ConsoleConfig());
    ~Console();

    bool handleEvent(const SDL_Event& e);
    void update();
    void render();
    bool isOpen() const;
    void toggle();
    void addOutput(const std::string& text);

private:
    SDL_Window* m_window;
    SDL_Renderer* m_renderer;
    TTF_Font* m_font;
    ConsoleConfig m_config;
    std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> m_textTexture;
    std::atomic<bool> m_open;
    std::deque<std::string> m_history;
    std::string m_input;
    bool m_needsUpdate;
    std::map<std::string, CommandCallback> m_commands;
    SDL3_FPS::FPSCounter m_fps;

    void redraw();
    void processCommand(const std::string& cmd);
    void registerDefaultCommands();
};

#endif // SDL3_CONSOLE_HPP