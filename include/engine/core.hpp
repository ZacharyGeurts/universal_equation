// AMOURANTH RTX Engine, October 2025 - Core interface definitions.
// Defines AMOURANTH, DimensionalNavigator, and VulkanRenderer interfaces.
// Dependencies: Vulkan 1.3+, GLM, SDL3, C++20 standard library.
// Supported platforms: Windows, Linux.
// Zachary Geurts 2025

#ifndef CORE_HPP
#define CORE_HPP

#include "engine/logging.hpp"
#include "ue_init.hpp"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <span>
#include <random>
#include <numbers>
#include <latch>

class AMOURANTH;

class VulkanRenderer {
public:
    VulkanRenderer(VkInstance instance, VkSurfaceKHR surface, std::span<const glm::vec3> vertices,
                  std::span<const uint32_t> indices, int width, int height, const Logging::Logger& logger);
    ~VulkanRenderer();
    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;

    void createDevice();
    void createSwapchain(int width, int height);
    void createRenderPass();
    void createGraphicsPipeline();
    void createFramebuffers();
    void createVertexBuffer(std::span<const glm::vec3> vertices, std::span<const uint32_t> indices);
    void renderFrame(AMOURANTH* amouranth);
    void handleResize(int width, int height);

private:
    VkInstance instance_;
    VkSurfaceKHR surface_;
    VkPhysicalDevice physicalDevice_;
    VkDevice device_;
    VkQueue graphicsQueue_;
    VkQueue presentQueue_;
    VkSwapchainKHR swapchain_;
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    VkRenderPass renderPass_;
    VkPipeline pipeline_;
    VkPipelineLayout pipelineLayout_;
    VkDescriptorSet descriptorSet_;
    VkBuffer vertexBuffer_;
    VkDeviceMemory vertexBufferMemory_;
    VkBuffer indexBuffer_;
    VkCommandPool commandPool_;
    std::vector<VkCommandBuffer> commandBuffers_;
    std::vector<VkFramebuffer> framebuffers_;
    const Logging::Logger& logger_;
};

class DimensionalNavigator {
public:
    static constexpr size_t kMaxRenderedDimensions = 8;

    DimensionalNavigator(const std::string& name, int width, int height, VulkanRenderer& renderer, const Logging::Logger& logger);
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
    const Logging::Logger& logger_;
};

class AMOURANTH {
public:
    struct Ball {
        glm::vec3 position;
        glm::vec3 velocity;
        float mass;
        float radius;
    };

    AMOURANTH(DimensionalNavigator* navigator, const Logging::Logger& logger, VkDevice device,
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

private:
    void initializeSphereGeometry();
    void initializeQuadGeometry();
    void initializeTriangleGeometry();
    void initializeVoxelGeometry();
    void initializeCalculator();
    void initializeBalls(float baseMass = 1.0f, float baseRadius = 0.1f, size_t numBalls = 10);
    void updateCache();

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

void renderMode1(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const UniversalEquation::DimensionData> cache, VkPipelineLayout pipelineLayout,
                 VkDescriptorSet descriptorSet, VkDevice device, VkDeviceMemory vertexBufferMemory,
                 VkPipeline pipeline, float deltaTime, VkRenderPass renderPass, VkFramebuffer framebuffer);
void renderMode2(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const UniversalEquation::DimensionData> cache, VkPipelineLayout pipelineLayout,
                 VkDescriptorSet descriptorSet, VkDevice device, VkDeviceMemory vertexBufferMemory,
                 VkPipeline pipeline, float deltaTime, VkRenderPass renderPass, VkFramebuffer framebuffer);
void renderMode3(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const UniversalEquation::DimensionData> cache, VkPipelineLayout pipelineLayout,
                 VkDescriptorSet descriptorSet, VkDevice device, VkDeviceMemory vertexBufferMemory,
                 VkPipeline pipeline, float deltaTime, VkRenderPass renderPass, VkFramebuffer framebuffer);
void renderMode4(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const UniversalEquation::DimensionData> cache, VkPipelineLayout pipelineLayout,
                 VkDescriptorSet descriptorSet, VkDevice device, VkDeviceMemory vertexBufferMemory,
                 VkPipeline pipeline, float deltaTime, VkRenderPass renderPass, VkFramebuffer framebuffer);
void renderMode5(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const UniversalEquation::DimensionData> cache, VkPipelineLayout pipelineLayout,
                 VkDescriptorSet descriptorSet, VkDevice device, VkDeviceMemory vertexBufferMemory,
                 VkPipeline pipeline, float deltaTime, VkRenderPass renderPass, VkFramebuffer framebuffer);
void renderMode6(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const UniversalEquation::DimensionData> cache, VkPipelineLayout pipelineLayout,
                 VkDescriptorSet descriptorSet, VkDevice device, VkDeviceMemory vertexBufferMemory,
                 VkPipeline pipeline, float deltaTime, VkRenderPass renderPass, VkFramebuffer framebuffer);
void renderMode7(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const UniversalEquation::DimensionData> cache, VkPipelineLayout pipelineLayout,
                 VkDescriptorSet descriptorSet, VkDevice device, VkDeviceMemory vertexBufferMemory,
                 VkPipeline pipeline, float deltaTime, VkRenderPass renderPass, VkFramebuffer framebuffer);
void renderMode8(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const UniversalEquation::DimensionData> cache, VkPipelineLayout pipelineLayout,
                 VkDescriptorSet descriptorSet, VkDevice device, VkDeviceMemory vertexBufferMemory,
                 VkPipeline pipeline, float deltaTime, VkRenderPass renderPass, VkFramebuffer framebuffer);
void renderMode9(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const UniversalEquation::DimensionData> cache, VkPipelineLayout pipelineLayout,
                 VkDescriptorSet descriptorSet, VkDevice device, VkDeviceMemory vertexBufferMemory,
                 VkPipeline pipeline, float deltaTime, VkRenderPass renderPass, VkFramebuffer framebuffer);

#endif // CORE_HPP