// AMOURANTH RTX Engine, October 2025 - Core simulation logic implementation.
// Implements AMOURANTH and DimensionalNavigator functionality.
// Dependencies: Vulkan 1.3+, GLM, SDL3, C++20 standard library.
// Zachary Geurts 2025

#include "engine/core.hpp"
#include "engine/logging.hpp"
#include "engine/Vulkan_init.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <algorithm>
#include <random>

DimensionalNavigator::DimensionalNavigator(const std::string& name, int width, int height, VulkanRenderer& renderer, const Logging::Logger& logger)
    : name_(name), width_(width), height_(height), mode_(1), zoomLevel_(1.0f), wavePhase_(0.0f), renderer_(renderer), logger_(logger) {
    logger_.log(Logging::LogLevel::Info, "Initializing DimensionalNavigator with name={}, width={}, height={}",
                std::source_location::current(), name, width, height);
    initializeCache();
}

void DimensionalNavigator::initialize(int dimension, uint64_t numVertices) {
    logger_.log(Logging::LogLevel::Debug, "Initializing DimensionalNavigator: dimension={}, numVertices={}",
                std::source_location::current(), dimension, numVertices);
    cache_.resize(std::min(static_cast<size_t>(dimension), static_cast<size_t>(kMaxRenderedDimensions)));
    initializeCache();
}

void DimensionalNavigator::initializeCache() {
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

void DimensionalNavigator::setMode(int mode) {
    mode_ = glm::clamp(mode, 1, 9);
    logger_.log(Logging::LogLevel::Debug, "DimensionalNavigator mode set to {}", std::source_location::current(), mode_);
}

void DimensionalNavigator::setZoomLevel(float zoom) {
    zoomLevel_ = std::max(0.1f, zoom);
    logger_.log(Logging::LogLevel::Debug, "DimensionalNavigator zoomLevel set to {}", std::source_location::current(), zoomLevel_);
}

void DimensionalNavigator::setWavePhase(float phase) {
    wavePhase_ = phase;
    logger_.log(Logging::LogLevel::Debug, "DimensionalNavigator wavePhase set to {}", std::source_location::current(), wavePhase_);
}

void DimensionalNavigator::setWidth(int width) {
    width_ = width;
    logger_.log(Logging::LogLevel::Debug, "DimensionalNavigator width set to {}", std::source_location::current(), width_);
}

void DimensionalNavigator::setHeight(int height) {
    height_ = height;
    logger_.log(Logging::LogLevel::Debug, "DimensionalNavigator height set to {}", std::source_location::current(), height_);
}

AMOURANTH::AMOURANTH(DimensionalNavigator* navigator, const Logging::Logger& logger, VkDevice device, VkDeviceMemory vertexBufferMemory, VkPipeline pipeline)
    : simulator_(navigator), mode_(1), wavePhase_(0.0f), waveSpeed_(1.0f), zoomLevel_(1.0f), isPaused_(false),
      userCamPos_(0.0f), isUserCamActive_(false), width_(navigator ? navigator->getWidth() : 800),
      height_(navigator ? navigator->getHeight() : 600), logger_(logger), device_(device),
      vertexBufferMemory_(vertexBufferMemory), pipeline_(pipeline), ue_(logger, 8, 8, 2.5, 0.0072973525693, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, true, 30000) {
    if (!navigator) {
        logger_.get().log(Logging::LogLevel::Error, "Null DimensionalNavigator provided", std::source_location::current());
        throw std::runtime_error("AMOURANTH: Null DimensionalNavigator provided");
    }
    if (!device_ || !vertexBufferMemory_ || !pipeline_) {
        logger_.get().log(Logging::LogLevel::Error, "Invalid Vulkan resources: device={:p}, vertexBufferMemory={:p}, pipeline={:p}",
                          std::source_location::current(), static_cast<void*>(device_), static_cast<void*>(vertexBufferMemory_), static_cast<void*>(pipeline_));
        throw std::runtime_error("AMOURANTH: Invalid Vulkan resources");
    }
    logger_.get().log(Logging::LogLevel::Info, "Initializing AMOURANTH with width={}, height={}",
                      std::source_location::current(), width_, height_);
    initializeSphereGeometry();
    initializeQuadGeometry();
    initializeTriangleGeometry();
    initializeVoxelGeometry();
    initializeCalculator();
    initializeBalls();
    logger_.get().log(Logging::LogLevel::Info, "AMOURANTH initialized successfully", std::source_location::current());
}

void AMOURANTH::render(uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                       VkBuffer indexBuffer, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                       VkRenderPass renderPass, VkFramebuffer framebuffer, float deltaTime) {
    logger_.get().log(Logging::LogLevel::Debug, "Rendering frame for image index {}", std::source_location::current(), imageIndex);
    if (!device_ || !vertexBuffer || !commandBuffer || !indexBuffer || !pipelineLayout || !descriptorSet || !renderPass || !framebuffer) {
        logger_.get().log(Logging::LogLevel::Error, "Invalid Vulkan resources in render: device={:p}, vertexBuffer={:p}, commandBuffer={:p}, indexBuffer={:p}, pipelineLayout={:p}, descriptorSet={:p}, renderPass={:p}, framebuffer={:p}",
                          std::source_location::current(), static_cast<void*>(device_), static_cast<void*>(vertexBuffer),
                          static_cast<void*>(commandBuffer), static_cast<void*>(indexBuffer), static_cast<void*>(pipelineLayout),
                          static_cast<void*>(descriptorSet), static_cast<void*>(renderPass), static_cast<void*>(framebuffer));
        throw std::runtime_error("Invalid Vulkan resources in AMOURANTH::render");
    }
    switch (simulator_->getMode()) {
        case 1: renderMode1(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(),
                           width_, height_, simulator_->getWavePhase(), simulator_->getCache(), pipelineLayout, descriptorSet,
                           device_, vertexBufferMemory_, pipeline_, deltaTime, renderPass, framebuffer); break;
        case 2: renderMode2(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(),
                           width_, height_, simulator_->getWavePhase(), simulator_->getCache(), pipelineLayout, descriptorSet,
                           device_, vertexBufferMemory_, pipeline_, deltaTime, renderPass, framebuffer); break;
        case 3: renderMode3(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(),
                           width_, height_, simulator_->getWavePhase(), simulator_->getCache(), pipelineLayout, descriptorSet,
                           device_, vertexBufferMemory_, pipeline_, deltaTime, renderPass, framebuffer); break;
        case 4: renderMode4(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(),
                           width_, height_, simulator_->getWavePhase(), simulator_->getCache(), pipelineLayout, descriptorSet,
                           device_, vertexBufferMemory_, pipeline_, deltaTime, renderPass, framebuffer); break;
        case 5: renderMode5(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(),
                           width_, height_, simulator_->getWavePhase(), simulator_->getCache(), pipelineLayout, descriptorSet,
                           device_, vertexBufferMemory_, pipeline_, deltaTime, renderPass, framebuffer); break;
        case 6: renderMode6(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(),
                           width_, height_, simulator_->getWavePhase(), simulator_->getCache(), pipelineLayout, descriptorSet,
                           device_, vertexBufferMemory_, pipeline_, deltaTime, renderPass, framebuffer); break;
        case 7: renderMode7(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(),
                           width_, height_, simulator_->getWavePhase(), simulator_->getCache(), pipelineLayout, descriptorSet,
                           device_, vertexBufferMemory_, pipeline_, deltaTime, renderPass, framebuffer); break;
        case 8: renderMode8(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(),
                           width_, height_, simulator_->getWavePhase(), simulator_->getCache(), pipelineLayout, descriptorSet,
                           device_, vertexBufferMemory_, pipeline_, deltaTime, renderPass, framebuffer); break;
        case 9: renderMode9(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(),
                           width_, height_, simulator_->getWavePhase(), simulator_->getCache(), pipelineLayout, descriptorSet,
                           device_, vertexBufferMemory_, pipeline_, deltaTime, renderPass, framebuffer); break;
        default: renderMode1(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, simulator_->getZoomLevel(),
                            width_, height_, simulator_->getWavePhase(), simulator_->getCache(), pipelineLayout, descriptorSet,
                            device_, vertexBufferMemory_, pipeline_, deltaTime, renderPass, framebuffer); break;
    }
}

void AMOURANTH::update(float deltaTime) {
    if (!isPaused_) {
        std::latch latch(1);
        wavePhase_ += waveSpeed_ * deltaTime;
        simulator_->setWavePhase(wavePhase_);
        updateCache();
        logger_.get().log(Logging::LogLevel::Debug, "Updated simulation with deltaTime={:.3f}, wavePhase={:.3f}",
                          std::source_location::current(), deltaTime, wavePhase_);
        latch.count_down();
        latch.wait();
    }
}

void AMOURANTH::adjustInfluence(double delta) {
    std::latch latch(1);
    ue_.setInfluence(ue_.getInfluence() + delta);
    updateCache();
    logger_.get().log(Logging::LogLevel::Debug, "Adjusted influence by {}", std::source_location::current(), delta);
    latch.count_down();
    latch.wait();
}

void AMOURANTH::adjustNurbMatter(double delta) {
    std::latch latch(1);
    for (auto& cache : cache_) cache.nurbMatter += delta;
    logger_.get().log(Logging::LogLevel::Debug, "Adjusted nurbMatter by {}", std::source_location::current(), delta);
    latch.count_down();
    latch.wait();
}

void AMOURANTH::adjustNurbEnergy(double delta) {
    std::latch latch(1);
    for (auto& cache : cache_) cache.nurbEnergy += delta;
    logger_.get().log(Logging::LogLevel::Debug, "Adjusted nurbEnergy by {}", std::source_location::current(), delta);
    latch.count_down();
    latch.wait();
}

void AMOURANTH::updateCache() {
    std::latch latch(1);
    auto result = ue_.compute();
    for (size_t i = 0; i < cache_.size(); ++i) {
        cache_[i].observable = result.observable;
        cache_[i].potential = result.potential;
        cache_[i].nurbMatter = result.nurbMatter;
        cache_[i].nurbEnergy = result.nurbEnergy;
    }
    logger_.get().log(Logging::LogLevel::Debug, "Updated cache with {} entries", std::source_location::current(), cache_.size());
    latch.count_down();
    latch.wait();
}

void AMOURANTH::updateZoom(bool zoomIn) {
    std::latch latch(1);
    zoomLevel_ *= zoomIn ? 1.1f : 0.9f;
    zoomLevel_ = std::max(0.1f, zoomLevel_);
    simulator_->setZoomLevel(zoomLevel_);
    logger_.get().log(Logging::LogLevel::Debug, "Updated zoom level to {:.3f}", std::source_location::current(), zoomLevel_);
    latch.count_down();
    latch.wait();
}

void AMOURANTH::setMode(int mode) {
    std::latch latch(1);
    mode_ = glm::clamp(mode, 1, 9);
    simulator_->setMode(mode_);
    ue_.setMode(mode_);
    logger_.get().log(Logging::LogLevel::Info, "Set rendering mode to {}", std::source_location::current(), mode_);
    latch.count_down();
    latch.wait();
}

void AMOURANTH::togglePause() {
    isPaused_ = !isPaused_;
    logger_.get().log(Logging::LogLevel::Debug, "Pause state set to {}", std::source_location::current(), isPaused_);
}

void AMOURANTH::toggleUserCam() {
    isUserCamActive_ = !isUserCamActive_;
    logger_.get().log(Logging::LogLevel::Debug, "User camera active set to {}", std::source_location::current(), isUserCamActive_);
}

void AMOURANTH::moveUserCam(float dx, float dy, float dz) {
    userCamPos_ += glm::vec3(dx, dy, dz);
    logger_.get().log(Logging::LogLevel::Debug, "Moved user camera to ({}, {}, {})",
                      std::source_location::current(), userCamPos_.x, userCamPos_.y, userCamPos_.z);
}

void AMOURANTH::setCurrentDimension(int dimension) {
    std::latch latch(1);
    ue_.setCurrentDimension(dimension);
    logger_.get().log(Logging::LogLevel::Debug, "Set current dimension to {}", std::source_location::current(), dimension);
    latch.count_down();
    latch.wait();
}

void AMOURANTH::setWidth(int width) {
    width_ = width;
    logger_.get().log(Logging::LogLevel::Debug, "AMOURANTH width set to {}", std::source_location::current(), width);
}

void AMOURANTH::setHeight(int height) {
    height_ = height;
    logger_.get().log(Logging::LogLevel::Debug, "AMOURANTH height set to {}", std::source_location::current(), height);
}

void AMOURANTH::initializeSphereGeometry() {
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
        logger_.get().log(Logging::LogLevel::Error, "Misaligned sphereVertices_: address={}",
                          std::source_location::current(), reinterpret_cast<std::uintptr_t>(sphereVertices_.data()));
        throw std::runtime_error("Misaligned sphereVertices_");
    }
    logger_.get().log(Logging::LogLevel::Info, "Initialized sphere geometry with {} vertices, {} indices",
                      std::source_location::current(), sphereVertices_.size(), sphereIndices_.size());
}

void AMOURANTH::initializeQuadGeometry() {
    quadVertices_ = {{-1.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f}};
    quadIndices_ = {0, 1, 2, 2, 3, 0};
    if (!quadVertices_.empty() && reinterpret_cast<std::uintptr_t>(quadVertices_.data()) % alignof(glm::vec3) != 0) {
        logger_.get().log(Logging::LogLevel::Error, "Misaligned quadVertices_: address={}",
                          std::source_location::current(), reinterpret_cast<std::uintptr_t>(quadVertices_.data()));
        throw std::runtime_error("Misaligned quadVertices_");
    }
    logger_.get().log(Logging::LogLevel::Info, "Initialized quad geometry with {} vertices, {} indices",
                      std::source_location::current(), quadVertices_.size(), quadIndices_.size());
}

void AMOURANTH::initializeTriangleGeometry() {
    triangleVertices_ = {{0.0f, 0.5f, 0.0f}, {-0.5f, -0.5f, 0.0f}, {0.5f, -0.5f, 0.0f}};
    triangleIndices_ = {0, 1, 2};
    if (!triangleVertices_.empty() && reinterpret_cast<std::uintptr_t>(triangleVertices_.data()) % alignof(glm::vec3) != 0) {
        logger_.get().log(Logging::LogLevel::Error, "Misaligned triangleVertices_: address={}",
                          std::source_location::current(), reinterpret_cast<std::uintptr_t>(triangleVertices_.data()));
        throw std::runtime_error("Misaligned triangleVertices_");
    }
    logger_.get().log(Logging::LogLevel::Info, "Initialized triangle geometry with {} vertices, {} indices",
                      std::source_location::current(), triangleVertices_.size(), triangleIndices_.size());
}

void AMOURANTH::initializeVoxelGeometry() {
    voxelVertices_ = {
        {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f},
        {-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}
    };
    voxelIndices_ = {
        0, 1, 2, 2, 3, 0, 4, 6, 5, 6, 4, 7, 0, 3, 7, 7, 4, 0,
        1, 5, 6, 6, 2, 1, 0, 4, 5, 5, 1, 0, 3, 2, 6, 6, 7, 3
    };
    if (!voxelVertices_.empty() && reinterpret_cast<std::uintptr_t>(voxelVertices_.data()) % alignof(glm::vec3) != 0) {
        logger_.get().log(Logging::LogLevel::Error, "Misaligned voxelVertices_: address={}",
                          std::source_location::current(), reinterpret_cast<std::uintptr_t>(voxelVertices_.data()));
        throw std::runtime_error("Misaligned voxelVertices_");
    }
    logger_.get().log(Logging::LogLevel::Info, "Initialized voxel geometry with {} vertices, {} indices",
                      std::source_location::current(), voxelVertices_.size(), voxelIndices_.size());
}

void AMOURANTH::initializeCalculator() {
    std::latch latch(1);
    try {
        if (ue_.getDebug()) {
            logger_.get().log(Logging::LogLevel::Debug, "Initializing calculator for UniversalEquation",
                              std::source_location::current());
        }
        ue_.initializeCalculator(this);
        updateCache();
        logger_.get().log(Logging::LogLevel::Info, "Calculator initialized successfully",
                          std::source_location::current());
    } catch (const std::exception& e) {
        logger_.get().log(Logging::LogLevel::Error, "Calculator initialization failed: {}",
                          std::source_location::current(), e.what());
        throw;
    }
    latch.count_down();
    latch.wait();
}

void AMOURANTH::initializeBalls(float baseMass, float baseRadius, size_t numBalls) {
    std::latch latch(1);
    balls_.resize(numBalls);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> posDist(-10.0f, 10.0f);
    std::uniform_real_distribution<float> velDist(-1.0f, 1.0f);
    for (size_t i = 0; i < numBalls; ++i) {
        balls_[i].position = glm::vec3(posDist(gen), posDist(gen), posDist(gen));
        balls_[i].velocity = glm::vec3(velDist(gen), velDist(gen), velDist(gen));
        balls_[i].mass = baseMass;
        balls_[i].radius = baseRadius;
    }
    logger_.get().log(Logging::LogLevel::Info, "Initialized {} balls with mass scale={:.3f}",
                      std::source_location::current(), balls_.size(), baseMass);
    latch.count_down();
    latch.wait();
}

std::span<const glm::vec3> AMOURANTH::getSphereVertices() const {
    if (!sphereVertices_.empty() && reinterpret_cast<std::uintptr_t>(sphereVertices_.data()) % alignof(glm::vec3) != 0) {
        logger_.get().log(Logging::LogLevel::Error, "Misaligned sphereVertices_: address={}",
                          std::source_location::current(), reinterpret_cast<std::uintptr_t>(sphereVertices_.data()));
        throw std::runtime_error("Misaligned sphereVertices_");
    }
    return sphereVertices_;
}

std::span<const uint32_t> AMOURANTH::getSphereIndices() const { return sphereIndices_; }

std::span<const glm::vec3> AMOURANTH::getQuadVertices() const {
    if (!quadVertices_.empty() && reinterpret_cast<std::uintptr_t>(quadVertices_.data()) % alignof(glm::vec3) != 0) {
        logger_.get().log(Logging::LogLevel::Error, "Misaligned quadVertices_: address={}",
                          std::source_location::current(), reinterpret_cast<std::uintptr_t>(quadVertices_.data()));
        throw std::runtime_error("Misaligned quadVertices_");
    }
    return quadVertices_;
}

std::span<const uint32_t> AMOURANTH::getQuadIndices() const { return quadIndices_; }

std::span<const glm::vec3> AMOURANTH::getTriangleVertices() const {
    if (!triangleVertices_.empty() && reinterpret_cast<std::uintptr_t>(triangleVertices_.data()) % alignof(glm::vec3) != 0) {
        logger_.get().log(Logging::LogLevel::Error, "Misaligned triangleVertices_: address={}",
                          std::source_location::current(), reinterpret_cast<std::uintptr_t>(triangleVertices_.data()));
        throw std::runtime_error("Misaligned triangleVertices_");
    }
    return triangleVertices_;
}

std::span<const uint32_t> AMOURANTH::getTriangleIndices() const { return triangleIndices_; }

std::span<const glm::vec3> AMOURANTH::getVoxelVertices() const {
    if (!voxelVertices_.empty() && reinterpret_cast<std::uintptr_t>(voxelVertices_.data()) % alignof(glm::vec3) != 0) {
        logger_.get().log(Logging::LogLevel::Error, "Misaligned voxelVertices_: address={}",
                          std::source_location::current(), reinterpret_cast<std::uintptr_t>(voxelVertices_.data()));
        throw std::runtime_error("Misaligned voxelVertices_");
    }
    return voxelVertices_;
}

std::span<const uint32_t> AMOURANTH::getVoxelIndices() const { return voxelIndices_; }