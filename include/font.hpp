#ifndef FONT_HPP
#define FONT_HPP

#include <SDL3/SDL.h>
#include <string>
#include <unordered_map>
#include <cstdint>

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

#endif // FONT_HPP