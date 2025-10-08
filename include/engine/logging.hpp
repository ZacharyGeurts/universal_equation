#ifndef ENGINE_LOGGING_HPP
#define ENGINE_LOGGING_HPP

// https://www.twitch.tv/elliefier Logging, October 2025
// Thread-safe, asynchronous logging with ANSI-colored output, source location, and delta time.
// Supports C++20 std::format for flexible message formatting.
// Uses std::jthread for async logging, OpenMP for parallel processing, and lock-free queue with std::atomic.
// No mutexes; designed for high-performance Vulkan applications on Windows and Linux.
// Delta time format: microseconds (<10ms), milliseconds (10ms-1s), seconds (1s-1min), minutes (1min-1hr), hours (>1hr).
// Usage: Logger logger; logger.log(LogLevel::Info, "Message: {}", "value");
// Zachary Geurts 2025

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
#include <cstdint> // Added for uint64_t

namespace Logging {

enum class LogLevel { Debug, Info, Warning, Error };

// ANSI color codes
inline constexpr std::string_view RESET = "\033[0m";
inline constexpr std::string_view MAGENTA = "\033[1;35m"; // Bold magenta for errors
inline constexpr std::string_view CYAN = "\033[1;36m";   // Bold cyan for debug
inline constexpr std::string_view YELLOW = "\033[1;33m"; // Bold yellow for warnings
inline constexpr std::string_view GREEN = "\033[1;32m";  // Bold green for info

// Log message structure
struct LogMessage {
    LogLevel level;
    std::string message;
    std::source_location location;
    std::string formattedMessage;
    std::chrono::steady_clock::time_point timestamp;

    LogMessage() = default;
    LogMessage(LogLevel lvl, std::string_view msg, const std::source_location& loc, std::chrono::steady_clock::time_point ts)
        : level(lvl), message(msg), location(loc), timestamp(ts) {}
};

class Logger {
public:
    // Default constructor
    Logger() : logQueue_{}, head_(0), tail_(0), running_(true), level_(LogLevel::Info), firstLogTime_(std::nullopt) {
        worker_ = std::make_unique<std::jthread>([this](std::stop_token stoken) {
            processLogQueue(stoken);
        });
    }

    // Constructor with log level and file
    Logger(LogLevel level, const std::string& filename)
        : logQueue_{}, head_(0), tail_(0), running_(true), level_(level), firstLogTime_(std::nullopt) {
        if (!filename.empty()) {
            logFile_.open(filename, std::ios::out | std::ios::app);
            if (!logFile_.is_open()) {
                std::osyncstream(std::cerr) << "Failed to open log file: " << filename << std::endl;
            }
        }
        worker_ = std::make_unique<std::jthread>([this](std::stop_token stoken) {
            processLogQueue(stoken);
        });
    }

    // Move constructor
    Logger(Logger&& other) noexcept
        : logQueue_(std::move(other.logQueue_)),
          head_(other.head_.load()),
          tail_(other.tail_.load()),
          running_(other.running_.load()),
          level_(other.level_.load()),
          logFile_(std::move(other.logFile_)),
          worker_(std::move(other.worker_)),
          firstLogTime_(other.firstLogTime_) {
        other.head_.store(0);
        other.tail_.store(0);
        other.running_.store(false);
    }

    // Move assignment operator
    Logger& operator=(Logger&& other) noexcept {
        if (this != &other) {
            stop();
            if (logFile_.is_open()) logFile_.close();
            logQueue_ = std::move(other.logQueue_);
            head_.store(other.head_.load());
            tail_.store(other.tail_.load());
            running_.store(other.running_.load());
            level_.store(other.level_.load());
            logFile_ = std::move(other.logFile_);
            worker_ = std::move(other.worker_);
            firstLogTime_ = other.firstLogTime_;
            other.head_.store(0);
            other.tail_.store(0);
            other.running_.store(false);
        }
        return *this;
    }

    // Delete copy constructor and assignment
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    ~Logger() {
        stop();
        if (logFile_.is_open()) {
            logFile_.close();
        }
    }

    // Log method for asynchronous logging
    template<typename... Args>
    void log(LogLevel level, std::string_view message, const std::source_location& location, const Args&... args) const {
        if (static_cast<int>(level) < static_cast<int>(level_.load(std::memory_order_relaxed))) {
            return; // Skip logging if level is below threshold
        }

        std::string formatted;
        try {
            // Validate arguments to prevent uninitialized value issues
            bool argsValid = true;
            (
                [&] {
                    // Check for invalid sizes (e.g., UINT64_MAX)
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

        // Ensure formatted message is not empty
        if (formatted.empty()) {
            formatted = "Empty log message";
        }

        // Enqueue message using lock-free circular buffer
        size_t currentHead = head_.load(std::memory_order_relaxed);
        size_t nextHead = (currentHead + 1) % QueueSize;
        while (nextHead == tail_.load(std::memory_order_acquire)) {
            // Queue full; spin until space is available
            std::this_thread::yield();
        }

        auto now = std::chrono::steady_clock::now();
        logQueue_[currentHead] = LogMessage(level, message, location, now);
        logQueue_[currentHead].formattedMessage = std::move(formatted);
        if (!firstLogTime_.has_value()) {
            firstLogTime_ = now;
        }
        head_.store(nextHead, std::memory_order_release);
    }

    // Stop the logger and process remaining messages
    void stop() {
        if (running_.exchange(false)) {
            worker_->request_stop();
            worker_->join();
        }
    }

private:
    static constexpr size_t QueueSize = 1024; // Fixed-size circular buffer

    void processLogQueue(std::stop_token stoken) {
        while (running_.load(std::memory_order_relaxed) || head_.load(std::memory_order_acquire) != tail_.load(std::memory_order_acquire)) {
            if (stoken.stop_requested() && head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire)) {
                break;
            }

            // Process available messages
            std::vector<LogMessage> batch;
            size_t currentTail = tail_.load(std::memory_order_relaxed);
            size_t currentHead = head_.load(std::memory_order_acquire);
            size_t batchSize = (currentHead >= currentTail) ? (currentHead - currentTail) : (QueueSize - currentTail + currentHead);
            batchSize = std::min(batchSize, static_cast<size_t>(100)); // Limit batch size
            batch.reserve(batchSize);

            for (size_t i = 0; i < batchSize; ++i) {
                batch.push_back(std::move(logQueue_[currentTail]));
                currentTail = (currentTail + 1) % QueueSize;
            }
            tail_.store(currentTail, std::memory_order_release);

            // Parallelize output with OpenMP tasks
            #pragma omp parallel
            {
                #pragma omp single
                {
                    for (size_t i = 0; i < batch.size(); ++i) {
                        #pragma omp task
                        {
                            const auto& msg = batch[i];
                            std::string_view color;
                            std::string_view levelStr;
                            switch (msg.level) {
                                case LogLevel::Debug:   color = CYAN;   levelStr = "[DEBUG]"; break;
                                case LogLevel::Info:    color = GREEN;  levelStr = "[INFO]";  break;
                                case LogLevel::Warning: color = YELLOW; levelStr = "[WARN]";  break;
                                case LogLevel::Error:   color = MAGENTA; levelStr = "[ERROR]"; break;
                            }

                            // Calculate delta time from first log
                            std::string timeStr = "0.000us";
                            if (firstLogTime_.has_value()) {
                                auto delta = std::chrono::duration_cast<std::chrono::microseconds>(msg.timestamp - *firstLogTime_).count();
                                if (delta < 10000) {
                                    timeStr = std::format("{}us", delta);
                                } else if (delta < 1000000) {
                                    timeStr = std::format("{:.3f}ms", delta / 1000.0);
                                } else if (delta < 60000000) {
                                    timeStr = std::format("{:.3f}s", delta / 1000000.0);
                                } else if (delta < 3600000000) {
                                    timeStr = std::format("{:.3f}m", delta / 60000000.0);
                                } else {
                                    timeStr = std::format("{:.3f}h", delta / 3600000000.0);
                                }
                            }

                            std::string output = std::format("{} {} [{}] [{}:{}] {}{}", 
                                                             color, levelStr, timeStr, msg.location.file_name(),
                                                             msg.location.line(), msg.formattedMessage, RESET);
                            std::osyncstream(std::cout) << output << std::endl;
                            if (logFile_.is_open()) {
                                std::osyncstream(logFile_) << output << std::endl;
                            }
                        }
                    }
                }
            }
        }
    }

    mutable std::array<LogMessage, QueueSize> logQueue_;
    mutable std::atomic<size_t> head_;
    mutable std::atomic<size_t> tail_;
    std::atomic<bool> running_;
    std::atomic<LogLevel> level_;
    std::ofstream logFile_;
    std::unique_ptr<std::jthread> worker_;
    mutable std::optional<std::chrono::steady_clock::time_point> firstLogTime_;
};

} // namespace Logging

namespace std {
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

// Formatter for uint64_t to catch invalid sizes
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
} // namespace std

#endif // ENGINE_LOGGING_HPP