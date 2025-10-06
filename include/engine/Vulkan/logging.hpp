#ifndef ENGINE_LOGGING_HPP
#define ENGINE_LOGGING_HPP
// AMOURANTH RTX Engine, October 2025 - Simple ANSI-colored console logging.
// Supports C++20, used by Vulkan pipeline and swapchain utilities.
// Zachary Geurts 2025

#include <string>
#include <string_view>
#include <iostream>
#include <format>
#include <syncstream>
#include <source_location>

// ANSI color codes
#define RESET "\033[0m"
#define MAGENTA "\033[1;35m" // Bold magenta for errors
#define CYAN "\033[1;36m"    // Bold cyan for debug
#define YELLOW "\033[1;33m"  // Bold yellow for warnings
#define GREEN "\033[1;32m"   // Bold green for info

namespace Logging {

enum class LogLevel { Info, Debug, Warning, Error };

class Logger {
public:
    template<typename... Args>
    void log(LogLevel level, std::string_view message, const std::source_location& loc = std::source_location::current(), Args&&... args) const {
        std::string color;
        std::string levelStr;
        switch (level) {
            case LogLevel::Info:    color = GREEN;   levelStr = "[INFO]";    break;
            case LogLevel::Debug:   color = CYAN;    levelStr = "[DEBUG]";   break;
            case LogLevel::Warning: color = YELLOW;  levelStr = "[WARNING]"; break;
            case LogLevel::Error:   color = MAGENTA; levelStr = "[ERROR]";   break;
        }
        std::osyncstream(std::cout) << color << levelStr << " [" << loc.file_name() << ":" << loc.line() << "] "
                                    << std::vformat(message, std::make_format_args(args...)) << RESET << std::endl;
    }
};

} // namespace Logging

#endif // ENGINE_LOGGING_HPP