// core.hpp
#ifndef CORE_HPP
#define CORE_HPP

// AMOURANTH RTX Engine Core September 2025
// This header provides the core rendering and navigation components for the AMOURANTH RTX engine, designed as the primary
// entry point for game developers building applications like 3D platformers (e.g., the next Mario).
// Integrates Vulkan-based ray tracing and SDL for rendering.
// Input handling is moved to handleinput.hpp for modularity.
// Font handling is managed via SDL3Font in SDL3_font.hpp (SDL3Initializer).
// Physics computations (e.g., dark energy, multidimensional phenomena) are handled in ue_init.hpp, allowing developers
// to focus on gameplay and rendering features.
// Standardized 256-byte PushConstants for both rasterization and ray tracing pipelines to support high-end RTX GPUs.
// Zachary Geurts 2025

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <cmath>
#include "ue_init.hpp"
#include "handleinput.hpp"

// PushConstants for shader data, standardized to 256 bytes for rasterization and ray tracing
struct PushConstants {
    alignas(16) glm::mat4 model;       // 64 bytes: Model transformation matrix
    alignas(16) glm::mat4 view_proj;   // 64 bytes: Combined view-projection matrix
    alignas(16) glm::vec4 extra[8];    // 128 bytes: Additional data for shaders (8 vec4s x 16 bytes)
};
static_assert(sizeof(PushConstants) == 256, "PushConstants size must be 256 bytes");

// Forward declaration of DimensionalNavigator
class DimensionalNavigator;

// Core class of the AMOURANTH RTX engine, orchestrating rendering and navigation for game development
class AMOURANTH {
public:
    static constexpr int kMaxRenderedDimensions = 9; // Maximum dimensions for rendering (configurable for gameplay)

    AMOURANTH(DimensionalNavigator* navigator);
    void render(uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                VkBuffer indexBuffer, VkPipelineLayout pipelineLayout);
    void update(float deltaTime);

    // Adjust physics parameters using UniversalEquation (optional for physics-based gameplay)
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
    void updateZoom(bool zoomIn);
    void setMode(int mode);
    void togglePause() { isPaused_ = !isPaused_; }
    void toggleUserCam() { isUserCamActive_ = !isUserCamActive_; }
    void moveUserCam(float dx, float dy, float dz) {
        userCamPos_ += glm::vec3(dx, dy, dz);
    }

    void setCurrentDimension(int dimension) { ue_.setCurrentDimension(dimension); }

    // Accessors for rendering, navigation, and optional physics data
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
    // Access physics computation results from UniversalEquation
    EnergyResult getEnergyResult() const { return ue_.compute(); }
    const std::vector<DimensionInteraction>& getInteractions() const { return ue_.getInteractions(); }

private:
    void initializeSphereGeometry();
    void initializeQuadGeometry();
    void initializeCalculator();

    UniversalEquation ue_;                     // Physics simulation instance (optional for gameplay)
    std::vector<DimensionData> cache_;         // Cache for dimension data from UniversalEquation
    std::vector<glm::vec3> sphereVertices_;    // Sphere geometry vertices for rendering
    std::vector<uint32_t> sphereIndices_;      // Sphere geometry indices
    std::vector<glm::vec3> quadVertices_;      // Quad geometry vertices for rendering
    std::vector<uint32_t> quadIndices_;        // Quad geometry indices
    DimensionalNavigator* simulator_;           // Navigation state manager
    int mode_ = 1;                             // Current rendering mode (1-9)
    float wavePhase_ = 0.0f;                   // Wave phase for animations
    float waveSpeed_ = 1.0f;                   // Wave animation speed
    float zoomLevel_ = 1.0f;                   // Zoom level for rendering
    bool isPaused_ = false;                    // Pause state for simulation
    glm::vec3 userCamPos_ = glm::vec3(0.0f);   // User-controlled camera position
    bool isUserCamActive_ = false;             // User camera active state
    int width_ = 800;                          // Window width
    int height_ = 600;                         // Window height
};

// Manages navigation and state for rendering, suitable for game camera and scene management
class DimensionalNavigator {
public:
    DimensionalNavigator(const std::string& name, int width, int height)
        : name_(name), width_(width), height_(height), mode_(1), zoomLevel_(1.0f), wavePhase_(0.0f) {
        cache_.resize(AMOURANTH::kMaxRenderedDimensions);
        for (size_t i = 0; i < cache_.size(); ++i) {
            cache_[i].dimension = static_cast<int>(i + 1);
            cache_[i].observable = 1.0;
            cache_[i].potential = 0.0;
            cache_[i].darkMatter = 0.0;
            cache_[i].darkEnergy = 0.0;
        }
    }

    int getMode() const { return mode_; }
    float getZoomLevel() const { return zoomLevel_; }
    float getWavePhase() const { return wavePhase_; }
    const std::vector<DimensionData>& getCache() const { return cache_; }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

    void setMode(int mode) { mode_ = glm::clamp(mode, 1, 9); }
    void setZoomLevel(float zoom) { zoomLevel_ = std::max(0.1f, zoom); }
    void setWavePhase(float phase) { wavePhase_ = phase; }

private:
    std::string name_;                    // Navigator identifier
    int width_, height_;                  // Window dimensions
    int mode_;                            // Current rendering mode
    float zoomLevel_;                     // Zoom level for camera
    float wavePhase_;                     // Wave phase for animations
    std::vector<DimensionData> cache_;    // Cache for dimension data (from ue_init.hpp)
};

// Forward declarations for mode-specific rendering functions (customizable for different game visuals)
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

// Inline implementations
inline AMOURANTH::AMOURANTH(DimensionalNavigator* navigator) {
    if (!navigator) {
        throw std::runtime_error("AMOURANTH: Null DimensionalNavigator provided.");
    }
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
    userCamPos_ = glm::vec3(0.0f);
    width_ = simulator_->getWidth();
    height_ = simulator_->getHeight();
}

inline void AMOURANTH::render(uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
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

inline void AMOURANTH::update(float deltaTime) {
    if (!isPaused_) {
        wavePhase_ += waveSpeed_ * deltaTime;
        simulator_->setWavePhase(wavePhase_);
        ue_.advanceCycle();
        updateCache();
    }
}

inline void AMOURANTH::updateCache() {
    auto result = ue_.compute();
    for (size_t i = 0; i < cache_.size(); ++i) {
        cache_[i].observable = result.observable;
        cache_[i].potential = result.potential;
        cache_[i].darkMatter = result.darkMatter;
        cache_[i].darkEnergy = result.darkEnergy;
    }
}

inline void AMOURANTH::updateZoom(bool zoomIn) {
    zoomLevel_ *= zoomIn ? 1.1f : 0.9f;
    zoomLevel_ = std::max(0.1f, zoomLevel_);
    simulator_->setZoomLevel(zoomLevel_);
}

inline void AMOURANTH::setMode(int mode) {
    mode_ = mode;
    simulator_->setMode(mode_);
}

inline void AMOURANTH::initializeSphereGeometry() {
    float radius = 1.0f;
    uint32_t sectors = 320, rings = 200;
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

inline void AMOURANTH::initializeQuadGeometry() {
    quadVertices_ = {
        {-1.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f},
        {1.0f, 1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f}
    };
    quadIndices_ = {0, 1, 2, 2, 3, 0};
}

inline void AMOURANTH::initializeCalculator() {
    cache_.clear();
    for (int i = 0; i < kMaxRenderedDimensions; ++i) {
        cache_.emplace_back();
        cache_[i].dimension = i + 1;
        cache_[i].observable = 1.0;
        cache_[i].potential = 0.0;
        cache_[i].darkMatter = 0.0;
        cache_[i].darkEnergy = 0.0;
    }
}

#endif // CORE_HPP