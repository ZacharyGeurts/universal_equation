// logging.hpp
// AMOURANTH RTX Engine, October 2025 - Enhanced thread-safe, asynchronous logging.
// Thread-safe, asynchronous logging with ANSI-colored output, source location, and delta time.
// Supports C++20 std::format, std::jthread, OpenMP, and lock-free queue with std::atomic.
// No mutexes for queue; designed for high-performance Vulkan applications on Windows and Linux.
// Delta time format: microseconds (<10ms), milliseconds (10ms-1s), seconds (1s-1min), minutes (1min-1hr), hours (>1hr).
// Usage: LOG_INFO("Message: {}", value); or Logger::get().log(LogLevel::Info, "Vulkan", "Message: {}", value);
// Features: Singleton, log rotation, environment variable config, automatic flush, extended colors, overloads.
// Extended features: Additional Vulkan/SDL types, GLM arrays, AMOURANTH camera, category filtering, high-frequency logging.
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
#include <SDL3/SDL.h>
#include <array>
#include <atomic>
#include <thread>
#include <memory>
#include <omp.h>
#include <chrono>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp> // Added for glm::vec2
#include <type_traits>
#include <concepts>
#include <filesystem>
#include <cstdlib>
#include <mutex>
#include <optional>
#include <map>
#include <ctime>
#include <span>
#include <set>

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
inline constexpr std::string_view BLUE = "\033[1;34m";    // Bold blue for Vulkan
inline constexpr std::string_view RED = "\033[1;31m";     // Bold red for critical
inline constexpr std::string_view WHITE = "\033[1;37m";   // Bold white for general
inline constexpr std::string_view PURPLE = "\033[1;35m";  // Bold purple for simulation
inline constexpr std::string_view ORANGE = "\033[38;5;208m"; // Orange for renderer
inline constexpr std::string_view TEAL = "\033[38;5;51m"; // Teal for audio
inline constexpr std::string_view YELLOW_GREEN = "\033[38;5;154m"; // Yellow-green for image
inline constexpr std::string_view BRIGHT_MAGENTA = "\033[38;5;201m"; // Bright magenta for input

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

// Forward declaration of AMOURANTH camera (assuming it's defined elsewhere)
struct AMOURANTH;

class Logger {
public:
    Logger(LogLevel level = LogLevel::Info, const std::string& logFile = getDefaultLogFile())
        : head_(0), tail_(0), running_(true), level_(level), maxLogFileSize_(10 * 1024 * 1024) {
        loadCategoryFilters();
        if (!logFile.empty()) {
            setLogFile(logFile);
        }
        worker_ = std::make_unique<std::jthread>([this](std::stop_token stoken) { processLogQueue(stoken); });
    }

    static Logger& get() {
        static Logger instance;
        return instance;
    }

    // Generic log with format string and arguments
    template<typename... Args>
    void log(LogLevel level, std::string_view category, std::string_view message,
             const std::source_location& location = std::source_location::current(), const Args&... args) const {
        if (!shouldLog(level, category)) {
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

    // Log without format arguments
    void log(LogLevel level, std::string_view category, std::string_view message,
             const std::source_location& location = std::source_location::current()) const {
        if (!shouldLog(level, category)) {
            return;
        }
        enqueueMessage(level, message, category, std::string(message), location);
    }

    // Log Vulkan handles
    template<typename T>
    requires (
        std::same_as<T, VkBuffer> || std::same_as<T, VkCommandBuffer> ||
        std::same_as<T, VkPipelineLayout> || std::same_as<T, VkDescriptorSet> ||
        std::same_as<T, VkRenderPass> || std::same_as<T, VkFramebuffer> ||
        std::same_as<T, VkImage> || std::same_as<T, VkDeviceMemory> ||
        std::same_as<T, VkDevice> || std::same_as<T, VkQueue> ||
        std::same_as<T, VkCommandPool> || std::same_as<T, VkPipeline> ||
        std::same_as<T, VkSwapchainKHR> || std::same_as<T, VkShaderModule> ||
        std::same_as<T, VkSemaphore> || std::same_as<T, VkFence> ||
        std::same_as<T, VkSurfaceKHR> || std::same_as<T, VkImageView> ||
        std::same_as<T, VkDescriptorSetLayout> || std::same_as<T, VkInstance> ||
        std::same_as<T, VkSampler> || std::same_as<T, VkDescriptorPool> ||
        std::same_as<T, VkAccelerationStructureKHR> || std::same_as<T, VkPhysicalDevice> ||
        std::same_as<T, VkExtent2D> || std::same_as<T, VkViewport> || // Added Vulkan types
        std::same_as<T, VkRect2D>
    )
    void log(LogLevel level, std::string_view category, T handle, std::string_view handleName,
             const std::source_location& location = std::source_location::current()) const {
        if (!shouldLog(level, category)) {
            return;
        }
        std::string formatted = formatVulkanType(handle, handleName);
        enqueueMessage(level, handleName, category, formatted, location);
    }

    // Log span of Vulkan handles
    template<typename T>
    requires (
        std::same_as<T, VkBuffer> || std::same_as<T, VkCommandBuffer> ||
        std::same_as<T, VkPipelineLayout> || std::same_as<T, VkDescriptorSet> ||
        std::same_as<T, VkRenderPass> || std::same_as<T, VkFramebuffer> ||
        std::same_as<T, VkImage> || std::same_as<T, VkDeviceMemory> ||
        std::same_as<T, VkDevice> || std::same_as<T, VkQueue> ||
        std::same_as<T, VkCommandPool> || std::same_as<T, VkPipeline> ||
        std::same_as<T, VkSwapchainKHR> || std::same_as<T, VkShaderModule> ||
        std::same_as<T, VkSemaphore> || std::same_as<T, VkFence> ||
        std::same_as<T, VkSurfaceKHR> || std::same_as<T, VkImageView> ||
        std::same_as<T, VkDescriptorSetLayout> || std::same_as<T, VkInstance> ||
        std::same_as<T, VkSampler> || std::same_as<T, VkDescriptorPool> ||
        std::same_as<T, VkAccelerationStructureKHR> || std::same_as<T, VkPhysicalDevice>
    )
    void log(LogLevel level, std::string_view category, std::span<const T> handles, std::string_view handleName,
             const std::source_location& location = std::source_location::current()) const {
        if (!shouldLog(level, category)) {
            return;
        }
        std::string formatted = std::format("{}[{}]{{", handleName, handles.size());
        for (size_t i = 0; i < handles.size(); ++i) {
            formatted += formatVulkanType(handles[i], "");
            if (i < handles.size() - 1) {
                formatted += ", ";
            }
        }
        formatted += "}";
        enqueueMessage(level, handleName, category, formatted, location);
    }

    // Log glm::vec3
    void log(LogLevel level, std::string_view category, const glm::vec3& vec,
             const std::source_location& location = std::source_location::current()) const {
        if (!shouldLog(level, category)) {
            return;
        }
        std::string formatted = std::format("vec3({:.3f}, {:.3f}, {:.3f})", vec.x, vec.y, vec.z);
        enqueueMessage(level, "glm::vec3", category, formatted, location);
    }

    // Log glm::vec2
    void log(LogLevel level, std::string_view category, const glm::vec2& vec,
             const std::source_location& location = std::source_location::current()) const {
        if (!shouldLog(level, category)) {
            return;
        }
        std::string formatted = std::format("vec2({:.3f}, {:.3f})", vec.x, vec.y);
        enqueueMessage(level, "glm::vec2", category, formatted, location);
    }

    // Log glm::mat4
    void log(LogLevel level, std::string_view category, const glm::mat4& mat, std::string_view message,
             const std::source_location& location = std::source_location::current()) const {
        if (!shouldLog(level, category)) {
            return;
        }
        std::string formatted = std::format("{}: [{:.3f}, {:.3f}, {:.3f}, {:.3f}; {:.3f}, {:.3f}, {:.3f}, {:.3f}; {:.3f}, {:.3f}, {:.3f}, {:.3f}; {:.3f}, {:.3f}, {:.3f}, {:.3f}]",
                                            message, mat[0][0], mat[0][1], mat[0][2], mat[0][3],
                                            mat[1][0], mat[1][1], mat[1][2], mat[1][3],
                                            mat[2][0], mat[2][1], mat[2][2], mat[2][3],
                                            mat[3][0], mat[3][1], mat[3][2], mat[3][3]);
        enqueueMessage(level, message, category, formatted, location);
    }

    // Log span of glm::vec3
    void log(LogLevel level, std::string_view category, std::span<const glm::vec3> vecs, std::string_view message,
             const std::source_location& location = std::source_location::current()) const {
        if (!shouldLog(level, category)) {
            return;
        }
        std::string formatted = std::format("{}[{}]{{", message, vecs.size());
        for (size_t i = 0; i < vecs.size(); ++i) {
            formatted += std::format("vec3({:.3f}, {:.3f}, {:.3f})", vecs[i].x, vecs[i].y, vecs[i].z);
            if (i < vecs.size() - 1) {
                formatted += ", ";
            }
        }
        formatted += "}";
        enqueueMessage(level, message, category, formatted, location);
    }

    // Log AMOURANTH camera (assuming it has position, orientation, etc.)
    void log(LogLevel level, std::string_view category, const AMOURANTH& camera, std::string_view message,
             const std::source_location& location = std::source_location::current()) const;

    // Log uint32_t
    void log(LogLevel level, std::string_view category, std::string_view message, uint32_t value,
             const std::source_location& location = std::source_location::current()) const {
        if (!shouldLog(level, category)) {
            return;
        }
        std::string formatted;
        try {
            formatted = std::vformat(message, std::make_format_args(value));
        } catch (const std::format_error& e) {
            formatted = std::string(message) + " [Format error: " + e.what() + "]";
        }
        enqueueMessage(level, message, category, formatted, location);
    }

    void setLogLevel(LogLevel level) {
        level_.store(level, std::memory_order_relaxed);
        log(LogLevel::Info, "General", "Log level set to: {}", std::source_location::current(), static_cast<int>(level));
    }

    bool setLogFile(const std::string& filename, size_t maxSizeBytes = 10 * 1024 * 1024) {
        std::lock_guard<std::mutex> lock(fileMutex_);
        if (logFile_.is_open()) {
            logFile_.close();
        }
        logFile_.open(filename, std::ios::out | std::ios::app);
        if (!logFile_.is_open()) {
            log(LogLevel::Error, "General", "Failed to open log file: {}", std::source_location::current(), filename);
            return false;
        }
        logFilePath_ = filename;
        maxLogFileSize_ = maxSizeBytes;
        log(LogLevel::Info, "General", "Log file set to: {}", std::source_location::current(), filename);
        return true;
    }

    void setCategoryFilter(std::string_view category, bool enable) {
        std::lock_guard<std::mutex> lock(categoryMutex_);
        if (enable) {
            enabledCategories_.insert(std::string(category));
        } else {
            enabledCategories_.erase(std::string(category));
        }
        log(LogLevel::Info, "General", "Category {} {}", std::source_location::current(), category, enable ? "enabled" : "disabled");
    }

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

    static std::string_view getCategoryColor(std::string_view category) {
        static const std::map<std::string_view, std::string_view, std::less<>> categoryColors = {
            {"General", WHITE},
            {"Vulkan", BLUE},
            {"Simulation", PURPLE},
            {"Renderer", ORANGE},
            {"Engine", GREEN},
            {"Audio", TEAL},
            {"Image", YELLOW_GREEN},
            {"Input", BRIGHT_MAGENTA}
        };
        auto it = categoryColors.find(category);
        return it != categoryColors.end() ? it->second : WHITE;
    }

    bool shouldLog(LogLevel level, std::string_view category) const {
        if (static_cast<int>(level) < static_cast<int>(level_.load(std::memory_order_relaxed))) {
            return false;
        }
        std::lock_guard<std::mutex> lock(categoryMutex_);
        return enabledCategories_.empty() || enabledCategories_.contains(std::string(category));
    }

    void loadCategoryFilters() {
        if (const char* categories = std::getenv("AMOURANTH_LOG_CATEGORIES")) {
            std::string cats(categories);
            std::string_view catsView(cats);
            size_t start = 0;
            size_t end;
            while ((end = catsView.find(',', start)) != std::string_view::npos) {
                std::string_view cat = catsView.substr(start, end - start);
                cat.remove_prefix(std::min(cat.find_first_not_of(" "), cat.size()));
                cat.remove_suffix(std::min(cat.size() - cat.find_last_not_of(" ") - 1, cat.size()));
                if (!cat.empty()) {
                    enabledCategories_.insert(std::string(cat));
                }
                start = end + 1;
            }
            std::string_view cat = catsView.substr(start);
            cat.remove_prefix(std::min(cat.find_first_not_of(" "), cat.size()));
            cat.remove_suffix(std::min(cat.size() - cat.find_last_not_of(" ") - 1, cat.size()));
            if (!cat.empty()) {
                enabledCategories_.insert(std::string(cat));
            }
        }
    }

    template<typename T>
    std::string formatVulkanType(T handle, std::string_view handleName) const {
        if constexpr (std::is_same_v<T, VkExtent2D>) {
            return std::format("{}: {{width: {}, height: {}}}", handleName, handle.width, handle.height);
        } else if constexpr (std::is_same_v<T, VkViewport>) {
            return std::format("{}: {{x: {:.1f}, y: {:.1f}, width: {:.1f}, height: {:.1f}, minDepth: {:.1f}, maxDepth: {:.1f}}}",
                               handleName, handle.x, handle.y, handle.width, handle.height, handle.minDepth, handle.maxDepth);
        } else if constexpr (std::is_same_v<T, VkRect2D>) {
            return std::format("{}: {{offset: {{x: {}, y: {}}}, extent: {{width: {}, height: {}}}}}", handleName,
                               handle.offset.x, handle.offset.y, handle.extent.width, handle.extent.height);
        } else {
            return handle == VK_NULL_HANDLE ?
                   std::format("{}: VK_NULL_HANDLE", handleName) :
                   std::format("{}: {:p}", handleName, static_cast<const void*>(handle));
        }
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
                            std::string_view categoryColor = getCategoryColor(msg.category);
                            std::string_view levelStr;
                            std::string_view levelColor;
                            switch (msg.level) {
                                case LogLevel::Debug:   levelStr = "[DEBUG]"; levelColor = CYAN; break;
                                case LogLevel::Info:    levelStr = "[INFO]";  levelColor = GREEN; break;
                                case LogLevel::Warning: levelStr = "[WARN]";  levelColor = YELLOW; break;
                                case LogLevel::Error:   levelStr = "[ERROR]"; levelColor = MAGENTA; break;
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

                            std::string output = std::format("{}{} [{}] [{}] [{}] {}{}",
                                                             levelColor, levelStr, timeStr, msg.category,
                                                             std::source_location(msg.location), // Use formatter
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
            std::string_view categoryColor = getCategoryColor(msg.category);
            std::string_view levelStr;
            std::string_view levelColor;
            switch (msg.level) {
                case LogLevel::Debug:   levelStr = "[DEBUG]"; levelColor = CYAN; break;
                case LogLevel::Info:    levelStr = "[INFO]";  levelColor = GREEN; break;
                case LogLevel::Warning: levelStr = "[WARN]";  levelColor = YELLOW; break;
                case LogLevel::Error:   levelStr = "[ERROR]"; levelColor = MAGENTA; break;
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

            std::string output = std::format("{}{} [{}] [{}] [{}] {}{}",
                                             levelColor, levelStr, timeStr, msg.category,
                                             std::source_location(msg.location), // Use formatter
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
    mutable std::mutex fileMutex_;
    mutable std::mutex categoryMutex_;
    std::set<std::string> enabledCategories_;
    std::unique_ptr<std::jthread> worker_;
    mutable std::optional<std::chrono::steady_clock::time_point> firstLogTime_;
};

} // namespace Logging

namespace std {
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

// Formatter for glm::vec2
template<>
struct formatter<glm::vec2, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(const glm::vec2& vec, FormatContext& ctx) const {
        return format_to(ctx.out(), "vec2({:.3f}, {:.3f})", vec.x, vec.y);
    }
};

// Formatter for glm::vec3
template<>
struct formatter<glm::vec3, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(const glm::vec3& vec, FormatContext& ctx) const {
        return format_to(ctx.out(), "vec3({:.3f}, {:.3f}, {:.3f})", vec.x, vec.y, vec.z);
    }
};

// Formatter for glm::mat4
template<>
struct formatter<glm::mat4, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(const glm::mat4& mat, FormatContext& ctx) const {
        return format_to(ctx.out(), "mat4[{:.3f}, {:.3f}, {:.3f}, {:.3f}; {:.3f}, {:.3f}, {:.3f}, {:.3f}; {:.3f}, {:.3f}, {:.3f}, {:.3f}; {:.3f}, {:.3f}, {:.3f}, {:.3f}]",
                        mat[0][0], mat[0][1], mat[0][2], mat[0][3],
                        mat[1][0], mat[1][1], mat[1][2], mat[1][3],
                        mat[2][0], mat[2][1], mat[2][2], mat[2][3],
                        mat[3][0], mat[3][1], mat[3][2], mat[3][3]);
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
    std::same_as<T, VkDeviceMemory> ||
    std::same_as<T, VkDevice> ||
    std::same_as<T, VkQueue> ||
    std::same_as<T, VkCommandPool> ||
    std::same_as<T, VkPipeline> ||
    std::same_as<T, VkSwapchainKHR> ||
    std::same_as<T, VkShaderModule> ||
    std::same_as<T, VkSemaphore> ||
    std::same_as<T, VkFence> ||
    std::same_as<T, VkSurfaceKHR> ||
    std::same_as<T, VkImageView> ||
    std::same_as<T, VkDescriptorSetLayout> ||
    std::same_as<T, VkInstance> ||
    std::same_as<T, VkSampler> ||
    std::same_as<T, VkDescriptorPool> ||
    std::same_as<T, VkAccelerationStructureKHR> ||
    std::same_as<T, VkPhysicalDevice>
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

// Formatter for VkExtent2D
template<>
struct formatter<VkExtent2D, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(const VkExtent2D& extent, FormatContext& ctx) const {
        return format_to(ctx.out(), "{{width: {}, height: {}}}", extent.width, extent.height);
    }
};

// Formatter for VkViewport
template<>
struct formatter<VkViewport, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(const VkViewport& viewport, FormatContext& ctx) const {
        return format_to(ctx.out(), "{{x: {:.1f}, y: {:.1f}, width: {:.1f}, height: {:.1f}, minDepth: {:.1f}, maxDepth: {:.1f}}}",
                        viewport.x, viewport.y, viewport.width, viewport.height, viewport.minDepth, viewport.maxDepth);
    }
};

// Formatter for VkRect2D
template<>
struct formatter<VkRect2D, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(const VkRect2D& rect, FormatContext& ctx) const {
        return format_to(ctx.out(), "{{offset: {{x: {}, y: {}}}, extent: {{width: {}, height: {}}}}}",
                        rect.offset.x, rect.offset.y, rect.extent.width, rect.extent.height);
    }
};

// Formatter for VkFormat
template<>
struct formatter<VkFormat, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(VkFormat format, FormatContext& ctx) const {
        switch (format) {
            case VK_FORMAT_UNDEFINED: return format_to(ctx.out(), "VK_FORMAT_UNDEFINED");
            case VK_FORMAT_R8G8B8A8_UNORM: return format_to(ctx.out(), "VK_FORMAT_R8G8B8A8_UNORM");
            case VK_FORMAT_B8G8R8A8_SRGB: return format_to(ctx.out(), "VK_FORMAT_B8G8R8A8_SRGB");
            default: return format_to(ctx.out(), "VkFormat({})", static_cast<int>(format));
        }
    }
};

// Formatter for VkResult
template<>
struct formatter<VkResult, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(VkResult result, FormatContext& ctx) const {
        switch (result) {
            case VK_SUCCESS: return format_to(ctx.out(), "VK_SUCCESS");
            case VK_NOT_READY: return format_to(ctx.out(), "VK_NOT_READY");
            case VK_TIMEOUT: return format_to(ctx.out(), "VK_TIMEOUT");
            case VK_EVENT_SET: return format_to(ctx.out(), "VK_EVENT_SET");
            case VK_EVENT_RESET: return format_to(ctx.out(), "VK_EVENT_RESET");
            case VK_INCOMPLETE: return format_to(ctx.out(), "VK_INCOMPLETE");
            case VK_ERROR_OUT_OF_HOST_MEMORY: return format_to(ctx.out(), "VK_ERROR_OUT_OF_HOST_MEMORY");
            case VK_ERROR_OUT_OF_DEVICE_MEMORY: return format_to(ctx.out(), "VK_ERROR_OUT_OF_DEVICE_MEMORY");
            case VK_ERROR_INITIALIZATION_FAILED: return format_to(ctx.out(), "VK_ERROR_INITIALIZATION_FAILED");
            case VK_ERROR_DEVICE_LOST: return format_to(ctx.out(), "VK_ERROR_DEVICE_LOST");
            case VK_ERROR_MEMORY_MAP_FAILED: return format_to(ctx.out(), "VK_ERROR_MEMORY_MAP_FAILED");
            case VK_ERROR_LAYER_NOT_PRESENT: return format_to(ctx.out(), "VK_ERROR_LAYER_NOT_PRESENT");
            case VK_ERROR_EXTENSION_NOT_PRESENT: return format_to(ctx.out(), "VK_ERROR_EXTENSION_NOT_PRESENT");
            case VK_ERROR_FEATURE_NOT_PRESENT: return format_to(ctx.out(), "VK_ERROR_FEATURE_NOT_PRESENT");
            case VK_ERROR_INCOMPATIBLE_DRIVER: return format_to(ctx.out(), "VK_ERROR_INCOMPATIBLE_DRIVER");
            case VK_ERROR_TOO_MANY_OBJECTS: return format_to(ctx.out(), "VK_ERROR_TOO_MANY_OBJECTS");
            case VK_ERROR_FORMAT_NOT_SUPPORTED: return format_to(ctx.out(), "VK_ERROR_FORMAT_NOT_SUPPORTED");
            case VK_ERROR_FRAGMENTED_POOL: return format_to(ctx.out(), "VK_ERROR_FRAGMENTED_POOL");
            case VK_ERROR_UNKNOWN: return format_to(ctx.out(), "VK_ERROR_UNKNOWN");
            default: return format_to(ctx.out(), "VkResult({})", static_cast<int>(result));
        }
    }
};

// Formatter for SDL_EventType
template<>
struct formatter<SDL_EventType, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(SDL_EventType type, FormatContext& ctx) const {
        switch (type) {
            case SDL_EVENT_FIRST: return format_to(ctx.out(), "SDL_EVENT_FIRST");
            case SDL_EVENT_QUIT: return format_to(ctx.out(), "SDL_EVENT_QUIT");
            case SDL_EVENT_TERMINATING: return format_to(ctx.out(), "SDL_EVENT_TERMINATING");
            case SDL_EVENT_LOW_MEMORY: return format_to(ctx.out(), "SDL_EVENT_LOW_MEMORY");
            case SDL_EVENT_WILL_ENTER_BACKGROUND: return format_to(ctx.out(), "SDL_EVENT_WILL_ENTER_BACKGROUND");
            case SDL_EVENT_DID_ENTER_BACKGROUND: return format_to(ctx.out(), "SDL_EVENT_DID_ENTER_BACKGROUND");
            case SDL_EVENT_WILL_ENTER_FOREGROUND: return format_to(ctx.out(), "SDL_EVENT_WILL_ENTER_FOREGROUND");
            case SDL_EVENT_DID_ENTER_FOREGROUND: return format_to(ctx.out(), "SDL_EVENT_DID_ENTER_FOREGROUND");
            case SDL_EVENT_LOCALE_CHANGED: return format_to(ctx.out(), "SDL_EVENT_LOCALE_CHANGED");
            case SDL_EVENT_SYSTEM_THEME_CHANGED: return format_to(ctx.out(), "SDL_EVENT_SYSTEM_THEME_CHANGED");
            case SDL_EVENT_DISPLAY_ORIENTATION: return format_to(ctx.out(), "SDL_EVENT_DISPLAY_ORIENTATION");
            case SDL_EVENT_DISPLAY_ADDED: return format_to(ctx.out(), "SDL_EVENT_DISPLAY_ADDED");
            case SDL_EVENT_DISPLAY_REMOVED: return format_to(ctx.out(), "SDL_EVENT_DISPLAY_REMOVED");
            case SDL_EVENT_DISPLAY_MOVED: return format_to(ctx.out(), "SDL_EVENT_DISPLAY_MOVED");
            case SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED: return format_to(ctx.out(), "SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED");
            case SDL_EVENT_WINDOW_SHOWN: return format_to(ctx.out(), "SDL_EVENT_WINDOW_SHOWN");
            case SDL_EVENT_WINDOW_HIDDEN: return format_to(ctx.out(), "SDL_EVENT_WINDOW_HIDDEN");
            case SDL_EVENT_WINDOW_EXPOSED: return format_to(ctx.out(), "SDL_EVENT_WINDOW_EXPOSED");
            case SDL_EVENT_WINDOW_MOVED: return format_to(ctx.out(), "SDL_EVENT_WINDOW_MOVED");
            case SDL_EVENT_WINDOW_RESIZED: return format_to(ctx.out(), "SDL_EVENT_WINDOW_RESIZED");
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: return format_to(ctx.out(), "SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED");
            case SDL_EVENT_WINDOW_MINIMIZED: return format_to(ctx.out(), "SDL_EVENT_WINDOW_MINIMIZED");
            case SDL_EVENT_WINDOW_MAXIMIZED: return format_to(ctx.out(), "SDL_EVENT_WINDOW_MAXIMIZED");
            case SDL_EVENT_WINDOW_RESTORED: return format_to(ctx.out(), "SDL_EVENT_WINDOW_RESTORED");
            case SDL_EVENT_WINDOW_MOUSE_ENTER: return format_to(ctx.out(), "SDL_EVENT_WINDOW_MOUSE_ENTER");
            case SDL_EVENT_WINDOW_MOUSE_LEAVE: return format_to(ctx.out(), "SDL_EVENT_WINDOW_MOUSE_LEAVE");
            case SDL_EVENT_WINDOW_FOCUS_GAINED: return format_to(ctx.out(), "SDL_EVENT_WINDOW_FOCUS_GAINED");
            case SDL_EVENT_WINDOW_FOCUS_LOST: return format_to(ctx.out(), "SDL_EVENT_WINDOW_FOCUS_LOST");
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED: return format_to(ctx.out(), "SDL_EVENT_WINDOW_CLOSE_REQUESTED");
            case SDL_EVENT_WINDOW_HIT_TEST: return format_to(ctx.out(), "SDL_EVENT_WINDOW_HIT_TEST");
            case SDL_EVENT_WINDOW_ICCPROF_CHANGED: return format_to(ctx.out(), "SDL_EVENT_WINDOW_ICCPROF_CHANGED");
            case SDL_EVENT_WINDOW_DISPLAY_CHANGED: return format_to(ctx.out(), "SDL_EVENT_WINDOW_DISPLAY_CHANGED");
            case SDL_EVENT_WINDOW_DESTROYED: return format_to(ctx.out(), "SDL_EVENT_WINDOW_DESTROYED");
            case SDL_EVENT_KEY_DOWN: return format_to(ctx.out(), "SDL_EVENT_KEY_DOWN");
            case SDL_EVENT_KEY_UP: return format_to(ctx.out(), "SDL_EVENT_KEY_UP");
            case SDL_EVENT_TEXT_EDITING: return format_to(ctx.out(), "SDL_EVENT_TEXT_EDITING");
            case SDL_EVENT_TEXT_INPUT: return format_to(ctx.out(), "SDL_EVENT_TEXT_INPUT");
            case SDL_EVENT_KEYMAP_CHANGED: return format_to(ctx.out(), "SDL_EVENT_KEYMAP_CHANGED");
            case SDL_EVENT_MOUSE_MOTION: return format_to(ctx.out(), "SDL_EVENT_MOUSE_MOTION");
            case SDL_EVENT_MOUSE_BUTTON_DOWN: return format_to(ctx.out(), "SDL_EVENT_MOUSE_BUTTON_DOWN");
            case SDL_EVENT_MOUSE_BUTTON_UP: return format_to(ctx.out(), "SDL_EVENT_MOUSE_BUTTON_UP");
            case SDL_EVENT_MOUSE_WHEEL: return format_to(ctx.out(), "SDL_EVENT_MOUSE_WHEEL");
            case SDL_EVENT_JOYSTICK_AXIS_MOTION: return format_to(ctx.out(), "SDL_EVENT_JOYSTICK_AXIS_MOTION");
            case SDL_EVENT_JOYSTICK_BALL_MOTION: return format_to(ctx.out(), "SDL_EVENT_JOYSTICK_BALL_MOTION");
            case SDL_EVENT_JOYSTICK_HAT_MOTION: return format_to(ctx.out(), "SDL_EVENT_JOYSTICK_HAT_MOTION");
            case SDL_EVENT_JOYSTICK_BUTTON_DOWN: return format_to(ctx.out(), "SDL_EVENT_JOYSTICK_BUTTON_DOWN");
            case SDL_EVENT_JOYSTICK_BUTTON_UP: return format_to(ctx.out(), "SDL_EVENT_JOYSTICK_BUTTON_UP");
            case SDL_EVENT_JOYSTICK_ADDED: return format_to(ctx.out(), "SDL_EVENT_JOYSTICK_ADDED");
            case SDL_EVENT_JOYSTICK_REMOVED: return format_to(ctx.out(), "SDL_EVENT_JOYSTICK_REMOVED");
            case SDL_EVENT_JOYSTICK_BATTERY_UPDATED: return format_to(ctx.out(), "SDL_EVENT_JOYSTICK_BATTERY_UPDATED");
            case SDL_EVENT_GAMEPAD_AXIS_MOTION: return format_to(ctx.out(), "SDL_EVENT_GAMEPAD_AXIS_MOTION");
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN: return format_to(ctx.out(), "SDL_EVENT_GAMEPAD_BUTTON_DOWN");
            case SDL_EVENT_GAMEPAD_BUTTON_UP: return format_to(ctx.out(), "SDL_EVENT_GAMEPAD_BUTTON_UP");
            case SDL_EVENT_GAMEPAD_ADDED: return format_to(ctx.out(), "SDL_EVENT_GAMEPAD_ADDED");
            case SDL_EVENT_GAMEPAD_REMOVED: return format_to(ctx.out(), "SDL_EVENT_GAMEPAD_REMOVED");
            case SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN: return format_to(ctx.out(), "SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN");
            case SDL_EVENT_GAMEPAD_TOUCHPAD_UP: return format_to(ctx.out(), "SDL_EVENT_GAMEPAD_TOUCHPAD_UP");
            case SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION: return format_to(ctx.out(), "SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION");
            case SDL_EVENT_GAMEPAD_SENSOR_UPDATE: return format_to(ctx.out(), "SDL_EVENT_GAMEPAD_SENSOR_UPDATE");
            case SDL_EVENT_FINGER_DOWN: return format_to(ctx.out(), "SDL_EVENT_FINGER_DOWN");
            case SDL_EVENT_FINGER_UP: return format_to(ctx.out(), "SDL_EVENT_FINGER_UP");
            case SDL_EVENT_FINGER_MOTION: return format_to(ctx.out(), "SDL_EVENT_FINGER_MOTION");
            case SDL_EVENT_CLIPBOARD_UPDATE: return format_to(ctx.out(), "SDL_EVENT_CLIPBOARD_UPDATE");
            case SDL_EVENT_DROP_FILE: return format_to(ctx.out(), "SDL_EVENT_DROP_FILE");
            case SDL_EVENT_DROP_TEXT: return format_to(ctx.out(), "SDL_EVENT_DROP_TEXT");
            case SDL_EVENT_DROP_BEGIN: return format_to(ctx.out(), "SDL_EVENT_DROP_BEGIN");
            case SDL_EVENT_DROP_COMPLETE: return format_to(ctx.out(), "SDL_EVENT_DROP_COMPLETE");
            case SDL_EVENT_DROP_POSITION: return format_to(ctx.out(), "SDL_EVENT_DROP_POSITION");
            case SDL_EVENT_SENSOR_UPDATE: return format_to(ctx.out(), "SDL_EVENT_SENSOR_UPDATE");
            case SDL_EVENT_RENDER_TARGETS_RESET: return format_to(ctx.out(), "SDL_EVENT_RENDER_TARGETS_RESET");
            case SDL_EVENT_RENDER_DEVICE_RESET: return format_to(ctx.out(), "SDL_EVENT_RENDER_DEVICE_RESET");
            case SDL_EVENT_POLL_SENTINEL: return format_to(ctx.out(), "SDL_EVENT_POLL_SENTINEL");
            case SDL_EVENT_USER: return format_to(ctx.out(), "SDL_EVENT_USER");
            case SDL_EVENT_LAST: return format_to(ctx.out(), "SDL_EVENT_LAST");
            default: return format_to(ctx.out(), "Unknown SDL_EventType({})", static_cast<uint32_t>(type));
        }
    }
};

// Formatter for std::source_location
template<>
struct formatter<std::source_location, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(const std::source_location& loc, FormatContext& ctx) const {
        return format_to(ctx.out(), "{}:{}", loc.file_name(), loc.line());
    }
};

} // namespace std

#endif // ENGINE_LOGGING_HPP