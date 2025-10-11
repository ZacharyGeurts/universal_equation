// ue_init.hpp
// Header file for UniversalEquation and related classes in the AMOURANTH RTX Engine.
// Defines core structures and classes for N-dimensional calculations and simulation logic.
// Copyright Zachary Geurts 2025 (powered by Grok with Science B*! precision)

#pragma once
#ifndef UE_INIT_HPP
#define UE_INIT_HPP

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <vulkan/vulkan.h>
#include "engine/logging.hpp"
#include "VulkanCore.hpp"
#include <atomic>
#include <mutex>
#include <thread>
#include <latch>
#include <numbers>
#include <cmath>
#include <stdexcept>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <format>
#include <source_location>

class VulkanRenderer; // Forward declaration
class AMOURANTH; // Forward declaration

// Namespace for UniversalEquation-related structures
namespace UE {
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
            std::stringstream ss;
            ss << std::fixed << std::setprecision(6);
            ss << "DimensionData{dimension=" << dimension
               << ", scale=" << scale
               << ", position=(" << position.x << "," << position.y << "," << position.z << ")"
               << ", value=" << value
               << ", nurbEnergy=" << nurbEnergy
               << ", nurbMatter=" << nurbMatter
               << ", potential=" << potential
               << ", observable=" << observable
               << ", spinEnergy=" << spinEnergy
               << ", momentumEnergy=" << momentumEnergy
               << ", fieldEnergy=" << fieldEnergy
               << ", GodWaveEnergy=" << GodWaveEnergy << "}";
            return ss.str();
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
            std::stringstream ss;
            ss << std::fixed << std::setprecision(6);
            ss << "EnergyResult{observable=" << observable
               << ", potential=" << potential
               << ", nurbMatter=" << nurbMatter
               << ", nurbEnergy=" << nurbEnergy
               << ", spinEnergy=" << spinEnergy
               << ", momentumEnergy=" << momentumEnergy
               << ", fieldEnergy=" << fieldEnergy
               << ", GodWaveEnergy=" << GodWaveEnergy << "}";
            return ss.str();
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

    struct UniformBufferObject {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
    };
} // namespace UE

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
    int getCurrentDimension() const;
    int getMode() const;
    bool getDebug() const;
    uint64_t getMaxVertices() const;
    int getMaxDimensions() const;
    long double getGodWaveFreq() const;
    long double getInfluence() const;
    long double getWeak() const;
    long double getCollapse() const;
    long double getTwoD() const;
    long double getThreeDInfluence() const;
    long double getOneDPermeation() const;
    long double getNurbMatterStrength() const;
    long double getNurbEnergyStrength() const;
    long double getAlpha() const;
    long double getBeta() const;
    long double getCarrollFactor() const;
    long double getMeanFieldApprox() const;
    long double getAsymCollapse() const;
    long double getPerspectiveTrans() const;
    long double getPerspectiveFocal() const;
    long double getSpinInteraction() const;
    long double getEMFieldStrength() const;
    long double getRenormFactor() const;
    long double getVacuumEnergy() const;
    bool getNeedsUpdate() const;
    long double getTotalCharge() const;
    long double getAvgProjScale() const;
    float getSimulationTime() const;
    long double getMaterialDensity() const;
    uint64_t getCurrentVertices() const;
    long double getOmega() const;
    long double getInvMaxDim() const;
    const std::vector<std::vector<long double>>& getNCubeVertices() const;
    const std::vector<std::vector<long double>>& getVertexMomenta() const;
    const std::vector<long double>& getVertexSpins() const;
    const std::vector<long double>& getVertexWaveAmplitudes() const;
    const std::vector<UE::DimensionInteraction>& getInteractions() const;
    const std::vector<glm::vec3>& getProjectedVerts() const;
    const std::vector<long double>& getCachedCos() const;
    const std::vector<long double>& getNurbMatterControlPoints() const;
    const std::vector<long double>& getNurbEnergyControlPoints() const;
    const std::vector<long double>& getNurbKnots() const;
    const std::vector<long double>& getNurbWeights() const;
    const std::vector<UE::DimensionData>& getDimensionData() const;
    DimensionalNavigator* getNavigator() const;
    const std::vector<long double>& getNCubeVertex(int vertexIndex) const;
    const std::vector<long double>& getVertexMomentum(int vertexIndex) const;
    long double getVertexSpin(int vertexIndex) const;
    long double getVertexWaveAmplitude(int vertexIndex) const;
    const glm::vec3& getProjectedVertex(int vertexIndex) const;

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

    // Core Methods
    void initializeNCube();
    void initializeWithRetry();
    void initializeCalculator(AMOURANTH* amouranth);
    void updateInteractions();
    UE::EnergyResult compute();
    void evolveTimeStep(long double dt);
    void updateMomentum();
    void advanceCycle();
    std::vector<UE::DimensionData> computeBatch(int startDim, int endDim);
    void exportToCSV(const std::string& filename, const std::vector<UE::DimensionData>& data) const;
    UE::DimensionData updateCache();
    long double computeGodWaveAmplitude(int vertexIndex, long double time) const;
    long double computeNurbMatter(int vertexIndex) const;
    long double computeNurbEnergy(int vertexIndex) const;
    long double computeSpinEnergy(int vertexIndex) const;
    long double computeEMField(int vertexIndex) const;
    long double computeGodWave(int vertexIndex) const;
    long double computeInteraction(int vertexIndex, long double distance) const;
    std::vector<long double> computeVectorPotential(int vertexIndex) const;
    long double computeGravitationalPotential(int vertexIndex, int otherIndex) const;
    std::vector<long double> computeGravitationalAcceleration(int vertexIndex) const;
    long double computeKineticEnergy(int vertexIndex) const;

    // Utility Methods
    long double safeExp(long double x) const;
    long double safe_div(long double a, long double b) const;
    void validateVertexIndex(int vertexIndex, const std::source_location& loc = std::source_location::current()) const;
    void validateProjectedVertices() const;

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
    const uint64_t maxVertices_;
    const int maxDimensions_;
    const long double omega_;
    const long double invMaxDim_;
    std::vector<std::vector<long double>> nCubeVertices_;
    std::vector<std::vector<long double>> vertexMomenta_;
    std::vector<long double> vertexSpins_;
    std::vector<long double> vertexWaveAmplitudes_;
    std::vector<UE::DimensionInteraction> interactions_;
    std::vector<glm::vec3> projectedVerts_;
    std::vector<long double> cachedCos_;
    std::vector<long double> nurbMatterControlPoints_;
    std::vector<long double> nurbEnergyControlPoints_;
    std::vector<long double> nurbKnots_;
    std::vector<long double> nurbWeights_;
    std::vector<UE::DimensionData> dimensionData_;
    DimensionalNavigator* navigator_;
};

class AMOURANTH {
public:
    AMOURANTH(DimensionalNavigator* navigator, VkDevice logicalDevice, VkDeviceMemory vertexMemory,
              VkDeviceMemory indexMemory, VkPipeline pipeline)
        : navigator_(navigator),
          logicalDevice_(logicalDevice),
          vertexMemory_(vertexMemory),
          indexMemory_(indexMemory),
          pipeline_(pipeline),
          mode_(1),
          currentDimension_(3),
          nurbMatter_(0.5f),
          nurbEnergy_(1.0f),
          universalEquation_(9, 1, 1.0L, 0.1L, false, 30000),
          position_(glm::vec3(0.0f, 0.0f, -5.0f)),
          target_(glm::vec3(0.0f)),
          up_(glm::vec3(0.0f, 1.0f, 0.0f)),
          fov_(45.0f),
          aspectRatio_(static_cast<float>(navigator->getWidth()) / navigator->getHeight()),
          nearPlane_(0.1f),
          farPlane_(100.0f),
          isPaused_(false),
          isUserCamActive_(false) {
        if (!navigator) {
            LOG_ERROR("AMOURANTH constructor: Null navigator provided", std::source_location::current());
            throw std::runtime_error("AMOURANTH: Null navigator provided");
        }
        universalEquation_.setNavigator(navigator_);
        universalEquation_.initializeCalculator(this);
        LOG_INFO("AMOURANTH initialized with dimension=3, vertices=30000", std::source_location::current());
    }

    ~AMOURANTH() {
        LOG_DEBUG("Destroying AMOURANTH", std::source_location::current());
    }

    const std::vector<glm::vec3>& getBalls() const {
        return universalEquation_.getProjectedVerts();
    }

    int getMode() const { return mode_; }
    int getCurrentDimension() const { return currentDimension_; }
    float getNurbMatter() const { return nurbMatter_; }
    float getNurbEnergy() const { return nurbEnergy_; }
    const UniversalEquation& getUniversalEquation() const { return universalEquation_; }
    bool isPaused() const { return isPaused_; }
    bool isUserCamActive() const { return isUserCamActive_; }

    glm::mat4 getViewMatrix() const {
        return glm::lookAt(position_, target_, up_);
    }

    glm::mat4 getProjectionMatrix() const {
        return glm::perspective(glm::radians(fov_), aspectRatio_, nearPlane_, farPlane_);
    }

    const std::vector<UE::DimensionData>& getCache() const {
        return universalEquation_.getDimensionData();
    }

    Logging::Logger& getLogger() const {
        return Logging::Logger::get();
    }

    void setMode(int mode, const std::source_location& loc = std::source_location::current()) {
        if (mode >= 1 && mode <= 9) {
            mode_ = mode;
            universalEquation_.setMode(mode);
            LOG_DEBUG("AMOURANTH: Set mode to {}", loc, mode);
        } else {
            LOG_WARNING("AMOURANTH: Invalid mode {}, keeping mode {}", loc, mode, mode_);
        }
    }

    void setCurrentDimension(int dimension, const std::source_location& loc = std::source_location::current()) {
        if (dimension >= 1 && dimension <= universalEquation_.getMaxDimensions()) {
            currentDimension_ = dimension;
            universalEquation_.setCurrentDimension(dimension);
            LOG_DEBUG("AMOURANTH: Set dimension to {}", loc, dimension);
        } else {
            LOG_WARNING("AMOURANTH: Invalid dimension {}, keeping dimension {}", loc, dimension, currentDimension_);
        }
    }

    void setNurbMatter(float matter, const std::source_location& loc = std::source_location::current()) {
        nurbMatter_ = std::max(0.0f, matter);
        universalEquation_.setNurbMatterStrength(nurbMatter_);
        LOG_DEBUG("AMOURANTH: Set nurb matter to {:.3f}", loc, nurbMatter_);
    }

    void setNurbEnergy(float energy, const std::source_location& loc = std::source_location::current()) {
        nurbEnergy_ = std::max(0.0f, energy);
        universalEquation_.setNurbEnergyStrength(nurbEnergy_);
        LOG_DEBUG("AMOURANTH: Set nurb energy to {:.3f}", loc, nurbEnergy_);
    }

    void adjustNurbMatter(float delta, const std::source_location& loc = std::source_location::current()) {
        nurbMatter_ = std::max(0.0f, nurbMatter_ + delta);
        universalEquation_.setNurbMatterStrength(nurbMatter_);
        LOG_DEBUG("AMOURANTH: Adjusted nurb matter by {:.3f} to {:.3f}", loc, delta, nurbMatter_);
    }

    void adjustNurbEnergy(float delta, const std::source_location& loc = std::source_location::current()) {
        nurbEnergy_ = std::max(0.0f, nurbEnergy_ + delta);
        universalEquation_.setNurbEnergyStrength(nurbEnergy_);
        LOG_DEBUG("AMOURANTH: Adjusted nurb energy by {:.3f} to {:.3f}", loc, delta, nurbEnergy_);
    }

    void adjustInfluence(float delta, const std::source_location& loc = std::source_location::current()) {
        long double current = universalEquation_.getInfluence();
        long double newInfluence = std::max(0.0L, current + static_cast<long double>(delta));
        universalEquation_.setInfluence(newInfluence);
        LOG_DEBUG("AMOURANTH: Adjusted influence by {:.3f} to {:.3f}", loc, delta, static_cast<float>(newInfluence));
    }

    void updateZoom(bool zoomIn, const std::source_location& loc = std::source_location::current()) {
        const float zoomSpeed = 5.0f;
        fov_ += zoomIn ? -zoomSpeed : zoomSpeed;
        fov_ = std::clamp(fov_, 10.0f, 120.0f);
        LOG_DEBUG("AMOURANTH: {} zoom, fov set to {:.3f}", loc, zoomIn ? "Increased" : "Decreased", fov_);
    }

    void togglePause(const std::source_location& loc = std::source_location::current()) {
        isPaused_ = !isPaused_;
        LOG_DEBUG("AMOURANTH: Simulation {}", loc, isPaused_ ? "paused" : "resumed");
    }

    void toggleUserCam(const std::source_location& loc = std::source_location::current()) {
        isUserCamActive_ = !isUserCamActive_;
        LOG_DEBUG("AMOURANTH: User camera {}", loc, isUserCamActive_ ? "activated" : "deactivated");
    }

    void moveUserCam(float dx, float dy, float dz, const std::source_location& loc = std::source_location::current()) {
        if (!isUserCamActive_) {
            LOG_WARNING("AMOURANTH: Attempted to move user camera while inactive", loc);
            return;
        }

        // Update position based on input
        glm::vec3 forward = glm::normalize(target_ - position_);
        glm::vec3 right = glm::normalize(glm::cross(forward, up_));
        glm::vec3 moveDir = right * dx + up_ * dy + forward * dz;
        position_ += moveDir * 0.1f;

        // Update target to maintain view direction
        target_ = position_ + forward;

        LOG_DEBUG("AMOURANTH: Moved user camera to position {}", loc, position_);
    }

    void update(float deltaTime, const std::source_location& loc = std::source_location::current()) {
        if (!isPaused_) {
            universalEquation_.evolveTimeStep(deltaTime);
            LOG_DEBUG("AMOURANTH: Updated simulation with deltaTime {:.3f}", loc, deltaTime);
        }
        aspectRatio_ = static_cast<float>(navigator_->getWidth()) / navigator_->getHeight();
    }

private:
    DimensionalNavigator* navigator_;
    VkDevice logicalDevice_;
    VkDeviceMemory vertexMemory_;
    VkDeviceMemory indexMemory_;
    VkPipeline pipeline_;
    int mode_;
    int currentDimension_;
    float nurbMatter_;
    float nurbEnergy_;
    UniversalEquation universalEquation_;
    glm::vec3 position_;
    glm::vec3 target_;
    glm::vec3 up_;
    float fov_;
    float aspectRatio_;
    float nearPlane_;
    float farPlane_;
    bool isPaused_;
    bool isUserCamActive_;
};

#endif // UE_INIT_HPP