#include "core.hpp"
#include <cmath>

// Constructor
AMOURANTH::AMOURANTH(DimensionalNavigator* navigator) {
    simulator_ = navigator;
    initializeSphereGeometry();
    initializeQuadGeometry();
    initializeCalculator();
    waveSpeed_ = 1.0f;
    wavePhase_ = 0.0f;
    zoomLevel_ = 1.0f;
    isPaused_ = false;
    isUserCamActive_ = false;
    mode_ = 1;
    userCamPos_ = glm::vec3(0.0f, 0.0f, 0.0f);
}

// Render method (already inline in Core.hpp, but ensuring completeness)
void AMOURANTH::render(uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                       VkBuffer indexBuffer, VkPipelineLayout pipelineLayout) {
    switch (simulator_->getMode()) {
        case 1: renderMode1(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(), width_, height_, simulator_->getWavePhase(), getCache(), pipelineLayout); break;
        case 2: renderMode2(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(), width_, height_, simulator_->getWavePhase(), getCache(), pipelineLayout); break;
        case 3: renderMode3(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(), width_, height_, simulator_->getWavePhase(), getCache(), pipelineLayout); break;
        case 4: renderMode4(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(), width_, height_, simulator_->getWavePhase(), getCache(), pipelineLayout); break;
        case 5: renderMode5(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(), width_, height_, simulator_->getWavePhase(), getCache(), pipelineLayout); break;
        case 6: renderMode6(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(), width_, height_, simulator_->getWavePhase(), getCache(), pipelineLayout); break;
        case 7: renderMode7(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(), width_, height_, simulator_->getWavePhase(), getCache(), pipelineLayout); break;
        case 8: renderMode8(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(), width_, height_, simulator_->getWavePhase(), getCache(), pipelineLayout); break;
        case 9: renderMode9(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(), width_, height_, simulator_->getWavePhase(), getCache(), pipelineLayout); break;
        default: renderMode1(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(), width_, height_, simulator_->getWavePhase(), getCache(), pipelineLayout); break;
    }
}

// Update method
void AMOURANTH::update(float deltaTime) {
    if (!isPaused_) {
        wavePhase_ += waveSpeed_ * deltaTime;
        simulator_->setWavePhase(wavePhase_);
        ue_.advanceCycle();
        updateCache();
    }
}

// Handle input method
void AMOURANTH::handleInput(const SDL_KeyboardEvent& key) {
    if (key.type == SDL_EVENT_KEY_DOWN) {
        switch (key.key) {
            case SDLK_PLUS: case SDLK_KP_PLUS: updateZoom(true); break;
            case SDLK_MINUS: case SDLK_KP_MINUS: updateZoom(false); break;
            case SDLK_I: adjustInfluence(0.1); break;
            case SDLK_O: adjustInfluence(-0.1); break;
            case SDLK_M: adjustDarkMatter(0.1); break;
            case SDLK_N: adjustDarkMatter(-0.1); break;
            case SDLK_J: adjustDarkEnergy(0.1); break;
            case SDLK_K: adjustDarkEnergy(-0.1); break;
            case SDLK_P: isPaused_ = !isPaused_; break;
            case SDLK_C: isUserCamActive_ = !isUserCamActive_; break;
            case SDLK_W: if (isUserCamActive_) userCamPos_.z -= 0.1f; break;
            case SDLK_S: if (isUserCamActive_) userCamPos_.z += 0.1f; break;
            case SDLK_A: if (isUserCamActive_) userCamPos_.x -= 0.1f; break;
            case SDLK_D: if (isUserCamActive_) userCamPos_.x += 0.1f; break;
            case SDLK_Q: if (isUserCamActive_) userCamPos_.y += 0.1f; break;
            case SDLK_E: if (isUserCamActive_) userCamPos_.y -= 0.1f; break;
            case SDLK_1: case SDLK_2: case SDLK_3: case SDLK_4: case SDLK_5:
            case SDLK_6: case SDLK_7: case SDLK_8: case SDLK_9:
                mode_ = key.key - SDLK_0;
                simulator_->setMode(mode_);
                break;
        }
    }
}

// Update cache method
void AMOURANTH::updateCache() {
    auto result = ue_.compute();
    for (size_t i = 0; i < cache_.size(); ++i) {
        cache_[i].observable = result.observable;
        cache_[i].potential = result.potential;
        cache_[i].darkMatter = result.darkMatter;
        cache_[i].darkEnergy = result.darkEnergy;
    }
}

// Initialize sphere geometry
void AMOURANTH::initializeSphereGeometry() {
    float radius = 1.0f;
    uint32_t sectors = 32, rings = 16;
    for (uint32_t i = 0; i <= rings; ++i) {
        float theta = i * M_PI / rings;
        float sinTheta = sin(theta), cosTheta = cos(theta);
        for (uint32_t j = 0; j <= sectors; ++j) {
            float phi = j * 2 * M_PI / sectors;
            float sinPhi = sin(phi), cosPhi = cos(phi);
            glm::vec3 vertex(radius * cosPhi * sinTheta, radius * cosTheta, radius * sinPhi * sinTheta);
            sphereVertices_.push_back(vertex);
        }
    }
    for (uint32_t i = 0; i < rings; ++i) {
        for (uint32_t j = 0; j < sectors; ++j) {
            uint32_t first = i * (sectors + 1) + j, second = first + sectors + 1;
            sphereIndices_.push_back(first);
            sphereIndices_.push_back(second);
            sphereIndices_.push_back(first + 1);
            sphereIndices_.push_back(second);
            sphereIndices_.push_back(second + 1);
            sphereIndices_.push_back(first + 1);
        }
    }
}

// Initialize quad geometry
void AMOURANTH::initializeQuadGeometry() {
    quadVertices_ = {
        {-1.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f},
        {1.0f, 1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f}
    };
    quadIndices_ = {0, 1, 2, 2, 3, 0};
}

// Initialize calculator
void AMOURANTH::initializeCalculator() {
    for (int i = 0; i < kMaxRenderedDimensions; ++i) {
        cache_.emplace_back();
        cache_[i].dimension = i + 1;
        cache_[i].observable = 1.0;
        cache_[i].potential = 0.0;
        cache_[i].darkMatter = 0.0;
        cache_[i].darkEnergy = 0.0;
    }
}

// Font class implementations
Font::Font(SDL_Renderer* renderer, int char_width, int char_height)
    : renderer_(renderer), char_width_(char_width), char_height_(char_height) {
    if (!load_font()) {
        throw std::runtime_error("Failed to load font");
    }
}

Font::~Font() {
    free_glyphs();
    if (font_) {
        TTF_CloseFont(font_);
        font_ = nullptr;
    }
}

bool Font::load_font() {
    font_ = TTF_OpenFont("assets/fonts/arial.ttf", char_height_);
    if (!font_) {
        return false;
    }
    SDL_Color white = {255, 255, 255, 255};
    for (char c = 32; c <= 126; ++c) {
        SDL_Surface* surface = TTF_RenderGlyph_Solid(font_, c, white);
        if (!surface) continue;
        Glyph glyph;
        glyph.texture = SDL_CreateTextureFromSurface(renderer_, surface);
        glyph.width = surface->w;
        glyph.height = surface->h;
        TTF_GetGlyphMetrics(font_, c, nullptr, nullptr, nullptr, nullptr, &glyph.advance);
        glyph.offset_x = 0;
        glyph.offset_y = 0;
        glyphs_[c] = glyph;
        SDL_DestroySurface(surface);
    }
    return true;
}

void Font::free_glyphs() {
    for (auto& pair : glyphs_) {
        if (pair.second.texture) {
            SDL_DestroyTexture(pair.second.texture);
            pair.second.texture = nullptr;
        }
    }
    glyphs_.clear();
}

void Font::render_text(SDL_Renderer* renderer, const std::string& text, int x, int y, SDL_Color color) {
    int current_x = x;
    for (char c : text) {
        auto it = glyphs_.find(c);
        if (it == glyphs_.end()) continue;
        const Glyph& glyph = it->second;
        SDL_SetTextureColorMod(glyph.texture, color.r, color.g, color.b);
        SDL_SetTextureAlphaMod(glyph.texture, color.a);
        SDL_FRect dst = {static_cast<float>(current_x + glyph.offset_x), static_cast<float>(y + glyph.offset_y),
                         static_cast<float>(glyph.width), static_cast<float>(glyph.height)};
        SDL_RenderTexture(renderer, glyph.texture, nullptr, &dst);
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
        }
    }
}