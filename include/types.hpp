#ifndef TYPES_HPP
#define TYPES_HPP

#include "glm/glm.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_ttf.h>
#include <string>
#include <unordered_map>

struct DimensionData {
    int dimension;
    double observable, potential, darkMatter, darkEnergy;
};

struct PushConstants {
    glm::mat4 model, view, proj;
    glm::vec3 baseColor;
    float value, dimension, wavePhase, cycleProgress, darkMatter, darkEnergy;
};

struct Glyph {
    SDL_Texture* texture;
    int width, height, advance, offset_x, offset_y;
};

class Font {
public:
    Font(SDL_Renderer* renderer, int char_width, int char_height);
    ~Font();
    void render_text(SDL_Renderer* renderer, const std::string& text, int x, int y, SDL_Color color);
    void measure_text(const std::string& text, int& width, int& height) const;

private:
    std::unordered_map<char, Glyph> glyphs_;
    SDL_Renderer* renderer_;
    TTF_Font* font_ = nullptr;
    int char_width_, char_height_;
    bool load_font();
    void free_glyphs();
};

#endif // TYPES_HPP