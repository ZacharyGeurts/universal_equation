#include "types.hpp"
#include <stdexcept>

using GlyphBitmap = uint8_t[8];

static const uint8_t unique_rows[] = {
    0b00000000, // 0
    0b11111110, // 1
    0b11110000, // 2
    0b01111000, // 3
    0b00111110, // 4
    0b00011110, // 5
    0b00011000, // 6
    0b10000010, // 7
    0b10000000, // 8
    0b00000010, // 9
    0b01000000, // 10
    0b00000100, // 11
    0b00001000, // 12
    0b00111000, // 13
    0b10000100, // 14
    0b01100000, // 15
    0b00100000, // 16
    0b00010000, // 17
    0b11000000, // 18
    0b00110000, // 19
    0b00000110, // 20
    0b01000100, // 21
    0b01000010, // 22
    0b10100000, // 23
    0b11000100, // 24
    0b10100100, // 25
    0b10011000, // 26
    0b00100100, // 27
    0b00001110, // 28
    0b11111000, // 29
    0b11000010, // 30
    0b10001000, // 31
    0b11011000  // 32
};

static const GlyphBitmap glyph_A = {3, 7, 9, 1, 7, 7, 7, 7};
static const GlyphBitmap glyph_B = {2, 7, 7, 1, 7, 7, 7, 2};
static const GlyphBitmap glyph_C = {3, 9, 8, 8, 8, 8, 9, 3};
static const GlyphBitmap glyph_D = {2, 7, 9, 9, 9, 9, 7, 2};
static const GlyphBitmap glyph_E = {1, 8, 8, 1, 8, 8, 8, 1};
static const GlyphBitmap glyph_F = {1, 8, 8, 1, 8, 8, 8, 8};
static const GlyphBitmap glyph_G = {3, 9, 8, 8, 4, 9, 9, 3};
static const GlyphBitmap glyph_H = {7, 7, 7, 1, 7, 7, 7, 7};
static const GlyphBitmap glyph_I = {6, 11, 11, 11, 11, 11, 11, 6};
static const GlyphBitmap glyph_J = {9, 9, 9, 9, 9, 9, 7, 5};
static const GlyphBitmap glyph_K = {7, 9, 12, 2, 12, 9, 7, 9};
static const GlyphBitmap glyph_L = {8, 8, 8, 8, 8, 8, 8, 1};
static const GlyphBitmap glyph_M = {7, 30, 25, 14, 7, 7, 7, 7};
static const GlyphBitmap glyph_N = {7, 30, 25, 14, 31, 30, 7, 7};
static const GlyphBitmap glyph_O = {3, 9, 9, 9, 9, 9, 9, 3};
static const GlyphBitmap glyph_P = {2, 7, 7, 1, 8, 8, 8, 8};
static const GlyphBitmap glyph_Q = {3, 9, 9, 9, 31, 30, 9, 4};
static const GlyphBitmap glyph_R = {2, 7, 7, 1, 9, 9, 7, 7};
static const GlyphBitmap glyph_S = {3, 9, 8, 3, 9, 12, 9, 3};
static const GlyphBitmap glyph_T = {1, 11, 11, 11, 11, 11, 11, 11};
static const GlyphBitmap glyph_U = {7, 7, 7, 7, 7, 7, 9, 3};
static const GlyphBitmap glyph_V = {7, 7, 7, 9, 9, 22, 22, 11};
static const GlyphBitmap glyph_W = {7, 7, 7, 14, 25, 25, 30, 7};
static const GlyphBitmap glyph_X = {7, 9, 22, 11, 22, 9, 7, 7};
static const GlyphBitmap glyph_Y = {7, 9, 22, 11, 11, 11, 11, 11};
static const GlyphBitmap glyph_Z = {1, 9, 12, 17, 16, 10, 9, 1};
static const GlyphBitmap glyph_a = {0, 0, 3, 9, 9, 4, 9, 9};
static const GlyphBitmap glyph_b = {0, 8, 8, 3, 7, 9, 7, 3};
static const GlyphBitmap glyph_c = {0, 0, 3, 8, 8, 8, 8, 3};
static const GlyphBitmap glyph_d = {0, 9, 9, 3, 7, 9, 7, 3};
static const GlyphBitmap glyph_e = {0, 0, 3, 8, 1, 8, 8, 3};
static const GlyphBitmap glyph_f = {6, 10, 10, 1, 10, 10, 10, 10};
static const GlyphBitmap glyph_g = {0, 0, 3, 9, 9, 4, 9, 3};
static const GlyphBitmap glyph_h = {0, 8, 8, 3, 7, 7, 7, 7};
static const GlyphBitmap glyph_i = {0, 11, 0, 11, 11, 11, 11, 11};
static const GlyphBitmap glyph_j = {0, 9, 0, 9, 9, 9, 7, 5};
static const GlyphBitmap glyph_k = {0, 8, 8, 9, 12, 2, 12, 9};
static const GlyphBitmap glyph_l = {0, 11, 11, 11, 11, 11, 11, 11};
static const GlyphBitmap glyph_m = {0, 0, 30, 14, 7, 7, 7, 7};
static const GlyphBitmap glyph_n = {0, 0, 3, 7, 7, 7, 7, 7};
static const GlyphBitmap glyph_o = {0, 0, 3, 9, 9, 9, 9, 3};
static const GlyphBitmap glyph_p = {0, 0, 3, 7, 9, 3, 8, 8};
static const GlyphBitmap glyph_q = {0, 0, 3, 7, 9, 4, 9, 9};
static const GlyphBitmap glyph_r = {0, 0, 3, 8, 8, 8, 8, 8};
static const GlyphBitmap glyph_s = {0, 0, 3, 8, 3, 9, 12, 3};
static const GlyphBitmap glyph_t = {11, 11, 1, 11, 11, 11, 11, 11};
static const GlyphBitmap glyph_u = {0, 0, 7, 7, 7, 7, 7, 3};
static const GlyphBitmap glyph_v = {0, 0, 7, 7, 9, 9, 22, 11};
static const GlyphBitmap glyph_w = {0, 0, 7, 7, 14, 14, 30, 7};
static const GlyphBitmap glyph_x = {0, 0, 7, 9, 11, 9, 7, 7};
static const GlyphBitmap glyph_y = {0, 0, 7, 7, 7, 4, 9, 3};
static const GlyphBitmap glyph_z = {0, 0, 1, 9, 17, 16, 10, 1};
static const GlyphBitmap glyph_0 = {3, 9, 9, 9, 9, 9, 9, 3};
static const GlyphBitmap glyph_1 = {11, 14, 11, 11, 11, 11, 11, 6};
static const GlyphBitmap glyph_2 = {3, 9, 9, 17, 16, 10, 9, 1};
static const GlyphBitmap glyph_3 = {3, 9, 9, 3, 9, 9, 9, 3};
static const GlyphBitmap glyph_4 = {9, 14, 25, 31, 7, 1, 9, 9};
static const GlyphBitmap glyph_5 = {1, 8, 8, 3, 9, 9, 9, 3};
static const GlyphBitmap glyph_6 = {3, 8, 8, 3, 7, 9, 9, 3};
static const GlyphBitmap glyph_7 = {1, 9, 17, 16, 10, 10, 10, 10};
static const GlyphBitmap glyph_8 = {3, 9, 9, 3, 9, 9, 9, 3};
static const GlyphBitmap glyph_9 = {3, 9, 9, 7, 4, 9, 9, 3};
static const GlyphBitmap glyph_period = {0, 0, 0, 0, 0, 0, 6, 6};
static const GlyphBitmap glyph_comma = {0, 0, 0, 0, 0, 0, 6, 20};
static const GlyphBitmap glyph_exclamation = {11, 11, 11, 11, 11, 0, 11, 11};
static const GlyphBitmap glyph_question = {3, 9, 9, 17, 13, 0, 11, 11};
static const GlyphBitmap glyph_semicolon = {0, 11, 11, 0, 0, 0, 11, 20};
static const GlyphBitmap glyph_colon = {0, 0, 6, 6, 0, 6, 6, 0};
static const GlyphBitmap glyph_single_quote = {11, 11, 11, 0, 0, 0, 0, 0};
static const GlyphBitmap glyph_double_quote = {27, 27, 27, 0, 0, 0, 0, 0};

static const std::unordered_map<char, const GlyphBitmap*> char_to_bitmap = {
    {'A', &glyph_A}, {'B', &glyph_B}, {'C', &glyph_C}, {'D', &glyph_D}, {'E', &glyph_E},
    {'F', &glyph_F}, {'G', &glyph_G}, {'H', &glyph_H}, {'I', &glyph_I}, {'J', &glyph_J},
    {'K', &glyph_K}, {'L', &glyph_L}, {'M', &glyph_M}, {'N', &glyph_N}, {'O', &glyph_O},
    {'P', &glyph_P}, {'Q', &glyph_Q}, {'R', &glyph_R}, {'S', &glyph_S}, {'T', &glyph_T},
    {'U', &glyph_U}, {'V', &glyph_V}, {'W', &glyph_W}, {'X', &glyph_X}, {'Y', &glyph_Y},
    {'Z', &glyph_Z}, {'a', &glyph_a}, {'b', &glyph_b}, {'c', &glyph_c}, {'d', &glyph_d},
    {'e', &glyph_e}, {'f', &glyph_f}, {'g', &glyph_g}, {'h', &glyph_h}, {'i', &glyph_i},
    {'j', &glyph_j}, {'k', &glyph_k}, {'l', &glyph_l}, {'m', &glyph_m}, {'n', &glyph_n},
    {'o', &glyph_o}, {'p', &glyph_p}, {'q', &glyph_q}, {'r', &glyph_r}, {'s', &glyph_s},
    {'t', &glyph_t}, {'u', &glyph_u}, {'v', &glyph_v}, {'w', &glyph_w}, {'x', &glyph_x},
    {'y', &glyph_y}, {'z', &glyph_z}, {'0', &glyph_0}, {'1', &glyph_1}, {'2', &glyph_2},
    {'3', &glyph_3}, {'4', &glyph_4}, {'5', &glyph_5}, {'6', &glyph_6}, {'7', &glyph_7},
    {'8', &glyph_8}, {'9', &glyph_9}, {'.', &glyph_period}, {',', &glyph_comma},
    {'!', &glyph_exclamation}, {'?', &glyph_question}, {';', &glyph_semicolon},
    {':', &glyph_colon}, {'\'', &glyph_single_quote}, {'"', &glyph_double_quote}
};

Font::Font(SDL_Renderer* renderer, int char_width, int char_height)
    : renderer_(renderer), char_width_(char_width), char_height_(char_height) {
    if (!load_font()) {
        throw std::runtime_error("Failed to load font");
    }
}

Font::~Font() {
    free_glyphs();
}

bool Font::load_font() {
    for (const auto& pair : char_to_bitmap) {
        char c = pair.first;
        const GlyphBitmap* row_indices = pair.second;

        SDL_Surface* surface = SDL_CreateSurface(char_width_, char_height_, SDL_PIXELFORMAT_RGBA32);
        if (!surface) {
            SDL_Log("Failed to create glyph surface: %s", SDL_GetError());
            free_glyphs();
            return false;
        }

        if (SDL_LockSurface(surface) != 0) {
            SDL_Log("Failed to lock surface: %s", SDL_GetError());
            SDL_DestroySurface(surface);
            free_glyphs();
            return false;
        }

        const SDL_PixelFormatDetails* format = SDL_GetPixelFormatDetails(surface->format);
        uint32_t* pixels = static_cast<uint32_t*>(surface->pixels);
        uint32_t white = SDL_MapRGBA(format, nullptr, 255, 255, 255, 255);
        uint32_t transparent = SDL_MapRGBA(format, nullptr, 0, 0, 0, 0);

        for (int y = 0; y < char_height_; ++y) {
            uint8_t row_data = unique_rows[(*row_indices)[y]];
            for (int x = 0; x < char_width_; ++x) {
                bool pixel_on = (row_data & (1 << (7 - x))) != 0;
                pixels[y * surface->w + x] = pixel_on ? white : transparent;
            }
        }

        SDL_UnlockSurface(surface);

        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
        SDL_DestroySurface(surface);
        if (!texture) {
            SDL_Log("Failed to create glyph texture: %s", SDL_GetError());
            free_glyphs();
            return false;
        }

        Glyph glyph;
        glyph.texture = texture;
        glyph.width = char_width_;
        glyph.height = char_height_;
        glyph.advance = char_width_;
        glyph.offset_x = 0;
        glyph.offset_y = 0;

        glyphs_[c] = glyph;
    }

    return true;
}

void Font::render_text(SDL_Renderer* renderer, const std::string& text, int x, int y, SDL_Color color) {
    int current_x = x;

    for (char c : text) {
        auto it = glyphs_.find(c);
        if (it == glyphs_.end()) {
            continue;
        }

        const Glyph& glyph = it->second;
        SDL_SetTextureColorMod(glyph.texture, color.r, color.g, color.b);
        SDL_SetTextureAlphaMod(glyph.texture, color.a);
        SDL_FRect dst_rect = {static_cast<float>(current_x + glyph.offset_x), 
                              static_cast<float>(y + glyph.offset_y), 
                              static_cast<float>(glyph.width), 
                              static_cast<float>(glyph.height)};
        SDL_RenderTexture(renderer, glyph.texture, nullptr, &dst_rect);
        current_x += glyph.advance;
    }
}

void Font::measure_text(const std::string& text, int& width, int& height) const {
    width = 0;
    height = char_height_;

    for (char c : text) {
        auto it = glyphs_.find(c);
        if (it != glyphs_.end()) {
            width += it->second.advance;
        } else {
            width += char_width_;
        }
    }
}

void Font::free_glyphs() {
    for (auto& pair : glyphs_) {
        SDL_DestroyTexture(pair.second.texture);
    }
    glyphs_.clear();
}