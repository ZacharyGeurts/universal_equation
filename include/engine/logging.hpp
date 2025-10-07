#ifndef ENGINE_LOGGING_HPP
#define ENGINE_LOGGING_HPP

// AMOURANTH RTX Engine Logging, October 2025
// Thread-safe, asynchronous logging with ANSI-colored output and source location.
// Supports C++20 std::format for flexible message formatting.
// Uses std::jthread for async logging, OpenMP for parallel processing, and lock-free queue with std::atomic.
// No mutexes; designed for high-performance Vulkan applications.
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

    LogMessage() = default; // Default constructor
    LogMessage(LogLevel lvl, std::string_view msg, const std::source_location& loc)
        : level(lvl), message(msg), location(loc) {}
};

class Logger {
public:
    // Default constructor
    Logger() : logQueue_{}, head_(0), tail_(0), running_(true), level_(LogLevel::Info) {
        worker_ = std::make_unique<std::jthread>([this](std::stop_token stoken) {
            processLogQueue(stoken);
        });
    }

    // Constructor with log level and file
    Logger(LogLevel level, const std::string& filename)
        : logQueue_{}, head_(0), tail_(0), running_(true), level_(level) {
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

    // Move constructor (Fix for deleted copy constructor error)
    Logger(Logger&& other) noexcept
        : logQueue_(std::move(other.logQueue_)),
          head_(other.head_.load()),
          tail_(other.tail_.load()),
          running_(other.running_.load()),
          level_(other.level_.load()),
          logFile_(std::move(other.logFile_)),
          worker_(std::move(other.worker_)) {
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
            formatted = std::vformat(message, std::make_format_args(args...));
        } catch (const std::format_error& e) {
            formatted = std::string(message) + " [Format error: " + e.what() + "]";
        }

        // Enqueue message using lock-free circular buffer
        size_t currentHead = head_.load(std::memory_order_relaxed);
        size_t nextHead = (currentHead + 1) % QueueSize;
        while (nextHead == tail_.load(std::memory_order_acquire)) {
            // Queue full; spin until space is available
            std::this_thread::yield();
        }

        logQueue_[currentHead] = LogMessage(level, message, location);
        logQueue_[currentHead].formattedMessage = std::move(formatted);
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

                            std::string output = std::format("{} {} [{}:{}] {}{}", 
                                                             color, levelStr, msg.location.file_name(),
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
} // namespace std

#endif // ENGINE_LOGGING_HPP