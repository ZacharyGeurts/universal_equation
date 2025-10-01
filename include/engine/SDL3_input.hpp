// SDL3_input.hpp
#ifndef SDL3_INPUT_HPP
#define SDL3_INPUT_HPP

// AMOURANTH RTX Engine September 2025 - Input handling for SDL3-based applications.
// Manages keyboard, mouse, touch, and gamepad input events with thread-safe gamepad event processing.
// Provides a class-based interface for initializing input systems, polling events, and handling callbacks.
// Key features:
// - Thread-safe gamepad management with worker threads for asynchronous event processing.
// - Configurable callbacks for keyboard, mouse, touch, and gamepad events.
// - Logging to file and console for debugging and diagnostics.
// - RAII-based resource management for gamepads and threads.
// Dependencies: SDL3 (SDL.h, SDL_mouse.h, SDL_gamepad.h), C++17 standard library (mutex, thread, queue, etc.).
// Best Practices:
// - Initialize with SDL3Input::initialize before polling events.
// - Set callbacks via SDL3Input::setCallbacks to handle specific input events.
// - Use SDL3Input::pollEvents in the main loop to process events.
// - Call SDL3Input::exportLog to save diagnostic logs.
// Potential Issues:
// - Ensure SDL_INIT_GAMEPAD is included in SDL_Init flags.
// - Verify thread safety when accessing gamepads from multiple threads.
// Usage example:
//   SDL3Input input;
//   input.initialize([](bool connected, SDL_JoystickID id, SDL_Gamepad* gp) { /* Handle gamepad connect */ });
//   input.setCallbacks(kbCallback, mbCallback, ...);
//   while (input.pollEvents(window, audioDevice, consoleOpen, true)) { /* Render loop */ }
// Zachary Geurts 2025

#include <SDL3/SDL.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_gamepad.h>
#include <map>
#include <mutex>
#include <queue>
#include <vector>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include "SDL3_window.hpp"

namespace SDL3Initializer {

class SDL3Input {
public:
    // Type aliases for event callbacks
    using KeyboardCallback = std::function<void(const SDL_KeyboardEvent&)>;
    using MouseButtonCallback = std::function<void(const SDL_MouseButtonEvent&)>;
    using MouseMotionCallback = std::function<void(const SDL_MouseMotionEvent&)>;
    using MouseWheelCallback = std::function<void(const SDL_MouseWheelEvent&)>;
    using TextInputCallback = std::function<void(const SDL_TextInputEvent&)>;
    using TouchCallback = std::function<void(const SDL_TouchFingerEvent&)>;
    using GamepadButtonCallback = std::function<void(const SDL_GamepadButtonEvent&)>;
    using GamepadAxisCallback = std::function<void(const SDL_GamepadAxisEvent&)>;
    using GamepadConnectCallback = std::function<void(bool, SDL_JoystickID, SDL_Gamepad*)>;
    using ResizeCallback = std::function<void(int, int)>;

    // Constructor: Initializes logging with the specified file path
    // @param logFilePath: Path to the log file (default: "sdl3_input.log")
    explicit SDL3Input(const std::string& logFilePath = "sdl3_input.log");

    // Destructor: Cleans up gamepads and worker threads
    ~SDL3Input();

    // Initializes the input system, setting up gamepad support and worker threads
    // @param gc: Callback for gamepad connection/disconnection events
    void initialize(GamepadConnectCallback gc);

    // Sets callbacks for input events
    // @param kb: Keyboard event callback
    // @param mb: Mouse button event callback
    // @param mm: Mouse motion event callback
    // @param mw: Mouse wheel event callback
    // @param ti: Text input event callback
    // @param tc: Touch event callback
    // @param gb: Gamepad button event callback
    // @param ga: Gamepad axis event callback
    // @param gc: Gamepad connect/disconnect callback
    // @param onResize: Window resize callback
    void setCallbacks(KeyboardCallback kb, MouseButtonCallback mb, MouseMotionCallback mm,
                      MouseWheelCallback mw, TextInputCallback ti, TouchCallback tc,
                      GamepadButtonCallback gb, GamepadAxisCallback ga, GamepadConnectCallback gc,
                      ResizeCallback onResize);

    // Polls and processes SDL events, returning false if a quit event is received
    // @param window: SDL window for input handling
    // @param audioDevice: SDL audio device ID for audio-related input actions
    // @param consoleOpen: Reference to console open state
    // @param exitOnClose: Whether to exit on window close or quit events
    // @return: True if the application should continue running, false otherwise
    bool pollEvents(SDL_Window* window, SDL_AudioDeviceID audioDevice, bool& consoleOpen, bool exitOnClose);

    // Enables or disables text input
    // @param window: SDL window for text input
    // @param enable: True to enable text input, false to disable
    void enableTextInput(SDL_Window* window, bool enable);

    // Returns the current map of connected gamepads
    // @return: Thread-safe reference to gamepad map
    const std::map<SDL_JoystickID, SDL_Gamepad*>& getGamepads() const;

    // Exports the input log to a file
    // @param filename: Path to export the log
    void exportLog(const std::string& filename) const;

private:
    // Handles keyboard events (e.g., fullscreen toggle, audio control)
    // @param k: Keyboard event data
    // @param window: SDL window for window-related actions
    // @param audioDevice: SDL audio device ID for audio actions
    // @param consoleOpen: Reference to console open state
    void handleKeyboard(const SDL_KeyboardEvent& k, SDL_Window* window, SDL_AudioDeviceID audioDevice, bool& consoleOpen);

    // Handles mouse button events (e.g., toggle relative mouse mode)
    // @param b: Mouse button event data
    // @param window: SDL window for mouse mode changes
    void handleMouseButton(const SDL_MouseButtonEvent& b, SDL_Window* window);

    // Handles touch events (logs touch coordinates and pressure)
    // @param t: Touch event data
    void handleTouch(const SDL_TouchFingerEvent& t);

    // Handles gamepad button events (e.g., audio toggle, quit)
    // @param g: Gamepad button event data
    // @param audioDevice: SDL audio device ID for audio actions
    void handleGamepadButton(const SDL_GamepadButtonEvent& g, SDL_AudioDeviceID audioDevice);

    // Starts worker threads for asynchronous gamepad event processing
    // @param numThreads: Number of worker threads to start
    void startWorkerThreads(int numThreads);

    // Cleans up gamepad resources
    void cleanup();

    // Logs a message to console and file
    // @param message: Message to log
    void logMessage(const std::string& message) const;

    std::map<SDL_JoystickID, SDL_Gamepad*> gamepads; // Map of connected gamepads
    mutable std::mutex gamepadMutex; // Mutex for thread-safe gamepad access
    std::queue<std::function<void()>> taskQueue; // Queue for gamepad event tasks
    std::mutex taskMutex; // Mutex for task queue
    std::condition_variable taskCond; // Condition variable for task queue
    std::vector<std::thread> workerThreads; // Worker threads for gamepad events
    std::atomic<bool> stopWorkers{false}; // Flag to stop worker threads
    mutable std::ofstream logFile; // Log file stream
    mutable std::stringstream logStream; // In-memory log stream
    KeyboardCallback m_kb; // Keyboard callback
    MouseButtonCallback m_mb; // Mouse button callback
    MouseMotionCallback m_mm; // Mouse motion callback
    MouseWheelCallback m_mw; // Mouse wheel callback
    TextInputCallback m_ti; // Text input callback
    TouchCallback m_tc; // Touch callback
    GamepadButtonCallback m_gb; // Gamepad button callback
    GamepadAxisCallback m_ga; // Gamepad axis callback
    GamepadConnectCallback m_gc; // Gamepad connect callback
    ResizeCallback m_onResize; // Window resize callback
};

} // namespace SDL3Initializer

#endif // SDL3_INPUT_HPP