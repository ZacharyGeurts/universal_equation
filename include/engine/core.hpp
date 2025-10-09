// Core definitions for AMOURANTH RTX Engine
// Zachary Geurts 2025

#ifndef CORE_HPP
#define CORE_HPP

#include "engine/logging.hpp"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <span>
#include <random>
#include <numbers>
#include <latch>

// Forward declaration of VulkanRenderer
class VulkanRenderer;

#include "ue_init.hpp"

class AMOURANTH {
public:
    struct Ball {
        glm::vec3 position;
        glm::vec3 velocity;
        float mass;
        float radius;
    };

    AMOURANTH(DimensionalNavigator* navigator, VkDevice device,
              VkDeviceMemory vertexBufferMemory, VkPipeline pipeline);
    AMOURANTH(AMOURANTH&& other) noexcept;
    AMOURANTH& operator=(AMOURANTH&& other) noexcept;
    AMOURANTH(const AMOURANTH&) = delete;
    AMOURANTH& operator=(const AMOURANTH&) = delete;

    void render(uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                VkBuffer indexBuffer, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                VkRenderPass renderPass, VkFramebuffer framebuffer, float deltaTime);
    void update(float deltaTime);
    void adjustInfluence(double delta);
    void adjustNurbMatter(double delta);
    void adjustNurbEnergy(double delta);
    void updateZoom(bool zoomIn);
    void setMode(int mode);
    void togglePause();
    void toggleUserCam();
    void moveUserCam(float dx, float dy, float dz);
    void setCurrentDimension(int dimension);
    void setWidth(int width);
    void setHeight(int height);
    std::span<const glm::vec3> getSphereVertices() const;
    std::span<const uint32_t> getSphereIndices() const;
    std::span<const glm::vec3> getQuadVertices() const;
    std::span<const uint32_t> getQuadIndices() const;
    std::span<const glm::vec3> getTriangleVertices() const;
    std::span<const uint32_t> getTriangleIndices() const;
    std::span<const glm::vec3> getVoxelVertices() const;
    std::span<const uint32_t> getVoxelIndices() const;
    bool isUserCamActive() const { return isUserCamActive_; }
    std::span<const Ball> getBalls() const { return balls_; }
    std::span<const UniversalEquation::DimensionData> getCache() const { return cache_; }
    const Logging::Logger& getLogger() const { return logger_; }

private:
    void initializeSphereGeometry();
    void initializeQuadGeometry();
    void initializeTriangleGeometry();
    void initializeVoxelGeometry();
    void initializeCalculator();
    void initializeBalls(float baseMass = 1.0f, float baseRadius = 0.1f, size_t numBalls = 30000);
    void updateCache();

    DimensionalNavigator* simulator_;
    const Logging::Logger& logger_;
    int mode_;
    float wavePhase_;
    float waveSpeed_;
    float zoomLevel_;
    bool isPaused_;
    glm::vec3 userCamPos_;
    bool isUserCamActive_;
    int width_;
    int height_;
    VkDevice device_;
    VkDeviceMemory vertexBufferMemory_;
    VkPipeline pipeline_;
    UniversalEquation ue_;
    std::vector<UniversalEquation::DimensionData> cache_;
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

class DimensionalNavigator {
public:
    static constexpr size_t kMaxRenderedDimensions = 8;

    DimensionalNavigator(const std::string& name, int width, int height, VulkanRenderer& renderer);
    void initialize(int dimension, uint64_t numVertices);
    void setMode(int mode);
    void setZoomLevel(float zoom);
    void setWavePhase(float phase);
    void setWidth(int width);
    void setHeight(int height);
    int getMode() const { return mode_; }
    float getZoomLevel() const { return zoomLevel_; }
    float getWavePhase() const { return wavePhase_; }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    std::span<const UniversalEquation::DimensionData> getCache() const { return cache_; }

private:
    void initializeCache();
    std::string name_;
    int width_;
    int height_;
    int mode_;
    float zoomLevel_;
    float wavePhase_;
    std::vector<UniversalEquation::DimensionData> cache_;
    VulkanRenderer& renderer_;
};

// Declare renderMode functions (implemented in separate .cpp files)
void renderMode1([[maybe_unused]] AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, [[maybe_unused]] VkBuffer vertexBuffer, [[maybe_unused]] VkCommandBuffer commandBuffer, [[maybe_unused]] VkBuffer indexBuffer, [[maybe_unused]] float zoomLevel, [[maybe_unused]] int width, [[maybe_unused]] int height, [[maybe_unused]] float wavePhase, [[maybe_unused]] std::span<const UniversalEquation::DimensionData> cache, [[maybe_unused]] VkPipelineLayout pipelineLayout, [[maybe_unused]] VkDescriptorSet descriptorSet, [[maybe_unused]] VkDevice device, [[maybe_unused]] VkDeviceMemory vertexBufferMemory, [[maybe_unused]] VkPipeline pipeline, [[maybe_unused]] float deltaTime, [[maybe_unused]] VkRenderPass renderPass, [[maybe_unused]] VkFramebuffer framebuffer);

void renderMode2([[maybe_unused]] AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, [[maybe_unused]] VkBuffer vertexBuffer, [[maybe_unused]] VkCommandBuffer commandBuffer, [[maybe_unused]] VkBuffer indexBuffer, [[maybe_unused]] float zoomLevel, [[maybe_unused]] int width, [[maybe_unused]] int height, [[maybe_unused]] float wavePhase, [[maybe_unused]] std::span<const UniversalEquation::DimensionData> cache, [[maybe_unused]] VkPipelineLayout pipelineLayout, [[maybe_unused]] VkDescriptorSet descriptorSet, [[maybe_unused]] VkDevice device, [[maybe_unused]] VkDeviceMemory vertexBufferMemory, [[maybe_unused]] VkPipeline pipeline, [[maybe_unused]] float deltaTime, [[maybe_unused]] VkRenderPass renderPass, [[maybe_unused]] VkFramebuffer framebuffer);

void renderMode3([[maybe_unused]] AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, [[maybe_unused]] VkBuffer vertexBuffer, [[maybe_unused]] VkCommandBuffer commandBuffer, [[maybe_unused]] VkBuffer indexBuffer, [[maybe_unused]] float zoomLevel, [[maybe_unused]] int width, [[maybe_unused]] int height, [[maybe_unused]] float wavePhase, [[maybe_unused]] std::span<const UniversalEquation::DimensionData> cache, [[maybe_unused]] VkPipelineLayout pipelineLayout, [[maybe_unused]] VkDescriptorSet descriptorSet, [[maybe_unused]] VkDevice device, [[maybe_unused]] VkDeviceMemory vertexBufferMemory, [[maybe_unused]] VkPipeline pipeline, [[maybe_unused]] float deltaTime, [[maybe_unused]] VkRenderPass renderPass, [[maybe_unused]] VkFramebuffer framebuffer);

void renderMode4([[maybe_unused]] AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, [[maybe_unused]] VkBuffer vertexBuffer, [[maybe_unused]] VkCommandBuffer commandBuffer, [[maybe_unused]] VkBuffer indexBuffer, [[maybe_unused]] float zoomLevel, [[maybe_unused]] int width, [[maybe_unused]] int height, [[maybe_unused]] float wavePhase, [[maybe_unused]] std::span<const UniversalEquation::DimensionData> cache, [[maybe_unused]] VkPipelineLayout pipelineLayout, [[maybe_unused]] VkDescriptorSet descriptorSet, [[maybe_unused]] VkDevice device, [[maybe_unused]] VkDeviceMemory vertexBufferMemory, [[maybe_unused]] VkPipeline pipeline, [[maybe_unused]] float deltaTime, [[maybe_unused]] VkRenderPass renderPass, [[maybe_unused]] VkFramebuffer framebuffer);

void renderMode5([[maybe_unused]] AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, [[maybe_unused]] VkBuffer vertexBuffer, [[maybe_unused]] VkCommandBuffer commandBuffer, [[maybe_unused]] VkBuffer indexBuffer, [[maybe_unused]] float zoomLevel, [[maybe_unused]] int width, [[maybe_unused]] int height, [[maybe_unused]] float wavePhase, [[maybe_unused]] std::span<const UniversalEquation::DimensionData> cache, [[maybe_unused]] VkPipelineLayout pipelineLayout, [[maybe_unused]] VkDescriptorSet descriptorSet, [[maybe_unused]] VkDevice device, [[maybe_unused]] VkDeviceMemory vertexBufferMemory, [[maybe_unused]] VkPipeline pipeline, [[maybe_unused]] float deltaTime, [[maybe_unused]] VkRenderPass renderPass, [[maybe_unused]] VkFramebuffer framebuffer);

void renderMode6([[maybe_unused]] AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, [[maybe_unused]] VkBuffer vertexBuffer, [[maybe_unused]] VkCommandBuffer commandBuffer, [[maybe_unused]] VkBuffer indexBuffer, [[maybe_unused]] float zoomLevel, [[maybe_unused]] int width, [[maybe_unused]] int height, [[maybe_unused]] float wavePhase, [[maybe_unused]] std::span<const UniversalEquation::DimensionData> cache, [[maybe_unused]] VkPipelineLayout pipelineLayout, [[maybe_unused]] VkDescriptorSet descriptorSet, [[maybe_unused]] VkDevice device, [[maybe_unused]] VkDeviceMemory vertexBufferMemory, [[maybe_unused]] VkPipeline pipeline, [[maybe_unused]] float deltaTime, [[maybe_unused]] VkRenderPass renderPass, [[maybe_unused]] VkFramebuffer framebuffer);

void renderMode7([[maybe_unused]] AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, [[maybe_unused]] VkBuffer vertexBuffer, [[maybe_unused]] VkCommandBuffer commandBuffer, [[maybe_unused]] VkBuffer indexBuffer, [[maybe_unused]] float zoomLevel, [[maybe_unused]] int width, [[maybe_unused]] int height, [[maybe_unused]] float wavePhase, [[maybe_unused]] std::span<const UniversalEquation::DimensionData> cache, [[maybe_unused]] VkPipelineLayout pipelineLayout, [[maybe_unused]] VkDescriptorSet descriptorSet, [[maybe_unused]] VkDevice device, [[maybe_unused]] VkDeviceMemory vertexBufferMemory, [[maybe_unused]] VkPipeline pipeline, [[maybe_unused]] float deltaTime, [[maybe_unused]] VkRenderPass renderPass, [[maybe_unused]] VkFramebuffer framebuffer);

void renderMode8([[maybe_unused]] AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, [[maybe_unused]] VkBuffer vertexBuffer, [[maybe_unused]] VkCommandBuffer commandBuffer, [[maybe_unused]] VkBuffer indexBuffer, [[maybe_unused]] float zoomLevel, [[maybe_unused]] int width, [[maybe_unused]] int height, [[maybe_unused]] float wavePhase, [[maybe_unused]] std::span<const UniversalEquation::DimensionData> cache, [[maybe_unused]] VkPipelineLayout pipelineLayout, [[maybe_unused]] VkDescriptorSet descriptorSet, [[maybe_unused]] VkDevice device, [[maybe_unused]] VkDeviceMemory vertexBufferMemory, [[maybe_unused]] VkPipeline pipeline, [[maybe_unused]] float deltaTime, [[maybe_unused]] VkRenderPass renderPass, [[maybe_unused]] VkFramebuffer framebuffer);

void renderMode9([[maybe_unused]] AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, [[maybe_unused]] VkBuffer vertexBuffer, [[maybe_unused]] VkCommandBuffer commandBuffer, [[maybe_unused]] VkBuffer indexBuffer, [[maybe_unused]] float zoomLevel, [[maybe_unused]] int width, [[maybe_unused]] int height, [[maybe_unused]] float wavePhase, [[maybe_unused]] std::span<const UniversalEquation::DimensionData> cache, [[maybe_unused]] VkPipelineLayout pipelineLayout, [[maybe_unused]] VkDescriptorSet descriptorSet, [[maybe_unused]] VkDevice device, [[maybe_unused]] VkDeviceMemory vertexBufferMemory, [[maybe_unused]] VkPipeline pipeline, [[maybe_unused]] float deltaTime, [[maybe_unused]] VkRenderPass renderPass, [[maybe_unused]] VkFramebuffer framebuffer);

#endif // CORE_HPP