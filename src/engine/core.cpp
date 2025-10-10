// src/engine/core.cpp
// AMOURANTH RTX Engine, October 2025 - Core simulation logic implementation.
// Implements AMOURANTH and rendering modes.
// Dependencies: Vulkan 1.3+, GLM, SDL3, C++20 standard library.
// Supported platforms: Windows, Linux.
// Zachary Geurts 2025

#include "engine/core.hpp"
#include "engine/Vulkan_init.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <source_location>
#include <random>

DimensionalNavigator::DimensionalNavigator(const std::string& name, int width, int height, VulkanRenderer& renderer)
    : name_(name), width_(width), height_(height), mode_(1), zoomLevel_(1.0f), wavePhase_(0.0f), renderer_(renderer) {
    LOG_INFO_CAT("Simulation", "Initializing DimensionalNavigator with name: {}, width: {}, height: {}", 
                 std::source_location::current(), name, width, height);
    initializeCache();
}

void DimensionalNavigator::initialize(int dimension, [[maybe_unused]] uint64_t numVertices) {
    LOG_DEBUG_CAT("Simulation", "Initializing DimensionalNavigator: dimension: {}, numVertices: {}", 
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
        cache_[i].spinEnergy = 0.0L;
        cache_[i].momentumEnergy = 0.0L;
        cache_[i].fieldEnergy = 0.0L;
        cache_[i].GodWaveEnergy = 0.0L;
    }
    LOG_DEBUG_CAT("Simulation", "DimensionalNavigator cache initialized with {} entries", 
                  std::source_location::current(), cache_.size());
}

void DimensionalNavigator::setMode(int mode) {
    mode_ = glm::clamp(mode, 1, 9);
    LOG_DEBUG_CAT("Simulation", "DimensionalNavigator mode set to {}", 
                  std::source_location::current(), mode_);
}

void DimensionalNavigator::setZoomLevel(float zoom) {
    zoomLevel_ = std::max(0.1f, zoom);
    LOG_DEBUG_CAT("Simulation", "DimensionalNavigator zoomLevel set to {:.3f}", 
                  std::source_location::current(), zoomLevel_);
}

void DimensionalNavigator::setWavePhase(float phase) {
    wavePhase_ = phase;
    LOG_DEBUG_CAT("Simulation", "DimensionalNavigator wavePhase set to {:.3f}", 
                  std::source_location::current(), wavePhase_);
}

void DimensionalNavigator::setWidth(int width) {
    width_ = width;
    LOG_DEBUG_CAT("Simulation", "DimensionalNavigator width set to {}", 
                  std::source_location::current(), width_);
}

void DimensionalNavigator::setHeight(int height) {
    height_ = height;
    LOG_DEBUG_CAT("Simulation", "DimensionalNavigator height set to {}", 
                  std::source_location::current(), height_);
}

AMOURANTH::AMOURANTH(DimensionalNavigator* navigator, VkDevice device, VkDeviceMemory vertexBufferMemory, VkPipeline pipeline)
    : simulator_(navigator), logger_(Logging::Logger::get()), mode_(1), wavePhase_(0.0f), waveSpeed_(1.0f), zoomLevel_(1.0f), isPaused_(false),
      userCamPos_(0.0f), isUserCamActive_(false), width_(navigator ? navigator->getWidth() : 800),
      height_(navigator ? navigator->getHeight() : 600), device_(device),
      vertexBufferMemory_(vertexBufferMemory), pipeline_(pipeline),
      ue_(8, 1, 2.5, 0.0072973525693, true, 30000) {
    if (!navigator) {
        LOG_ERROR_CAT("Simulation", "Null DimensionalNavigator provided");
        throw std::runtime_error("Null DimensionalNavigator provided");
    }
    if (!device_ || !vertexBufferMemory_ || !pipeline_) {
        LOG_ERROR_CAT("Vulkan", "Invalid Vulkan resources: device={:p}, vertexBufferMemory={:p}, pipeline={:p}", 
                      std::source_location::current(), static_cast<void*>(device_), static_cast<void*>(vertexBufferMemory_), static_cast<void*>(pipeline_));
        throw std::runtime_error("Invalid Vulkan resources");
    }
    LOG_INFO_CAT("Simulation", "Initializing AMOURANTH with width: {}, height: {}", 
                 std::source_location::current(), width_, height_);
    initializeSphereGeometry();
    initializeQuadGeometry();
    initializeTriangleGeometry();
    initializeVoxelGeometry();
    initializeCalculator();
    initializeBalls();
    LOG_INFO_CAT("Simulation", "AMOURANTH initialized successfully");
}

AMOURANTH::AMOURANTH(AMOURANTH&& other) noexcept
    : simulator_(other.simulator_),
      logger_(other.logger_),
      mode_(other.mode_),
      wavePhase_(other.wavePhase_),
      waveSpeed_(other.waveSpeed_),
      zoomLevel_(other.zoomLevel_),
      isPaused_(other.isPaused_),
      userCamPos_(other.userCamPos_),
      isUserCamActive_(other.isUserCamActive_),
      width_(other.width_),
      height_(other.height_),
      device_(other.device_),
      vertexBufferMemory_(other.vertexBufferMemory_),
      pipeline_(other.pipeline_),
      ue_(std::move(other.ue_)),
      cache_(std::move(other.cache_)),
      sphereVertices_(std::move(other.sphereVertices_)),
      sphereIndices_(std::move(other.sphereIndices_)),
      quadVertices_(std::move(other.quadVertices_)),
      quadIndices_(std::move(other.quadIndices_)),
      triangleVertices_(std::move(other.triangleVertices_)),
      triangleIndices_(std::move(other.triangleIndices_)),
      voxelVertices_(std::move(other.voxelVertices_)),
      voxelIndices_(std::move(other.voxelIndices_)),
      balls_(std::move(other.balls_)) {
    other.simulator_ = nullptr;
    other.device_ = VK_NULL_HANDLE;
    other.vertexBufferMemory_ = VK_NULL_HANDLE;
    other.pipeline_ = VK_NULL_HANDLE;
}

AMOURANTH& AMOURANTH::operator=(AMOURANTH&& other) noexcept {
    if (this != &other) {
        simulator_ = other.simulator_;
        // logger_ is a const reference, cannot be reassigned
        mode_ = other.mode_;
        wavePhase_ = other.wavePhase_;
        waveSpeed_ = other.waveSpeed_;
        zoomLevel_ = other.zoomLevel_;
        isPaused_ = other.isPaused_;
        userCamPos_ = other.userCamPos_;
        isUserCamActive_ = other.isUserCamActive_;
        width_ = other.width_;
        height_ = other.height_;
        device_ = other.device_;
        vertexBufferMemory_ = other.vertexBufferMemory_;
        pipeline_ = other.pipeline_;
        ue_ = std::move(other.ue_);
        cache_ = std::move(other.cache_);
        sphereVertices_ = std::move(other.sphereVertices_);
        sphereIndices_ = std::move(other.sphereIndices_);
        quadVertices_ = std::move(other.quadVertices_);
        quadIndices_ = std::move(other.quadIndices_);
        triangleVertices_ = std::move(other.triangleVertices_);
        triangleIndices_ = std::move(other.triangleIndices_);
        voxelVertices_ = std::move(other.voxelVertices_);
        voxelIndices_ = std::move(other.voxelIndices_);
        balls_ = std::move(other.balls_);
        other.simulator_ = nullptr;
        other.device_ = VK_NULL_HANDLE;
        other.vertexBufferMemory_ = VK_NULL_HANDLE;
        other.pipeline_ = VK_NULL_HANDLE;
    }
    return *this;
}

void AMOURANTH::render(uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                       VkBuffer indexBuffer, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                       VkRenderPass renderPass, VkFramebuffer framebuffer, float deltaTime) {
    LOG_DEBUG_CAT("Renderer", "Rendering frame for image index: {}", 
                  std::source_location::current(), imageIndex);
    if (!device_ || !vertexBuffer || !commandBuffer || !indexBuffer || !pipelineLayout || !descriptorSet || !renderPass || !framebuffer) {
        LOG_ERROR_CAT("Vulkan", "Invalid Vulkan resources: device={:p}, vertexBuffer={:p}, commandBuffer={:p}, indexBuffer={:p}, pipelineLayout={:p}, descriptorSet={:p}, renderPass={:p}, framebuffer={:p}", 
                      std::source_location::current(), static_cast<void*>(device_), static_cast<void*>(vertexBuffer), static_cast<void*>(commandBuffer), 
                      static_cast<void*>(indexBuffer), static_cast<void*>(pipelineLayout), static_cast<void*>(descriptorSet), 
                      static_cast<void*>(renderPass), static_cast<void*>(framebuffer));
        throw std::runtime_error("Invalid Vulkan resources in render");
    }
    switch (mode_) {
        case 1: renderMode1(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, zoomLevel_,
                           width_, height_, wavePhase_, cache_, pipelineLayout, descriptorSet,
                           device_, vertexBufferMemory_, pipeline_, deltaTime, renderPass, framebuffer); break;
        case 2: renderMode2(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, zoomLevel_,
                           width_, height_, wavePhase_, cache_, pipelineLayout, descriptorSet,
                           device_, vertexBufferMemory_, pipeline_, deltaTime, renderPass, framebuffer); break;
        case 3: renderMode3(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, zoomLevel_,
                           width_, height_, wavePhase_, cache_, pipelineLayout, descriptorSet,
                           device_, vertexBufferMemory_, pipeline_, deltaTime, renderPass, framebuffer); break;
        case 4: renderMode4(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, zoomLevel_,
                           width_, height_, wavePhase_, cache_, pipelineLayout, descriptorSet,
                           device_, vertexBufferMemory_, pipeline_, deltaTime, renderPass, framebuffer); break;
        case 5: renderMode5(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, zoomLevel_,
                           width_, height_, wavePhase_, cache_, pipelineLayout, descriptorSet,
                           device_, vertexBufferMemory_, pipeline_, deltaTime, renderPass, framebuffer); break;
        case 6: renderMode6(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, zoomLevel_,
                           width_, height_, wavePhase_, cache_, pipelineLayout, descriptorSet,
                           device_, vertexBufferMemory_, pipeline_, deltaTime, renderPass, framebuffer); break;
        case 7: renderMode7(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, zoomLevel_,
                           width_, height_, wavePhase_, cache_, pipelineLayout, descriptorSet,
                           device_, vertexBufferMemory_, pipeline_, deltaTime, renderPass, framebuffer); break;
        case 8: renderMode8(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, zoomLevel_,
                           width_, height_, wavePhase_, cache_, pipelineLayout, descriptorSet,
                           device_, vertexBufferMemory_, pipeline_, deltaTime, renderPass, framebuffer); break;
        case 9: renderMode9(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, zoomLevel_,
                           width_, height_, wavePhase_, cache_, pipelineLayout, descriptorSet,
                           device_, vertexBufferMemory_, pipeline_, deltaTime, renderPass, framebuffer); break;
        default: renderMode1(this, imageIndex, vertexBuffer, commandBuffer, indexBuffer, zoomLevel_,
                            width_, height_, wavePhase_, cache_, pipelineLayout, descriptorSet,
                            device_, vertexBufferMemory_, pipeline_, deltaTime, renderPass, framebuffer); break;
    }
}

void AMOURANTH::update(float deltaTime) {
    if (!isPaused_) {
        std::latch latch(1);
        wavePhase_ += waveSpeed_ * deltaTime;
        simulator_->setWavePhase(wavePhase_);
        updateCache();
        LOG_DEBUG_CAT("Simulation", "Updated simulation with deltaTime: {:.3f}, wavePhase: {:.3f}", 
                      std::source_location::current(), deltaTime, wavePhase_);
        latch.count_down();
        latch.wait();
    }
}

void AMOURANTH::adjustInfluence(double delta) {
    std::latch latch(1);
    ue_.setInfluence(ue_.getInfluence() + delta);
    updateCache();
    LOG_DEBUG_CAT("Simulation", "Adjusted influence by {}", 
                  std::source_location::current(), delta);
    latch.count_down();
    latch.wait();
}

void AMOURANTH::adjustNurbMatter(double delta) {
    std::latch latch(1);
    ue_.setNurbMatterStrength(ue_.getNurbMatterStrength() + delta);
    updateCache();
    LOG_DEBUG_CAT("Simulation", "Adjusted nurbMatter by {}", 
                  std::source_location::current(), delta);
    latch.count_down();
    latch.wait();
}

void AMOURANTH::adjustNurbEnergy(double delta) {
    std::latch latch(1);
    ue_.setNurbEnergyStrength(ue_.getNurbEnergyStrength() + delta);
    updateCache();
    LOG_DEBUG_CAT("Simulation", "Adjusted nurbEnergy by {}", 
                  std::source_location::current(), delta);
    latch.count_down();
    latch.wait();
}

void AMOURANTH::updateZoom(bool zoomIn) {
    std::latch latch(1);
    zoomLevel_ *= zoomIn ? 1.1f : 0.9f;
    zoomLevel_ = std::max(0.1f, zoomLevel_);
    simulator_->setZoomLevel(zoomLevel_);
    LOG_DEBUG_CAT("Simulation", "Updated zoom level to {:.3f}", 
                  std::source_location::current(), zoomLevel_);
    latch.count_down();
    latch.wait();
}

void AMOURANTH::setMode(int mode) {
    std::latch latch(1);
    mode_ = glm::clamp(mode, 1, 9);
    simulator_->setMode(mode_);
    ue_.setMode(mode_);
    LOG_INFO_CAT("Renderer", "Set rendering mode to {}", 
                 std::source_location::current(), mode_);
    latch.count_down();
    latch.wait();
}

void AMOURANTH::togglePause() {
    isPaused_ = !isPaused_;
    LOG_DEBUG_CAT("Simulation", "Pause state set to {}", 
                  std::source_location::current(), isPaused_ ? "true" : "false");
}

void AMOURANTH::toggleUserCam() {
    isUserCamActive_ = !isUserCamActive_;
    LOG_DEBUG_CAT("Simulation", "User camera active set to {}", 
                  std::source_location::current(), isUserCamActive_ ? "true" : "false");
}

void AMOURANTH::moveUserCam(float dx, float dy, float dz) {
    userCamPos_ += glm::vec3(dx, dy, dz);
    LOG_DEBUG_CAT("Simulation", "Moved user camera to position: ({:.3f}, {:.3f}, {:.3f})", 
                  std::source_location::current(), userCamPos_.x, userCamPos_.y, userCamPos_.z);
}

void AMOURANTH::setCurrentDimension(int dimension) {
    std::latch latch(1);
    ue_.setCurrentDimension(dimension);
    LOG_DEBUG_CAT("Simulation", "Set current dimension to {}", 
                  std::source_location::current(), dimension);
    latch.count_down();
    latch.wait();
}

void AMOURANTH::setWidth(int width) {
    width_ = width;
    LOG_DEBUG_CAT("Simulation", "AMOURANTH width set to {}", 
                  std::source_location::current(), width_);
}

void AMOURANTH::setHeight(int height) {
    height_ = height;
    LOG_DEBUG_CAT("Simulation", "AMOURANTH height set to {}", 
                  std::source_location::current(), height_);
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
        LOG_ERROR_CAT("Simulation", "Misaligned sphereVertices_: address: {:#x}", 
                      std::source_location::current(), reinterpret_cast<std::uintptr_t>(sphereVertices_.data()));
        throw std::runtime_error("Misaligned sphereVertices_");
    }
    LOG_INFO_CAT("Simulation", "Initialized sphere geometry with {} vertices, {} indices", 
                 std::source_location::current(), sphereVertices_.size(), sphereIndices_.size());
}

void AMOURANTH::initializeQuadGeometry() {
    quadVertices_ = {{-1.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f}};
    quadIndices_ = {0, 1, 2, 2, 3, 0};
    if (!quadVertices_.empty() && reinterpret_cast<std::uintptr_t>(quadVertices_.data()) % alignof(glm::vec3) != 0) {
        LOG_ERROR_CAT("Simulation", "Misaligned quadVertices_: address: {:#x}", 
                      std::source_location::current(), reinterpret_cast<std::uintptr_t>(quadVertices_.data()));
        throw std::runtime_error("Misaligned quadVertices_");
    }
    LOG_INFO_CAT("Simulation", "Initialized quad geometry with {} vertices, {} indices", 
                 std::source_location::current(), quadVertices_.size(), quadIndices_.size());
}

void AMOURANTH::initializeTriangleGeometry() {
    triangleVertices_ = {{0.0f, 0.5f, 0.0f}, {-0.5f, -0.5f, 0.0f}, {0.5f, -0.5f, 0.0f}};
    triangleIndices_ = {0, 1, 2};
    if (!triangleVertices_.empty() && reinterpret_cast<std::uintptr_t>(triangleVertices_.data()) % alignof(glm::vec3) != 0) {
        LOG_ERROR_CAT("Simulation", "Misaligned triangleVertices_: address: {:#x}", 
                      std::source_location::current(), reinterpret_cast<std::uintptr_t>(triangleVertices_.data()));
        throw std::runtime_error("Misaligned triangleVertices_");
    }
    LOG_INFO_CAT("Simulation", "Initialized triangle geometry with {} vertices, {} indices", 
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
        LOG_ERROR_CAT("Simulation", "Misaligned voxelVertices_: address: {:#x}", 
                      std::source_location::current(), reinterpret_cast<std::uintptr_t>(voxelVertices_.data()));
        throw std::runtime_error("Misaligned voxelVertices_");
    }
    LOG_INFO_CAT("Simulation", "Initialized voxel geometry with {} vertices, {} indices", 
                 std::source_location::current(), voxelVertices_.size(), voxelIndices_.size());
}

void AMOURANTH::initializeCalculator() {
    std::latch latch(1);
    try {
        LOG_DEBUG_CAT("Simulation", "Initializing calculator for UniversalEquation");
        ue_.initializeCalculator(this);
        updateCache();
        LOG_INFO_CAT("Simulation", "Calculator initialized successfully");
    } catch (const std::exception& e) {
        LOG_ERROR_CAT("Simulation", "Calculator initialization failed: {}", 
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
    LOG_INFO_CAT("Simulation", "Initialized {} balls with mass scale: {:.3f}", 
                 std::source_location::current(), balls_.size(), baseMass);
    latch.count_down();
    latch.wait();
}

void AMOURANTH::updateCache() {
    cache_.resize(DimensionalNavigator::kMaxRenderedDimensions);
    for (size_t i = 0; i < cache_.size(); ++i) {
        cache_[i].dimension = static_cast<int>(i + 1);
        cache_[i].observable = ue_.getObservable(i + 1);
        cache_[i].potential = ue_.getPotential(i + 1);
        cache_[i].nurbMatter = ue_.getNurbMatter(i + 1);
        cache_[i].nurbEnergy = ue_.getNurbEnergy(i + 1);
        cache_[i].spinEnergy = 0.0L;
        cache_[i].momentumEnergy = 0.0L;
        cache_[i].fieldEnergy = 0.0L;
        cache_[i].GodWaveEnergy = 0.0L;
    }
    LOG_DEBUG_CAT("Simulation", "Updated cache with {} entries", 
                  std::source_location::current(), cache_.size());
}

std::span<const glm::vec3> AMOURANTH::getSphereVertices() const {
    if (!sphereVertices_.empty() && reinterpret_cast<std::uintptr_t>(sphereVertices_.data()) % alignof(glm::vec3) != 0) {
        LOG_ERROR_CAT("Simulation", "Misaligned sphereVertices_: address: {:#x}", 
                      std::source_location::current(), reinterpret_cast<std::uintptr_t>(sphereVertices_.data()));
        throw std::runtime_error("Misaligned sphereVertices_");
    }
    return sphereVertices_;
}

std::span<const uint32_t> AMOURANTH::getSphereIndices() const { return sphereIndices_; }

std::span<const glm::vec3> AMOURANTH::getQuadVertices() const {
    if (!quadVertices_.empty() && reinterpret_cast<std::uintptr_t>(quadVertices_.data()) % alignof(glm::vec3) != 0) {
        LOG_ERROR_CAT("Simulation", "Misaligned quadVertices_: address: {:#x}", 
                      std::source_location::current(), reinterpret_cast<std::uintptr_t>(quadVertices_.data()));
        throw std::runtime_error("Misaligned quadVertices_");
    }
    return quadVertices_;
}

std::span<const uint32_t> AMOURANTH::getQuadIndices() const { return quadIndices_; }

std::span<const glm::vec3> AMOURANTH::getTriangleVertices() const {
    if (!triangleVertices_.empty() && reinterpret_cast<std::uintptr_t>(triangleVertices_.data()) % alignof(glm::vec3) != 0) {
        LOG_ERROR_CAT("Simulation", "Misaligned triangleVertices_: address: {:#x}", 
                      std::source_location::current(), reinterpret_cast<std::uintptr_t>(triangleVertices_.data()));
        throw std::runtime_error("Misaligned triangleVertices_");
    }
    return triangleVertices_;
}

std::span<const uint32_t> AMOURANTH::getTriangleIndices() const { return triangleIndices_; }

std::span<const glm::vec3> AMOURANTH::getVoxelVertices() const {
    if (!voxelVertices_.empty() && reinterpret_cast<std::uintptr_t>(voxelVertices_.data()) % alignof(glm::vec3) != 0) {
        LOG_ERROR_CAT("Simulation", "Misaligned voxelVertices_: address: {:#x}", 
                      std::source_location::current(), reinterpret_cast<std::uintptr_t>(voxelVertices_.data()));
        throw std::runtime_error("Misaligned voxelVertices_");
    }
    return voxelVertices_;
}

std::span<const uint32_t> AMOURANTH::getVoxelIndices() const { return voxelIndices_; }