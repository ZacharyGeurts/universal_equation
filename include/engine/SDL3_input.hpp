#ifndef SDL3_INPUT_HPP
#define SDL3_INPUT_HPP

#include <SDL3/SDL.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_gamepad.h>
#include <mutex>
#include <thread>
#include <queue>
#include <atomic>
#include <map>
#include <functional>
#include <condition_variable> // Added for std::condition_variable
#include "SDL3_window.hpp"
// Processes input events (keyboard, mouse, touch, gamepad) and worker threads.
// AMOURANTH RTX Engine September 2025
// Zachary Geurts 2025
namespace SDL3Initializer {

    using KeyboardCallback = std::function<void(const SDL_KeyboardEvent&)>;
    using MouseButtonCallback = std::function<void(const SDL_MouseButtonEvent&)>;
    using MouseMotionCallback = std::function<void(const SDL_MouseMotionEvent&)>;
    using MouseWheelCallback = std::function<void(const SDL_MouseWheelEvent&)>;
    using TextInputCallback = std::function<void(const SDL_TextInputEvent&)>;
    using TouchCallback = std::function<void(const SDL_TouchFingerEvent&)>;
    using GamepadButtonCallback = std::function<void(const SDL_GamepadButtonEvent&)>;
    using GamepadAxisCallback = std::function<void(const SDL_GamepadAxisEvent&)>;
    using GamepadConnectCallback = std::function<void(bool, SDL_JoystickID, SDL_Gamepad*)>;

    // Initializes input system
    void initInput(std::map<SDL_JoystickID, SDL_Gamepad*>& gamepads, std::mutex& gamepadMutex,
                   GamepadConnectCallback gc, std::function<void(const std::string&)> logMessage);

    // Handles keyboard events
    void handleKeyboard(const SDL_KeyboardEvent& k, SDL_Window* window, SDL_AudioDeviceID audioDevice, bool& consoleOpen,
                       std::function<void(const std::string&)> logMessage);

    // Handles mouse button events
    void handleMouseButton(const SDL_MouseButtonEvent& b, SDL_Window* window, std::function<void(const std::string&)> logMessage);

    // Handles touch events
    void handleTouch(const SDL_TouchFingerEvent& t, std::function<void(const std::string&)> logMessage);

    // Handles gamepad button events
    void handleGamepadButton(const SDL_GamepadButtonEvent& g, SDL_AudioDeviceID audioDevice,
                            std::function<void(const std::string&)> logMessage);

    // Starts worker threads for gamepad events
    void startWorkerThreads(int numThreads, std::queue<std::function<void()>>& taskQueue, std::mutex& taskMutex,
                           std::condition_variable& taskCond, std::atomic<bool>& stopWorkers,
                           std::vector<std::thread>& workerThreads, std::function<void(const std::string&)> logMessage);

}

#endif // SDL3_INPUT_HPP