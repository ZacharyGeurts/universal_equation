// include/ue_init.hpp
// Universal Equation initialization for AMOURANTH RTX Engine
// Zachary Geurts 2025

#ifndef UE_INIT_HPP
#define UE_INIT_HPP

#include "engine/logging.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <string>
#include <cstdint>
#include <atomic>
#include <numbers>
#include <format>
#include <span>
#include <sstream>
#include <iomanip>
#include <vulkan/vulkan.h>
#include <random>

// Forward declarations
class VulkanRenderer;

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
    std::span<const UniversalEquation::DimensionData> getCache() const;

private:
    void initializeCache();
    std::string name_;
    int width_;
    int height_;
    int mode_;
    float zoomLevel_;
    float wavePhase_;
    VulkanRenderer& renderer_;
    std::vector<UniversalEquation::DimensionData> cache_;
};

class UniversalEquation {
public:
    struct DimensionData {
        int dimension;
        long double observable;
        long double potential;
        long double nurbMatter;
        long double nurbEnergy;
        long double spinEnergy;
        long double momentumEnergy;
        long double fieldEnergy;
        long double GodWaveEnergy;
        std::string toString() const {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(10)
                << "Dimension: " << dimension << ", Observable: " << observable
                << ", Potential: " << potential << ", NURB Matter: " << nurbMatter
                << ", NURB Energy: " << nurbEnergy << ", Spin Energy: " << spinEnergy
                << ", Momentum Energy: " << momentumEnergy << ", Field Energy: " << fieldEnergy
                << ", God Wave Energy: " << GodWaveEnergy;
            return oss.str();
        }
    };

    // Rest of UniversalEquation definition remains the same as in previous response
    // (Omitted for brevity, but includes EnergyResult, DimensionInteraction, constructors, etc.)
};

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
    glm::mat4 getTransform() const {
        glm::mat4 transform = glm::mat4(1.0f);
        transform = glm::translate(transform, userCamPos_);
        transform = glm::scale(transform, glm::vec3(zoomLevel_));
        return transform;
    }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    DimensionalNavigator* getNavigator() const { return simulator_; }

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

#endif // UE_INIT_HPP