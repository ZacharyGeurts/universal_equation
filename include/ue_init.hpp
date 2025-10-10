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
#include <source_location>

// Forward declarations
class VulkanRenderer;
class AMOURANTH;
class DimensionalNavigator;

class UniversalEquation {
public:
    struct DimensionData {
        int dimension;
        long double scale;
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
                << "Dimension: " << dimension << ", Scale: " << scale
                << ", Observable: " << observable << ", Potential: " << potential
                << ", NURB Matter: " << nurbMatter << ", NURB Energy: " << nurbEnergy
                << ", Spin Energy: " << spinEnergy << ", Momentum Energy: " << momentumEnergy
                << ", Field Energy: " << fieldEnergy << ", God Wave Energy: " << GodWaveEnergy;
            return oss.str();
        }
    };

    struct EnergyResult {
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
                << "Observable: " << observable << ", Potential: " << potential
                << ", NURB Matter: " << nurbMatter << ", NURB Energy: " << nurbEnergy
                << ", Spin Energy: " << spinEnergy << ", Momentum Energy: " << momentumEnergy
                << ", Field Energy: " << fieldEnergy << ", God Wave Energy: " << GodWaveEnergy;
            return oss.str();
        }
    };

    struct DimensionInteraction {
        int vertexIndex;
        long double distance;
        long double strength;
        std::vector<long double> vectorPotential;
        long double godWaveAmplitude;
        DimensionInteraction() : vertexIndex(0), distance(0.0L), strength(0.0L), vectorPotential(), godWaveAmplitude(0.0L) {}
        DimensionInteraction(int idx, long double dist, long double str, const std::vector<long double>& vecPot, long double gwa)
            : vertexIndex(idx), distance(dist), strength(str), vectorPotential(vecPot), godWaveAmplitude(gwa) {}
    };

    // Constructors
    UniversalEquation(int maxDimensions, int mode, long double influence, long double weak, long double collapse,
                     long double twoD, long double threeDInfluence, long double oneDPermeation,
                     long double nurbMatterStrength, long double nurbEnergyStrength, long double alpha,
                     long double beta, long double carrollFactor, long double meanFieldApprox,
                     long double asymCollapse, long double perspectiveTrans, long double perspectiveFocal,
                     long double spinInteraction, long double emFieldStrength, long double renormFactor,
                     long double vacuumEnergy, long double godWaveFreq, bool debug, uint64_t numVertices);
    UniversalEquation(int maxDimensions, int mode, long double influence, long double weak, bool debug, uint64_t numVertices);
    UniversalEquation(const UniversalEquation&);
    UniversalEquation(UniversalEquation&&) noexcept = default;
    UniversalEquation& operator=(const UniversalEquation&);
    UniversalEquation& operator=(UniversalEquation&&) noexcept = default;
    ~UniversalEquation();

    // Methods
    void initializeCalculator(AMOURANTH*);
    void initializeNCube();
    void validateProjectedVertices() const;
    long double computeKineticEnergy(int vertexIndex) const;
    void updateInteractions();
    EnergyResult compute();
    void initializeWithRetry();
    void setGodWaveFreq(long double value);
    long double computeNurbMatter(int vertexIndex) const;
    long double computeNurbEnergy(int vertexIndex) const;
    long double computeSpinEnergy(int vertexIndex) const;
    long double computeEMField(int vertexIndex) const;
    long double computeGodWave(int vertexIndex) const;
    long double computeInteraction(int vertexIndex, long double distance) const;
    std::vector<long double> computeVectorPotential(int vertexIndex) const;
    long double safeExp(long double x) const;
    long double safe_div(long double a, long double b) const;
    void validateVertexIndex(int vertexIndex, const std::source_location& loc = std::source_location::current()) const;
    void setInfluence(long double value);
    void setWeak(long double value);
    void setCollapse(long double value);
    void setTwoD(long double value);
    void setThreeDInfluence(long double value);
    void setOneDPermeation(long double value);
    void setNurbMatterStrength(long double value);
    void setNurbEnergyStrength(long double value);
    void setAlpha(long double value);
    void setBeta(long double value);
    void setCarrollFactor(long double value);
    void setMeanFieldApprox(long double value);
    void setAsymCollapse(long double value);
    void setPerspectiveTrans(long double value);
    void setPerspectiveFocal(long double value);
    void setSpinInteraction(long double value);
    void setEMFieldStrength(long double value);
    void setRenormFactor(long double value);
    void setVacuumEnergy(long double value);
    void setDebug(bool value);
    void setCurrentVertices(uint64_t value);
    void setNavigator(DimensionalNavigator* nav);
    void setNCubeVertex(int vertexIndex, const std::vector<long double>& vertex);
    void setVertexMomentum(int vertexIndex, const std::vector<long double>& momentum);
    void setVertexSpin(int vertexIndex, long double spin);
    void setVertexWaveAmplitude(int vertexIndex, long double amplitude);
    void setProjectedVertex(int vertexIndex, const glm::vec3& vertex);
    void setNCubeVertices(const std::vector<std::vector<long double>>& vertices);
    void setVertexMomenta(const std::vector<std::vector<long double>>& momenta);
    void setVertexSpins(const std::vector<long double>& spins);
    void setVertexWaveAmplitudes(const std::vector<long double>& amplitudes);
    void setProjectedVertices(const std::vector<glm::vec3>& vertices);
    void setTotalCharge(long double value);
    void setMaterialDensity(long double density);
    void evolveTimeStep(long double dt);
    void updateMomentum();
    void advanceCycle();
    std::vector<DimensionData> computeBatch(int startDim, int endDim);
    void exportToCSV(const std::string& filename, const std::vector<DimensionData>& data) const;
    DimensionData updateCache();
    long double computeGodWaveAmplitude(int vertexIndex, long double time) const;
    const std::vector<long double>& getNCubeVertex(int vertexIndex) const;
    const std::vector<long double>& getVertexMomentum(int vertexIndex) const;
    long double getVertexSpin(int vertexIndex) const;
    long double getVertexWaveAmplitude(int vertexIndex) const;
    const glm::vec3& getProjectedVertex(int vertexIndex) const;

    // Getter methods
    long double getInfluence() const { return influence_.load(); }
    long double getWeak() const { return weak_.load(); }
    long double getCollapse() const { return collapse_.load(); }
    long double getTwoD() const { return twoD_.load(); }
    long double getThreeDInfluence() const { return threeDInfluence_.load(); }
    long double getOneDPermeation() const { return oneDPermeation_.load(); }
    long double getNurbMatterStrength() const { return nurbMatterStrength_.load(); }
    long double getNurbEnergyStrength() const { return nurbEnergyStrength_.load(); }
    long double getAlpha() const { return alpha_.load(); }
    long double getBeta() const { return beta_.load(); }
    long double getCarrollFactor() const { return carrollFactor_.load(); }
    long double getMeanFieldApprox() const { return meanFieldApprox_.load(); }
    long double getAsymCollapse() const { return asymCollapse_.load(); }
    long double getPerspectiveTrans() const { return perspectiveTrans_.load(); }
    long double getPerspectiveFocal() const { return perspectiveFocal_.load(); }
    long double getSpinInteraction() const { return spinInteraction_.load(); }
    long double getEMFieldStrength() const { return emFieldStrength_.load(); }
    long double getRenormFactor() const { return renormFactor_.load(); }
    long double getVacuumEnergy() const { return vacuumEnergy_.load(); }
    long double getGodWaveFreq() const { return godWaveFreq_.load(); }
    long double getTotalCharge() const { return totalCharge_.load(); }
    bool getDebug() const { return debug_.load(); }
    bool getNeedsUpdate() const { return needsUpdate_.load(); }
    long double getAvgProjScale() const { return avgProjScale_.load(); }
    float getSimulationTime() const { return simulationTime_.load(); }
    long double getMaterialDensity() const { return materialDensity_.load(); }
    uint64_t getCurrentVertices() const { return currentVertices_.load(); }
    uint64_t getMaxVertices() const { return maxVertices_; }
    int getMaxDimensions() const { return maxDimensions_; }
    int getMode() const { return mode_.load(); }
    int getCurrentDimension() const { return currentDimension_.load(); }
    long double getOmega() const { return omega_; }
    long double getInvMaxDim() const { return invMaxDim_; }
    const std::vector<DimensionInteraction>& getInteractions() const { return interactions_; }
    const std::vector<glm::vec3>& getProjectedVertices() const { return projectedVerts_; }
    const std::vector<long double>& getCachedCos() const { return cachedCos_; }
    const std::vector<long double>& getNurbMatterControlPoints() const { return nurbMatterControlPoints_; }
    const std::vector<long double>& getNurbEnergyControlPoints() const { return nurbEnergyControlPoints_; }
    const std::vector<long double>& getNurbKnots() const { return nurbKnots_; }
    const std::vector<long double>& getNurbWeights() const { return nurbWeights_; }
    const std::vector<DimensionData>& getDimensionData() const { return dimensionData_; }
    const std::vector<long double>& getVertexSpins() const { return vertexSpins_; }
    const std::vector<long double>& getVertexWaveAmplitudes() const { return vertexWaveAmplitudes_; }

    // Methods from universal_equation_quantum.cpp
    long double computeGravitationalPotential(int vertexIndex1, int vertexIndex2) const;
    std::vector<long double> computeGravitationalAcceleration(int vertexIndex) const;

    // Existing methods
    void setMode(int mode);
    void setCurrentDimension(int dimension);
    long double getObservable(int dimension) const { return dimensionData_[dimension - 1].observable; }
    long double getPotential(int dimension) const { return dimensionData_[dimension - 1].potential; }
    long double getNurbMatter(int dimension) const { return dimensionData_[dimension - 1].nurbMatter; }
    long double getNurbEnergy(int dimension) const { return dimensionData_[dimension - 1].nurbEnergy; }

private:
    // Reordered to match constructor initialization list
    std::atomic<long double> influence_;
    std::atomic<long double> weak_;
    std::atomic<long double> collapse_;
    std::atomic<long double> twoD_;
    std::atomic<long double> threeDInfluence_;
    std::atomic<long double> oneDPermeation_;
    std::atomic<long double> nurbMatterStrength_;
    std::atomic<long double> nurbEnergyStrength_;
    std::atomic<long double> alpha_;
    std::atomic<long double> beta_;
    std::atomic<long double> carrollFactor_;
    std::atomic<long double> meanFieldApprox_;
    std::atomic<long double> asymCollapse_;
    std::atomic<long double> perspectiveTrans_;
    std::atomic<long double> perspectiveFocal_;
    std::atomic<long double> spinInteraction_;
    std::atomic<long double> emFieldStrength_;
    std::atomic<long double> renormFactor_;
    std::atomic<long double> vacuumEnergy_;
    std::atomic<long double> godWaveFreq_;
    std::atomic<int> currentDimension_;
    std::atomic<int> mode_;
    std::atomic<bool> debug_;
    std::atomic<bool> needsUpdate_;
    std::atomic<long double> totalCharge_;
    std::atomic<long double> avgProjScale_;
    std::atomic<float> simulationTime_;
    std::atomic<long double> materialDensity_;
    std::atomic<uint64_t> currentVertices_;
    const uint64_t maxVertices_;
    const int maxDimensions_;
    const long double omega_;
    const long double invMaxDim_;
    std::vector<std::vector<long double>> nCubeVertices_;
    std::vector<std::vector<long double>> vertexMomenta_;
    std::vector<long double> vertexSpins_;
    std::vector<long double> vertexWaveAmplitudes_;
    std::vector<DimensionInteraction> interactions_;
    std::vector<glm::vec3> projectedVerts_;
    std::vector<long double> cachedCos_;
    std::vector<long double> nurbMatterControlPoints_;
    std::vector<long double> nurbEnergyControlPoints_;
    std::vector<long double> nurbKnots_;
    std::vector<long double> nurbWeights_;
    std::vector<DimensionData> dimensionData_;
    DimensionalNavigator* navigator_;
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
    VulkanRenderer& renderer_;
    std::vector<UniversalEquation::DimensionData> cache_;
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