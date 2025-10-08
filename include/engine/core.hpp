#ifndef CORE_HPP
#define CORE_HPP

// AMOURANTH RTX Engine, October 2025 - Core simulation logic.
// Manages the simulation state, rendering modes, and DimensionalNavigator.
// Dependencies: Vulkan 1.3+, GLM, SDL3, C++20 standard library.
// Uses Nurb matter and energy for simulation parameters.
// Zachary Geurts 2025

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
#include <algorithm>

static constexpr int kMaxRenderedDimensions = 9;

// Define DimensionData and Ball structs before their usage
struct DimensionData {
    int dimension;
    double observable;
    double potential;
    double nurbMatter;
    double nurbEnergy;
};

struct Ball {
    glm::vec3 position;
    glm::vec3 velocity;
    float mass;
    float radius;
};

class VulkanRenderer;

class DimensionalNavigator {
public:
    DimensionalNavigator(const std::string& name, int width, int height, VulkanRenderer& renderer, const Logging::Logger& logger);
    void initialize(int dimension, uint64_t numVertices);
    int getMode() const { return mode_; }
    float getZoomLevel() const { return zoomLevel_; }
    float getWavePhase() const { return wavePhase_; }
    std::span<const DimensionData> getCache() const { return cache_; }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    void setMode(int mode);
    void setZoomLevel(float zoom);
    void setWavePhase(float phase);
    void setWidth(int width);
    void setHeight(int height);
    VulkanRenderer& getRenderer() const { return renderer_; }

private:
    void initializeCache();
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
    AMOURANTH(DimensionalNavigator* navigator, const Logging::Logger& logger, VkDevice device, VkDeviceMemory vertexBufferMemory, VkPipeline pipeline);
    void render(uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                VkBuffer indexBuffer, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                VkRenderPass renderPass, VkFramebuffer framebuffer, float deltaTime);
    void update(float deltaTime);
    void adjustInfluence(double delta);
    void adjustNurbMatter(double delta);
    void adjustNurbEnergy(double delta);
    void updateCache();
    void updateZoom(bool zoomIn);
    void setMode(int mode);
    void togglePause();
    void toggleUserCam();
    void moveUserCam(float dx, float dy, float dz);
    void setCurrentDimension(int dimension);
    void setWidth(int width);
    void setHeight(int height);

    bool getDebug() const { return ue_.getDebug(); }
    std::span<const glm::vec3> getSphereVertices() const;
    std::span<const uint32_t> getSphereIndices() const;
    std::span<const glm::vec3> getQuadVertices() const;
    std::span<const uint32_t> getQuadIndices() const;
    std::span<const glm::vec3> getTriangleVertices() const;
    std::span<const uint32_t> getTriangleIndices() const;
    std::span<const glm::vec3> getVoxelVertices() const;
    std::span<const uint32_t> getVoxelIndices() const;
    std::span<const DimensionData> getCache() const { return cache_; }
    std::span<const Ball> getBalls() const { return balls_; }
    DimensionalNavigator* getNavigator() const { return simulator_; } // Added getNavigator
    int getMode() const { return mode_; }
    float getWavePhase() const { return wavePhase_; }
    float getZoomLevel() const { return zoomLevel_; }
    glm::vec3 getUserCamPos() const { return userCamPos_; }
    bool isUserCamActive() const { return isUserCamActive_; }
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
    std::vector<Ball> balls_;
};

// Forward declarations for mode-specific rendering
void renderMode1(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const DimensionData> cache, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 VkDevice device, VkDeviceMemory vertexBufferMemory, VkPipeline pipeline, float deltaTime,
                 VkRenderPass renderPass, VkFramebuffer framebuffer);
void renderMode2(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const DimensionData> cache, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 VkDevice device, VkDeviceMemory vertexBufferMemory, VkPipeline pipeline, float deltaTime,
                 VkRenderPass renderPass, VkFramebuffer framebuffer);
void renderMode3(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const DimensionData> cache, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 VkDevice device, VkDeviceMemory vertexBufferMemory, VkPipeline pipeline, float deltaTime,
                 VkRenderPass renderPass, VkFramebuffer framebuffer);
void renderMode4(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const DimensionData> cache, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 VkDevice device, VkDeviceMemory vertexBufferMemory, VkPipeline pipeline, float deltaTime,
                 VkRenderPass renderPass, VkFramebuffer framebuffer);
void renderMode5(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const DimensionData> cache, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 VkDevice device, VkDeviceMemory vertexBufferMemory, VkPipeline pipeline, float deltaTime,
                 VkRenderPass renderPass, VkFramebuffer framebuffer);
void renderMode6(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const DimensionData> cache, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 VkDevice device, VkDeviceMemory vertexBufferMemory, VkPipeline pipeline, float deltaTime,
                 VkRenderPass renderPass, VkFramebuffer framebuffer);
void renderMode7(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const DimensionData> cache, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 VkDevice device, VkDeviceMemory vertexBufferMemory, VkPipeline pipeline, float deltaTime,
                 VkRenderPass renderPass, VkFramebuffer framebuffer);
void renderMode8(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const DimensionData> cache, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 VkDevice device, VkDeviceMemory vertexBufferMemory, VkPipeline pipeline, float deltaTime,
                 VkRenderPass renderPass, VkFramebuffer framebuffer);
void renderMode9(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const DimensionData> cache, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 VkDevice device, VkDeviceMemory vertexBufferMemory, VkPipeline pipeline, float deltaTime,
                 VkRenderPass renderPass, VkFramebuffer framebuffer);

#endif // CORE_HPP