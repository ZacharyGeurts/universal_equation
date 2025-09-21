#ifndef SDL3_FPS_COUNTER_HPP
#define SDL3_FPS_COUNTER_HPP

#include <SDL3/SDL.h>
#include <SDL3/SDL_ttf.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <memory>
#include <iostream>
#include <vulkan/vulkan.h>

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
    FPSCounter(SDL_Window* window, TTF_Font* font, SDL_Renderer* renderer = nullptr)
        : m_window(window), m_font(font), m_renderer(renderer), m_showFPS(false), m_frameCount(0), m_lastTime(SDL_GetTicks64()), m_mode(1) {
        std::cout << "Constructing FPSCounter" << std::endl;
        if (!window || !font) {
            std::cout << "Invalid window or font pointer detected" << std::endl;
            throw std::runtime_error("Invalid window or font pointer");
        }
        if (!renderer) {
            std::cout << "Warning: No renderer provided, FPS counter will not render" << std::endl;
        }

        std::cout << "Setting text color to white" << std::endl;
        m_textColor = {255, 255, 255, 255}; // White text

        // Initialize static info using SDL3 functions
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
        nvmlReturn_t result = nvmlInit();
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
        if (m_textTexture) {
            std::cout << "Destroying text texture" << std::endl;
            SDL_DestroyTexture(m_textTexture);
        }
#if USE_NVML
        if (m_nvmlInit) {
            std::cout << "Shutting down NVML" << std::endl;
            nvmlShutdown();
        }
#endif
    }

    void handleEvent(const SDL_KeyboardEvent& key) {
        if (key.type == SDL_EVENT_KEY_DOWN && key.key == SDLK_F1) {
            std::cout << "F1 pressed, toggling FPS display" << std::endl;
            m_showFPS = !m_showFPS;
        }
    }

    void update() {
        std::cout << "Updating FPSCounter" << std::endl;
        m_frameCount++;
        Uint64 currentTime = SDL_GetTicks64();
        if (currentTime - m_lastTime >= 1000) {
            std::cout << "Calculating FPS" << std::endl;
            m_fps = m_frameCount * 1000.0f / (currentTime - m_lastTime);
            m_frameCount = 0;
            m_lastTime = currentTime;

            // Update dynamic info
            std::cout << "Getting CPU usage" << std::endl;
            m_cpuUsage = getCPUUsage();
            std::cout << "Getting CPU temperature" << std::endl;
            m_cpuTemp = getCPUTemperature();
            std::cout << "Getting battery info" << std::endl;
            int seconds, percent;
            m_batteryPercent = SDL_GetPowerInfo(&seconds, &percent);
            m_batteryPercent = percent;

#if USE_NVML
            if (m_nvmlInit) {
                std::cout << "Getting GPU usage via NVML" << std::endl;
                unsigned int util;
                if (nvmlDeviceGetUtilizationRates(m_device, &util) == NVML_SUCCESS) {
                    m_gpuUsage = util.gpu;
                    std::cout << "GPU usage: " << m_gpuUsage << "%" << std::endl;
                }
                std::cout << "Getting GPU temperature via NVML" << std::endl;
                unsigned int temp;
                if (nvmlDeviceGetTemperature(m_device, NVML_TEMPERATURE_GPU, &temp) == NVML_SUCCESS) {
                    m_gpuTemp = temp;
                    std::cout << "GPU temperature: " << m_gpuTemp << "C" << std::endl;
                }
            }
#endif

            // Update text
            std::cout << "Generating FPS text" << std::endl;
            std::stringstream ss;
            ss << "Device: " << m_deviceName << "\n"
               << "CPU: " << m_cpuCount << " cores, " << std::fixed << std::setprecision(1) << m_cpuUsage << "%"
               << (m_cpuTemp >= 0 ? ", " + std::to_string(m_cpuTemp) + "C" : "") << "\n"
               << "GPU: " << m_gpuName
               << (m_gpuUsage >= 0 ? ", " + std::to_string(m_gpuUsage) + "%" : "")
               << (m_gpuTemp >= 0 ? ", " + std::to_string(m_gpuTemp) + "C" : "") << "\n"
               << "RAM: " << m_systemRAM << "MB\n"
               << "Battery: " << (m_batteryPercent >= 0 ? std::to_string(m_batteryPercent) + "%" : "N/A") << "\n"
               << "FPS: " << std::fixed << std::setprecision(1) << m_fps << "\n"
               << "Mode: " << m_mode << "D";

            if (m_renderer) {
                std::cout << "Creating text surface" << std::endl;
                SDL_Surface* surface = TTF_RenderText_Blended_Wrapped(m_font, ss.str().c_str(), m_textColor, 0);
                if (!surface) {
                    std::cout << "TTF_RenderText_Blended_Wrapped failed: " << SDL_GetError() << std::endl;
                    return;
                }
                std::cout << "Creating text texture" << std::endl;
                if (m_textTexture) {
                    std::cout << "Destroying old text texture" << std::endl;
                    SDL_DestroyTexture(m_textTexture);
                }
                m_textTexture = SDL_CreateTextureFromSurface(m_renderer, surface);
                if (!m_textTexture) {
                    std::cout << "SDL_CreateTextureFromSurface failed: " << SDL_GetError() << std::endl;
                    SDL_DestroySurface(surface);
                    return;
                }
                std::cout << "Querying text texture dimensions" << std::endl;
                SDL_QueryTexture(m_textTexture, nullptr, nullptr, &m_textRect.w, &m_textRect.h);
                m_textRect.x = 10;
                m_textRect.y = 10;
                SDL_DestroySurface(surface);
            }
        }
    }

    void render() {
        if (!m_showFPS || !m_renderer || !m_textTexture) {
            std::cout << "Skipping FPS render: " << (!m_showFPS ? "FPS hidden" : !m_renderer ? "No renderer" : "No text texture") << std::endl;
            return;
        }
        std::cout << "Rendering FPS counter" << std::endl;
        SDL_RenderTexture(m_renderer, m_textTexture, nullptr, &m_textRect);
    }

    void setMode(int mode) {
        std::cout << "Setting FPS counter mode to " << mode << std::endl;
        m_mode = mode;
    }

private:
    SDL_Window* m_window;
    TTF_Font* m_font;
    SDL_Renderer* m_renderer;
    SDL_Texture* m_textTexture = nullptr;
    SDL_FRect m_textRect = {10, 10, 0, 0};
    SDL_Color m_textColor;
    bool m_showFPS;
    int m_frameCount;
    Uint64 m_lastTime;
    float m_fps = 0.0f;
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
#if USE_NVML
    bool m_nvmlInit = false;
    nvmlDevice_t m_device;
#endif

    std::string getDeviceName() {
#if defined(_WIN32)
        std::cout << "Getting device name on Windows" << std::endl;
        char name[256];
        DWORD size = sizeof(name);
        if (GetComputerNameA(name, &size)) {
            return std::string(name);
        }
        return "Unknown";
#elif defined(__linux__)
        std::cout << "Getting device name on Linux" << std::endl;
        std::ifstream ifs("/proc/sys/kernel/hostname");
        std::string name;
        if (ifs && std::getline(ifs, name)) {
            return name;
        }
        return "Unknown";
#elif defined(__APPLE__)
        std::cout << "Getting device name on macOS" << std::endl;
        char name[256];
        size_t size = sizeof(name);
        if (sysctlbyname("kern.hostname", name, &size, nullptr, 0) == 0) {
            return std::string(name);
        }
        return "Unknown";
#else
        std::cout << "Device name not supported on this platform" << std::endl;
        return "Unknown";
#endif
    }

    float getCPUUsage() {
#if defined(_WIN32)
        std::cout << "Getting CPU usage on Windows" << std::endl;
        HRESULT hr = CoInitialize(nullptr);
        if (FAILED(hr)) {
            std::cout << "CoInitialize failed: " << hr << std::endl;
            return -1.0f;
        }
        IWbemLocator* locator = nullptr;
        hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (void**)&locator);
        if (FAILED(hr)) {
            std::cout << "CoCreateInstance failed: " << hr << std::endl;
            CoUninitialize();
            return -1.0f;
        }
        IWbemServices* services = nullptr;
        hr = locator->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, nullptr, 0, nullptr, nullptr, &services);
        locator->Release();
        if (FAILED(hr)) {
            std::cout << "ConnectServer failed: " << hr << std::endl;
            CoUninitialize();
            return -1.0f;
        }
        hr = CoSetProxyBlanket(services, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
        if (FAILED(hr)) {
            std::cout << "CoSetProxyBlanket failed: " << hr << std::endl;
            services->Release();
            CoUninitialize();
            return -1.0f;
        }
        IEnumWbemClassObject* enumerator = nullptr;
        hr = services->ExecQuery(_bstr_t(L"WQL"), _bstr_t(L"SELECT PercentProcessorTime FROM Win32_PerfFormattedData_PerfOS_Processor WHERE Name='_Total'"), WBEM_FLAG_FORWARD_ONLY, nullptr, &enumerator);
        if (FAILED(hr)) {
            std::cout << "ExecQuery failed: " << hr << std::endl;
            services->Release();
            CoUninitialize();
            return -1.0f;
        }
        IWbemClassObject* obj = nullptr;
        ULONG returned = 0;
        hr = enumerator->Next(WBEM_INFINITE, 1, &obj, &returned);
        enumerator->Release();
        services->Release();
        if (FAILED(hr) || returned == 0) {
            std::cout << "Enumerator Next failed: " << hr << std::endl;
            CoUninitialize();
            return -1.0f;
        }
        VARIANT vt;
        VariantInit(&vt);
        hr = obj->Get(L"PercentProcessorTime", 0, &vt, nullptr, nullptr);
        float usage = -1.0f;
        if (SUCCEEDED(hr) && vt.vt == VT_BSTR) {
            usage = static_cast<float>(_wtof(vt.bstrVal));
        }
        VariantClear(&vt);
        obj->Release();
        CoUninitialize();
        std::cout << "CPU usage: " << usage << "%" << std::endl;
        return usage;
#elif defined(__linux__)
        std::cout << "Getting CPU usage on Linux" << std::endl;
        static long long prevTotal = 0, prevIdle = 0;
        std::ifstream stat("/proc/stat");
        if (!stat) {
            std::cout << "Failed to open /proc/stat" << std::endl;
            return -1.0f;
        }
        std::string line;
        std::getline(stat, line);
        long long user, nice, system, idle, iowait, irq, softirq;
        sscanf(line.c_str(), "cpu %lld %lld %lld %lld %lld %lld %lld", &user, &nice, &system, &idle, &iowait, &irq, &softirq);
        long long total = user + nice + system + idle + iowait + irq + softirq;
        long long idleTime = idle + iowait;
        float usage = prevTotal == 0 ? -1.0f : 100.0f * (1.0f - (float)(idleTime - prevIdle) / (total - prevTotal));
        prevTotal = total;
        prevIdle = idleTime;
        std::cout << "CPU usage: " << usage << "%" << std::endl;
        return usage;
#elif defined(__APPLE__)
        std::cout << "CPU usage not implemented on macOS" << std::endl;
        return -1.0f; // macOS requires mach APIs or sysctl, not implemented here
#else
        std::cout << "CPU usage not supported on this platform" << std::endl;
        return -1.0f;
#endif
    }

    int getCPUTemperature() {
#if defined(_WIN32)
        std::cout << "Getting CPU temperature on Windows" << std::endl;
        HRESULT hr = CoInitialize(nullptr);
        if (FAILED(hr)) {
            std::cout << "CoInitialize failed: " << hr << std::endl;
            return -1;
        }
        IWbemLocator* locator = nullptr;
        hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (void**)&locator);
        if (FAILED(hr)) {
            std::cout << "CoCreateInstance failed: " << hr << std::endl;
            CoUninitialize();
            return -1;
        }
        IWbemServices* services = nullptr;
        hr = locator->ConnectServer(_bstr_t(L"ROOT\\WMI"), nullptr, nullptr, nullptr, 0, nullptr, nullptr, &services);
        locator->Release();
        if (FAILED(hr)) {
            std::cout << "ConnectServer failed: " << hr << std::endl;
            CoUninitialize();
            return -1;
        }
        hr = CoSetProxyBlanket(services, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
        if (FAILED(hr)) {
            std::cout << "CoSetProxyBlanket failed: " << hr << std::endl;
            services->Release();
            CoUninitialize();
            return -1;
        }
        IEnumWbemClassObject* enumerator = nullptr;
        hr = services->ExecQuery(_bstr_t(L"WQL"), _bstr_t(L"SELECT CurrentTemperature FROM MSAcpi_ThermalZoneTemperature"), WBEM_FLAG_FORWARD_ONLY, nullptr, &enumerator);
        if (FAILED(hr)) {
            std::cout << "ExecQuery failed: " << hr << std::endl;
            services->Release();
            CoUninitialize();
            return -1;
        }
        IWbemClassObject* obj = nullptr;
        ULONG returned = 0;
        hr = enumerator->Next(WBEM_INFINITE, 1, &obj, &returned);
        enumerator->Release();
        services->Release();
        if (FAILED(hr) || returned == 0) {
            std::cout << "Enumerator Next failed: " << hr << std::endl;
            CoUninitialize();
            return -1;
        }
        VARIANT vt;
        VariantInit(&vt);
        hr = obj->Get(L"CurrentTemperature", 0, &vt, nullptr, nullptr);
        int temp = -1;
        if (SUCCEEDED(hr) && vt.vt == VT_I4) {
            temp = vt.lVal / 10 - 273; // Convert from deci-Kelvin to Celsius
        }
        VariantClear(&vt);
        obj->Release();
        CoUninitialize();
        std::cout << "CPU temperature: " << temp << "C" << std::endl;
        return temp;
#elif defined(__linux__)
        std::cout << "Getting CPU temperature on Linux" << std::endl;
        std::ifstream temp("/sys/class/thermal/thermal_zone0/temp");
        if (!temp) {
            std::cout << "Failed to open thermal_zone0/temp" << std::endl;
            return -1;
        }
        int value;
        temp >> value;
        std::cout << "CPU temperature: " << value / 1000 << "C" << std::endl;
        return value / 1000; // Convert from milli-Celsius to Celsius
#elif defined(__APPLE__)
        std::cout << "CPU temperature not implemented on macOS" << std::endl;
        return -1; // macOS requires SMC access, not implemented here
#else
        std::cout << "CPU temperature not supported on this platform" << std::endl;
        return -1;
#endif
    }

    void initGPUInfo() {
        std::cout << "Initializing GPU info for Vulkan" << std::endl;
        // Since we're using Vulkan, try to get GPU name from Vulkan if possible
        // Note: Vulkan instance is not available here, so rely on SDL_Renderer or NVML
        if (m_renderer) {
            std::cout << "Getting renderer info" << std::endl;
            const char* rendererName = SDL_GetRendererName(m_renderer);
            if (rendererName) {
                m_gpuName = rendererName;
                std::cout << "GPU name from SDL_Renderer: " << m_gpuName << std::endl;
            } else {
                std::cout << "SDL_GetRendererName failed: " << SDL_GetError() << std::endl;
            }
        } else {
            std::cout << "No renderer provided, using default GPU name" << std::endl;
            m_gpuName = "Vulkan Device";
        }
    }
};

#endif