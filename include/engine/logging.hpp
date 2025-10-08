// AMOURANTH RTX Engine, October 2025 - Enhanced thread-safe, asynchronous logging.
// Thread-safe, asynchronous logging with ANSI-colored output, source location, and delta time.
// Supports C++20 std::format, std::jthread, OpenMP, and lock-free queue with std::atomic.
// No mutexes for queue; designed for high-performance Vulkan applications on Windows and Linux.
// Delta time format: microseconds (<10ms), milliseconds (10ms-1s), seconds (1s-1min), minutes (1min-1hr), hours (>1hr).
// Usage: LOG_INFO("Message: {}", value); or Logger::get().log(LogLevel::Info, "Vulkan", "Message: {}", value);
// Features: Singleton, log rotation, environment variable config, automatic flush, extended colors, overloads.
// Zachary Geurts 2025

#ifndef ENGINE_LOGGING_HPP
#define ENGINE_LOGGING_HPP

#include <string_view>
#include <source_location>
#include <format>
#include <syncstream>
#include <iostream>
#include <fstream>
#include <vulkan/vulkan.h>
#include <array>
#include <atomic>
#include <thread>
#include <memory>
#include <omp.h>
#include <chrono>
#include <cstdint>
#include <glm/glm.hpp>
#include <type_traits>
#include <concepts>
#include <filesystem>
#include <cstdlib>
#include <mutex>
#include <optional>
#include <map>
#include <ctime>

// Logging macros for simplicity
#define LOG_DEBUG(...) Logging::Logger::get().log(Logging::LogLevel::Debug, "General", __VA_ARGS__)
#define LOG_INFO(...) Logging::Logger::get().log(Logging::LogLevel::Info, "General", __VA_ARGS__)
#define LOG_WARNING(...) Logging::Logger::get().log(Logging::LogLevel::Warning, "General", __VA_ARGS__)
#define LOG_ERROR(...) Logging::Logger::get().log(Logging::LogLevel::Error, "General", __VA_ARGS__)
#define LOG_DEBUG_CAT(category, ...) Logging::Logger::get().log(Logging::LogLevel::Debug, category, __VA_ARGS__)
#define LOG_INFO_CAT(category, ...) Logging::Logger::get().log(Logging::LogLevel::Info, category, __VA_ARGS__)
#define LOG_WARNING_CAT(category, ...) Logging::Logger::get().log(Logging::LogLevel::Warning, category, __VA_ARGS__)
#define LOG_ERROR_CAT(category, ...) Logging::Logger::get().log(Logging::LogLevel::Error, category, __VA_ARGS__)

namespace Logging {

enum class LogLevel { Debug, Info, Warning, Error };

// ANSI color codes
inline constexpr std::string_view RESET = "\033[0m";
inline constexpr std::string_view CYAN = "\033[1;36m";    // Bold cyan for debug
inline constexpr std::string_view GREEN = "\033[1;32m";   // Bold green for info
inline constexpr std::string_view YELLOW = "\033[1;33m";  // Bold yellow for warnings
inline constexpr std::string_view MAGENTA = "\033[1;35m"; // Bold magenta for errors
inline constexpr std::string_view BLUE = "\033[1;34m";    // Bold blue for Vulkan category
inline constexpr std::string_view RED = "\033[1;31m";     // Bold red for critical errors
inline constexpr std::string_view WHITE = "\033[1;37m";   // Bold white for general
inline constexpr std::string_view PURPLE = "\033[1;35m";  // Bold purple for simulation
inline constexpr std::string_view ORANGE = "\033[38;5;208m"; // Orange for custom

// Log message structure
struct LogMessage {
    LogLevel level;
    std::string message;
    std::string category;
    std::source_location location;
    std::string formattedMessage;
    std::chrono::steady_clock::time_point timestamp;

    LogMessage() = default;
    LogMessage(LogLevel lvl, std::string_view msg, std::string_view cat, const std::source_location& loc, std::chrono::steady_clock::time_point ts)
        : level(lvl), message(msg), category(cat), location(loc), timestamp(ts) {}
};

class Logger {
public:
    // Singleton access
    static Logger& get() {
        static Logger instance(getDefaultLogLevel(), getDefaultLogFile());
        return instance;
    }

    // Primary log method with category and variadic args
    template<typename... Args>
    void log(LogLevel level, std::string_view category, std::string_view message,
             const std::source_location& location = std::source_location::current(), const Args&... args) const {
        if (static_cast<int>(level) < static_cast<int>(level_.load(std::memory_order_relaxed))) {
            return;
        }

        std::string formatted;
        try {
            bool argsValid = true;
            (
                [&] {
                    if constexpr (std::is_same_v<std::decay_t<Args>, uint64_t>) {
                        if (args == UINT64_MAX) {
                            argsValid = false;
                            formatted = std::string(message) + " [Invalid argument: UINT64_MAX detected]";
                        }
                    }
                }(),
                ...
            );

            if (argsValid) {
                formatted = std::vformat(message, std::make_format_args(args...));
            }
        } catch (const std::format_error& e) {
            formatted = std::string(message) + " [Format error: " + e.what() + "]";
        }

        if (formatted.empty()) {
            formatted = "Empty log message";
        }

        enqueueMessage(level, message, category, formatted, location);
    }

    // Overload for simple string literals
    void log(LogLevel level, std::string_view category, std::string_view message,
             const std::source_location& location = std::source_location::current()) const {
        if (static_cast<int>(level) < static_cast<int>(level_.load(std::memory_order_relaxed))) {
            return;
        }
        enqueueMessage(level, message, category, std::string(message), location);
    }

    // Overload for Vulkan types
    void log(LogLevel level, std::string_view category, VkResult result,
             const std::source_location& location = std::source_location::current()) const {
        if (static_cast<int>(level) < static_cast<int>(level_.load(std::memory_order_relaxed))) {
            return;
        }
        std::string formatted = std::format("VkResult: {}", result);
        enqueueMessage(level, "VkResult", category, formatted, location);
    }

    template<typename T>
    requires (std::same_as<T, VkBuffer> || std::same_as<T, VkCommandBuffer> ||
              std::same_as<T, VkPipelineLayout> || std::same_as<T, VkDescriptorSet> ||
              std::same_as<T, VkRenderPass> || std::same_as<T, VkFramebuffer> ||
              std::same_as<T, VkImage> || std::same_as<T, VkDeviceMemory>)
    void log(LogLevel level, std::string_view category, T handle, std::string_view handleName,
             const std::source_location& location = std::source_location::current()) const {
        if (static_cast<int>(level) < static_cast<int>(level_.load(std::memory_order_relaxed))) {
            return;
        }
        std::string formatted = std::format("{}: {}", handleName, handle);
        enqueueMessage(level, handleName, category, formatted, location);
    }

    // Overload for glm::vec3
    void log(LogLevel level, std::string_view category, const glm::vec3& vec,
             const std::source_location& location = std::source_location::current()) const {
        if (static_cast<int>(level) < static_cast<int>(level_.load(std::memory_order_relaxed))) {
            return;
        }
        std::string formatted = std::format("vec3({:.3f}, {:.3f}, {:.3f})", vec.x, vec.y, vec.z);
        enqueueMessage(level, "glm::vec3", category, formatted, location);
    }

    // Set log level dynamically
    void setLogLevel(LogLevel level) {
        level_.store(level, std::memory_order_relaxed);
        log(LogLevel::Info, "General", "Log level set to {}", std::to_string(static_cast<int>(level)));
    }

    // Set log file with rotation
    bool setLogFile(const std::string& filename, size_t maxSizeBytes = 10 * 1024 * 1024) {
        std::lock_guard<std::mutex> lock(fileMutex_);
        if (logFile_.is_open()) {
            logFile_.close();
        }
        logFile_.open(filename, std::ios::out | std::ios::app);
        if (!logFile_.is_open()) {
            log(LogLevel::Error, "General", "Failed to open log file: {}", filename);
            return false;
        }
        logFilePath_ = filename;
        maxLogFileSize_ = maxSizeBytes;
        log(LogLevel::Info, "General", "Log file set to: {}", filename);
        return true;
    }

    // Stop the logger
    void stop() {
        if (running_.exchange(false)) {
            worker_->request_stop();
            worker_->join();
            flushQueue();
        }
    }

private:
    static constexpr size_t QueueSize = 1024;
    static constexpr size_t MaxFiles = 5;

    Logger(LogLevel level, const std::string& filename)
        : logQueue_{}, head_(0), tail_(0), running_(true), level_(level), maxLogFileSize_(10 * 1024 * 1024) {
        if (!filename.empty()) {
            setLogFile(filename);
        }
        worker_ = std::make_unique<std::jthread>([this](std::stop_token stoken) {
            processLogQueue(stoken);
        });
        std::atexit([]() { Logger::get().stop(); });
    }

    static LogLevel getDefaultLogLevel() {
        if (const char* levelStr = std::getenv("AMOURANTH_LOG_LEVEL")) {
            std::string level(levelStr);
            if (level == "Debug") return LogLevel::Debug;
            if (level == "Warning") return LogLevel::Warning;
            if (level == "Error") return LogLevel::Error;
        }
        return LogLevel::Info;
    }

    static std::string getDefaultLogFile() {
        if (const char* file = std::getenv("AMOURANTH_LOG_FILE")) {
            return std::string(file);
        }
        return "";
    }

    // Map categories to colors
    static std::string_view getCategoryColor(std::string_view category) {
        static const std::map<std::string_view, std::string_view, std::less<>> categoryColors = {
            {"General", WHITE},
            {"Vulkan", BLUE},
            {"Simulation", PURPLE},
            {"Renderer", ORANGE},
            {"Engine", GREEN}
        };
        auto it = categoryColors.find(category);
        return it != categoryColors.end() ? it->second : WHITE;
    }

    void enqueueMessage(LogLevel level, std::string_view message, std::string_view category,
                        std::string formatted, const std::source_location& location) const {
        size_t currentHead = head_.load(std::memory_order_relaxed);
        size_t nextHead = (currentHead + 1) % QueueSize;
        if (nextHead == tail_.load(std::memory_order_acquire)) {
            std::osyncstream(std::cerr) << RED << "[ERROR] [0.000us] [Logger] Log queue full, dropping oldest message" << RESET << std::endl;
            tail_.fetch_add(1, std::memory_order_release);
        }

        auto now = std::chrono::steady_clock::now();
        logQueue_[currentHead] = LogMessage(level, message, category, location, now);
        logQueue_[currentHead].formattedMessage = std::move(formatted);
        if (!firstLogTime_.has_value()) {
            firstLogTime_ = now;
        }
        head_.store(nextHead, std::memory_order_release);
    }

    void processLogQueue(std::stop_token stoken) {
        while (running_.load(std::memory_order_relaxed) || head_.load(std::memory_order_acquire) != tail_.load(std::memory_order_acquire)) {
            if (stoken.stop_requested() && head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire)) {
                break;
            }

            std::vector<LogMessage> batch;
            size_t currentTail = tail_.load(std::memory_order_relaxed);
            size_t currentHead = head_.load(std::memory_order_acquire);
            size_t batchSize = (currentHead >= currentTail) ? (currentHead - currentTail) : (QueueSize - currentTail + currentHead);
            batchSize = std::min(batchSize, static_cast<size_t>(100));
            batch.reserve(batchSize);

            for (size_t i = 0; i < batchSize; ++i) {
                batch.push_back(std::move(logQueue_[currentTail]));
                currentTail = (currentTail + 1) % QueueSize;
            }
            tail_.store(currentTail, std::memory_order_release);

            // Rotate log file if needed
            if (logFile_.is_open()) {
                std::lock_guard<std::mutex> lock(fileMutex_);
                if (std::filesystem::file_size(logFilePath_) > maxLogFileSize_) {
                    logFile_.close();
                    rotateLogFile();
                    logFile_.open(logFilePath_, std::ios::out | std::ios::app);
                }
            }

            #pragma omp parallel
            {
                #pragma omp single
                {
                    for (size_t i = 0; i < batch.size(); ++i) {
                        #pragma omp task
                        {
                            const auto& msg = batch[i];
                            std::string_view levelColor;
                            std::string_view levelStr;
                            switch (msg.level) {
                                case LogLevel::Debug:   levelColor = CYAN;   levelStr = "[DEBUG]"; break;
                                case LogLevel::Info:    levelColor = GREEN;  levelStr = "[INFO]";  break;
                                case LogLevel::Warning: levelColor = YELLOW; levelStr = "[WARN]";  break;
                                case LogLevel::Error:   levelColor = MAGENTA; levelStr = "[ERROR]"; break;
                            }

                            auto delta = std::chrono::duration_cast<std::chrono::microseconds>(msg.timestamp - *firstLogTime_).count();
                            std::string timeStr;
                            if (delta < 10000) {
                                timeStr = std::format("{:>6}us", delta);
                            } else if (delta < 1000000) {
                                timeStr = std::format("{:>6.3f}ms", delta / 1000.0);
                            } else if (delta < 60000000) {
                                timeStr = std::format("{:>6.3f}s", delta / 1000000.0);
                            } else if (delta < 3600000000LL) {
                                timeStr = std::format("{:>6.3f}m", delta / 60000000.0);
                            } else {
                                timeStr = std::format("{:>6.3f}h", delta / 3600000000.0);
                            }

                            std::string output = std::format("{} {} [{}] [{}] [{}:{}] {}{}",
                                                             levelColor, levelStr, timeStr, msg.category,
                                                             msg.location.file_name(), msg.location.line(),
                                                             msg.formattedMessage, RESET);
                            std::osyncstream(std::cout) << output << std::endl;
                            if (logFile_.is_open()) {
                                std::lock_guard<std::mutex> lock(fileMutex_);
                                std::osyncstream(logFile_) << output << std::endl;
                            }
                        }
                    }
                }
            }
        }
    }

    void rotateLogFile() {
        std::time_t now = std::time(nullptr);
        char timestamp[20];
        std::strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", std::localtime(&now));
        std::string newFile = std::format("{}.{}.log", logFilePath_.string(), timestamp);

        std::filesystem::rename(logFilePath_, newFile);

        // Remove oldest files if exceeding MaxFiles
        std::vector<std::filesystem::path> logs;
        for (const auto& entry : std::filesystem::directory_iterator(logFilePath_.parent_path())) {
            if (entry.path().extension() == ".log" && entry.path().stem().string().find(logFilePath_.stem().string()) == 0) {
                logs.push_back(entry.path());
            }
        }
        std::sort(logs.begin(), logs.end(), [](const auto& a, const auto& b) {
            return std::filesystem::last_write_time(a) < std::filesystem::last_write_time(b);
        });
        while (logs.size() > MaxFiles) {
            std::filesystem::remove(logs.front());
            logs.erase(logs.begin());
        }
    }

    void flushQueue() {
        std::vector<LogMessage> batch;
        size_t currentTail = tail_.load(std::memory_order_relaxed);
        size_t currentHead = head_.load(std::memory_order_acquire);
        size_t batchSize = (currentHead >= currentTail) ? (currentHead - currentTail) : (QueueSize - currentTail + currentHead);
        batch.reserve(batchSize);

        for (size_t i = 0; i < batchSize; ++i) {
            batch.push_back(std::move(logQueue_[currentTail]));
            currentTail = (currentTail + 1) % QueueSize;
        }
        tail_.store(currentTail, std::memory_order_release);

        for (const auto& msg : batch) {
            std::string_view levelColor;
            std::string_view levelStr;
            switch (msg.level) {
                case LogLevel::Debug:   levelColor = CYAN;   levelStr = "[DEBUG]"; break;
                case LogLevel::Info:    levelColor = GREEN;  levelStr = "[INFO]";  break;
                case LogLevel::Warning: levelColor = YELLOW; levelStr = "[WARN]";  break;
                case LogLevel::Error:   levelColor = MAGENTA; levelStr = "[ERROR]"; break;
            }

            auto delta = std::chrono::duration_cast<std::chrono::microseconds>(msg.timestamp - *firstLogTime_).count();
            std::string timeStr;
            if (delta < 10000) {
                timeStr = std::format("{:>6}us", delta);
            } else if (delta < 1000000) {
                timeStr = std::format("{:>6.3f}ms", delta / 1000.0);
            } else if (delta < 60000000) {
                timeStr = std::format("{:>6.3f}s", delta / 1000000.0);
            } else if (delta < 3600000000LL) {
                timeStr = std::format("{:>6.3f}m", delta / 60000000.0);
            } else {
                timeStr = std::format("{:>6.3f}h", delta / 3600000000.0);
            }

            std::string output = std::format("{} {} [{}] [{}] [{}:{}] {}{}",
                                             levelColor, levelStr, timeStr, msg.category,
                                             msg.location.file_name(), msg.location.line(),
                                             msg.formattedMessage, RESET);
            std::osyncstream(std::cout) << output << std::endl;
            if (logFile_.is_open()) {
                std::lock_guard<std::mutex> lock(fileMutex_);
                std::osyncstream(logFile_) << output << std::endl;
            }
        }
    }

    mutable std::array<LogMessage, QueueSize> logQueue_;
    mutable std::atomic<size_t> head_;
    mutable std::atomic<size_t> tail_;
    std::atomic<bool> running_;
    std::atomic<LogLevel> level_;
    std::ofstream logFile_;
    std::filesystem::path logFilePath_;
    size_t maxLogFileSize_;
    std::mutex fileMutex_;
    std::unique_ptr<std::jthread> worker_;
    mutable std::optional<std::chrono::steady_clock::time_point> firstLogTime_;
};

} // namespace Logging

namespace std {
// Formatter for VkResult
template<>
struct formatter<VkResult, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(VkResult result, FormatContext& ctx) const {
        string_view name;
        switch (result) {
            case VK_SUCCESS: name = "VK_SUCCESS"; break;
            case VK_NOT_READY: name = "VK_NOT_READY"; break;
            case VK_TIMEOUT: name = "VK_TIMEOUT"; break;
            case VK_EVENT_SET: name = "VK_EVENT_SET"; break;
            case VK_EVENT_RESET: name = "VK_EVENT_RESET"; break;
            case VK_INCOMPLETE: name = "VK_INCOMPLETE"; break;
            case VK_ERROR_OUT_OF_HOST_MEMORY: name = "VK_ERROR_OUT_OF_HOST_MEMORY"; break;
            case VK_ERROR_OUT_OF_DEVICE_MEMORY: name = "VK_ERROR_OUT_OF_DEVICE_MEMORY"; break;
            case VK_ERROR_INITIALIZATION_FAILED: name = "VK_ERROR_INITIALIZATION_FAILED"; break;
            case VK_ERROR_DEVICE_LOST: name = "VK_ERROR_DEVICE_LOST"; break;
            case VK_ERROR_MEMORY_MAP_FAILED: name = "VK_ERROR_MEMORY_MAP_FAILED"; break;
            case VK_ERROR_LAYER_NOT_PRESENT: name = "VK_ERROR_LAYER_NOT_PRESENT"; break;
            case VK_ERROR_EXTENSION_NOT_PRESENT: name = "VK_ERROR_EXTENSION_NOT_PRESENT"; break;
            case VK_ERROR_FEATURE_NOT_PRESENT: name = "VK_ERROR_FEATURE_NOT_PRESENT"; break;
            case VK_ERROR_INCOMPATIBLE_DRIVER: name = "VK_ERROR_INCOMPATIBLE_DRIVER"; break;
            case VK_ERROR_TOO_MANY_OBJECTS: name = "VK_ERROR_TOO_MANY_OBJECTS"; break;
            case VK_ERROR_FORMAT_NOT_SUPPORTED: name = "VK_ERROR_FORMAT_NOT_SUPPORTED"; break;
            case VK_ERROR_FRAGMENTED_POOL: name = "VK_ERROR_FRAGMENTED_POOL"; break;
            case VK_ERROR_OUT_OF_DATE_KHR: name = "VK_ERROR_OUT_OF_DATE_KHR"; break;
            default: name = std::format("Unknown VkResult({})", static_cast<int>(result)); break;
        }
        return format_to(ctx.out(), "{}", name);
    }
};

// Formatter for uint64_t
template<>
struct formatter<uint64_t, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(uint64_t value, FormatContext& ctx) const {
        if (value == UINT64_MAX) {
            return format_to(ctx.out(), "INVALID_SIZE");
        }
        return format_to(ctx.out(), "{}", value);
    }
};

// Formatter for glm::vec3
template<>
struct formatter<glm::vec3, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(const glm::vec3& vec, FormatContext& ctx) const {
        return format_to(ctx.out(), "vec3({:.3f}, {:.3f}, {:.3f})", vec tellraw: vec.x, vec.y, vec.z);
    }
};

// Formatter for Vulkan pointer types
template<typename T>
requires (
    std::same_as<T, VkBuffer> ||
    std::same_as<T, VkCommandBuffer> ||
    std::same_as<T, VkPipelineLayout> ||
    std::same_as<T, VkDescriptorSet> ||
    std::same_as<T, VkRenderPass> ||
    std::same_as<T, VkFramebuffer> ||
    std::same_as<T, VkImage> ||
    std::same_as<T, VkDeviceMemory>
)
struct formatter<T, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(T ptr, FormatContext& ctx) const {
        if (ptr == VK_NULL_HANDLE) {
            return format_to(ctx.out(), "VK_NULL_HANDLE");
        }
        return format_to(ctx.out(), "{:p}", static_cast<const void*>(ptr));
    }
};

} // namespace std

#endif // ENGINE_LOGGING_HPP