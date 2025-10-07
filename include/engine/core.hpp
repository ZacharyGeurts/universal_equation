#ifndef CORE_HPP
#define CORE_HPP

#include "universal_equation.hpp"
#include "engine/logging.hpp"
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
#include <random>

static constexpr int kMaxRenderedDimensions = 9;

class Xorshift {
public:
    Xorshift(uint32_t seed) : state_(seed) {}
    float nextFloat(float min, float max) {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return min + (max - min) * (state_ & 0x7FFFFFFF) / static_cast<float>(0x7FFFFFFF);
    }
private:
    uint32_t state_;
};

struct Ball {
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 acceleration;
    float mass;
    float radius;
    float startTime;
    Ball(const glm::vec3& pos, const glm::vec3& vel, float m, float r, float start)
        : position(pos), velocity(vel), acceleration(0.0f), mass(m), radius(r), startTime(start) {}
};

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
    std::span<const UniversalEquation::DimensionData> getCache() const { return cache_; }
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
            cache_[i].nurbMatter = 0.0;
            cache_[i].nurbEnergy = 0.0;
            cache_[i].spinEnergy = 0.0;
            cache_[i].momentumEnergy = 0.0;
            cache_[i].fieldEnergy = 0.0;
            cache_[i].GodWaveEnergy = 0.0;
        }
        logger_.log(Logging::LogLevel::Debug, "DimensionalNavigator cache initialized with {} entries",
                    std::source_location::current(), cache_.size());
    }

    std::string name_;
    int width_, height_;
    int mode_;
    float zoomLevel_;
    float wavePhase_;
    std::vector<UniversalEquation::DimensionData> cache_;
    const Logging::Logger& logger_;
};

class AMOURANTH {
public:
    AMOURANTH(DimensionalNavigator* navigator, const Logging::Logger& logger, VkDevice device, VkDeviceMemory vertexBufferMemory, VkPipeline pipeline)
        : ue_(logger, 8, 8, 2.5, 0.1, 5.0, 1.5, 5.0, 1.0, 0.5, 1.0, 0.0072973525693, 0.5, 0.1, 0.5, 0.5, 2.0, 4.0, 1.0, 1.0e6, 1.0, 0.5, 2.0, true, 256),
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
          logger_(logger),
          device_(device),
          vertexBufferMemory_(vertexBufferMemory),
          pipeline_(pipeline) {
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
        initializeBalls();
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
    void adjustNurbMatter(double delta) {
        for (auto& cache : cache_) cache.nurbMatter += delta;
    }
    void adjustNurbEnergy(double delta) {
        for (auto& cache : cache_) cache.nurbEnergy += delta;
    }
    void updateCache();
    void updateZoom(bool zoomIn);
    void setMode(int mode);
    void togglePause() { isPaused_ = !isPaused_; }
    void toggleUserCam() { isUserCamActive_ = !isUserCamActive_; }
    void moveUserCam(float dx, float dy, float dz) { userCamPos_ += glm::vec3(dx, dy, dz); }
    void setCurrentDimension(int dimension) { ue_.setCurrentDimension(dimension); }

    bool getDebug() const { return ue_.getDebug(); }
    double computeInteraction(int vertexIndex, double distance) const { return static_cast<double>(ue_.computeInteraction(vertexIndex, static_cast<long double>(distance))); }
    double computePermeation(int vertexIndex) const { return static_cast<double>(ue_.computePermeation(vertexIndex)); }
    double computeNurbEnergy(double distance) const { return static_cast<double>(ue_.computeNurbEnergy(static_cast<long double>(distance))); }
    double getAlpha() const { return static_cast<double>(ue_.getAlpha()); }
    std::span<const glm::vec3> getSphereVertices() const { return sphereVertices_; }
    std::span<const uint32_t> getSphereIndices() const { return sphereIndices_; }
    std::span<const glm::vec3> getQuadVertices() const { return quadVertices_; }
    std::span<const uint32_t> getQuadIndices() const { return quadIndices_; }
    std::span<const glm::vec3> getTriangleVertices() const { return triangleVertices_; }
    std::span<const uint32_t> getTriangleIndices() const { return triangleIndices_; }
    std::span<const glm::vec3> getVoxelVertices() const { return voxelVertices_; }
    std::span<const uint32_t> getVoxelIndices() const { return voxelIndices_; }
    std::span<const UniversalEquation::DimensionData> getCache() const { return cache_; }
    std::span<const Ball> getBalls() const { return balls_; }
    int getMode() const { return mode_; }
    float getWavePhase() const { return wavePhase_; }
    float getZoomLevel() const { return zoomLevel_; }
    glm::vec3 getUserCamPos() const { return userCamPos_; }
    bool isUserCamActive() const { return isUserCamActive_; }
    UniversalEquation::EnergyResult getEnergyResult() const { return ue_.compute(); }
    std::span<const UniversalEquation::DimensionInteraction> getInteractions() const { return ue_.getInteractions(); }
    VkDevice getDevice() const { return device_; }
    VkDeviceMemory getVertexBufferMemory() const { return vertexBufferMemory_; }
    VkPipeline getGraphicsPipeline() const { return pipeline_; }
    const Logging::Logger& getLogger() const { return logger_; }

private:
    void initializeSphereGeometry();
    void initializeQuadGeometry();
    void initializeTriangleGeometry();
    void initializeVoxelGeometry();
    void initializeCalculator();
    void initializeBalls(float baseMass = 1.2f, float baseRadius = 0.12f, size_t numBalls = 30000);
    void updateBalls(float deltaTime);

    UniversalEquation ue_;
    std::vector<UniversalEquation::DimensionData> cache_;
    std::vector<Ball> balls_;
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
    VkDevice device_;
    VkDeviceMemory vertexBufferMemory_;
    VkPipeline pipeline_;
};

// Forward declarations for mode-specific rendering
void renderMode1(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const UniversalEquation::DimensionData> cache, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 VkDevice device, VkDeviceMemory vertexBufferMemory, VkPipeline pipeline);
void renderMode2(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const UniversalEquation::DimensionData> cache, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 VkDevice device, VkDeviceMemory vertexBufferMemory, VkPipeline pipeline);
void renderMode3(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const UniversalEquation::DimensionData> cache, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 VkDevice device, VkDeviceMemory vertexBufferMemory, VkPipeline pipeline);
void renderMode4(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const UniversalEquation::DimensionData> cache, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 VkDevice device, VkDeviceMemory vertexBufferMemory, VkPipeline pipeline);
void renderMode5(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const UniversalEquation::DimensionData> cache, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 VkDevice device, VkDeviceMemory vertexBufferMemory, VkPipeline pipeline);
void renderMode6(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const UniversalEquation::DimensionData> cache, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 VkDevice device, VkDeviceMemory vertexBufferMemory, VkPipeline pipeline);
void renderMode7(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const UniversalEquation::DimensionData> cache, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 VkDevice device, VkDeviceMemory vertexBufferMemory, VkPipeline pipeline);
void renderMode8(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const UniversalEquation::DimensionData> cache, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 VkDevice device, VkDeviceMemory vertexBufferMemory, VkPipeline pipeline);
void renderMode9(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const UniversalEquation::DimensionData> cache, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 VkDevice device, VkDeviceMemory vertexBufferMemory, VkPipeline pipeline);

// Inline implementations
inline void AMOURANTH::render(uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                              VkBuffer indexBuffer, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet) {
    logger_.log(Logging::LogLevel::Debug, "Rendering frame for image index {}", std::source_location::current(), imageIndex);
    switch (simulator_->getMode()) {
        case 1: renderMode1(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(),
                            width_, height_, simulator_->getWavePhase(), getCache(), pipelineLayout, descriptorSet,
                            device_, vertexBufferMemory_, pipeline_); break;
        case 2: renderMode2(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(),
                            width_, height_, simulator_->getWavePhase(), getCache(), pipelineLayout, descriptorSet,
                            device_, vertexBufferMemory_, pipeline_); break;
        case 3: renderMode3(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(),
                            width_, height_, simulator_->getWavePhase(), getCache(), pipelineLayout, descriptorSet,
                            device_, vertexBufferMemory_, pipeline_); break;
        case 4: renderMode4(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(),
                            width_, height_, simulator_->getWavePhase(), getCache(), pipelineLayout, descriptorSet,
                            device_, vertexBufferMemory_, pipeline_); break;
        case 5: renderMode5(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(),
                            width_, height_, simulator_->getWavePhase(), getCache(), pipelineLayout, descriptorSet,
                            device_, vertexBufferMemory_, pipeline_); break;
        case 6: renderMode6(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(),
                            width_, height_, simulator_->getWavePhase(), getCache(), pipelineLayout, descriptorSet,
                            device_, vertexBufferMemory_, pipeline_); break;
        case 7: renderMode7(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(),
                            width_, height_, simulator_->getWavePhase(), getCache(), pipelineLayout, descriptorSet,
                            device_, vertexBufferMemory_, pipeline_); break;
        case 8: renderMode8(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(),
                            width_, height_, simulator_->getWavePhase(), getCache(), pipelineLayout, descriptorSet,
                            device_, vertexBufferMemory_, pipeline_); break;
        case 9: renderMode9(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(),
                            width_, height_, simulator_->getWavePhase(), getCache(), pipelineLayout, descriptorSet,
                            device_, vertexBufferMemory_, pipeline_); break;
        default: renderMode1(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(),
                             width_, height_, simulator_->getWavePhase(), getCache(), pipelineLayout, descriptorSet,
                             device_, vertexBufferMemory_, pipeline_); break;
    }
}

inline void AMOURANTH::update(float deltaTime) {
    if (!isPaused_) {
        wavePhase_ += waveSpeed_ * deltaTime;
        simulator_->setWavePhase(wavePhase_);
        ue_.evolveTimeStep(deltaTime);
        updateBalls(deltaTime);
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
        cache_[i].nurbMatter = result.nurbMatter;
        cache_[i].nurbEnergy = result.nurbEnergy;
        cache_[i].spinEnergy = result.spinEnergy;
        cache_[i].momentumEnergy = result.momentumEnergy;
        cache_[i].fieldEnergy = result.fieldEnergy;
        cache_[i].GodWaveEnergy = result.GodWaveEnergy;
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
    mode_ = glm::clamp(mode, 1, 9);
    simulator_->setMode(mode_);
    ue_.setMode(mode_);
    logger_.log(Logging::LogLevel::Info, "Set rendering mode to {}", std::source_location::current(), mode_);
}

inline void AMOURANTH::initializeSphereGeometry() {
    float radius = 0.1f; // Smaller radius for balls
    uint32_t sectors = 16, rings = 16; // Reduced for performance
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
    triangleVertices_ = {{0.0f, 0.5f, 0.0f}, {-0.5f, -0.5f, 0.0f}, {0.5f, -0.5f, 0.0f}};
    triangleIndices_ = {0, 1, 2};
    logger_.log(Logging::LogLevel::Info, "Initialized triangle geometry with {} vertices, {} indices",
                std::source_location::current(), triangleVertices_.size(), triangleIndices_.size());
}

inline void AMOURANTH::initializeVoxelGeometry() {
    voxelVertices_ = {
        {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f},
        {-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}
    };
    voxelIndices_ = {
        0, 1, 2, 2, 3, 0, 4, 6, 5, 6, 4, 7, 0, 3, 7, 7, 4, 0,
        1, 5, 6, 6, 2, 1, 0, 4, 5, 5, 1, 0, 3, 2, 6, 6, 7, 3
    };
    logger_.log(Logging::LogLevel::Info, "Initialized voxel geometry with {} vertices, {} indices",
                std::source_location::current(), voxelVertices_.size(), voxelIndices_.size());
}

inline void AMOURANTH::initializeCalculator() {
    try {
        if (getDebug()) {
            logger_.log(Logging::LogLevel::Debug, "Initializing calculator for UniversalEquation",
                        std::source_location::current());
        }
        ue_.initializeCalculator(simulator_);
        updateCache();
        logger_.log(Logging::LogLevel::Info, "Calculator initialized successfully",
                    std::source_location::current());
    } catch (const std::exception& e) {
        logger_.log(Logging::LogLevel::Error, "Calculator initialization failed: {}",
                    std::source_location::current(), e.what());
        throw;
    }
}

inline void AMOURANTH::initializeBalls(float baseMass, float baseRadius, size_t numBalls) {
    balls_.clear();
    balls_.reserve(numBalls);
    auto result = ue_.compute();
    float massScale = static_cast<float>(result.nurbMatter);
    Xorshift rng(12345);
    for (size_t i = 0; i < numBalls; ++i) {
        glm::vec3 pos(rng.nextFloat(-5.0f, 5.0f), rng.nextFloat(-5.0f, 5.0f), rng.nextFloat(-2.0f, 2.0f));
        glm::vec3 vel(rng.nextFloat(-1.0f, 1.0f), rng.nextFloat(-1.0f, 1.0f), rng.nextFloat(-1.0f, 1.0f));
        float startTime = i * 0.1f;
        balls_.emplace_back(pos, vel, baseMass * massScale, baseRadius, startTime);
    }
    logger_.log(Logging::LogLevel::Info, "Initialized {} balls with mass scale={:.3f}",
                std::source_location::current(), balls_.size(), massScale);
}

inline void AMOURANTH::updateBalls(float deltaTime) {
    auto interactions = ue_.getInteractions();
    auto result = ue_.compute();
    float simulationTime = wavePhase_;

    std::vector<glm::vec3> forces(balls_.size(), glm::vec3(0.0f));
#pragma omp parallel for
    for (size_t i = 0; i < balls_.size(); ++i) {
        if (simulationTime < balls_[i].startTime) continue;
        double interactionStrength = (i < interactions.size()) ? interactions[i].strength : 0.0;
        forces[i] = glm::vec3(
            static_cast<float>(result.observable),
            static_cast<float>(result.potential),
            static_cast<float>(result.nurbEnergy)
        ) * static_cast<float>(interactionStrength);
        balls_[i].acceleration = forces[i] / balls_[i].mass;
    }

    const glm::vec3 boundsMin(-5.0f, -5.0f, -2.0f);
    const glm::vec3 boundsMax(5.0f, 5.0f, 2.0f);
#pragma omp parallel for
    for (size_t i = 0; i < balls_.size(); ++i) {
        if (simulationTime < balls_[i].startTime) continue;
        auto& pos = balls_[i].position;
        auto& vel = balls_[i].velocity;
        if (pos.x < boundsMin.x) { pos.x = boundsMin.x; vel.x = -vel.x; }
        if (pos.x > boundsMax.x) { pos.x = boundsMax.x; vel.x = -vel.x; }
        if (pos.y < boundsMin.y) { pos.y = boundsMin.y; vel.y = -vel.y; }
        if (pos.y > boundsMax.y) { pos.y = boundsMax.y; vel.y = -vel.y; }
        if (pos.z < boundsMin.z) { pos.z = boundsMin.z; vel.z = -vel.z; }
        if (pos.z > boundsMax.z) { pos.z = boundsMax.z; vel.z = -vel.z; }
    }

    const int gridSize = 10;
    const float cellSize = 10.0f / gridSize;
    std::vector<std::vector<size_t>> grid(gridSize * gridSize * gridSize);
    for (size_t i = 0; i < balls_.size(); ++i) {
        if (simulationTime < balls_[i].startTime) continue;
        glm::vec3 pos = balls_[i].position;
        int x = static_cast<int>((pos.x + 5.0f) / cellSize);
        int y = static_cast<int>((pos.y + 5.0f) / cellSize);
        int z = static_cast<int>((pos.z + 2.0f) / (cellSize * 0.5f));
        x = std::clamp(x, 0, gridSize - 1);
        y = std::clamp(y, 0, gridSize - 1);
        z = std::clamp(z, 0, gridSize - 1);
        int cellIdx = z * gridSize * gridSize + y * gridSize + x;
        grid[cellIdx].push_back(i);
    }

    std::vector<std::vector<std::pair<size_t, size_t>>> threadCollisions(omp_get_max_threads());
#pragma omp parallel for
    for (size_t i = 0; i < balls_.size(); ++i) {
        if (simulationTime < balls_[i].startTime) continue;
        glm::vec3 pos = balls_[i].position;
        int x = static_cast<int>((pos.x + 5.0f) / cellSize);
        int y = static_cast<int>((pos.y + 5.0f) / cellSize);
        int z = static_cast<int>((pos.z + 2.0f) / (cellSize * 0.5f));
        x = std::clamp(x, 0, gridSize - 1);
        y = std::clamp(y, 0, gridSize - 1);
        z = std::clamp(z, 0, gridSize - 1);

        int threadId = omp_get_thread_num();
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    int nx = x + dx, ny = y + dy, nz = z + dz;
                    if (nx < 0 || nx >= gridSize || ny < 0 || ny >= gridSize || nz < 0 || nz >= gridSize) continue;
                    int cellIdx = nz * gridSize * gridSize + ny * gridSize + nx;
                    for (size_t j : grid[cellIdx]) {
                        if (j <= i || simulationTime < balls_[j].startTime) continue;
                        glm::vec3 delta = balls_[j].position - balls_[i].position;
                        float distance = glm::length(delta);
                        float minDistance = balls_[i].radius + balls_[j].radius;
                        if (distance < minDistance && distance > 0.0f) {
                            threadCollisions[threadId].emplace_back(i, j);
                        }
                    }
                }
            }
        }
    }

    for (const auto& collisions : threadCollisions) {
        for (const auto& [i, j] : collisions) {
            glm::vec3 delta = balls_[j].position - balls_[i].position;
            float distance = glm::length(delta);
            float minDistance = balls_[i].radius + balls_[j].radius;
            if (distance < minDistance && distance > 0.0f) {
                glm::vec3 normal = delta / distance;
                glm::vec3 relVelocity = balls_[j].velocity - balls_[i].velocity;
                float impulse = -2.0f * glm::dot(relVelocity, normal) / (1.0f / balls_[i].mass + 1.0f / balls_[j].mass);
                balls_[i].velocity += (impulse / balls_[i].mass) * normal;
                balls_[j].velocity -= (impulse / balls_[j].mass) * normal;
                float overlap = minDistance - distance;
                balls_[i].position -= normal * (overlap * 0.5f);
                balls_[j].position += normal * (overlap * 0.5f);
            }
        }
    }

#pragma omp parallel for
    for (size_t i = 0; i < balls_.size(); ++i) {
        if (simulationTime < balls_[i].startTime) continue;
        balls_[i].velocity += balls_[i].acceleration * deltaTime;
        balls_[i].position += balls_[i].velocity * deltaTime;
    }
}

#endif // CORE_HPP