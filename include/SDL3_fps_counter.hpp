#ifndef SDL3_FPS_COUNTER_HPP
#define SDL3_FPS_COUNTER_HPP

#include <SDL3/SDL.h>
#include <SDL3/SDL_ttf.h>
#include <SDL3/SDL_gpu.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <vector>
#include <memory>
#include <iostream>

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
    FPSCounter(SDL_Window* window, TTF_Font* font, SDL_GPUDevice* gpu_device = nullptr) 
        : m_window(window), m_font(font), m_gpu_device(gpu_device), m_showFPS(false), m_frameCount(0), m_lastTime(SDL_GetTicks64()), m_mode(1) {
        std::cout << "Constructing FPSCounter" << std::endl;
        if (!window || !font) {
            std::cout << "Invalid window or font pointer detected" << std::endl;
            throw std::runtime_error("Invalid window or font pointer");
        }

        std::cout << "Setting text color to white" << std::endl;
        m_textColor = {255, 255, 255, 255}; // White text

        // Initialize static info
        std::cout << "Getting device name" << std::endl;
        m_deviceName = getDeviceName();
        std::cout << "Getting CPU core count" << std::endl;
        m_cpuCount = SDL_GetNumLogicalCPUCores();
        std::cout << "Getting system RAM" << std::endl;
        m_systemRAM = SDL_GetSystemRAM();

        // Initialize GPU info
        std::cout << "Initializing GPU information" << std::endl;
        initGPUInfo();

#if USE_NVML
        std::cout << "Initializing NVML" << std::endl;
        nvmlReturn_t result = nvmlInit_v2();
        if (result == NVML_SUCCESS) {
            std::cout << "NVML initialized successfully" << std::endl;
            m_nvmlInit = true;
            std::cout << "Getting NVML device handle" << std::endl;
            result = nvmlDeviceGetHandleByIndex(0, &m_device);
            if (result != NVML_SUCCESS) {
                std::cout << "Failed to get NVML device handle: " << result << std::endl;
                m_nvmlInit = false;
            } else {
                char name[NVML_DEVICE_NAME_BUFFER_SIZE];
                std::cout << "Getting NVML device name" << std::endl;
                if (nvmlDeviceGetName(m_device, name, sizeof(name)) == NVML_SUCCESS) {
                    std::cout << "NVML device name: " << name << std::endl;
                    m_gpuName = name; // Override with NVML name if available
                } else {
                    std::cout << "Failed to get NVML device name" << std::endl;
                }
            }
        } else {
            std::cout << "NVML initialization failed: " << result << std::endl;
        }
#endif
    }

    ~FPSCounter() {
        std::cout << "Destructing FPSCounter" << std::endl;
#if USE_NVML
        if (m_nvmlInit) {
            std::cout << "Shutting down NVML" << std::endl;
            nvmlShutdown();
        }
#endif
    }

    void handleEvent(const SDL_KeyboardEvent& key) {
        std::cout << "Handling keyboard event in FPSCounter" << std::endl;
        if (key.type == SDL_EVENT_KEY_DOWN && key.key == SDLK_F1) {
            std::cout << "F1 key pressed, toggling FPS display from " << m_showFPS << " to " << !m_showFPS << std::endl;
            m_showFPS = !m_showFPS; // Toggle FPS display on F1 press
        }
    }

    void setMode(int mode) {
        std::cout << "Setting render mode to " << mode << "D" << std::endl;
        m_mode = mode; // Update the current render mode
    }

    void update() {
        std::cout << "Updating FPSCounter, frame count: " << m_frameCount << std::endl;
        m_frameCount++;
        Uint64 currentTime = SDL_GetTicks64();
        float deltaTime = (currentTime - m_lastTime) / 1000.0f;

        if (deltaTime >= 1.0f) { // Update every second
            std::cout << "Calculating FPS, deltaTime: " << deltaTime << std::endl;
            m_fps = static_cast<int>(m_frameCount / deltaTime);
            m_frameCount = 0;
            m_lastTime = currentTime;

            // Update power info
            std::cout << "Getting power info" << std::endl;
            int seconds;
            m_batteryPercent = SDL_GetPowerInfo(&seconds, &m_powerState);
            std::cout << "Battery percent: " << m_batteryPercent << ", Power state: " << m_powerState << std::endl;

            // Update hardware stats
            std::cout << "Updating hardware stats" << std::endl;
            updateHardwareStats();

            // Build text
            std::cout << "Building FPS text string" << std::endl;
            std::stringstream ss;
            ss << "Device: " << m_deviceName << " | CPU: " << m_cpuCount << " cores " 
               << std::fixed << std::setprecision(1) << (m_cpuUsage * 100.0f) << "% " << m_cpuTemp << "°C"
               << " | RAM: " << m_systemRAM << " MB"
               << " | GPU: " << m_gpuName;
            if (!m_gpuVendor.empty()) {
                ss << " (" << m_gpuVendor << ")";
            }
#if USE_NVML
            if (m_nvmlInit) {
                ss << " " << std::fixed << std::setprecision(1) << (m_gpuUsage * 100.0f) << "% " << m_gpuTemp << "°C";
            }
#endif
            if (!m_gpuDriver.empty()) {
                ss << " | Driver: " << m_gpuDriver;
            }
            if (m_batteryPercent != -1) {
                ss << " | Battery: " << m_batteryPercent << "%";
            }
            ss << " | FPS: " << m_fps << " | Mode: " << m_mode << "D";

            m_fpsText = ss.str();
            std::cout << "FPS text: " << m_fpsText << std::endl;

            // Create texture
            std::cout << "Creating text surface" << std::endl;
            SDL_Surface* surface = TTF_RenderText_Blended(m_font, m_fpsText.c_str(), m_fpsText.length(), m_textColor);
            if (!surface) {
                std::cout << "TTF_RenderText_Blended failed: " << TTF_GetError() << std::endl;
                throw std::runtime_error("TTF_RenderText_Blended failed: " + std::string(TTF_GetError()));
            }
            std::cout << "Creating texture from surface" << std::endl;
            m_texture.reset(SDL_CreateTextureFromSurface(SDL_GetRenderer(m_window), surface));
            SDL_DestroySurface(surface);
            if (!m_texture) {
                std::cout << "SDL_CreateTextureFromSurface failed: " << SDL_GetError() << std::endl;
                throw std::runtime_error("SDL_CreateTextureFromSurface failed: " + std::string(SDL_GetError()));
            }

            // Get dimensions
            std::cout << "Getting texture dimensions" << std::endl;
            float w, h;
            SDL_GetTextureSize(m_texture.get(), &w, &h);
            m_destRect = {10, 10, w, h};
            std::cout << "Texture dimensions: " << w << "x" << h << std::endl;
        }
    }

    void render() const {
        std::cout << "Rendering FPSCounter, showFPS: " << m_showFPS << std::endl;
        if (m_showFPS && m_texture) {
            SDL_Renderer* renderer = SDL_GetRenderer(m_window);
            if (renderer) {
                std::cout << "Rendering FPS texture" << std::endl;
                SDL_RenderTexture(renderer, m_texture.get(), nullptr, &m_destRect);
            } else {
                std::cout << "No renderer available for FPS rendering" << std::endl;
            }
        }
    }

private:
    struct SDLTextureDeleter {
        void operator()(SDL_Texture* texture) const { 
            std::cout << "Destroying SDL texture" << std::endl;
            if (texture) SDL_DestroyTexture(texture);
        }
    };

    SDL_Window* m_window;
    TTF_Font* m_font;
    SDL_GPUDevice* m_gpu_device;
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
    std::string m_gpuVendor;
    std::string m_gpuDriver;
    float m_gpuUsage = 0.0f;
    float m_gpuTemp = 0.0f;

#if USE_NVML
    bool m_nvmlInit = false;
    nvmlDevice_t m_device;
#endif

    void initGPUInfo() {
        std::cout << "Initializing GPU info" << std::endl;
        if (m_gpu_device) {
            // Use SDL3 GPU API to get detailed GPU properties
            std::cout << "Getting GPU device properties" << std::endl;
            SDL_GPUDeviceProperties properties;
            if (SDL_GetGPUDeviceProperties(m_gpu_device, &properties) == SDL_SUCCESS) {
                m_gpuName = properties.name ? properties.name : "Unknown GPU";
                std::cout << "GPU name: " << m_gpuName << std::endl;
                m_gpuVendor = getVendorString(properties.vendor_id);
                std::cout << "GPU vendor: " << m_gpuVendor << std::endl;
                char driver_version[256];
                if (SDL_GetGPUDeviceDriverVersion(m_gpu_device, driver_version, sizeof(driver_version)) == SDL_SUCCESS) {
                    m_gpuDriver = driver_version;
                    std::cout << "GPU driver version: " << m_gpuDriver << std::endl;
                } else {
                    std::cout << "Failed to get GPU driver version" << std::endl;
                }
            } else {
                std::cout << "SDL_GetGPUDeviceProperties failed, falling back to renderer info" << std::endl;
                // Fallback to renderer info
                SDL_Renderer* renderer = SDL_GetRenderer(m_window);
                if (renderer) {
                    SDL_RendererInfo info;
                    if (SDL_GetRendererInfo(renderer, &info) == 0) {
                        m_gpuName = info.name ? info.name : "Unknown";
                        std::cout << "Renderer name: " << m_gpuName << std::endl;
                    } else {
                        std::cout << "SDL_GetRendererInfo failed" << std::endl;
                        m_gpuName = "Unknown";
                    }
                } else {
                    std::cout << "No renderer available" << std::endl;
                    m_gpuName = "No Renderer";
                }
            }
        } else {
            std::cout << "No GPU device provided, using renderer info" << std::endl;
            // Fallback if no GPU device provided
            SDL_Renderer* renderer = SDL_GetRenderer(m_window);
            if (renderer) {
                SDL_RendererInfo info;
                if (SDL_GetRendererInfo(renderer, &info) == 0) {
                    m_gpuName = info.name ? info.name : "Unknown";
                    std::cout << "Renderer name: " << m_gpuName << std::endl;
                } else {
                    std::cout << "SDL_GetRendererInfo failed" << std::endl;
                    m_gpuName = "Unknown";
                }
            } else {
                std::cout << "No renderer available" << std::endl;
                m_gpuName = "No Renderer";
            }
        }
    }

    std::string getVendorString(uint32_t vendor_id) {
        std::cout << "Mapping vendor ID: " << vendor_id << std::endl;
        switch (vendor_id) {
            case 0x10DE: return "NVIDIA";
            case 0x8086: return "Intel";
            case 0x1002: return "AMD";
            default: return "Unknown";
        }
    }

    std::string getDeviceName() {
        std::cout << "Getting device name" << std::endl;
#if defined(_WIN32)
        char computerName[MAX_COMPUTERNAME_LENGTH + 1];
        DWORD size = sizeof(computerName);
        if (GetComputerNameA(computerName, &size)) {
            std::cout << "Device name (Windows): " << computerName << std::endl;
            return std::string(computerName);
        }
        std::cout << "Failed to get device name (Windows)" << std::endl;
        return "Unknown";
#elif defined(__linux__) || defined(__APPLE__)
        char hostname[256];
        if (gethostname(hostname, sizeof(hostname)) == 0) {
            std::cout << "Device name (Linux/macOS): " << hostname << std::endl;
            return std::string(hostname);
        }
        std::cout << "Failed to get device name (Linux/macOS)" << std::endl;
        return "Unknown";
#else
        std::cout << "Device name not supported on this platform" << std::endl;
        return "Unknown";
#endif
    }

    void updateHardwareStats() {
        std::cout << "Updating hardware stats" << std::endl;
#if defined(_WIN32)
        std::cout << "Getting CPU temperature (Windows)" << std::endl;
        m_cpuTemp = getCpuTempWindows();
        std::cout << "CPU temperature: " << m_cpuTemp << "°C" << std::endl;
        std::cout << "Getting CPU usage (Windows)" << std::endl;
        m_cpuUsage = getCpuUsageWindows();
        std::cout << "CPU usage: " << (m_cpuUsage * 100.0f) << "%" << std::endl;
#elif defined(__linux__)
        std::cout << "Getting CPU temperature (Linux)" << std::endl;
        m_cpuTemp = getCpuTempLinux();
        std::cout << "CPU temperature: " << m_cpuTemp << "°C" << std::endl;
        std::cout << "Getting CPU usage (Linux)" << std::endl;
        m_cpuUsage = getCpuUsageLinux();
        std::cout << "CPU usage: " << (m_cpuUsage * 100.0f) << "%" << std::endl;
#elif defined(__APPLE__)
        std::cout << "Getting CPU temperature (macOS)" << std::endl;
        m_cpuTemp = getCpuTempMacOS();
        std::cout << "CPU temperature: " << m_cpuTemp << "°C" << std::endl;
        std::cout << "Getting CPU usage (macOS)" << std::endl;
        m_cpuUsage = getCpuUsageMacOS();
        std::cout << "CPU usage: " << (m_cpuUsage * 100.0f) << "%" << std::endl;
#endif

#if USE_NVML
        if (m_nvmlInit) {
            std::cout << "Getting NVML GPU temperature" << std::endl;
            unsigned int temp;
            if (nvmlDeviceGetTemperature(m_device, NVML_TEMPERATURE_GPU, &temp) == NVML_SUCCESS) {
                m_gpuTemp = static_cast<float>(temp);
                std::cout << "GPU temperature: " << m_gpuTemp << "°C" << std::endl;
            } else {
                std::cout << "Failed to get NVML GPU temperature" << std::endl;
            }
            std::cout << "Getting NVML GPU utilization" << std::endl;
            nvmlUtilization_t util;
            if (nvmlDeviceGetUtilizationRates(m_device, &util) == NVML_SUCCESS) {
                m_gpuUsage = util.gpu / 100.0f;
                std::cout << "GPU usage: " << (m_gpuUsage * 100.0f) << "%" << std::endl;
            } else {
                std::cout << "Failed to get NVML GPU utilization" << std::endl;
            }
        }
#endif
    }

#if defined(_WIN32)
    float getCpuUsageWindows() {
        std::cout << "Calculating CPU usage (Windows)" << std::endl;
        static unsigned long long _previousTotalTicks = 0;
        static unsigned long long _previousIdleTicks = 0;

        FILETIME idleTime, kernelTime, userTime;
        if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
            std::cout << "GetSystemTimes failed" << std::endl;
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
        std::cout << "CPU usage calculated: " << (ret * 100.0f) << "%" << std::endl;
        return ret;
    }

    float getCpuTempWindows() {
        std::cout << "Querying CPU temperature (Windows)" << std::endl;
        LONG temp = -1;
        HRESULT hr = GetCpuTemperatureWindows(&temp);
        if (SUCCEEDED(hr) && temp != -1) {
            float celsius = static_cast<float>(temp) / 10.0f - 273.15f; // Convert from deci-Kelvin to Celsius
            std::cout << "CPU temperature retrieved: " << celsius << "°C" << std::endl;
            return celsius;
        }
        std::cout << "Failed to retrieve CPU temperature" << std::endl;
        return 0.0f;
    }

    HRESULT GetCpuTemperatureWindows(LPLONG pTemperature) {
        std::cout << "Initializing COM for CPU temperature query" << std::endl;
        if (!pTemperature) {
            std::cout << "Invalid temperature pointer" << std::endl;
            return E_INVALIDARG;
        }

        *pTemperature = -1;
        HRESULT ci = CoInitialize(NULL);
        HRESULT hr = CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
        if (FAILED(hr)) {
            std::cout << "CoInitializeSecurity failed: " << hr << std::endl;
            if (ci == S_OK) CoUninitialize();
            return hr;
        }

        IWbemLocator* pLocator = nullptr;
        std::cout << "Creating WBEM locator" << std::endl;
        hr = CoCreateInstance(CLSID_WbemAdministrativeLocator, NULL, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&pLocator);
        if (SUCCEEDED(hr)) {
            IWbemServices* pServices = nullptr;
            BSTR ns = SysAllocString(L"root\\WMI");
            std::cout << "Connecting to WMI service" << std::endl;
            hr = pLocator->ConnectServer(ns, NULL, NULL, NULL, 0, NULL, NULL, &pServices);
            pLocator->Release();
            SysFreeString(ns);
            if (SUCCEEDED(hr)) {
                BSTR query = SysAllocString(L"SELECT * FROM MSAcpi_ThermalZoneTemperature");
                BSTR wql = SysAllocString(L"WQL");
                IEnumWbemClassObject* pEnum = nullptr;
                std::cout << "Executing WMI query" << std::endl;
                hr = pServices->ExecQuery(wql, query, WBEM_FLAG_RETURN_IMMEDIATELY | WBEM_FLAG_FORWARD_ONLY, NULL, &pEnum);
                SysFreeString(wql);
                SysFreeString(query);
                pServices->Release();
                if (SUCCEEDED(hr)) {
                    IWbemClassObject* pObject = nullptr;
                    ULONG returned = 0;
                    std::cout << "Fetching WMI query results" << std::endl;
                    hr = pEnum->Next(WBEM_INFINITE, 1, &pObject, &returned);
                    pEnum->Release();
                    if (SUCCEEDED(hr) && returned > 0) {
                        BSTR tempStr = SysAllocString(L"CurrentTemperature");
                        VARIANT v;
                        VariantInit(&v);
                        std::cout << "Getting temperature property" << std::endl;
                        hr = pObject->Get(tempStr, 0, &v, NULL, NULL);
                        pObject->Release();
                        SysFreeString(tempStr);
                        if (SUCCEEDED(hr) && v.vt == VT_I4) {
                            *pTemperature = V_I4(&v);
                            std::cout << "Temperature retrieved: " << *pTemperature << std::endl;
                        }
                        VariantClear(&v);
                    } else {
                        std::cout << "No WMI query results" << std::endl;
                    }
                } else {
                    std::cout << "WMI query execution failed" << std::endl;
                }
            } else {
                std::cout << "WMI connection failed" << std::endl;
            }
            if (ci == S_OK) {
                std::cout << "Uninitializing COM" << std::endl;
                CoUninitialize();
            }
        } else {
            std::cout << "CoCreateInstance failed" << std::endl;
        }
        return hr;
    }
#endif

#if defined(__linux__)
    float getCpuUsageLinux() {
        std::cout << "Reading /proc/stat for CPU usage (Linux)" << std::endl;
        std::ifstream stat("/proc/stat");
        if (!stat) {
            std::cout << "Failed to open /proc/stat" << std::endl;
            return 0.0f;
        }

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
            std::cout << "CPU usage calculated: " << (usage * 100.0f) << "%" << std::endl;
        } else {
            std::cout << "Initial CPU usage measurement, no previous data" << std::endl;
        }

        prev_total = total_all;
        prev_idle = idle_all;
        return usage;
    }

    float getCpuTempLinux() {
        std::cout << "Reading CPU temperature from thermal_zone0 (Linux)" << std::endl;
        std::ifstream f("/sys/class/thermal/thermal_zone0/temp");
        if (!f) {
            std::cout << "Failed to open thermal_zone0" << std::endl;
            return 0.0f;
        }

        int temp;
        f >> temp;
        float celsius = static_cast<float>(temp) / 1000.0f;
        std::cout << "CPU temperature: " << celsius << "°C" << std::endl;
        return celsius;
    }
#endif

#if defined(__APPLE__)
    float getCpuUsageMacOS() {
        std::cout << "Placeholder for CPU usage (macOS)" << std::endl;
        // Basic implementation; may need more sophisticated approach
        return 0.0f; // Placeholder
    }

    float getCpuTempMacOS() {
        std::cout << "Placeholder for CPU temperature (macOS)" << std::endl;
        // Note: macOS CPU temperature reading requires SMC access, which is complex
        // This is a placeholder; implement with SMC calls if needed
        return 0.0f;
    }
#endif
};

#endif // SDL3_FPS_COUNTER_HPP