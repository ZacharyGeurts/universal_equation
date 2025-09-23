#ifndef CORE_HPP
#define CORE_HPP

// AMOURANTH RTX Engine Core
// This header defines the core components of the AMOURANTH RTX engine, serving as the primary entry point for developers.
// The engine integrates Vulkan-based ray tracing (VulkanRTX) and core initialization (VulkanInitializer) with a physics-based
// simulation framework for rendering multidimensional phenomena, driven by the UniversalEquation and DimensionalNavigator classes.

#include <glm/glm.hpp>
#include <SDL3/SDL.h>
#include <SDL3/SDL_ttf.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

#include "universal_equation.hpp"
#include "Vulkan_RTX.hpp"
#include "Vulkan_init.hpp"

// Structure to hold data for each rendered dimension, used for caching physics simulation results.
struct DimensionData {
    int dimension;           // Dimension index (1 to kMaxRenderedDimensions)
    double observable;       // Observable energy component
    double potential;        // Potential energy component
    double darkMatter;       // Dark matter influence
    double darkEnergy;       // Dark energy influence
};

// Push constants for passing transformation and simulation data to Vulkan shaders.
struct PushConstants {
    glm::mat4 model;         // Model transformation matrix
    glm::mat4 view;          // View transformation matrix
    glm::mat4 proj;          // Projection transformation matrix
    glm::vec3 baseColor;     // Base color for rendering
    float value;             // General simulation value
    float dimValue;          // Dimension-specific value
    float wavePhase;         // Wave phase for dynamic effects
    float cycleProgress;     // Simulation cycle progress
    float darkMatter;        // Dark matter influence for shaders
    float darkEnergy;        // Dark energy influence for shaders
};

// Structure for storing font glyph data for text rendering with SDL_ttf.
struct Glyph {
    SDL_Texture* texture;     // Texture for the glyph
    int width, height;       // Glyph dimensions
    int advance;             // Horizontal advance for text layout
    int offset_x, offset_y;  // Offset for positioning
};

// Font class for rendering text overlays (e.g., debug information or UI).
class Font {
public:
    Font(SDL_Renderer* renderer, int char_width, int char_height);
    ~Font();
    void render_text(SDL_Renderer* renderer, const std::string& text, int x, int y, SDL_Color color);
    void measure_text(const std::string& text, int& width, int& height) const;

private:
    bool load_font();
    void free_glyphs();

    std::unordered_map<char, Glyph> glyphs_;
    SDL_Renderer* renderer_;
    TTF_Font* font_ = nullptr;
    int char_width_, char_height_;
};

// Manages navigation and state for multidimensional rendering.
class DimensionalNavigator {
public:
    DimensionalNavigator(const std::string& name, int width, int height)
        : name_(name), width_(width), height_(height), mode_(1), zoomLevel_(1.0f), wavePhase_(0.0f) {}

    int getMode() const { return mode_; }
    float getZoomLevel() const { return zoomLevel_; }
    float getWavePhase() const { return wavePhase_; }
    const std::vector<DimensionData>& getCache() const { return cache_; }

    void setMode(int mode) { mode_ = std::clamp(mode, 1, 9); }
    void setZoomLevel(float zoom) { zoomLevel_ = std::max(0.1f, zoom); }
    void setWavePhase(float phase) { wavePhase_ = phase; }

private:
    std::string name_;
    int width_, height_;
    int mode_;
    float zoomLevel_;
    float wavePhase_;
    std::vector<DimensionData> cache_;
};

// Core class of the AMOURANTH RTX engine, orchestrating rendering and simulation.
class AMOURANTH {
public:
    AMOURANTH(DimensionalNavigator* navigator); // Constructor to be defined in Core.cpp

    void render(uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                VkBuffer indexBuffer, VkPipelineLayout pipelineLayout);

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

    void updateCache();

    void updateZoom(bool zoomIn) {
        zoomLevel_ *= zoomIn ? 1.1f : 0.9f;
        zoomLevel_ = std::max(0.1f, zoomLevel_);
        simulator_->setZoomLevel(zoomLevel_);
    }

    void update(float deltaTime);

    void handleInput(const SDL_KeyboardEvent& key);

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
    void initializeSphereGeometry();
    void initializeQuadGeometry();
    void initializeCalculator();

    UniversalEquation ue_;
    std::vector<DimensionData> cache_;
    std::vector<glm::vec3> sphereVertices_;
    std::vector<uint32_t> sphereIndices_;
    std::vector<glm::vec3> quadVertices_;
    std::vector<uint32_t> quadIndices_;
    DimensionalNavigator* simulator_;
    int mode_ = 1;
    float wavePhase_ = 0.0f;
    float waveSpeed_ = 1.0f;
    float zoomLevel_ = 1.0f;
    bool isPaused_ = false;
    glm::vec3 userCamPos_ = glm::vec3(0.0f);
    bool isUserCamActive_ = false;
    int width_ = 800;
    int height_ = 600;
    static constexpr int kMaxRenderedDimensions = 9;
};

// Forward declarations for mode-specific rendering functions.
void renderMode1(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout);
void renderMode2(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout);
void renderMode3(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout);
void renderMode4(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout);
void renderMode5(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout);
void renderMode6(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout);
void renderMode7(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout);
void renderMode8(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout);
void renderMode9(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout);

#endif // CORE_HPP