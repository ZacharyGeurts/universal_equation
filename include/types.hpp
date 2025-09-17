#ifndef TYPES_HPP
#define TYPES_HPP

#include "glm/glm.hpp"
#include <SDL3/SDL.h>
#include <string>
#include <unordered_map>
#include <cstdint>

struct DimensionData {
    int dimension;
    double observable;
    double potential;
    double darkMatter;
    double darkEnergy;
};

struct PushConstants {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec3 baseColor;
    float value;
    float dimension;
    float wavePhase;
    float cycleProgress;
    float darkMatter;
    float darkEnergy;
};

struct Glyph {
    SDL_Texture* texture;
    int width;
    int height;
    int advance;
    int offset_x;
    int offset_y;
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
    int char_width_;
    int char_height_;
    bool load_font();
    void free_glyphs();
};

#endif // TYPES_HPP