#include "SDL3_input.hpp"
#include <condition_variable>
// AMOURANTH RTX Engine September 2025
// Zachary Geurts 2025
namespace SDL3Initializer {

    void initInput(std::map<SDL_JoystickID, SDL_Gamepad*>& gamepads, std::mutex& gamepadMutex,
                   GamepadConnectCallback gc, std::function<void(const std::string&)> logMessage) {
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
                        gamepads[joysticks[i]] = gp;
                        if (gc) gc(true, joysticks[i], gp);
                    }
                }
            }
            SDL_free(joysticks);
        }
    }

    void handleKeyboard(const SDL_KeyboardEvent& k, SDL_Window* window, SDL_AudioDeviceID audioDevice, bool& consoleOpen,
                       std::function<void(const std::string&)> logMessage) {
        if (k.type != SDL_EVENT_KEY_DOWN) {
            logMessage("Ignoring non-key-down event");
            return;
        }
        logMessage("Handling key: " + std::string(SDL_GetKeyName(k.key)));
        switch (k.key) {
            case SDLK_F:
                logMessage("Toggling fullscreen mode");
                {
                    Uint32 flags = SDL_GetWindowFlags(window);
                    bool isFullscreen = flags & SDL_WINDOW_FULLSCREEN;
                    SDL_SetWindowFullscreen(window, isFullscreen ? 0 : SDL_WINDOW_FULLSCREEN);
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
                if (audioDevice) {
                    if (SDL_AudioDevicePaused(audioDevice)) {
                        SDL_ResumeAudioDevice(audioDevice);
                    } else {
                        SDL_PauseAudioDevice(audioDevice);
                    }
                }
                break;
            case SDLK_M:
                logMessage("Toggling audio mute");
                if (audioDevice) {
                    float gain = SDL_GetAudioDeviceGain(audioDevice);
                    SDL_SetAudioDeviceGain(audioDevice, gain == 0.0f ? 1.0f : 0.0f);
                    logMessage("Audio " + std::string(gain == 0.0f ? "unmuted" : "muted"));
                }
                break;
            case SDLK_GRAVE:
                logMessage("Toggling console");
                consoleOpen = !consoleOpen;
                break;
        }
    }

    void handleMouseButton(const SDL_MouseButtonEvent& b, SDL_Window* window, std::function<void(const std::string&)> logMessage) {
        logMessage((b.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? "Pressed" : "Released") + std::string(" mouse button ") +
                   (b.button == SDL_BUTTON_LEFT ? "Left" : b.button == SDL_BUTTON_RIGHT ? "Right" : "Middle") +
                   " at (" + std::to_string(b.x) + ", " + std::to_string(b.y) + ")");
        if (b.type == SDL_EVENT_MOUSE_BUTTON_DOWN && b.button == SDL_BUTTON_RIGHT) {
            logMessage("Toggling relative mouse mode");
            bool relative = SDL_GetWindowRelativeMouseMode(window);
            SDL_SetWindowRelativeMouseMode(window, !relative);
        }
    }

    void handleTouch(const SDL_TouchFingerEvent& t, std::function<void(const std::string&)> logMessage) {
        logMessage("Touch " + std::string(t.type == SDL_EVENT_FINGER_DOWN ? "DOWN" : t.type == SDL_EVENT_FINGER_UP ? "UP" : "MOTION") +
                   " fingerID: " + std::to_string(t.fingerID) + " at (" + std::to_string(t.x) + ", " + std::to_string(t.y) +
                   ") pressure: " + std::to_string(t.pressure));
    }

    void handleGamepadButton(const SDL_GamepadButtonEvent& g, SDL_AudioDeviceID audioDevice,
                            std::function<void(const std::string&)> logMessage) {
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
                    if (audioDevice) {
                        logMessage("Toggling audio pause/resume");
                        if (SDL_AudioDevicePaused(audioDevice)) {
                            SDL_ResumeAudioDevice(audioDevice);
                        } else {
                            SDL_PauseAudioDevice(audioDevice);
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

    void startWorkerThreads(int numThreads, std::queue<std::function<void()>>& taskQueue, std::mutex& taskMutex,
                           std::condition_variable& taskCond, std::atomic<bool>& stopWorkers,
                           std::vector<std::thread>& workerThreads, std::function<void(const std::string&)> logMessage) {
        logMessage("Starting " + std::to_string(numThreads) + " worker threads for gamepad events");
        for (int i = 0; i < numThreads; ++i) {
            workerThreads.emplace_back([&taskQueue, &taskMutex, &taskCond, &stopWorkers, &logMessage] {
                while (true) {
                    std::vector<std::function<void()>> tasks;
                    {
                        std::unique_lock<std::mutex> lock(taskMutex);
                        taskCond.wait(lock, [&] { return !taskQueue.empty() || stopWorkers; });
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

}