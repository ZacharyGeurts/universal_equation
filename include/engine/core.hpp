#ifndef CORE_HPP
#define CORE_HPP

// AMOURANTH RTX Engine Core, October 2025
// Core rendering and navigation for Vulkan-based ray tracing and SDL integration.
// Manages physics via UniversalEquation (ue_init.hpp) and input via handleinput.hpp.
// Uses 256-byte PushConstants for rasterization and ray tracing pipelines.
// Supports sphere, quad, triangle, and voxel geometries for fast 3D rendering.
// Dependencies: Vulkan 1.3+, SDL3, GLM, ue_init.hpp, handleinput.hpp, logging.hpp.
// Usage: Instantiate AMOURANTH with DimensionalNavigator; call render and update for gameplay.
// Zachary Geurts 2025

#include "ue_init.hpp" // For DimensionData, EnergyResult, etc.
#include "logging.hpp" // For Logging::Logger and ANSI color codes
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <stdexcept>
#include <numbers>
#include <format>
#include <span>

static constexpr int kMaxRenderedDimensions = 9;

struct PushConstants {
    alignas(16) glm::mat4 model;       // 64 bytes
    alignas(16) glm::mat4 view_proj;   // 64 bytes
    alignas(16) glm::vec4 extra[8];    // 128 bytes
};
static_assert(sizeof(PushConstants) == 256, "PushConstants must be 256 bytes");

class DimensionalNavigator {
public:
    DimensionalNavigator(const std::string& name, int width, int height, const Logging::Logger& logger)
        : name_(name), width_(width), height_(height), mode_(1), zoomLevel_(1.0f), wavePhase_(0.0f), logger_(logger) {
        logger_.log(Logging::LogLevel::Info, "Initializing DimensionalNavigator with name={}, width={}, height={}",
                    std::source_location::current(), name, width, height);
        initializeCache();
    }

    int getMode() const { return mode_; }
    float getZoomLevel() const { return zoomLevel_; }
    float getWavePhase() const { return wavePhase_; }
    std::span<const DimensionData> getCache() const { return cache_; }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    void setMode(int mode) { mode_ = glm::clamp(mode, 1, 9); }
    void setZoomLevel(float zoom) { zoomLevel_ = std::max(0.1f, zoom); }
    void setWavePhase(float phase) { wavePhase_ = phase; }

private:
    void initializeCache() {
        cache_.resize(kMaxRenderedDimensions);
        for (size_t i = 0; i < cache_.size(); ++i) {
            cache_[i].dimension = static_cast<int>(i + 1);
            cache_[i].observable = 1.0;
            cache_[i].potential = 0.0;
            cache_[i].darkMatter = 0.0;
            cache_[i].darkEnergy = 0.0;
        }
        logger_.log(Logging::LogLevel::Debug, "DimensionalNavigator cache initialized with {} entries",
                    std::source_location::current(), cache_.size());
    }

    std::string name_;
    int width_, height_;
    int mode_;
    float zoomLevel_;
    float wavePhase_;
    std::vector<DimensionData> cache_;
    const Logging::Logger& logger_;
};

class AMOURANTH {
public:
    AMOURANTH(DimensionalNavigator* navigator, const Logging::Logger& logger)
        : ue_(8, 8, 2.5, 0.0072973525693, true),
          simulator_(navigator),
          mode_(1),
          wavePhase_(0.0f),
          waveSpeed_(1.0f),
          zoomLevel_(1.0f),
          isPaused_(false),
          userCamPos_(0.0f),
          isUserCamActive_(false),
          width_(navigator ? navigator->getWidth() : 800),
          height_(navigator ? navigator->getHeight() : 600),
          logger_(logger) {
        if (!navigator) {
            logger_.log(Logging::LogLevel::Error, "Null DimensionalNavigator provided",
                        std::source_location::current());
            throw std::runtime_error("AMOURANTH: Null DimensionalNavigator provided");
        }
        logger_.log(Logging::LogLevel::Info, "Initializing AMOURANTH with width={}, height={}",
                    std::source_location::current(), width_, height_);
        initializeSphereGeometry();
        initializeQuadGeometry();
        initializeTriangleGeometry();
        initializeVoxelGeometry();
        initializeCalculator();
        logger_.log(Logging::LogLevel::Info, "AMOURANTH initialized successfully",
                    std::source_location::current());
    }

    void render(uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                VkBuffer indexBuffer, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet);
    void update(float deltaTime);

    void adjustInfluence(double delta) {
        ue_.setInfluence(ue_.getInfluence() + delta);
        updateCache();
    }
    void adjustDarkMatter(double delta) {
        for (auto& cache : cache_) cache.darkMatter += delta;
    }
    void adjustDarkEnergy(double delta) {
        for (auto& cache : cache_) cache.darkEnergy += delta;
    }
    void updateCache();
    void updateZoom(bool zoomIn);
    void setMode(int mode);
    void togglePause() { isPaused_ = !isPaused_; }
    void toggleUserCam() { isUserCamActive_ = !isUserCamActive_; }
    void moveUserCam(float dx, float dy, float dz) { userCamPos_ += glm::vec3(dx, dy, dz); }
    void setCurrentDimension(int dimension) { ue_.setCurrentDimension(dimension); }

    bool getDebug() const { return ue_.getDebug(); }
    double computeInteraction(int vertexIndex, double distance) const { return ue_.computeInteraction(vertexIndex, distance); }
    double computePermeation(int vertexIndex) const { return ue_.computePermeation(vertexIndex); }
    double computeDarkEnergy(double distance) const { return ue_.computeDarkEnergy(distance); }
    double getAlpha() const { return ue_.getAlpha(); }
    std::span<const glm::vec3> getSphereVertices() const { return sphereVertices_; }
    std::span<const uint32_t> getSphereIndices() const { return sphereIndices_; }
    std::span<const glm::vec3> getQuadVertices() const { return quadVertices_; }
    std::span<const uint32_t> getQuadIndices() const { return quadIndices_; }
    std::span<const glm::vec3> getTriangleVertices() const { return triangleVertices_; }
    std::span<const uint32_t> getTriangleIndices() const { return triangleIndices_; }
    std::span<const glm::vec3> getVoxelVertices() const { return voxelVertices_; }
    std::span<const uint32_t> getVoxelIndices() const { return voxelIndices_; }
    std::span<const DimensionData> getCache() const { return cache_; }
    int getMode() const { return mode_; }
    float getWavePhase() const { return wavePhase_; }
    float getZoomLevel() const { return zoomLevel_; }
    glm::vec3 getUserCamPos() const { return userCamPos_; }
    bool isUserCamActive() const { return isUserCamActive_; }
    EnergyResult getEnergyResult() const { return ue_.compute(); }
    std::span<const DimensionInteraction> getInteractions() const { return ue_.getInteractions(); }

private:
    void initializeSphereGeometry();
    void initializeQuadGeometry();
    void initializeTriangleGeometry();
    void initializeVoxelGeometry();
    void initializeCalculator();

    UniversalEquation ue_;
    std::vector<DimensionData> cache_;
    std::vector<glm::vec3> sphereVertices_;
    std::vector<uint32_t> sphereIndices_;
    std::vector<glm::vec3> quadVertices_;
    std::vector<uint32_t> quadIndices_;
    std::vector<glm::vec3> triangleVertices_;
    std::vector<uint32_t> triangleIndices_;
    std::vector<glm::vec3> voxelVertices_;
    std::vector<uint32_t> voxelIndices_;
    DimensionalNavigator* simulator_;
    int mode_;
    float wavePhase_;
    float waveSpeed_;
    float zoomLevel_;
    bool isPaused_;
    glm::vec3 userCamPos_;
    bool isUserCamActive_;
    int width_;
    int height_;
    const Logging::Logger& logger_;
};

#include "handleinput.hpp"

// Forward declarations for mode-specific rendering
void renderMode1(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const DimensionData> cache, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet);
void renderMode2(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const DimensionData> cache, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet);
void renderMode3(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const DimensionData> cache, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet);
void renderMode4(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const DimensionData> cache, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet);
void renderMode5(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const DimensionData> cache, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet);
void renderMode6(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const DimensionData> cache, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet);
void renderMode7(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const DimensionData> cache, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet);
void renderMode8(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const DimensionData> cache, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet);
void renderMode9(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const DimensionData> cache, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet);

// Inline implementations
inline void AMOURANTH::render(uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                              VkBuffer indexBuffer, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet) {
    logger_.log(Logging::LogLevel::Debug, "Rendering frame for image index {}", std::source_location::current(), imageIndex);
    switch (simulator_->getMode()) {
        case 1: renderMode1(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(), width_, height_, simulator_->getWavePhase(), getCache(), pipelineLayout, descriptorSet); break;
        case 2: renderMode2(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(), width_, height_, simulator_->getWavePhase(), getCache(), pipelineLayout, descriptorSet); break;
        case 3: renderMode3(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(), width_, height_, simulator_->getWavePhase(), getCache(), pipelineLayout, descriptorSet); break;
        case 4: renderMode4(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(), width_, height_, simulator_->getWavePhase(), getCache(), pipelineLayout, descriptorSet); break;
        case 5: renderMode5(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(), width_, height_, simulator_->getWavePhase(), getCache(), pipelineLayout, descriptorSet); break;
        case 6: renderMode6(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(), width_, height_, simulator_->getWavePhase(), getCache(), pipelineLayout, descriptorSet); break;
        case 7: renderMode7(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(), width_, height_, simulator_->getWavePhase(), getCache(), pipelineLayout, descriptorSet); break;
        case 8: renderMode8(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(), width_, height_, simulator_->getWavePhase(), getCache(), pipelineLayout, descriptorSet); break;
        case 9: renderMode9(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(), width_, height_, simulator_->getWavePhase(), getCache(), pipelineLayout, descriptorSet); break;
        default: renderMode1(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(), width_, height_, simulator_->getWavePhase(), getCache(), pipelineLayout, descriptorSet); break;
    }
}

inline void AMOURANTH::update(float deltaTime) {
    if (!isPaused_) {
        wavePhase_ += waveSpeed_ * deltaTime;
        simulator_->setWavePhase(wavePhase_);
        ue_.advanceCycle();
        updateCache();
        logger_.log(Logging::LogLevel::Debug, "Updated simulation with deltaTime={:.3f}, wavePhase={:.3f}",
                    std::source_location::current(), deltaTime, wavePhase_);
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
    logger_.log(Logging::LogLevel::Debug, "Updated cache with {} entries", std::source_location::current(), cache_.size());
}

inline void AMOURANTH::updateZoom(bool zoomIn) {
    zoomLevel_ *= zoomIn ? 1.1f : 0.9f;
    zoomLevel_ = std::max(0.1f, zoomLevel_);
    simulator_->setZoomLevel(zoomLevel_);
    logger_.log(Logging::LogLevel::Debug, "Updated zoom level to {:.3f}", std::source_location::current(), zoomLevel_);
}

inline void AMOURANTH::setMode(int mode) {
    mode_ = mode;
    simulator_->setMode(mode_);
    logger_.log(Logging::LogLevel::Info, "Set rendering mode to {}", std::source_location::current(), mode_);
}

inline void AMOURANTH::initializeSphereGeometry() {
    float radius = 1.0f;
    uint32_t sectors = 320, rings = 200;
    for (uint32_t i = 0; i <= rings; ++i) {
        float theta = i * std::numbers::pi_v<float> / rings;
        float sinTheta = std::sin(theta), cosTheta = std::cos(theta);
        for (uint32_t j = 0; j <= sectors; ++j) {
            float phi = j * 2 * std::numbers::pi_v<float> / sectors;
            float sinPhi = std::sin(phi), cosPhi = std::cos(phi);
            sphereVertices_.push_back({radius * cosPhi * sinTheta, radius * cosTheta, radius * sinPhi * sinTheta});
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
    logger_.log(Logging::LogLevel::Info, "Initialized sphere geometry with {} vertices, {} indices",
                std::source_location::current(), sphereVertices_.size(), sphereIndices_.size());
}

inline void AMOURANTH::initializeQuadGeometry() {
    quadVertices_ = {{-1.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f}};
    quadIndices_ = {0, 1, 2, 2, 3, 0};
    logger_.log(Logging::LogLevel::Info, "Initialized quad geometry with {} vertices, {} indices",
                std::source_location::current(), quadVertices_.size(), quadIndices_.size());
}

inline void AMOURANTH::initializeTriangleGeometry() {
    triangleVertices_ = {
        {0.0f, 0.5f, 0.0f},    // Top vertex
        {-0.5f, -0.5f, 0.0f}, // Bottom-left vertex
        {0.5f, -0.5f, 0.0f}   // Bottom-right vertex
    };
    triangleIndices_ = {0, 1, 2};
    logger_.log(Logging::LogLevel::Info, "Initialized triangle geometry with {} vertices, {} indices",
                std::source_location::current(), triangleVertices_.size(), triangleIndices_.size());
}

inline void AMOURANTH::initializeVoxelGeometry() {
    // Define a cube centered at origin with side length 1
    voxelVertices_ = {
        {-0.5f, -0.5f, -0.5f}, // 0: Back-bottom-left
        { 0.5f, -0.5f, -0.5f}, // 1: Back-bottom-right
        { 0.5f,  0.5f, -0.5f}, // 2: Back-top-right
        {-0.5f,  0.5f, -0.5f}, // 3: Back-top-left
        {-0.5f, -0.5f,  0.5f}, // 4: Front-bottom-left
        { 0.5f, -0.5f,  0.5f}, // 5: Front-bottom-right
        { 0.5f,  0.5f,  0.5f}, // 6: Front-top-right
        {-0.5f,  0.5f,  0.5f}  // 7: Front-top-left
    };
    // Define 12 triangles (2 per face) for the cube
    voxelIndices_ = {
        // Back face (z = -0.5)
        0, 1, 2,  2, 3, 0,
        // Front face (z = 0.5)
        4, 6, 5,  6, 4, 7,
        // Left face (x = -0.5)
        0, 3, 7,  7, 4, 0,
        // Right face (x = 0.5)
        1, 5, 6,  6, 2, 1,
        // Bottom face (y = -0.5)
        0, 4, 5,  5, 1, 0,
        // Top face (y = 0.5)
        3, 2, 6,  6, 7, 3
    };
    logger_.log(Logging::LogLevel::Info, "Initialized voxel geometry with {} vertices, {} indices",
                std::source_location::current(), voxelVertices_.size(), voxelIndices_.size());
}

inline void AMOURANTH::initializeCalculator() {
    try {
        if (getDebug()) {
            logger_.log(Logging::LogLevel::Debug, "Initializing cache for UniversalEquation",
                        std::source_location::current());
        }
        cache_.clear();
        cache_.resize(kMaxRenderedDimensions);
        for (int i = 0; i < kMaxRenderedDimensions; ++i) {
            cache_[i].dimension = i + 1;
            cache_[i].observable = 1.0;
            cache_[i].potential = 0.0;
            cache_[i].darkMatter = 0.0;
            cache_[i].darkEnergy = 0.0;
        }
        logger_.log(Logging::LogLevel::Info, "Cache initialized successfully with {} entries",
                    std::source_location::current(), cache_.size());
    } catch (const std::exception& e) {
        logger_.log(Logging::LogLevel::Error, "Cache initialization failed: {}", std::source_location::current(), e.what());
        throw;
    }
}

#endif // CORE_HPP