// SDL3_Console.cpp

#include "SDL3_Console.hpp"

Console::Console(SDL_Window* window, SDL_Renderer* renderer, TTF_Font* font, const ConsoleConfig& config)
    : m_window(window), m_renderer(renderer), m_font(font), m_config(config),
      m_textTexture(nullptr, SDL_DestroyTexture), m_open(false), m_needsUpdate(true),
      m_fps([=]() {
          SDL3_FPS::FPSCounterConfig fpsConfig;
          fpsConfig.fpsUpdateInterval = config.updateInterval;
          fpsConfig.frameTimeWindow = config.frameWindow;
          fpsConfig.logToFile = config.logToFile;
          fpsConfig.logFilePath = config.logFilePath;
          fpsConfig.textColor = config.textColor;
          return SDL3_FPS::FPSCounter(window, font, fpsConfig);
      }()) {
    if (!window || !renderer || !font) {
        throw std::runtime_error("Invalid window, renderer, or font pointer");
    }

    registerDefaultCommands();
}

Console::~Console() {
}

bool Console::handleEvent(const SDL_Event& e) {
    if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_GRAVE) {
        toggle();
        return true;
    }
    if (!m_open) return false;

    if (e.type == SDL_EVENT_TEXT_INPUT) {
        m_input += e.text.text;
        m_needsUpdate = true;
        return true;
    } else if (e.type == SDL_EVENT_KEY_DOWN) {
        if (e.key.key == SDLK_BACKSPACE && !m_input.empty()) {
            m_input.pop_back();
            m_needsUpdate = true;
            return true;
        } else if (e.key.key == SDLK_RETURN) {
            if (!m_input.empty()) {
                processCommand(m_input);
                m_history.push_back("> " + m_input);
                if (m_history.size() > static_cast<size_t>(m_config.maxHistory)) {
                    m_history.pop_front();
                }
                m_input.clear();
                m_needsUpdate = true;
            }
            return true;
        }
    }
    return false;
}

void Console::update() {
    m_fps.update();
    if (m_needsUpdate) {
        redraw();
        m_needsUpdate = false;
    }
}

void Console::render() {
    if (!m_open) return;

    int w, h;
    SDL_GetWindowSizeInPixels(m_window, &w, &h);
    SDL_FRect bgRect = {0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h / 2)};
    SDL_SetRenderDrawColor(m_renderer, m_config.bgColor.r, m_config.bgColor.g, m_config.bgColor.b, m_config.bgColor.a);
    SDL_RenderFillRect(m_renderer, &bgRect);

    if (m_textTexture) {
        SDL_FRect textRect = {10.0f, 10.0f, static_cast<float>(w - 20), static_cast<float>(h / 2 - 20)};
        SDL_RenderTexture(m_renderer, m_textTexture.get(), nullptr, &textRect);
    }
}

bool Console::isOpen() const {
    return m_open.load();
}

void Console::toggle() {
    m_open = !m_open.load();
    m_needsUpdate = true;
    if (!m_open) {
        m_input.clear();
    }
    if (m_open) {
        SDL_StartTextInput(m_window);
    } else {
        SDL_StopTextInput(m_window);
    }
}

void Console::addOutput(const std::string& text) {
    m_history.push_back(text);
    if (m_history.size() > static_cast<size_t>(m_config.maxHistory)) {
        m_history.pop_front();
    }
    m_needsUpdate = true;
}

void Console::redraw() {
    std::stringstream ss;
    ss << m_fps.getStatsString() << "\n";
    for (const auto& line : m_history) {
        ss << line << "\n";
    }
    ss << "> " << m_input;

    SDL_Color color = m_config.textColor;
    SDL_Surface* surface = TTF_RenderText_Blended_Wrapped(m_font, ss.str().c_str(), ss.str().length(), color, 0);
    if (surface) {
        m_textTexture.reset(SDL_CreateTextureFromSurface(m_renderer, surface));
        SDL_DestroySurface(surface);
    } else {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to render console text: %s", SDL_GetError());
    }
}

void Console::processCommand(const std::string& cmd) {
    std::istringstream iss(cmd);
    std::string name;
    iss >> name;
    std::string arg;
    std::getline(iss, arg);
    if (!arg.empty() && arg[0] == ' ') arg.erase(0, 1); // trim leading space

    if (m_commands.find(name) != m_commands.end()) {
        m_commands[name](arg);
    } else {
        addOutput("Unknown command: " + name);
    }
}

void Console::registerDefaultCommands() {
    m_commands["help"] = [this](const std::string&) {
        std::stringstream ss;
        ss << "Available commands: ";
        for (const auto& p : m_commands) {
            ss << p.first << " ";
        }
        addOutput(ss.str());
    };
    m_commands["clear"] = [this](const std::string&) {
        m_history.clear();
        m_needsUpdate = true;
    };
    m_commands["export"] = [this](const std::string&) {
        m_fps.exportBenchmarkStats();
        addOutput("Stats exported");
    };
    m_commands["mode"] = [this](const std::string& arg) {
        try {
            int mode = std::stoi(arg);
            m_fps.setMode(mode);
            addOutput("Set mode to " + std::to_string(mode));
            m_needsUpdate = true;
        } catch (...) {
            addOutput("Invalid mode");
        }
    };
    m_commands["quit"] = [this](const std::string&) {
        SDL_Event evt = {};
        evt.type = SDL_EVENT_QUIT;
        SDL_PushEvent(&evt);
    };
}