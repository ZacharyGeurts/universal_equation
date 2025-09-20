#ifndef SDL3_FPS_COUNTER_HPP
#define SDL3_FPS_COUNTER_HPP

#include <SDL3/SDL.h>
#include <SDL3/SDL_ttf.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <stdexcept>

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

class FPSCounter {
public:
    FPSCounter(SDL_Window* window, TTF_Font* font) 
        : m_window(window), m_font(font), m_showFPS(false), m_frameCount(0), m_lastTime(SDL_GetTicks64()), m_mode(1) {
        if (!window || !font) {
            throw std::runtime_error("Invalid window or font pointer");
        }

        m_textColor = {255, 255, 255, 255}; // White text

        // Initialize static info
        m_deviceName = getDeviceName();
        m_cpuCount = SDL_GetCPUCount();
        m_systemRAM = SDL_GetSystemRAM();

        SDL_Renderer* renderer = SDL_GetRenderer(m_window);
        if (renderer) {
            SDL_RendererInfo info;
            if (SDL_GetRendererInfo(renderer, &info) == 0) {
                m_gpuName = info.name ? info.name : "Unknown";
            } else {
                m_gpuName = "Unknown";
            }
        } else {
            m_gpuName = "No Renderer";
        }

#if USE_NVML
        nvmlReturn_t result = nvmlInit_v2();
        if (result == NVML_SUCCESS) {
            m_nvmlInit = true;
            result = nvmlDeviceGetHandleByIndex(0, &m_device);
            if (result != NVML_SUCCESS) {
                m_nvmlInit = false;
            } else {
                char name[NVML_DEVICE_NAME_BUFFER_SIZE];
                if (nvmlDeviceGetName(m_device, name, sizeof(name)) == NVML_SUCCESS) {
                    m_gpuName = name; // Override with NVML name if available
                }
            }
        }
#endif
    }

    ~FPSCounter() {
#if USE_NVML
        if (m_nvmlInit) {
            nvmlShutdown();
        }
#endif
    }

    void handleEvent(const SDL_KeyboardEvent& key) {
        if (key.type == SDL_EVENT_KEY_DOWN && key.key == SDLK_F1) {
            m_showFPS = !m_showFPS; // Toggle FPS display on F1 press
        }
    }

    void setMode(int mode) {
        m_mode = mode; // Update the current render mode
    }

    void update() {
        m_frameCount++;
        Uint64 currentTime = SDL_GetTicks64();
        float deltaTime = (currentTime - m_lastTime) / 1000.0f;

        if (deltaTime >= 1.0f) { // Update every second
            m_fps = static_cast<int>(m_frameCount / deltaTime);
            m_frameCount = 0;
            m_lastTime = currentTime;

            // Update power info
            int seconds;
            m_batteryPercent = SDL_GetPowerInfo(&seconds, &m_powerState);

            // Update hardware stats
            updateHardwareStats();

            // Build text
            std::stringstream ss;
            ss << "Device: " << m_deviceName << " | CPU: " << m_cpuCount << " cores " 
               << std::fixed << std::setprecision(1) << (m_cpuUsage * 100.0f) << "% " << m_cpuTemp << "°C"
               << " | RAM: " << m_systemRAM << " MB"
               << " | GPU: " << m_gpuName;
#if USE_NVML
            if (m_nvmlInit) {
                ss << " " << std::fixed << std::setprecision(1) << (m_gpuUsage * 100.0f) << "% " << m_gpuTemp << "°C";
            }
#endif
            if (m_batteryPercent != -1) {
                ss << " | Battery: " << m_batteryPercent << "%";
            }
            ss << " | FPS: " << m_fps << " | Mode: " << m_mode << "D";

            m_fpsText = ss.str();

            // Create texture
            SDL_Surface* surface = TTF_RenderText_Blended(m_font, m_fpsText.c_str(), m_fpsText.length(), m_textColor);
            if (!surface) {
                throw std::runtime_error("TTF_RenderText_Blended failed: " + std::string(TTF_GetError()));
            }
            m_texture.reset(SDL_CreateTextureFromSurface(SDL_GetRenderer(m_window), surface));
            SDL_DestroySurface(surface);
            if (!m_texture) {
                throw std::runtime_error("SDL_CreateTextureFromSurface failed: " + std::string(SDL_GetError()));
            }

            // Get dimensions
            float w, h;
            SDL_GetTextureSize(m_texture.get(), &w, &h);
            m_destRect = {10, 10, w, h};
        }
    }

    void render() const {
        if (m_showFPS && m_texture) {
            SDL_Renderer* renderer = SDL_GetRenderer(m_window);
            if (renderer) {
                SDL_RenderTexture(renderer, m_texture.get(), nullptr, &m_destRect);
            }
        }
    }

private:
    struct SDLTextureDeleter {
        void operator()(SDL_Texture* texture) const { if (texture) SDL_DestroyTexture(texture); }
    };

    SDL_Window* m_window;
    TTF_Font* m_font;
    bool m_showFPS;
    int m_frameCount;
    Uint64 m_lastTime;
    int m_fps = 0;
    int m_mode;
    std::string m_fpsText;
    SDL_Color m_textColor;
    std::unique_ptr<SDL_Texture, SDLTextureDeleter> m_texture;
    SDL_FRect m_destRect;

    // Additional info
    std::string m_deviceName;
    int m_cpuCount;
    int m_systemRAM;
    int m_batteryPercent = -1;
    SDL_PowerState m_powerState;
    float m_cpuUsage = 0.0f;
    float m_cpuTemp = 0.0f;
    std::string m_gpuName;
    float m_gpuUsage = 0.0f;
    float m_gpuTemp = 0.0f;

#if USE_NVML
    bool m_nvmlInit = false;
    nvmlDevice_t m_device;
#endif

    std::string getDeviceName() {
#if defined(_WIN32)
        char computerName[MAX_COMPUTERNAME_LENGTH + 1];
        DWORD size = sizeof(computerName);
        if (GetComputerNameA(computerName, &size)) {
            return std::string(computerName);
        }
        return "Unknown";
#elif defined(__linux__) || defined(__APPLE__)
        char hostname[256];
        if (gethostname(hostname, sizeof(hostname)) == 0) {
            return std::string(hostname);
        }
        return "Unknown";
#else
        return "Unknown";
#endif
    }

    void updateHardwareStats() {
#if defined(_WIN32)
        m_cpuTemp = getCpuTempWindows();
        m_cpuUsage = getCpuUsageWindows();
#elif defined(__linux__)
        m_cpuTemp = getCpuTempLinux();
        m_cpuUsage = getCpuUsageLinux();
#elif defined(__APPLE__)
        m_cpuTemp = getCpuTempMacOS();
        m_cpuUsage = getCpuUsageMacOS();
#endif

#if USE_NVML
        if (m_nvmlInit) {
            unsigned int temp;
            if (nvmlDeviceGetTemperature(m_device, NVML_TEMPERATURE_GPU, &temp) == NVML_SUCCESS) {
                m_gpuTemp = static_cast<float>(temp);
            }
            nvmlUtilization_t util;
            if (nvmlDeviceGetUtilizationRates(m_device, &util) == NVML_SUCCESS) {
                m_gpuUsage = util.gpu / 100.0f;
            }
        }
#endif
    }

#if defined(_WIN32)
    float getCpuUsageWindows() {
        static unsigned long long _previousTotalTicks = 0;
        static unsigned long long _previousIdleTicks = 0;

        FILETIME idleTime, kernelTime, userTime;
        if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
            return 0.0f;
        }

        auto FileTimeToInt64 = [](const FILETIME& ft) -> unsigned long long {
            return (((unsigned long long)(ft.dwHighDateTime)) << 32) | ((unsigned long long)ft.dwLowDateTime);
        };

        unsigned long long idleTicks = FileTimeToInt64(idleTime);
        unsigned long long totalTicks = FileTimeToInt64(kernelTime) + FileTimeToInt64(userTime);

        unsigned long long totalTicksSinceLastTime = totalTicks - _previousTotalTicks;
        unsigned long long idleTicksSinceLastTime = idleTicks - _previousIdleTicks;

        float ret = 1.0f - ((totalTicksSinceLastTime > 0) ? static_cast<float>(idleTicksSinceLastTime) / totalTicksSinceLastTime : 0.0f);

        _previousTotalTicks = totalTicks;
        _previousIdleTicks = idleTicks;
        return ret;
    }

    float getCpuTempWindows() {
        LONG temp = -1;
        HRESULT hr = GetCpuTemperatureWindows(&temp);
        if (SUCCEEDED(hr) && temp != -1) {
            return static_cast<float>(temp) / 10.0f - 273.15f; // Convert from deci-Kelvin to Celsius
        }
        return 0.0f;
    }

    HRESULT GetCpuTemperatureWindows(LPLONG pTemperature) {
        if (!pTemperature) return E_INVALIDARG;

        *pTemperature = -1;
        HRESULT ci = CoInitialize(NULL);
        HRESULT hr = CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
        if (FAILED(hr)) {
            if (ci == S_OK) CoUninitialize();
            return hr;
        }

        IWbemLocator* pLocator = nullptr;
        hr = CoCreateInstance(CLSID_WbemAdministrativeLocator, NULL, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&pLocator);
        if (SUCCEEDED(hr)) {
            IWbemServices* pServices = nullptr;
            BSTR ns = SysAllocString(L"root\\WMI");
            hr = pLocator->ConnectServer(ns, NULL, NULL, NULL, 0, NULL, NULL, &pServices);
            pLocator->Release();
            SysFreeString(ns);
            if (SUCCEEDED(hr)) {
                BSTR query = SysAllocString(L"SELECT * FROM MSAcpi_ThermalZoneTemperature");
                BSTR wql = SysAllocString(L"WQL");
                IEnumWbemClassObject* pEnum = nullptr;
                hr = pServices->ExecQuery(wql, query, WBEM_FLAG_RETURN_IMMEDIATELY | WBEM_FLAG_FORWARD_ONLY, NULL, &pEnum);
                SysFreeString(wql);
                SysFreeString(query);
                pServices->Release();
                if (SUCCEEDED(hr)) {
                    IWbemClassObject* pObject = nullptr;
                    ULONG returned = 0;
                    hr = pEnum->Next(WBEM_INFINITE, 1, &pObject, &returned);
                    pEnum->Release();
                    if (SUCCEEDED(hr) && returned > 0) {
                        BSTR tempStr = SysAllocString(L"CurrentTemperature");
                        VARIANT v;
                        VariantInit(&v);
                        hr = pObject->Get(tempStr, 0, &v, NULL, NULL);
                        pObject->Release();
                        SysFreeString(tempStr);
                        if (SUCCEEDED(hr) && v.vt == VT_I4) {
                            *pTemperature = V_I4(&v);
                        }
                        VariantClear(&v);
                    }
                }
            }
            if (ci == S_OK) {
                CoUninitialize();
            }
        }
        return hr;
    }
#endif

#if defined(__linux__)
    float getCpuUsageLinux() {
        std::ifstream stat("/proc/stat");
        if (!stat) return 0.0f;

        std::string line;
        std::getline(stat, line);
        std::istringstream iss(line);
        std::string cpu;
        iss >> cpu;

        long long user, nice, system, idle, iowait, irq, softirq, steal = 0;
        iss >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;

        long long idle_all = idle + iowait;
        long long total_all = user + nice + system + idle_all + irq + softirq + steal;

        static long long prev_total = 0;
        static long long prev_idle = 0;

        float usage = 0.0f;
        if (prev_total > 0) {
            long long delta_total = total_all - prev_total;
            long long delta_idle = idle_all - prev_idle;
            usage = static_cast<float>(delta_total - delta_idle) / static_cast<float>(delta_total);
        }

        prev_total = total_all;
        prev_idle = idle_all;
        return usage;
    }

    float getCpuTempLinux() {
        // Assuming thermal_zone0 is the CPU temp; may need adjustment
        std::ifstream f("/sys/class/thermal/thermal_zone0/temp");
        if (!f) return 0.0f;

        int temp;
        f >> temp;
        return static_cast<float>(temp) / 1000.0f;
    }
#endif

#if defined(__APPLE__)
    float getCpuUsageMacOS() {
        // Basic implementation; may need more sophisticated approach
        return 0.0f; // Placeholder
    }

    float getCpuTempMacOS() {
        // Note: macOS CPU temperature reading requires SMC access, which is complex
        // This is a placeholder; implement with SMC calls if needed
        return 0.0f;
    }
#endif
};

#endif // SDL3_FPS_COUNTER_HPP