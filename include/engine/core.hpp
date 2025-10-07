// AMOURANTH RTX Engine, October 2025 - Core simulation logic.
// Manages the simulation state, rendering modes, and DimensionalNavigator.
// Dependencies: Vulkan 1.3+, GLM, SDL3, C++20 standard library.
// Uses Nurb matter and energy for simulation parameters.
// Zachary Geurts 2025

#ifndef CORE_HPP
#define CORE_HPP

#include "ue_init.hpp"
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
#include <latch>

static constexpr int kMaxRenderedDimensions = 9;

class VulkanRenderer; // Forward declaration

class DimensionalNavigator {
public:
    DimensionalNavigator(const std::string& name, int width, int height, VulkanRenderer& renderer, const Logging::Logger& logger)
        : name_(name), width_(width), height_(height), mode_(1), zoomLevel_(1.0f), wavePhase_(0.0f), renderer_(renderer), logger_(logger) {
        logger_.log(Logging::LogLevel::Info, "Initializing DimensionalNavigator with name={}, width={}, height={}",
                    std::source_location::current(), name, width, height);
        initializeCache();
    }

    void initialize(int dimension, uint64_t numVertices) {
        logger_.log(Logging::LogLevel::Debug, "Initializing DimensionalNavigator: dimension={}, numVertices={}",
                    std::source_location::current(), dimension, numVertices);
        cache_.resize(std::min(static_cast<size_t>(dimension), static_cast<size_t>(kMaxRenderedDimensions)));
        initializeCache();
    }

    int getMode() const { return mode_; }
    float getZoomLevel() const { return zoomLevel_; }
    float getWavePhase() const { return wavePhase_; }
    std::span<const DimensionData> getCache() const { return cache_; }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    void setMode(int mode) {
        mode_ = glm::clamp(mode, 1, 9);
        logger_.log(Logging::LogLevel::Debug, "DimensionalNavigator mode set to {}", std::source_location::current(), mode_);
    }
    void setZoomLevel(float zoom) {
        zoomLevel_ = std::max(0.1f, zoom);
        logger_.log(Logging::LogLevel::Debug, "DimensionalNavigator zoomLevel set to {}", std::source_location::current(), zoomLevel_);
    }
    void setWavePhase(float phase) {
        wavePhase_ = phase;
        logger_.log(Logging::LogLevel::Debug, "DimensionalNavigator wavePhase set to {}", std::source_location::current(), wavePhase_);
    }
    void setWidth(int width) {
        width_ = width;
        logger_.log(Logging::LogLevel::Debug, "DimensionalNavigator width set to {}", std::source_location::current(), width_);
    }
    void setHeight(int height) {
        height_ = height;
        logger_.log(Logging::LogLevel::Debug, "DimensionalNavigator height set to {}", std::source_location::current(), height_);
    }
    VulkanRenderer& getRenderer() const { return renderer_; }

private:
    void initializeCache() {
        cache_.resize(kMaxRenderedDimensions);
        for (size_t i = 0; i < cache_.size(); ++i) {
            cache_[i].dimension = static_cast<int>(i + 1);
            cache_[i].observable = 1.0;
            cache_[i].potential = 0.0;
            cache_[i].nurbMatter = 0.0;
            cache_[i].nurbEnergy = 0.0;
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
    VulkanRenderer& renderer_;
    const Logging::Logger& logger_;
};

class AMOURANTH {
public:
    AMOURANTH(DimensionalNavigator* navigator, const Logging::Logger& logger, VkDevice device, VkDeviceMemory vertexBufferMemory, VkPipeline pipeline)
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
          logger_(logger),
          device_(device),
          vertexBufferMemory_(vertexBufferMemory),
          pipeline_(pipeline) {
        if (!navigator) {
            logger_.log(Logging::LogLevel::Error, "Null DimensionalNavigator provided",
                        std::source_location::current());
            throw std::runtime_error("AMOURANTH: Null DimensionalNavigator provided");
        }
        if (!device_ || !vertexBufferMemory_ || !pipeline_) {
            logger_.log(Logging::LogLevel::Error, "Invalid Vulkan resources: device={:p}, vertexBufferMemory={:p}, pipeline={:p}",
                        std::source_location::current(), static_cast<void*>(device_), static_cast<void*>(vertexBufferMemory_), static_cast<void*>(pipeline_));
            throw std::runtime_error("AMOURANTH: Invalid Vulkan resources");
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
        std::latch latch(1);
        ue_.setInfluence(ue_.getInfluence() + delta);
        updateCache();
        logger_.log(Logging::LogLevel::Debug, "Adjusted influence by {}", std::source_location::current(), delta);
        latch.count_down();
        latch.wait();
    }
    void adjustNurbMatter(double delta) {
        std::latch latch(1);
        for (auto& cache : cache_) cache.nurbMatter += delta;
        logger_.log(Logging::LogLevel::Debug, "Adjusted nurbMatter by {}", std::source_location::current(), delta);
        latch.count_down();
        latch.wait();
    }
    void adjustNurbEnergy(double delta) {
        std::latch latch(1);
        for (auto& cache : cache_) cache.nurbEnergy += delta;
        logger_.log(Logging::LogLevel::Debug, "Adjusted nurbEnergy by {}", std::source_location::current(), delta);
        latch.count_down();
        latch.wait();
    }
    void updateCache();
    void updateZoom(bool zoomIn);
    void setMode(int mode);
    void togglePause() {
        isPaused_ = !isPaused_;
        logger_.log(Logging::LogLevel::Debug, "Pause state set to {}", std::source_location::current(), isPaused_);
    }
    void toggleUserCam() {
        isUserCamActive_ = !isUserCamActive_;
        logger_.log(Logging::LogLevel::Debug, "User camera active set to {}", std::source_location::current(), isUserCamActive_);
    }
    void moveUserCam(float dx, float dy, float dz) {
        userCamPos_ += glm::vec3(dx, dy, dz);
        logger_.log(Logging::LogLevel::Debug, "Moved user camera to ({}, {}, {})",
                    std::source_location::current(), userCamPos_.x, userCamPos_.y, userCamPos_.z);
    }
    void setCurrentDimension(int dimension) {
        std::latch latch(1);
        ue_.setCurrentDimension(dimension);
        logger_.log(Logging::LogLevel::Debug, "Set current dimension to {}", std::source_location::current(), dimension);
        latch.count_down();
        latch.wait();
    }
    void setWidth(int width) {
        width_ = width;
        logger_.log(Logging::LogLevel::Debug, "AMOURANTH width set to {}", std::source_location::current(), width);
    }
    void setHeight(int height) {
        height_ = height;
        logger_.log(Logging::LogLevel::Debug, "AMOURANTH height set to {}", std::source_location::current(), height);
    }

    bool getDebug() const { return ue_.getDebug(); }
    double computeInteraction(int vertexIndex, double distance) const {
        logger_.log(Logging::LogLevel::Debug, "Computing interaction: vertexIndex={}, distance={}",
                    std::source_location::current(), vertexIndex, distance);
        return ue_.computeInteraction(vertexIndex, distance);
    }
    double computeNurbEnergy(double distance) const {
        logger_.log(Logging::LogLevel::Debug, "Computing nurbEnergy: distance={}",
                    std::source_location::current(), distance);
        return ue_.computenurbEnergy(distance);
    }
    double getAlpha() const {
        return ue_.getAlpha();
    }
    std::span<const glm::vec3> getSphereVertices() const {
        if (!sphereVertices_.empty() && reinterpret_cast<std::uintptr_t>(sphereVertices_.data()) % alignof(glm::vec3) != 0) {
            logger_.log(Logging::LogLevel::Error, "Misaligned sphereVertices_: address={}",
                        std::source_location::current(), reinterpret_cast<std::uintptr_t>(sphereVertices_.data()));
            throw std::runtime_error("Misaligned sphereVertices_");
        }
        return sphereVertices_;
    }
    std::span<const uint32_t> getSphereIndices() const { return sphereIndices_; }
    std::span<const glm::vec3> getQuadVertices() const {
        if (!quadVertices_.empty() && reinterpret_cast<std::uintptr_t>(quadVertices_.data()) % alignof(glm::vec3) != 0) {
            logger_.log(Logging::LogLevel::Error, "Misaligned quadVertices_: address={}",
                        std::source_location::current(), reinterpret_cast<std::uintptr_t>(quadVertices_.data()));
            throw std::runtime_error("Misaligned quadVertices_");
        }
        return quadVertices_;
    }
    std::span<const uint32_t> getQuadIndices() const { return quadIndices_; }
    std::span<const glm::vec3> getTriangleVertices() const {
        if (!triangleVertices_.empty() && reinterpret_cast<std::uintptr_t>(triangleVertices_.data()) % alignof(glm::vec3) != 0) {
            logger_.log(Logging::LogLevel::Error, "Misaligned triangleVertices_: address={}",
                        std::source_location::current(), reinterpret_cast<std::uintptr_t>(triangleVertices_.data()));
            throw std::runtime_error("Misaligned triangleVertices_");
        }
        return triangleVertices_;
    }
    std::span<const uint32_t> getTriangleIndices() const { return triangleIndices_; }
    std::span<const glm::vec3> getVoxelVertices() const {
        if (!voxelVertices_.empty() && reinterpret_cast<std::uintptr_t>(voxelVertices_.data()) % alignof(glm::vec3) != 0) {
            logger_.log(Logging::LogLevel::Error, "Misaligned voxelVertices_: address={}",
                        std::source_location::current(), reinterpret_cast<std::uintptr_t>(voxelVertices_.data()));
            throw std::runtime_error("Misaligned voxelVertices_");
        }
        return voxelVertices_;
    }
    std::span<const uint32_t> getVoxelIndices() const { return voxelIndices_; }
    std::span<const DimensionData> getCache() const { return cache_; }
    std::span<const Ball> getBalls() const { return balls_; }
    int getMode() const { return mode_; }
    float getWavePhase() const { return wavePhase_; }
    float getZoomLevel() const { return zoomLevel_; }
    glm::vec3 getUserCamPos() const { return userCamPos_; }
    bool isUserCamActive() const { return isUserCamActive_; }
    EnergyResult getEnergyResult() const {
        logger_.log(Logging::LogLevel::Debug, "Getting energy result", std::source_location::current());
        return ue_.compute();
    }
    std::span<const DimensionInteraction> getInteractions() const { return ue_.getInteractions(); }
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

    mutable UniversalEquation ue_;
    std::vector<DimensionData> cache_;
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

// Forward declarations for mode-specific rendering with [[maybe_unused]] for unused parameters
void renderMode1([[maybe_unused]] AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, [[maybe_unused]] VkBuffer vertexBuffer, [[maybe_unused]] VkCommandBuffer commandBuffer,
                 [[maybe_unused]] VkBuffer indexBuffer, [[maybe_unused]] float zoomLevel, [[maybe_unused]] int width, [[maybe_unused]] int height, [[maybe_unused]] float wavePhase,
                 [[maybe_unused]] std::span<const DimensionData> cache, [[maybe_unused]] VkPipelineLayout pipelineLayout, [[maybe_unused]] VkDescriptorSet descriptorSet,
                 [[maybe_unused]] VkDevice device, [[maybe_unused]] VkDeviceMemory vertexBufferMemory, [[maybe_unused]] VkPipeline pipeline);
void renderMode2([[maybe_unused]] AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, [[maybe_unused]] VkBuffer vertexBuffer, [[maybe_unused]] VkCommandBuffer commandBuffer,
                 [[maybe_unused]] VkBuffer indexBuffer, [[maybe_unused]] float zoomLevel, [[maybe_unused]] int width, [[maybe_unused]] int height, [[maybe_unused]] float wavePhase,
                 [[maybe_unused]] std::span<const DimensionData> cache, [[maybe_unused]] VkPipelineLayout pipelineLayout, [[maybe_unused]] VkDescriptorSet descriptorSet,
                 [[maybe_unused]] VkDevice device, [[maybe_unused]] VkDeviceMemory vertexBufferMemory, [[maybe_unused]] VkPipeline pipeline);
void renderMode3([[maybe_unused]] AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, [[maybe_unused]] VkBuffer vertexBuffer, [[maybe_unused]] VkCommandBuffer commandBuffer,
                 [[maybe_unused]] VkBuffer indexBuffer, [[maybe_unused]] float zoomLevel, [[maybe_unused]] int width, [[maybe_unused]] int height, [[maybe_unused]] float wavePhase,
                 [[maybe_unused]] std::span<const DimensionData> cache, [[maybe_unused]] VkPipelineLayout pipelineLayout, [[maybe_unused]] VkDescriptorSet descriptorSet,
                 [[maybe_unused]] VkDevice device, [[maybe_unused]] VkDeviceMemory vertexBufferMemory, [[maybe_unused]] VkPipeline pipeline);
void renderMode4([[maybe_unused]] AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, [[maybe_unused]] VkBuffer vertexBuffer, [[maybe_unused]] VkCommandBuffer commandBuffer,
                 [[maybe_unused]] VkBuffer indexBuffer, [[maybe_unused]] float zoomLevel, [[maybe_unused]] int width, [[maybe_unused]] int height, [[maybe_unused]] float wavePhase,
                 [[maybe_unused]] std::span<const DimensionData> cache, [[maybe_unused]] VkPipelineLayout pipelineLayout, [[maybe_unused]] VkDescriptorSet descriptorSet,
                 [[maybe_unused]] VkDevice device, [[maybe_unused]] VkDeviceMemory vertexBufferMemory, [[maybe_unused]] VkPipeline pipeline);
void renderMode5([[maybe_unused]] AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, [[maybe_unused]] VkBuffer vertexBuffer, [[maybe_unused]] VkCommandBuffer commandBuffer,
                 [[maybe_unused]] VkBuffer indexBuffer, [[maybe_unused]] float zoomLevel, [[maybe_unused]] int width, [[maybe_unused]] int height, [[maybe_unused]] float wavePhase,
                 [[maybe_unused]] std::span<const DimensionData> cache, [[maybe_unused]] VkPipelineLayout pipelineLayout, [[maybe_unused]] VkDescriptorSet descriptorSet,
                 [[maybe_unused]] VkDevice device, [[maybe_unused]] VkDeviceMemory vertexBufferMemory, [[maybe_unused]] VkPipeline pipeline);
void renderMode6([[maybe_unused]] AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, [[maybe_unused]] VkBuffer vertexBuffer, [[maybe_unused]] VkCommandBuffer commandBuffer,
                 [[maybe_unused]] VkBuffer indexBuffer, [[maybe_unused]] float zoomLevel, [[maybe_unused]] int width, [[maybe_unused]] int height, [[maybe_unused]] float wavePhase,
                 [[maybe_unused]] std::span<const DimensionData> cache, [[maybe_unused]] VkPipelineLayout pipelineLayout, [[maybe_unused]] VkDescriptorSet descriptorSet,
                 [[maybe_unused]] VkDevice device, [[maybe_unused]] VkDeviceMemory vertexBufferMemory, [[maybe_unused]] VkPipeline pipeline);
void renderMode7([[maybe_unused]] AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, [[maybe_unused]] VkBuffer vertexBuffer, [[maybe_unused]] VkCommandBuffer commandBuffer,
                 [[maybe_unused]] VkBuffer indexBuffer, [[maybe_unused]] float zoomLevel, [[maybe_unused]] int width, [[maybe_unused]] int height, [[maybe_unused]] float wavePhase,
                 [[maybe_unused]] std::span<const DimensionData> cache, [[maybe_unused]] VkPipelineLayout pipelineLayout, [[maybe_unused]] VkDescriptorSet descriptorSet,
                 [[maybe_unused]] VkDevice device, [[maybe_unused]] VkDeviceMemory vertexBufferMemory, [[maybe_unused]] VkPipeline pipeline);
void renderMode8([[maybe_unused]] AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, [[maybe_unused]] VkBuffer vertexBuffer, [[maybe_unused]] VkCommandBuffer commandBuffer,
                 [[maybe_unused]] VkBuffer indexBuffer, [[maybe_unused]] float zoomLevel, [[maybe_unused]] int width, [[maybe_unused]] int height, [[maybe_unused]] float wavePhase,
                 [[maybe_unused]] std::span<const DimensionData> cache, [[maybe_unused]] VkPipelineLayout pipelineLayout, [[maybe_unused]] VkDescriptorSet descriptorSet,
                 [[maybe_unused]] VkDevice device, [[maybe_unused]] VkDeviceMemory vertexBufferMemory, [[maybe_unused]] VkPipeline pipeline);
void renderMode9([[maybe_unused]] AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, [[maybe_unused]] VkBuffer vertexBuffer, [[maybe_unused]] VkCommandBuffer commandBuffer,
                 [[maybe_unused]] VkBuffer indexBuffer, [[maybe_unused]] float zoomLevel, [[maybe_unused]] int width, [[maybe_unused]] int height, [[maybe_unused]] float wavePhase,
                 [[maybe_unused]] std::span<const DimensionData> cache, [[maybe_unused]] VkPipelineLayout pipelineLayout, [[maybe_unused]] VkDescriptorSet descriptorSet,
                 [[maybe_unused]] VkDevice device, [[maybe_unused]] VkDeviceMemory vertexBufferMemory, [[maybe_unused]] VkPipeline pipeline);

// Inline implementations
inline void AMOURANTH::render(uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                              VkBuffer indexBuffer, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet) {
    logger_.log(Logging::LogLevel::Debug, "Rendering frame for image index {}", std::source_location::current(), imageIndex);
    if (!device_ || !vertexBuffer || !commandBuffer || !indexBuffer || !pipelineLayout || !descriptorSet) {
        logger_.log(Logging::LogLevel::Error, "Invalid Vulkan resources in render: device={:p}, vertexBuffer={:p}, commandBuffer={:p}, indexBuffer={:p}, pipelineLayout={:p}, descriptorSet={:p}",
                    std::source_location::current(), static_cast<void*>(device_), static_cast<void*>(vertexBuffer),
                    static_cast<void*>(commandBuffer), static_cast<void*>(indexBuffer), static_cast<void*>(pipelineLayout),
                    static_cast<void*>(descriptorSet));
        throw std::runtime_error("Invalid Vulkan resources in AMOURANTH::render");
    }
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
        std::latch latch(1);
        wavePhase_ += waveSpeed_ * deltaTime;
        simulator_->setWavePhase(wavePhase_);
        ue_.updateBalls(deltaTime);
        updateCache();
        logger_.log(Logging::LogLevel::Debug, "Updated simulation with deltaTime={:.3f}, wavePhase={:.3f}",
                    std::source_location::current(), deltaTime, wavePhase_);
        latch.count_down();
        latch.wait();
    }
}

inline void AMOURANTH::updateCache() {
    std::latch latch(1);
    auto result = ue_.compute();
    for (size_t i = 0; i < cache_.size(); ++i) {
        cache_[i].observable = result.observable;
        cache_[i].potential = result.potential;
        cache_[i].nurbMatter = result.nurbMatter;
        cache_[i].nurbEnergy = result.nurbEnergy;
    }
    logger_.log(Logging::LogLevel::Debug, "Updated cache with {} entries", std::source_location::current(), cache_.size());
    latch.count_down();
    latch.wait();
}

inline void AMOURANTH::updateZoom(bool zoomIn) {
    std::latch latch(1);
    zoomLevel_ *= zoomIn ? 1.1f : 0.9f;
    zoomLevel_ = std::max(0.1f, zoomLevel_);
    simulator_->setZoomLevel(zoomLevel_);
    logger_.log(Logging::LogLevel::Debug, "Updated zoom level to {:.3f}", std::source_location::current(), zoomLevel_);
    latch.count_down();
    latch.wait();
}

inline void AMOURANTH::setMode(int mode) {
    std::latch latch(1);
    mode_ = glm::clamp(mode, 1, 9);
    simulator_->setMode(mode_);
    ue_.setMode(mode_);
    logger_.log(Logging::LogLevel::Info, "Set rendering mode to {}", std::source_location::current(), mode_);
    latch.count_down();
    latch.wait();
}

inline void AMOURANTH::initializeSphereGeometry() {
    float radius = 0.1f;
    uint32_t sectors = 16, rings = 16;
    sphereVertices_.clear();
    sphereIndices_.clear();
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
    if (!sphereVertices_.empty() && reinterpret_cast<std::uintptr_t>(sphereVertices_.data()) % alignof(glm::vec3) != 0) {
        logger_.log(Logging::LogLevel::Error, "Misaligned sphereVertices_: address={}",
                    std::source_location::current(), reinterpret_cast<std::uintptr_t>(sphereVertices_.data()));
        throw std::runtime_error("Misaligned sphereVertices_");
    }
    logger_.log(Logging::LogLevel::Info, "Initialized sphere geometry with {} vertices, {} indices",
                std::source_location::current(), sphereVertices_.size(), sphereIndices_.size());
}

inline void AMOURANTH::initializeQuadGeometry() {
    quadVertices_ = {{-1.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f}};
    quadIndices_ = {0, 1, 2, 2, 3, 0};
    if (!quadVertices_.empty() && reinterpret_cast<std::uintptr_t>(quadVertices_.data()) % alignof(glm::vec3) != 0) {
        logger_.log(Logging::LogLevel::Error, "Misaligned quadVertices_: address={}",
                    std::source_location::current(), reinterpret_cast<std::uintptr_t>(quadVertices_.data()));
        throw std::runtime_error("Misaligned quadVertices_");
    }
    logger_.log(Logging::LogLevel::Info, "Initialized quad geometry with {} vertices, {} indices",
                std::source_location::current(), quadVertices_.size(), quadIndices_.size());
}

inline void AMOURANTH::initializeTriangleGeometry() {
    triangleVertices_ = {{0.0f, 0.5f, 0.0f}, {-0.5f, -0.5f, 0.0f}, {0.5f, -0.5f, 0.0f}};
    triangleIndices_ = {0, 1, 2};
    if (!triangleVertices_.empty() && reinterpret_cast<std::uintptr_t>(triangleVertices_.data()) % alignof(glm::vec3) != 0) {
        logger_.log(Logging::LogLevel::Error, "Misaligned triangleVertices_: address={}",
                    std::source_location::current(), reinterpret_cast<std::uintptr_t>(triangleVertices_.data()));
        throw std::runtime_error("Misaligned triangleVertices_");
    }
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
    if (!voxelVertices_.empty() && reinterpret_cast<std::uintptr_t>(voxelVertices_.data()) % alignof(glm::vec3) != 0) {
        logger_.log(Logging::LogLevel::Error, "Misaligned voxelVertices_: address={}",
                    std::source_location::current(), reinterpret_cast<std::uintptr_t>(voxelVertices_.data()));
        throw std::runtime_error("Misaligned voxelVertices_");
    }
    logger_.log(Logging::LogLevel::Info, "Initialized voxel geometry with {} vertices, {} indices",
                std::source_location::current(), voxelVertices_.size(), voxelIndices_.size());
}

inline void AMOURANTH::initializeCalculator() {
    std::latch latch(1);
    try {
        if (ue_.getDebug()) {
            logger_.log(Logging::LogLevel::Debug, "Initializing calculator for UniversalEquation",
                        std::source_location::current());
        }
        ue_.initializeCalculator(this);
        updateCache();
        logger_.log(Logging::LogLevel::Info, "Calculator initialized successfully",
                    std::source_location::current());
    } catch (const std::exception& e) {
        logger_.log(Logging::LogLevel::Error, "Calculator initialization failed: {}",
                    std::source_location::current(), e.what());
        throw;
    }
    latch.count_down();
    latch.wait();
}

inline void AMOURANTH::initializeBalls(float baseMass, float baseRadius, size_t numBalls) {
    std::latch latch(1);
    balls_.clear();
    ue_.initializeBalls(baseMass, baseRadius, numBalls);
    balls_ = ue_.getBalls();
    logger_.log(Logging::LogLevel::Info, "Initialized {} balls with mass scale={:.3f}",
                std::source_location::current(), balls_.size(), static_cast<float>(ue_.compute().nurbMatter));
    latch.count_down();
    latch.wait();
}

inline void AMOURANTH::updateBalls(float deltaTime) {
    std::latch latch(1);
    ue_.updateBalls(deltaTime);
    balls_ = ue_.getBalls();
    logger_.log(Logging::LogLevel::Debug, "Updated {} balls", std::source_location::current(), balls_.size());
    latch.count_down();
    latch.wait();
}

#endif // CORE_HPP