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
#include <algorithm>
#include <vector>
#include <fstream>
#include <ctime>

#if defined(_WIN32)
#include <windows.h>
#include <winsock2.h>
#include <wbemidl.h>
#include <comdef.h>
#pragma comment(lib, "wbemuuid.lib")
#elif defined(__linux__)
#include <unistd.h>
#include <fstream>
#elif defined(__APPLE__)
#include <unistd.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <IOKit/IOKitLib.h>
#endif

// Set to 1 if NVML is available (NVIDIA GPUs only)
#define USE_NVML 0
#if USE_NVML
#include <nvml.h>
#endif

// Benchmarking mode: 1 = Basic FPS, 2 = +Frame Times, 3 = Full stats
#define BENCHMARK_MODE 2

namespace SDL3_FPS {

/**
 * @brief Configuration struct for AMOURANTH RTX FPS counter.
 */
struct FPSCounterConfig {
    SDL_FRect textPosition = {10.0f, 10.0f, 0.0f, 0.0f};  ///< Text position and size
    SDL_Color textColor = {255, 255, 255, 255};           ///< Text color (RGBA)
    float fpsUpdateInterval = 0.1f;                       ///< FPS update interval (seconds)
    int frameTimeWindow = 1000;                           ///< Number of frames for stats
    bool logToFile = false;                               ///< Enable file logging
    std::string logFilePath = "amouranth_rtx_benchmark.csv"; ///< Log file path
};

/**
 * @brief AMOURANTH RTX: A high-performance FPS counter and system benchmark toolkit for SDL3 applications.
 */
class FPSCounter {
public:
    /**
     * @brief Constructs an FPSCounter for performance monitoring.
     * @param window SDL3 window for rendering.
     * @param font SDL_ttf font for text rendering.
     * @param config Configuration for text position, color, and logging.
     * @throws std::runtime_error if window or font is null.
     */
    FPSCounter(SDL_Window* window, TTF_Font* font,
               const FPSCounterConfig& config = FPSCounterConfig())
        : m_window(window), m_font(font), m_config(config),
          m_textColor(config.textColor), m_frameCount(0), m_fps(0.0f),
          m_mode(BENCHMARK_MODE), m_powerState(SDL_POWERSTATE_UNKNOWN),
          m_monitorThread(nullptr), m_monitorStop(false), m_needsUpdate(true) {
        if (!window || !font) {
            throw std::runtime_error("Invalid window or font pointer");
        }

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
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to get NVML device handle");
            }
        } else {
            m_nvmlInit = false;
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize NVML");
        }
#endif

#if defined(_WIN32)
        m_wmiQuery = std::make_unique<WMIQuery>();
#endif

        // Start background monitoring thread
        m_monitorThread = std::make_unique<std::thread>(&FPSCounter::monitorLoop, this);

        // Initialize timing
        m_lastTime = std::chrono::high_resolution_clock::now();
        m_fpsUpdateTime = m_lastTime;
    }

    /**
     * @brief Destructor. Cleans up resources and stops monitoring thread.
     */
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

    /**
     * @brief Handles keyboard events to change modes.
     * @param key SDL keyboard event.
     */
    void handleEvent(const SDL_KeyboardEvent& key) {
        if (key.type == SDL_EVENT_KEY_DOWN && key.key == SDLK_F2) {
            m_mode = (m_mode % 3) + 1;  // Cycle modes: 1=Basic, 2=Frame times, 3=Full
            m_needsUpdate = true;
        }
    }

    /**
     * @brief Updates FPS and frame time data.
     */
    void update() {
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto deltaTime = std::chrono::duration<double, std::milli>(currentTime - m_lastTime).count();
        m_frameTimes.push_back(deltaTime);
        if (m_frameTimes.size() > static_cast<size_t>(m_config.frameTimeWindow)) {
            m_frameTimes.pop_front();
        }
        m_lastTime = currentTime;
        m_frameCount++;

        // Update FPS every m_config.fpsUpdateInterval seconds
        if (std::chrono::duration<double>(currentTime - m_fpsUpdateTime).count() >= m_config.fpsUpdateInterval) {
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

    /**
     * @brief Gets the stats string for display.
     * @return Formatted stats string.
     */
    std::string getStatsString() const {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        std::stringstream ss;
        ss << "Device: " << m_deviceName << "\n"
           << "CPU: " << m_cpuCount << " cores, " << std::fixed << std::setprecision(1) << m_cpuUsage.load() << "%"
           << (m_cpuTemp >= 0 ? ", " + std::to_string(m_cpuTemp.load()) + "C" : "") << "\n"
           << "GPU: " << m_gpuName
           << (m_gpuUsage >= 0 ? ", " + std::to_string(static_cast<int>(m_gpuUsage.load())) + "%" : "")
           << (m_gpuTemp >= 0 ? ", " + std::to_string(m_gpuTemp.load()) + "C" : "") << "\n"
           << "RAM: " << m_systemRAM << "MB\n"
           << "Battery: " << (m_batteryPercent >= 0 ? std::to_string(m_batteryPercent.load()) + "%" : "N/A") << "\n"
           << "FPS: " << std::fixed << std::setprecision(1) << m_fps.load() << "\n";

        if (m_mode >= 2) {
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

        return ss.str();
    }

    /**
     * @brief Sets the benchmark mode (1=Basic FPS, 2=Frame Times, 3=Full Stats).
     * @param mode The mode to set (clamped to 1-3).
     */
    void setMode(int mode) {
        m_mode = std::max(1, std::min(3, mode));
        m_needsUpdate = true;
    }

    int getMode() const {
        return m_mode;
    }

    /**
     * @brief Exports benchmark statistics to console and optionally to a file.
     */
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

        // Calculate 1% low FPS
        std::vector<double> sortedTimes(m_frameTimes.begin(), m_frameTimes.end());
        std::sort(sortedTimes.begin(), sortedTimes.end());
        size_t lowIndex = sortedTimes.size() * 0.01;
        float p1LowFPS = 1000.0f / static_cast<float>(sortedTimes[lowIndex]);

        // Log to console
        SDL_Log("Benchmark: Avg FPS: %.1f, 1%% Low: %.1f, Frame Time (avg/min/max): %.2f/%.2f/%.2f ms",
                avgFPS, p1LowFPS, avgFrameTime, minFrameTime, maxFrameTime);

        // Full stats for mode 3
        if (m_mode == 3) {
            SDL_Log("CPU: %.1f%%, %dC | GPU: %s, %.0f%%, %dC | RAM: %dMB | Battery: %d%%",
                    m_cpuUsage.load(), m_cpuTemp.load(), m_gpuName.c_str(), m_gpuUsage.load(),
                    m_gpuTemp.load(), m_systemRAM, m_batteryPercent.load());
        }

        // Log to file if enabled
        if (m_config.logToFile) {
            std::stringstream timeStr;
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            timeStr << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
            std::ofstream ofs(m_config.logFilePath, std::ios::app);
            if (ofs) {
                if (ofs.tellp() == 0) {
                    ofs << "Timestamp,AvgFPS,1%LowFPS,AvgFrameTime,MinFrameTime,MaxFrameTime,CPUUsage,CPUTemp,GPUUsage,GPUTemp,RAM,Battery\n";
                }
                ofs << timeStr.str() << "," << avgFPS << "," << p1LowFPS << "," << avgFrameTime << ","
                    << minFrameTime << "," << maxFrameTime << "," << m_cpuUsage.load() << ","
                    << m_cpuTemp.load() << "," << m_gpuUsage.load() << "," << m_gpuTemp.load() << ","
                    << m_systemRAM << "," << m_batteryPercent.load() << "\n";
            } else {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to write to log file: %s",
                            m_config.logFilePath.c_str());
            }
        }
    }

private:
    SDL_Window* m_window;
    TTF_Font* m_font;
    FPSCounterConfig m_config;
    SDL_Color m_textColor;
    std::atomic<int> m_frameCount;
    std::atomic<float> m_fps;
    int m_mode;
    std::string m_deviceName;
    int m_cpuCount;
    std::atomic<float> m_cpuUsage{-1.0f};
    std::atomic<int> m_cpuTemp{-1};
    int m_systemRAM;
    std::atomic<int> m_batteryPercent{-1};
    SDL_PowerState m_powerState;
    std::string m_gpuName{"Unknown"};
    std::atomic<float> m_gpuUsage{-1.0f};
    std::atomic<int> m_gpuTemp{-1};

    // Timing and stats
    std::chrono::high_resolution_clock::time_point m_lastTime;
    std::chrono::high_resolution_clock::time_point m_fpsUpdateTime;
    std::deque<double> m_frameTimes;  // ms, rolling window for benchmarks
    mutable std::mutex m_statsMutex;

    // Background monitoring
    std::unique_ptr<std::thread> m_monitorThread;
    std::atomic<bool> m_monitorStop;
    std::mutex m_monitorMutex;
    std::condition_variable m_monitorCV;
    std::atomic<bool> m_needsUpdate;

    // Platform-specific
#if defined(_WIN32)
    class WMIQuery {
    public:
        WMIQuery() {
            HRESULT hr = CoInitialize(nullptr);
            if (FAILED(hr)) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "CoInitialize failed: %ld", hr);
                return;
            }
            hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_IWbemLocator, (void**)&locator);
            if (FAILED(hr)) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "CoCreateInstance failed: %ld", hr);
                return;
            }
            hr = locator->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, nullptr, 0,
                                        nullptr, nullptr, &services);
            if (FAILED(hr)) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "ConnectServer failed: %ld", hr);
                locator->Release();
                locator = nullptr;
                return;
            }
            hr = CoSetProxyBlanket(services, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                                   RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
            if (FAILED(hr)) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "CoSetProxyBlanket failed: %ld", hr);
                services->Release();
                locator->Release();
                services = nullptr;
                locator = nullptr;
            }
        }

        ~WMIQuery() {
            if (services) services->Release();
            if (locator) locator->Release();
            CoUninitialize();
        }

        float getCPUUsage() {
            if (!services) return -1.0f;
            HRESULT hr;
            IEnumWbemClassObject* enumerator = nullptr;
            hr = services->ExecQuery(_bstr_t(L"WQL"),
                                     _bstr_t(L"SELECT PercentProcessorTime FROM Win32_PerfFormattedData_PerfOS_Processor WHERE Name='_Total'"),
                                     WBEM_FLAG_FORWARD_ONLY, nullptr, &enumerator);
            if (FAILED(hr)) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "WMI CPU query failed: %ld", hr);
                return -1.0f;
            }

            IWbemClassObject* obj = nullptr;
            ULONG returned = 0;
            float usage = -1.0f;
            if (SUCCEEDED(enumerator->Next(WBEM_INFINITE, 1, &obj, &returned)) && returned) {
                VARIANT value;
                VariantInit(&value);
                if (SUCCEEDED(obj->Get(L"PercentProcessorTime", 0, &value, nullptr, nullptr))) {
                    usage = static_cast<float>(value.uintVal);
                    VariantClear(&value);
                }
                obj->Release();
            }
            enumerator->Release();
            return usage;
        }

        int getCPUTemperature() {
            if (!services) return -1;
            HRESULT hr;
            IEnumWbemClassObject* enumerator = nullptr;
            hr = services->ExecQuery(_bstr_t(L"WQL"),
                                     _bstr_t(L"SELECT CurrentTemperature FROM MSAcpi_ThermalZoneTemperature"),
                                     WBEM_FLAG_FORWARD_ONLY, nullptr, &enumerator);
            if (FAILED(hr)) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "WMI temperature query failed: %ld", hr);
                return -1;
            }

            IWbemClassObject* obj = nullptr;
            ULONG returned = 0;
            int temp = -1;
            if (SUCCEEDED(enumerator->Next(WBEM_INFINITE, 1, &obj, &returned)) && returned) {
                VARIANT value;
                VariantInit(&value);
                if (SUCCEEDED(obj->Get(L"CurrentTemperature", 0, &value, nullptr, nullptr))) {
                    temp = value.uintVal / 10 - 273;  // Convert from deciKelvin to Celsius
                    VariantClear(&value);
                }
                obj->Release();
            }
            enumerator->Release();
            return temp;
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
#elif defined(__APPLE__)
    processor_cpu_load_info_t m_prevCpuInfo = nullptr;
    natural_t m_prevCpuCount = 0;
    mach_msg_type_number_t m_prevCpuInfoCount = 0;
#endif

#if USE_NVML
    bool m_nvmlInit = false;
    nvmlDevice_t m_device;
#endif

    /**
     * @brief Gets the device hostname.
     * @return Device name as a string.
     */
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

    /**
     * @brief Gets CPU usage as a percentage.
     * @return CPU usage (0-100) or -1 if unavailable.
     */
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
        host_t host = mach_host_self();
        processor_cpu_load_info_t cpuInfo;
        mach_msg_type_number_t count = 0;
        natural_t cpuCount;
        if (host_processor_info(host, PROCESSOR_CPU_LOAD_INFO, &cpuCount, (processor_info_array_t*)&cpuInfo, &count) == KERN_SUCCESS) {
            float usage = 0.0f;
            for (unsigned i = 0; i < cpuCount; ++i) {
                unsigned long user = cpuInfo[i].cpu_ticks[CPU_STATE_USER];
                unsigned long system = cpuInfo[i].cpu_ticks[CPU_STATE_SYSTEM];
                unsigned long idle = cpuInfo[i].cpu_ticks[CPU_STATE_IDLE];
                unsigned long total = user + system + idle;
                usage += (total > 0) ? 100.0f * (user + system) / total : 0.0f;
            }
            vm_deallocate(mach_task_self(), (vm_address_t)cpuInfo, count * sizeof(*cpuInfo));
            mach_host_self();
            return cpuCount > 0 ? usage / cpuCount : -1.0f;
        }
        return -1.0f;
#else
        return -1.0f;
#endif
    }

    /**
     * @brief Gets CPU temperature in Celsius.
     * @return CPU temperature or -1 if unavailable.
     */
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
        // Note: Requires SMC access via IOKit, not implemented here
        return -1;
#else
        return -1;
#endif
    }

    /**
     * @brief Initializes GPU information.
     */
    void initGPUInfo() {
        m_gpuName = "Unknown Device";
    }

    /**
     * @brief Background thread loop for monitoring system stats.
     */
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

            m_needsUpdate = false;
        }
    }
};

}  // namespace SDL3_FPS

#endif