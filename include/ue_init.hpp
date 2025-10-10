#pragma once
#ifndef UE_INIT_HPP
#define UE_INIT_HPP

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <vulkan/vulkan.h>
#include "engine/logging.hpp"

class VulkanRenderer; // Forward declaration

namespace UniversalEquation {
    struct DimensionData {
        int dimension = 0;
        long double scale = 1.0L;
        glm::vec3 position = glm::vec3(0.0f);
        float value = 1.0f;
        long double nurbEnergy = 1.0L;
        long double nurbMatter = 0.032774L;
        long double potential = 1.0L;
        long double observable = 1.0L;
        long double spinEnergy = 0.0L;
        long double momentumEnergy = 0.0L;
        long double fieldEnergy = 0.0L;
        long double GodWaveEnergy = 0.0L;

        std::string toString() const {
            return "DimensionData{dimension=" + std::to_string(dimension) +
                   ", scale=" + std::to_string(scale) +
                   ", position=(" + std::to_string(position.x) + "," +
                   std::to_string(position.y) + "," + std::to_string(position.z) + ")" +
                   ", value=" + std::to_string(value) +
                   ", nurbEnergy=" + std::to_string(nurbEnergy) +
                   ", nurbMatter=" + std::to_string(nurbMatter) +
                   ", potential=" + std::to_string(potential) +
                   ", observable=" + std::to_string(observable) +
                   ", spinEnergy=" + std::to_string(spinEnergy) +
                   ", momentumEnergy=" + std::to_string(momentumEnergy) +
                   ", fieldEnergy=" + std::to_string(fieldEnergy) +
                   ", GodWaveEnergy=" + std::to_string(GodWaveEnergy) + "}";
        }
    };

    struct EnergyResult {
        long double observable = 0.0L;
        long double potential = 0.0L;
        long double nurbMatter = 0.0L;
        long double nurbEnergy = 0.0L;
        long double spinEnergy = 0.0L;
        long double momentumEnergy = 0.0L;
        long double fieldEnergy = 0.0L;
        long double GodWaveEnergy = 0.0L;

        std::string toString() const {
            return "EnergyResult{observable=" + std::to_string(observable) +
                   ", potential=" + std::to_string(potential) +
                   ", nurbMatter=" + std::to_string(nurbMatter) +
                   ", nurbEnergy=" + std::to_string(nurbEnergy) +
                   ", spinEnergy=" + std::to_string(spinEnergy) +
                   ", momentumEnergy=" + std::to_string(momentumEnergy) +
                   ", fieldEnergy=" + std::to_string(fieldEnergy) +
                   ", GodWaveEnergy=" + std::to_string(GodWaveEnergy) + "}";
        }
    };

    struct DimensionInteraction {
        int index;
        long double distance;
        long double strength;
        std::vector<long double> vectorPotential;
        long double godWaveAmplitude;

        DimensionInteraction(int idx, long double dist, long double str, std::vector<long double> vecPot, long double gwAmp)
            : index(idx), distance(dist), strength(str), vectorPotential(std::move(vecPot)), godWaveAmplitude(gwAmp) {}
    };
}

class DimensionalNavigator {
public:
    DimensionalNavigator(const char* name, int width, int height, [[maybe_unused]] VulkanRenderer& renderer)
        : name_(name), width_(width), height_(height) {}

    void setWidth(int width) { width_ = width; }
    void setHeight(int height) { height_ = height; }
    void setMode(int mode) { mode_ = mode; }
    void initialize(int dimension, uint64_t numVertices) {
        dimension_ = dimension;
        numVertices_ = numVertices;
    }

    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    int getMode() const { return mode_; }
    int getDimension() const { return dimension_; }
    uint64_t getNumVertices() const { return numVertices_; }

private:
    std::string name_;
    int width_;
    int height_;
    int mode_ = 1;
    int dimension_ = 3;
    uint64_t numVertices_ = 30000;
};

class AMOURANTH {
public:
    AMOURANTH(DimensionalNavigator* navigator, VkDevice device, VkDeviceMemory vertexBufferMemory,
              VkDeviceMemory indexBufferMemory, VkPipeline pipeline)
        : navigator_(navigator), device_(device), vertexBufferMemory_(vertexBufferMemory),
          indexBufferMemory_(indexBufferMemory), pipeline_(pipeline),
          universalEquation_(9, 1, 1.0L, 0.1L, false, 30000) {
        cache_.reserve(100); // Limit cache size
    }

    // Getters
    glm::mat4 getViewMatrix() const { return isUserCamActive_ ? userCamMatrix_ : glm::mat4(1.0f); }
    glm::mat4 getProjectionMatrix() const {
        return glm::perspective(glm::radians(45.0f), static_cast<float>(width_) / height_, 0.1f, 100.0f);
    }
    const std::vector<uint32_t>& getSphereIndices() const { return sphereIndices_; }
    std::span<const UniversalEquation::DimensionData> getCache() const { return cache_; }
    Logging::Logger& getLogger() const {
        static Logging::Logger logger(Logging::LogLevel::Info, "AMOURANTH");
        return logger;
    }
    const std::vector<glm::vec3>& getBalls() const { return universalEquation_.getProjectedVertices(); }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    int getMode() const { return mode_; }
    int getCurrentDimension() const { return currentDimension_; }
    float getZoomLevel() const { return zoomLevel_; }
    float getInfluence() const { return influence_; }
    float getNurbMatter() const { return nurbMatter_; }
    float getNurbEnergy() const { return nurbEnergy_; }
    bool isPaused() const { return isPaused_; }
    bool isUserCamActive() const { return isUserCamActive_; }
    VkDevice getDevice() const { return device_; }
    VkDeviceMemory getVertexBufferMemory() const { return vertexBufferMemory_; }
    VkDeviceMemory getIndexBufferMemory() const { return indexBufferMemory_; }
    VkPipeline getPipeline() const { return pipeline_; }
    const UniversalEquation& getUniversalEquation() const { return universalEquation_; }

    // Setters
    void setSphereIndices(const std::vector<uint32_t>& indices) { sphereIndices_ = indices; }
    void setWidth(int width) { width_ = width; }
    void setHeight(int height) { height_ = height; }
    void setMode(int mode) { mode_ = mode; universalEquation_.setMode(mode); }
    void setCurrentDimension(int dimension) { currentDimension_ = dimension; universalEquation_.setCurrentDimension(dimension); }
    void setZoomLevel(float zoom) { zoomLevel_ = std::max(0.1f, zoom); }
    void setInfluence(float influence) { influence_ = std::max(0.0f, influence); }
    void setNurbMatter(float matter) { nurbMatter_ = std::max(0.0f, matter); universalEquation_.setNurbMatterStrength(matter); }
    void setNurbEnergy(float energy) { nurbEnergy_ = std::max(0.0f, energy); universalEquation_.setNurbEnergyStrength(energy); }
    void setUserCamMatrix(const glm::mat4& matrix) { userCamMatrix_ = matrix; }
    void updateZoom(bool zoomIn) { zoomLevel_ += zoomIn ? 0.1f : -0.1f; zoomLevel_ = std::max(0.1f, zoomLevel_); }
    void adjustInfluence(float delta) { influence_ += delta; influence_ = std::max(0.0f, influence_); }
    void adjustNurbMatter(float delta) { nurbMatter_ += delta; nurbMatter_ = std::max(0.0f, nurbMatter_); universalEquation_.setNurbMatterStrength(nurbMatter_); }
    void adjustNurbEnergy(float delta) { nurbEnergy_ += delta; nurbEnergy_ = std::max(0.0f, nurbEnergy_); universalEquation_.setNurbEnergyStrength(nurbEnergy_); }
    void togglePause() { isPaused_ = !isPaused_; }
    void toggleUserCam() { isUserCamActive_ = !isUserCamActive_; }
    void moveUserCam(float dx, float dy, float dz) {
        if (isUserCamActive_) {
            userCamMatrix_ = glm::translate(userCamMatrix_, glm::vec3(dx, dy, dz));
        }
    }

    void update(float deltaTime) {
        if (!isPaused_) {
            universalEquation_.evolveTimeStep(deltaTime);
            cache_.clear(); // Prevent unbounded growth
            auto data = universalEquation_.updateCache();
            cache_.push_back(data);
            if (cache_.size() > 100) cache_.erase(cache_.begin());
        }
    }

private:
    DimensionalNavigator* navigator_;
    VkDevice device_;
    VkDeviceMemory vertexBufferMemory_;
    VkDeviceMemory indexBufferMemory_;
    VkPipeline pipeline_;
    UniversalEquation universalEquation_;
    std::vector<uint32_t> sphereIndices_;
    std::vector<UniversalEquation::DimensionData> cache_;
    int width_ = 800;
    int height_ = 600;
    int mode_ = 1;
    int currentDimension_ = 3;
    float zoomLevel_ = 1.0f;
    float influence_ = 1.0f;
    float nurbMatter_ = 1.0f;
    float nurbEnergy_ = 1.0f;
    bool isPaused_ = false;
    bool isUserCamActive_ = false;
    glm::mat4 userCamMatrix_ = glm::mat4(1.0f);
};

class UniversalEquation {
public:
    UniversalEquation(int maxDimensions, int mode, long double influence, long double weak, bool debug, uint64_t numVertices);
    UniversalEquation(int maxDimensions, int mode, long double influence, long double weak, long double collapse,
                     long double twoD, long double threeDInfluence, long double oneDPermeation,
                     long double nurbMatterStrength, long double nurbEnergyStrength, long double alpha,
                     long double beta, long double carrollFactor, long double meanFieldApprox,
                     long double asymCollapse, long double perspectiveTrans, long double perspectiveFocal,
                     long double spinInteraction, long double emFieldStrength, long double renormFactor,
                     long double vacuumEnergy, long double godWaveFreq, bool debug, uint64_t numVertices);
    UniversalEquation(const UniversalEquation& other);
    UniversalEquation& operator=(const UniversalEquation& other);
    ~UniversalEquation();

    // Getters
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
    int getCurrentDimension() const { return currentDimension_.load(); }
    int getMode() const { return mode_.load(); }
    bool getDebug() const { return debug_.load(); }
    bool getNeedsUpdate() const { return needsUpdate_.load(); }
    long double getTotalCharge() const { return totalCharge_.load(); }
    long double getAvgProjScale() const { return avgProjScale_.load(); }
    float getSimulationTime() const { return simulationTime_.load(); }
    long double getMaterialDensity() const { return materialDensity_.load(); }
    uint64_t getCurrentVertices() const { return currentVertices_.load(); }
    uint64_t getMaxVertices() const { return maxVertices_; }
    int getMaxDimensions() const { return maxDimensions_; }
    long double getOmega() const { return omega_; }
    long double getInvMaxDim() const { return invMaxDim_; }
    const std::vector<std::vector<long double>>& getNCubeVertices() const { return nCubeVertices_; }
    const std::vector<std::vector<long double>>& getVertexMomenta() const { return vertexMomenta_; }
    const std::vector<long double>& getVertexSpins() const { return vertexSpins_; }
    const std::vector<long double>& getVertexWaveAmplitudes() const { return vertexWaveAmplitudes_; }
    const std::vector<UniversalEquation::DimensionInteraction>& getInteractions() const { return interactions_; }
    const std::vector<glm::vec3>& getProjectedVertices() const { return projectedVerts_; }
    const std::vector<long double>& getCachedCos() const { return cachedCos_; }
    const std::vector<long double>& getNurbMatterControlPoints() const { return nurbMatterControlPoints_; }
    const std::vector<long double>& getNurbEnergyControlPoints() const { return nurbEnergyControlPoints_; }
    const std::vector<long double>& getNurbKnots() const { return nurbKnots_; }
    const std::vector<long double>& getNurbWeights() const { return nurbWeights_; }
    const std::vector<UniversalEquation::DimensionData>& getDimensionData() const { return dimensionData_; }
    DimensionalNavigator* getNavigator() const { return navigator_; }

    // Setters
    void setCurrentDimension(int dimension);
    void setMode(int mode);
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
    void setGodWaveFreq(long double value);
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

    // Methods
    void initializeNCube();
    void validateProjectedVertices() const;
    long double computeKineticEnergy(int vertexIndex) const;
    void updateInteractions();
    EnergyResult compute();
    void initializeWithRetry();
    void initializeCalculator(AMOURANTH* amouranth);
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
    long double computeGravitationalPotential(int vertexIndex1, int vertexIndex2) const;
    std::vector<long double> computeGravitationalAcceleration(int vertexIndex) const;

private:
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
    uint64_t maxVertices_;
    int maxDimensions_;
    long double omega_;
    long double invMaxDim_;
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

#endif // UE_INIT_HPP