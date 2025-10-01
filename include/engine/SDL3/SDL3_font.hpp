// SDL3_font.hpp
#ifndef SDL3_FONT_HPP
#define SDL3_FONT_HPP

// AMOURANTH RTX Engine September 2025 - Font handling for SDL3-based applications.
// Manages asynchronous TTF font loading and cleanup using RAII.
// Provides access to loaded fonts for rendering text in SDL applications.
// Key features:
// - Asynchronous font loading to reduce startup latency.
// - Thread-safe font access via std::future.
// - Comprehensive logging to file and console for debugging and diagnostics.
// - RAII-based resource management for TTF fonts and SDL_ttf subsystem.
// Dependencies: SDL3_ttf (SDL_ttf.h), C++17 standard library (string, fstream, sstream, future, etc.).
// Best Practices:
// - Call initialize() with a valid TTF font path before using getFont().
// - Use getFont() to access the loaded font, which waits for async loading if necessary.
// - Call exportLog() to save diagnostic logs for debugging font issues.
// - On Android, ensure font paths are relative to the assets/ directory.
// Potential Issues:
// - Ensure SDL_ttf is initialized before loading fonts (handled in initialize()).
// - Verify font file accessibility, especially on Android (use assets/ prefix).
// - Handle exceptions from async font loading to prevent crashes.
// Usage example:
//   SDL3Font font("font.log");
//   font.initialize("assets/fonts/custom.ttf");
//   TTF_Font* f = font.getFont();
//   // Use font for rendering text
//   font.exportLog("font_log.txt");
// Zachary Geurts 2025

#include <SDL3_ttf/SDL_ttf.h>
#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <future>
#include <stdexcept>

namespace SDL3Initializer {

class SDL3Font {
public:
    // Constructor: Initializes logging with the specified file path
    // @param logFilePath: Path to the log file (default: "sdl3_font.log")
    explicit SDL3Font(const std::string& logFilePath = "sdl3_font.log")
        : logFile(logFilePath, std::ios::app) {
        logMessage("Constructing SDL3Font");
    }

    // Destructor: Cleans up font resources and closes log file
    ~SDL3Font() {
        logMessage("Destructing SDL3Font");
        cleanup();
        if (logFile.is_open()) {
            logFile.close();
        }
    }

    // Initializes SDL_ttf and starts asynchronous font loading
    // @param fontPath: Path to the TTF font file
    // Throws std::runtime_error if SDL_ttf initialization fails
    void initialize(const std::string& fontPath) {
        logMessage("Initializing TTF");
        if (TTF_Init() == 0) {
            std::string error = "TTF_Init failed: " + std::string(SDL_GetError());
            logMessage(error);
            throw std::runtime_error(error);
        }

        std::string resolvedFontPath = fontPath;
#ifdef __ANDROID__
        // Adjust font path for Android asset system
        if (!resolvedFontPath.empty() && resolvedFontPath[0] != '/') {
            resolvedFontPath = "assets/" + resolvedFontPath;
        }
#endif
        logMessage("Loading TTF font asynchronously: " + resolvedFontPath);
        m_fontFuture = std::async(std::launch::async, [resolvedFontPath]() -> TTF_Font* {
            TTF_Font* font = TTF_OpenFont(resolvedFontPath.c_str(), 24);
            if (!font) {
                std::string error = "TTF_OpenFont failed for " + resolvedFontPath + ": " + std::string(SDL_GetError());
                std::cerr << error << "\n";
                throw std::runtime_error(error);
            }
            return font;
        });
    }

    // Returns the loaded TTF font, waiting for async loading if necessary
    // @return: Pointer to the loaded font, or nullptr if loading failed
    TTF_Font* getFont() const {
        if (m_font == nullptr && m_fontFuture.valid()) {
            try {
                m_font = m_fontFuture.get();
                logMessage("Font loaded successfully");
            } catch (const std::runtime_error& e) {
                logMessage("Font loading failed: " + std::string(e.what()));
                m_font = nullptr;
            }
        }
        logMessage("Getting TTF font");
        return m_font;
    }

    // Exports the font log to a file
    // @param filename: Path to export the log
    void exportLog(const std::string& filename) const {
        logMessage("Exporting log to " + filename);
        std::ofstream out(filename, std::ios::app);
        if (out.is_open()) {
            out << logStream.str();
            out.close();
            std::cout << "Exported log to " << filename << "\n";
        } else {
            std::cerr << "Failed to export log to " << filename << "\n";
        }
    }

private:
    // Cleans up font resources and SDL_ttf subsystem
    void cleanup() {
        logMessage("Starting font cleanup");
        if (m_fontFuture.valid()) {
            try {
                TTF_Font* pending = m_fontFuture.get();
                if (pending) {
                    TTF_CloseFont(pending);
                    logMessage("Closed pending font in cleanup");
                }
            } catch (...) {
                logMessage("Error closing pending font");
            }
        }
        if (m_font) {
            logMessage("Closing TTF font");
            TTF_CloseFont(m_font);
            m_font = nullptr;
        }
        logMessage("Quitting TTF");
        TTF_Quit();
    }

    // Logs a message to console and file with timestamp
    // @param message: Message to log
    void logMessage(const std::string& message) const {
        std::string timestamp = "[" + std::to_string(SDL_GetTicks()) + "ms] " + message;
        std::cout << timestamp << "\n";
        logStream << timestamp << "\n";
        if (logFile.is_open()) {
            logFile << timestamp << "\n";
            logFile.flush();
        }
    }

    mutable std::ofstream logFile; // Log file stream
    mutable std::stringstream logStream; // In-memory log stream
    mutable TTF_Font* m_font = nullptr; // Loaded font
    mutable std::future<TTF_Font*> m_fontFuture; // Async font loading future
};

} // namespace SDL3Initializer

#endif // SDL3_FONT_HPP