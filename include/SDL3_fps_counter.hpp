#ifndef SDL3_FPS_COUNTER_HPP
#define SDL3_FPS_COUNTER_HPP

#include <SDL3/SDL.h>
#include <SDL3/SDL_ttf.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <memory>
#include <deque>
#include <atomic>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vulkan/vulkan.h>  // Retained if Vulkan integration planned

#if defined(_WIN32)
#include <windows.h>
#include <winsock2.h>
#include <wbemidl.h>
#include <comdef.h>
#pragma comment(lib, "wbemuuid.lib")
#include <processthreadsapi.h>
#elif defined(__linux__)
#include <unistd.h>
#include <fstream>
#elif defined(__APPLE__)
#include <unistd.h>
#include <sys/sysctl.h>
#endif

// Set to 1 if you have NVML library and want GPU monitoring (NVIDIA only)
#define USE_NVML 0
#if USE_NVML
#include <nvml.h>
#endif

// Benchmarking mode: 1 = Basic FPS, 2 = +Frame Times (avg/min/max/1% low), 3 = Full stats
#define BENCHMARK_MODE 2

class FPSCounter {
public:
    FPSCounter(SDL_Window* window, TTF_Font* font, SDL_Renderer* renderer = nullptr)
        : m_window(window), m_font(font), m_renderer(renderer), m_showFPS(false),
          m_frameCount(0), m_fps(0.0f), m_mode(1), m_powerState(SDL_POWERSTATE_UNKNOWN),
          m_prevTotal(0LL), m_prevIdle(0LL), m_wmiQuery(), m_monitorThread(nullptr),
          m_monitorStop(false), m_needsUpdate(true), m_textTexture(nullptr, SDL_DestroyTexture),
          m_textRect{10.0f, 10.0f, 0.0f, 0.0f} {
        if (!window || !font) {
            throw std::runtime_error("Invalid window or font pointer");
        }
        if (!renderer) {
            // Warning omitted for low overhead
        }

        m_textColor = {255, 255, 255, 255};  // White text

        // Initialize static info
        m_deviceName = getDeviceName();
        m_cpuCount = SDL_GetNumLogicalCPUCores();
        m_systemRAM = SDL_GetSystemRAM();

        // Initialize GPU info
        initGPUInfo();

#if USE_NVML
        nvmlReturn_t result = nvmlInit();
        if (result == NVML_SUCCESS) {
            m_nvmlInit = true;
            result = nvmlDeviceGetHandleByIndex(0, &m_device);
            if (result == NVML_SUCCESS) {
                char name[NVML_DEVICE_NAME_BUFFER_SIZE];
                if (nvmlDeviceGetName(m_device, name, sizeof(name)) == NVML_SUCCESS) {
                    m_gpuName = name;
                }
            } else {
                m_nvmlInit = false;
            }
        }
#endif

        // Initialize WMI for Windows (RAII-managed)
#if defined(_WIN32)
        m_wmiQuery = std::make_unique<WMIQuery>();
#endif

        // Start background monitoring thread for low-overhead updates
        m_monitorThread = std::make_unique<std::thread>(&FPSCounter::monitorLoop, this);

        // Initialize timing
        m_lastTime = std::chrono::high_resolution_clock::now();
        m_fpsUpdateTime = m_lastTime;
    }

    ~FPSCounter() {
        if (m_monitorThread) {
            {
                std::lock_guard<std::mutex> lock(m_monitorMutex);
                m_monitorStop = true;
                m_monitorCV.notify_one();
            }
            m_monitorThread->join();
        }
#if USE_NVML
        if (m_nvmlInit) {
            nvmlShutdown();
        }
#endif
    }

    void handleEvent(const SDL_KeyboardEvent& key) {
        if (key.type == SDL_EVENT_KEY_DOWN && key.key == SDLK_F1) {
            m_showFPS = !m_showFPS;
        } else if (key.type == SDL_EVENT_KEY_DOWN && key.key == SDLK_F2) {
            m_mode = (m_mode % 3) + 1;  // Cycle modes: 1=Basic, 2=Frame times, 3=Full
            m_needsUpdate = true;
        }
    }

    void update() {
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto deltaTime = std::chrono::duration<double, std::milli>(currentTime - m_lastTime).count();
        m_frameTimes.push_back(deltaTime);
        if (m_frameTimes.size() > 1000) {  // Rolling window of 1000 frames for stats
            m_frameTimes.pop_front();
        }
        m_lastTime = currentTime;
        m_frameCount++;

        // FPS calculation every 100ms for smoother updates (low overhead)
        if (std::chrono::duration<double>(currentTime - m_fpsUpdateTime).count() >= 0.1) {
            double elapsed = std::chrono::duration<double>(currentTime - m_fpsUpdateTime).count();
            m_fps = static_cast<float>(m_frameCount / elapsed);
            m_frameCount = 0;
            m_fpsUpdateTime = currentTime;

            // Queue update for background thread
            {
                std::lock_guard<std::mutex> lock(m_monitorMutex);
                m_needsUpdate = true;
                m_monitorCV.notify_one();
            }
        }
    }

    void render() {
        if (!m_showFPS || !m_renderer || !m_textTexture) {
            return;
        }
        SDL_RenderTexture(m_renderer, m_textTexture.get(), nullptr, &m_textRect);
    }

    void setMode(int mode) {
        m_mode = std::max(1, std::min(3, mode));  // Clamp to 1-3
        m_needsUpdate = true;
    }

    // Benchmark export: Log stats to console or file (call periodically)
    void exportBenchmarkStats() {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        if (m_frameTimes.empty()) return;

        double avgFrameTime = 0.0;
        double minFrameTime = m_frameTimes.front();
        double maxFrameTime = m_frameTimes.front();
        for (auto ft : m_frameTimes) {
            avgFrameTime += ft;
            minFrameTime = std::min(minFrameTime, ft);
            maxFrameTime = std::max(maxFrameTime, ft);
        }
        avgFrameTime /= m_frameTimes.size();
        float avgFPS = 1000.0f / static_cast<float>(avgFrameTime);

        // 1% low: Sort and take bottom 1%
        std::vector<double> sortedTimes = m_frameTimes;
        std::sort(sortedTimes.begin(), sortedTimes.end());
        size_t lowIndex = sortedTimes.size() * 0.01;
        float p1LowFPS = 1000.0f / static_cast<float>(sortedTimes[lowIndex]);

        // Use SDL_Log for low-overhead output
        SDL_Log("Benchmark: Avg FPS: %.1f, 1%% Low: %.1f, Frame Time (avg/min/max): %.2f/%.2f/%.2f ms",
                avgFPS, p1LowFPS, avgFrameTime, minFrameTime, maxFrameTime);

        // Optional: CPU/GPU stats here if m_mode == 3
        if (m_mode == 3) {
            SDL_Log("CPU: %.1f%%, %.0dC | GPU: %s, %.0f%%, %.0dC | RAM: %dMB | Battery: %d%%",
                    m_cpuUsage, m_cpuTemp, m_gpuName.c_str(), m_gpuUsage, m_gpuTemp, m_systemRAM, m_batteryPercent);
        }
    }

private:
    SDL_Window* m_window;
    TTF_Font* m_font;
    SDL_Renderer* m_renderer;
    std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> m_textTexture;
    SDL_FRect m_textRect;
    SDL_Color m_textColor;
    std::atomic<bool> m_showFPS{false};
    std::atomic<int> m_frameCount{0};
    std::atomic<float> m_fps{0.0f};
    int m_mode;
    std::string m_deviceName;
    int m_cpuCount;
    float m_cpuUsage = -1.0f;
    int m_cpuTemp = -1;
    int m_systemRAM;
    int m_batteryPercent = -1;
    SDL_PowerState m_powerState;
    std::string m_gpuName = "Unknown";
    float m_gpuUsage = -1.0f;
    int m_gpuTemp = -1;

    // Timing and stats
    std::chrono::high_resolution_clock::time_point m_lastTime;
    std::chrono::high_resolution_clock::time_point m_fpsUpdateTime;
    std::deque<double> m_frameTimes;  // ms, rolling for benchmarks

    // Background monitoring
    std::unique_ptr<std::thread> m_monitorThread;
    std::atomic<bool> m_monitorStop{false};
    std::mutex m_monitorMutex;
    std::condition_variable m_monitorCV;
    bool m_needsUpdate;

    // Platform-specific
#if defined(_WIN32)
    class WMIQuery {
    public:
        WMIQuery() {
            CoInitialize(nullptr);
            CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (void**)&locator);
            locator->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, nullptr, 0, nullptr, nullptr, &services);
            locator->Release();
            CoSetProxyBlanket(services, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
        }
        ~WMIQuery() {
            if (services) services->Release();
            CoUninitialize();
        }
        float getCPUUsage() {
            // Simplified: Reuse connection, exec query for PercentProcessorTime
            // Implementation similar to original but using services
            // Return parsed value or -1
            return -1.0f;  // Placeholder; implement reuse
        }
        int getCPUTemperature() {
            // Similar for thermal zone
            return -1;
        }
    private:
        IWbemLocator* locator = nullptr;
        IWbemServices* services = nullptr;
    };
    std::unique_ptr<WMIQuery> m_wmiQuery;
    long long m_prevTotal = 0LL;
    long long m_prevIdle = 0LL;
#elif defined(__linux__)
    long long m_prevTotal = 0LL;
    long long m_prevIdle = 0LL;
#endif

#if USE_NVML
    bool m_nvmlInit = false;
    nvmlDevice_t m_device;
#endif

    std::mutex m_statsMutex;

    std::string getDeviceName() {
#if defined(_WIN32)
        char name[256];
        DWORD size = sizeof(name);
        return GetComputerNameA(name, &size) ? std::string(name) : "Unknown";
#elif defined(__linux__)
        std::ifstream ifs("/proc/sys/kernel/hostname");
        std::string name;
        return (ifs && std::getline(ifs, name)) ? name : "Unknown";
#elif defined(__APPLE__)
        char name[256];
        size_t size = sizeof(name);
        return (sysctlbyname("kern.hostname", name, &size, nullptr, 0) == 0) ? std::string(name) : "Unknown";
#else
        return "Unknown";
#endif
    }

    float getCPUUsage() {
#if defined(_WIN32)
        return m_wmiQuery ? m_wmiQuery->getCPUUsage() : -1.0f;
#elif defined(__linux__)
        std::ifstream stat("/proc/stat");
        if (!stat) return -1.0f;
        std::string line;
        std::getline(stat, line);
        long long user, nice, system, idle, iowait, irq, softirq;
        sscanf(line.c_str(), "cpu %lld %lld %lld %lld %lld %lld %lld", &user, &nice, &system, &idle, &iowait, &irq, &softirq);
        long long total = user + nice + system + idle + iowait + irq + softirq;
        long long idleTime = idle + iowait;
        float usage = (m_prevTotal == 0) ? -1.0f : 100.0f * (1.0f - static_cast<float>(idleTime - m_prevIdle) / (total - m_prevTotal));
        m_prevTotal = total;
        m_prevIdle = idleTime;
        return usage;
#elif defined(__APPLE__)
        return -1.0f;
#else
        return -1.0f;
#endif
    }

    int getCPUTemperature() {
#if defined(_WIN32)
        return m_wmiQuery ? m_wmiQuery->getCPUTemperature() : -1;
#elif defined(__linux__)
        std::ifstream temp("/sys/class/thermal/thermal_zone0/temp");
        if (!temp) return -1;
        int value;
        temp >> value;
        return value / 1000;
#elif defined(__APPLE__)
        return -1;
#else
        return -1;
#endif
    }

    void initGPUInfo() {
        if (m_renderer) {
            const char* rendererName = SDL_GetRendererName(m_renderer);
            if (rendererName) {
                m_gpuName = rendererName;
            }
        } else {
            m_gpuName = "Vulkan Device";
        }
    }

    void monitorLoop() {
        while (!m_monitorStop) {
            std::unique_lock<std::mutex> lock(m_monitorMutex);
            m_monitorCV.wait(lock, [this] { return m_needsUpdate || m_monitorStop; });
            if (m_monitorStop) break;

            m_cpuUsage = getCPUUsage();
            m_cpuTemp = getCPUTemperature();
            int seconds, percent;
            m_powerState = SDL_GetPowerInfo(&seconds, &percent);
            m_batteryPercent = (m_powerState != SDL_POWERSTATE_UNKNOWN) ? percent : -1;

#if USE_NVML
            if (m_nvmlInit) {
                unsigned int util;
                if (nvmlDeviceGetUtilizationRates(m_device, &util) == NVML_SUCCESS) {
                    m_gpuUsage = static_cast<float>(util.gpu);
                }
                unsigned int temp;
                if (nvmlDeviceGetTemperature(m_device, NVML_TEMPERATURE_GPU, &temp) == NVML_SUCCESS) {
                    m_gpuTemp = static_cast<int>(temp);
                }
            }
#endif

            generateText();
            m_needsUpdate = false;
        }
    }

    void generateText() {
        if (!m_renderer) return;

        std::stringstream ss;
        ss << "Device: " << m_deviceName << "\n"
           << "CPU: " << m_cpuCount << " cores, " << std::fixed << std::setprecision(1) << m_cpuUsage << "%"
           << (m_cpuTemp >= 0 ? ", " + std::to_string(m_cpuTemp) + "C" : "") << "\n"
           << "GPU: " << m_gpuName
           << (m_gpuUsage >= 0 ? ", " + std::to_string(static_cast<int>(m_gpuUsage)) + "%" : "")
           << (m_gpuTemp >= 0 ? ", " + std::to_string(m_gpuTemp) + "C" : "") << "\n"
           << "RAM: " << m_systemRAM << "MB\n"
           << "Battery: " << (m_batteryPercent >= 0 ? std::to_string(m_batteryPercent) + "%" : "N/A") << "\n"
           << "FPS: " << std::fixed << std::setprecision(1) << m_fps << "\n";

        if (m_mode >= 2) {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            if (!m_frameTimes.empty()) {
                double avgFT = 0.0;
                double minFT = m_frameTimes.front(), maxFT = m_frameTimes.front();
                for (auto ft : m_frameTimes) {
                    avgFT += ft;
                    minFT = std::min(minFT, ft);
                    maxFT = std::max(maxFT, ft);
                }
                avgFT /= m_frameTimes.size();
                ss << "Frame Time (avg/min/max): " << std::setprecision(2) << avgFT << "/" << minFT << "/" << maxFT << " ms\n";
            }
        }

        ss << "Mode: " << m_mode << "D";

        SDL_Surface* surface = TTF_RenderText_Blended_Wrapped(m_font, ss.str().c_str(), m_textColor, 0);
        if (!surface) return;

        m_textTexture.reset(SDL_CreateTextureFromSurface(m_renderer, surface));
        if (m_textTexture) {
            int w, h;
            SDL_QueryTexture(m_textTexture.get(), nullptr, nullptr, &w, &h);
            m_textRect.w = static_cast<float>(w);
            m_textRect.h = static_cast<float>(h);
        }
        SDL_DestroySurface(surface);
    }
};

#endif