#ifndef TYPES_UE_HPP
#define TYPES_UE_HPP

#include <glm/glm.hpp>
#include <SDL3/SDL.h>
#include <SDL3/SDL_ttf.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

// Forward declaration of AMOURANTH class
class AMOURANTH;

// DimensionData struct defined first
struct DimensionData {
    int dimension;
    double observable, potential, darkMatter, darkEnergy;
};

// Forward declarations for renderMode functions
void renderMode1(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer, VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase, const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout);
void renderMode2(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer, VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase, const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout);
void renderMode3(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer, VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase, const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout);
void renderMode4(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer, VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase, const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout);
void renderMode5(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer, VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase, const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout);
void renderMode6(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer, VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase, const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout);
void renderMode7(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer, VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase, const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout);
void renderMode8(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer, VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase, const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout);
void renderMode9(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer, VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase, const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout);

// Include universal_equation.hpp after DimensionData
#include "universal_equation.hpp"

struct PushConstants {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec3 baseColor;
    float value;
    float dimValue;
    float wavePhase;
    float cycleProgress;
    float darkMatter;
    float darkEnergy;
};

struct Glyph {
    SDL_Texture* texture;
    int width, height;
    int advance;
    int offset_x, offset_y;
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

class DimensionalNavigator {
public:
    DimensionalNavigator(const std::string& name, int width, int height)
        : name_(name), width_(width), height_(height), mode_(1), zoomLevel_(1.0f), wavePhase_(0.0f) {}

    int getMode() const { return mode_; }
    float getZoomLevel() const { return zoomLevel_; }
    float getWavePhase() const { return wavePhase_; }
    const std::vector<DimensionData>& getCache() const { return cache_; }

    void setMode(int mode) { mode_ = mode; }
    void setZoomLevel(float zoom) { zoomLevel_ = zoom; }
    void setWavePhase(float phase) { wavePhase_ = phase; }

private:
    std::string name_;
    int width_, height_;
    int mode_;
    float zoomLevel_;
    float wavePhase_;
    std::vector<DimensionData> cache_;
};

class AMOURANTH {
public:
    AMOURANTH(DimensionalNavigator* navigator) {
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

	void render(uint32_t imageIndex, VkBuffer vertexBuffer_, VkCommandBuffer commandBuffer_, VkBuffer indexBuffer_, VkPipelineLayout pipelineLayout_) {
    	switch (simulator_->getMode()) {
        	case 1: renderMode1(this, imageIndex, vertexBuffer_, commandBuffer_, indexBuffer_, simulator_->getZoomLevel(), width_, height_, simulator_->getWavePhase(), this->getCache(), pipelineLayout_); break;
        	case 2: renderMode2(this, imageIndex, vertexBuffer_, commandBuffer_, indexBuffer_, simulator_->getZoomLevel(), width_, height_, simulator_->getWavePhase(), this->getCache(), pipelineLayout_); break;
        	case 3: renderMode3(this, imageIndex, vertexBuffer_, commandBuffer_, indexBuffer_, simulator_->getZoomLevel(), width_, height_, simulator_->getWavePhase(), this->getCache(), pipelineLayout_); break;
        	case 4: renderMode4(this, imageIndex, vertexBuffer_, commandBuffer_, indexBuffer_, simulator_->getZoomLevel(), width_, height_, simulator_->getWavePhase(), this->getCache(), pipelineLayout_); break;
        	case 5: renderMode5(this, imageIndex, vertexBuffer_, commandBuffer_, indexBuffer_, simulator_->getZoomLevel(), width_, height_, simulator_->getWavePhase(), this->getCache(), pipelineLayout_); break;
        	case 6: renderMode6(this, imageIndex, vertexBuffer_, commandBuffer_, indexBuffer_, simulator_->getZoomLevel(), width_, height_, simulator_->getWavePhase(), this->getCache(), pipelineLayout_); break;
        	case 7: renderMode7(this, imageIndex, vertexBuffer_, commandBuffer_, indexBuffer_, simulator_->getZoomLevel(), width_, height_, simulator_->getWavePhase(), this->getCache(), pipelineLayout_); break;
        	case 8: renderMode8(this, imageIndex, vertexBuffer_, commandBuffer_, indexBuffer_, simulator_->getZoomLevel(), width_, height_, simulator_->getWavePhase(), this->getCache(), pipelineLayout_); break;
        	case 9: renderMode9(this, imageIndex, vertexBuffer_, commandBuffer_, indexBuffer_, simulator_->getZoomLevel(), width_, height_, simulator_->getWavePhase(), this->getCache(), pipelineLayout_); break;
        	default: renderMode1(this, imageIndex, vertexBuffer_, commandBuffer_, indexBuffer_, simulator_->getZoomLevel(), width_, height_, simulator_->getWavePhase(), this->getCache(), pipelineLayout_); break;
    	}
	}

    void adjustInfluence(double delta) {
        ue_.setInfluence(ue_.getInfluence() + delta);
        updateCache();
    }

    void adjustDarkMatter(double delta) {
        for (auto& cache : cache_) {
            cache.darkMatter += delta;
        }
    }

    void adjustDarkEnergy(double delta) {
        for (auto& cache : cache_) {
            cache.darkEnergy += delta;
        }
    }

    void updateCache() {
        auto result = ue_.compute();
        for (size_t i = 0; i < cache_.size(); ++i) {
            cache_[i].observable = result.observable;
            cache_[i].potential = result.potential;
            cache_[i].darkMatter = result.darkMatter;
            cache_[i].darkEnergy = result.darkEnergy;
        }
    }

    void updateZoom(bool zoomIn) {
        zoomLevel_ *= zoomIn ? 1.1f : 0.9f;
        zoomLevel_ = std::max(0.1f, zoomLevel_);
        simulator_->setZoomLevel(zoomLevel_);
    }

    void update(float deltaTime) {
        if (!isPaused_) {
            wavePhase_ += waveSpeed_ * deltaTime;
            simulator_->setWavePhase(wavePhase_);
            ue_.advanceCycle();
            updateCache();
        }
    }

    void handleInput(const SDL_KeyboardEvent& key) {
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

    void setMode(int mode) { 
        mode_ = mode; 
        simulator_->setMode(mode_);
    }

    void setCurrentDimension(int dimension) { ue_.setCurrentDimension(dimension); }
    bool getDebug() const { return ue_.getDebug(); }
    double computeInteraction(int vertexIndex, double distance) const { return ue_.computeInteraction(vertexIndex, distance); }
    double computePermeation(int vertexIndex) const { return ue_.computePermeation(vertexIndex); }
    double computeDarkEnergy(double distance) const { return ue_.computeDarkEnergy(distance); }
    double getAlpha() const { return ue_.getAlpha(); }

    const std::vector<glm::vec3>& getSphereVertices() const { return sphereVertices_; }
    const std::vector<uint32_t>& getSphereIndices() const { return sphereIndices_; }
    const std::vector<glm::vec3>& getQuadVertices() const { return quadVertices_; }
    const std::vector<uint32_t>& getQuadIndices() const { return quadIndices_; }
    const std::vector<DimensionData>& getCache() const { return cache_; }
    int getMode() const { return mode_; }
    float getWavePhase() const { return wavePhase_; }
    float getZoomLevel() const { return zoomLevel_; }
    glm::vec3 getUserCamPos() const { return userCamPos_; }
    bool isUserCamActive() const { return isUserCamActive_; }
    UniversalEquation::EnergyResult getEnergyResult() const { return ue_.compute(); }
    const std::vector<UniversalEquation::DimensionInteraction>& getInteractions() const { return ue_.getInteractions(); }

private:
    void initializeSphereGeometry() {
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

    void initializeQuadGeometry() {
        quadVertices_ = {
            {-1.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f},
            {1.0f, 1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f}
        };
        quadIndices_ = {0, 1, 2, 2, 3, 0};
    }

    void initializeCalculator() {
        for (int i = 0; i < kMaxRenderedDimensions; ++i) {
            cache_.emplace_back();
            cache_[i].dimension = i + 1;
            cache_[i].observable = 1.0;
            cache_[i].potential = 0.0;
            cache_[i].darkMatter = 0.0;
            cache_[i].darkEnergy = 0.0;
        }
    }

    UniversalEquation ue_;
    std::vector<DimensionData> cache_;
    std::vector<glm::vec3> sphereVertices_;
    std::vector<uint32_t> sphereIndices_;
    std::vector<glm::vec3> quadVertices_;
    std::vector<uint32_t> quadIndices_;
    DimensionalNavigator* simulator_;
    int mode_;
    float wavePhase_;
    float waveSpeed_;
    float zoomLevel_;
    bool isPaused_;
    glm::vec3 userCamPos_;
    bool isUserCamActive_;
    int width_ = 800;
    int height_ = 600;
    static constexpr int kMaxRenderedDimensions = 9;
};

#endif // TYPES_UE_HPP