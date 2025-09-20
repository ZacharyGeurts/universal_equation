#ifndef SDL3_FPS_COUNTER_HPP
#define SDL3_FPS_COUNTER_HPP

#include <SDL3/SDL.h>
#include <SDL3/SDL_ttf.h>
#include <string>
#include <sstream>
#include <iomanip>

class FPSCounter {
public:
    FPSCounter(SDL_Window* window, TTF_Font* font) 
        : m_window(window), m_font(font), m_showFPS(false), m_frameCount(0), m_lastTime(0), m_mode(1) {
        m_textColor = {255, 255, 255, 255}; // White text
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
        Uint64 currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - m_lastTime) / 1000.0f;

        if (deltaTime >= 1.0f) { // Update FPS every second
            m_fps = static_cast<int>(m_frameCount / deltaTime);
            m_frameCount = 0;
            m_lastTime = currentTime;

            // Update FPS and mode text
            std::stringstream ss;
            ss << "FPS: " << m_fps << " | Mode: " << m_mode << "D";
            m_fpsText = ss.str();

            // Create texture for FPS and mode text
            SDL_Surface* surface = TTF_RenderText_Blended(m_font, m_fpsText.c_str(), m_fpsText.length(), m_textColor);
            if (!surface) {
                throw std::runtime_error("TTF_RenderText_Blended failed: " + std::string(SDL_GetError()));
            }
            m_texture.reset(SDL_CreateTextureFromSurface(SDL_GetRenderer(m_window), surface));
            SDL_DestroySurface(surface);
            if (!m_texture) {
                throw std::runtime_error("SDL_CreateTextureFromSurface failed: " + std::string(SDL_GetError()));
            }

            // Get texture dimensions
            float w, h;
            SDL_GetTextureSize(m_texture.get(), &w, &h);
            m_destRect = {10, 10, w, h}; // Position at top-left corner (10, 10)
        }
    }

    void render() const {
        if (m_showFPS && m_texture) {
            SDL_RenderTexture(SDL_GetRenderer(m_window), m_texture.get(), nullptr, &m_destRect);
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
    int m_mode; // Current render mode
    std::string m_fpsText;
    SDL_Color m_textColor;
    std::unique_ptr<SDL_Texture, SDLTextureDeleter> m_texture;
    SDL_FRect m_destRect;
};

#endif // SDL3_FPS_COUNTER_HPP