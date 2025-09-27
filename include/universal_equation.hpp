#ifndef UNIVERSAL_EQUATION_HPP
#define UNIVERSAL_EQUATION_HPP

#include <vector>
#include <string>
#include <cstdint>
#include <cmath>
#include <limits>
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <mutex>
#include <atomic>
#include <omp.h>
#include <sstream>
#include <iomanip>
#include <glm/glm.hpp>

// Forward declaration for Vulkan rendering
class DimensionalNavigator;

// Simulates quantum-like interactions in n-dimensional hypercube lattices.
// Addresses Schrödinger cracks: Carroll relativistic limit, asymmetric collapse, mean-field approximation.
// Thread-safe with atomics and mutexes.
// Vulkan: Use with DimensionalNavigator for rendering energy distributions/vertices.
// Usage: Create with UniversalEquation(4, 3, 1.0, ...), initialize with DimensionalNavigator, compute energies.
// Zachary Geurts 2025

class UniversalEquation {
public:
    // Energy computation results
    struct EnergyResult {
        double observable;    // Measurable energy
        double potential;     // Stored energy
        double darkMatter;    // Invisible mass-like effects
        double darkEnergy;    // Expansive force-like effects
        std::string toString() const {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(6)
                << "Observable: " << observable
                << ", Potential: " << potential
                << ", Dark Matter: " << darkMatter
                << ", Dark Energy: " << darkEnergy;
            return oss.str();
        }
    };

    // Vertex interaction data
    struct DimensionInteraction {
        int vertexIndex;      // Interacting vertex index
        double distance;      // Euclidean distance in n-space
        double strength;      // Interaction strength
        DimensionInteraction(int idx, double dist, double str)
            : vertexIndex(idx), distance(dist), strength(str) {}
    };

    // Constructor with simulation parameters
    // Parameters: maxDimensions (1-20), mode (1-maxDimensions), influence (0-10), weak (0-1),
    // collapse (0-5), twoD (0-5), threeDInfluence (0-5), oneDPermeation (0-5), darkMatterStrength (0-1),
    // darkEnergyStrength (0-2), alpha (0.1-10), beta (0-1), carrollFactor (0-1), meanFieldApprox (0-1),
    // asymCollapse (0-1), perspectiveTrans (0-10), perspectiveFocal (1-20), debug (bool)
    // Throws: std::invalid_argument on invalid parameters
    UniversalEquation(
        int maxDimensions = 11,
        int mode = 3,
        double influence = 1.0,
        double weak = 0.01,
        double collapse = 5.0,
        double twoD = 0.0,
        double threeDInfluence = 5.0,
        double oneDPermeation = 0.0,
        double darkMatterStrength = 0.27,
        double darkEnergyStrength = 0.68,
        double alpha = 0.1,
        double beta = 0.5,
        double carrollFactor = 0.0,
        double meanFieldApprox = 0.5,
        double asymCollapse = 0.5,
        double perspectiveTrans = 2.0,
        double perspectiveFocal = 4.0,
        bool debug = false
    );

    // Parameter setters (thread-safe, clamped)
    void setInfluence(double value) { influence_ = std::clamp(value, 0.0, 10.0); needsUpdate_ = true; }
    void setWeak(double value) { weak_ = std::clamp(value, 0.0, 1.0); needsUpdate_ = true; }
    void setCollapse(double value) { collapse_ = std::clamp(value, 0.0, 5.0); needsUpdate_ = true; }
    void setTwoD(double value) { twoD_ = std::clamp(value, 0.0, 5.0); needsUpdate_ = true; }
    void setThreeDInfluence(double value) { threeDInfluence_ = std::clamp(value, 0.0, 5.0); needsUpdate_ = true; }
    void setOneDPermeation(double value) { oneDPermeation_ = std::clamp(value, 0.0, 5.0); needsUpdate_ = true; }
    void setDarkMatterStrength(double value) { darkMatterStrength_ = std::clamp(value, 0.0, 1.0); needsUpdate_ = true; }
    void setDarkEnergyStrength(double value) { darkEnergyStrength_ = std::clamp(value, 0.0, 2.0); needsUpdate_ = true; }
    void setAlpha(double value) { alpha_ = std::clamp(value, 0.1, 10.0); needsUpdate_ = true; }
    void setBeta(double value) { beta_ = std::clamp(value, 0.0, 1.0); needsUpdate_ = true; }
    void setCarrollFactor(double value) { carrollFactor_ = std::clamp(value, 0.0, 1.0); needsUpdate_ = true; }
    void setMeanFieldApprox(double value) { meanFieldApprox_ = std::clamp(value, 0.0, 1.0); needsUpdate_ = true; }
    void setAsymCollapse(double value) { asymCollapse_ = std::clamp(value, 0.0, 1.0); needsUpdate_ = true; }
    void setPerspectiveTrans(double value) { perspectiveTrans_ = std::clamp(value, 0.0, 10.0); needsUpdate_ = true; }
    void setPerspectiveFocal(double value) { perspectiveFocal_ = std::clamp(value, 1.0, 20.0); needsUpdate_ = true; }
    void setDebug(bool value) { debug_ = value; }
    void setMode(int mode) { mode_ = std::clamp(mode, 1, maxDimensions_); needsUpdate_ = true; }
    void setCurrentDimension(int dimension) { currentDimension_ = std::clamp(dimension, 1, maxDimensions_); needsUpdate_ = true; }

    // Parameter getters (thread-safe)
    double getInfluence() const { return influence_.load(); }
    double getWeak() const { return weak_.load(); }
    double getCollapse() const { return collapse_.load(); }
    double getTwoD() const { return twoD_.load(); }
    double getThreeDInfluence() const { return threeDInfluence_.load(); }
    double getOneDPermeation() const { return oneDPermeation_.load(); }
    double getDarkMatterStrength() const { return darkMatterStrength_.load(); }
    double getDarkEnergyStrength() const { return darkEnergyStrength_.load(); }
    double getAlpha() const { return alpha_.load(); }
    double getBeta() const { return beta_.load(); }
    double getCarrollFactor() const { return carrollFactor_.load(); }
    double getMeanFieldApprox() const { return meanFieldApprox_.load(); }
    double getAsymCollapse() const { return asymCollapse_.load(); }
    double getPerspectiveTrans() const { return perspectiveTrans_.load(); }
    double getPerspectiveFocal() const { return perspectiveFocal_.load(); }
    bool getDebug() const { return debug_.load(); }
    int getMode() const { return mode_.load(); }
    int getCurrentDimension() const { return currentDimension_.load(); }
    int getMaxDimensions() const { return maxDimensions_; }

    // Additional getters for internal state
    double getOmega() const { return omega_; }
    double getInvMaxDim() const { return invMaxDim_; }
    uint64_t getMaxVertices() const { return maxVertices_; }
    size_t getCachedCosSize() const { std::lock_guard<std::mutex> lock(mutex_); return cachedCos_.size(); }
    const std::vector<DimensionInteraction>& getInteractions() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (needsUpdate_) updateInteractions();
        return interactions_;
    }
    const std::vector<std::vector<double>>& getNCubeVertices() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return nCubeVertices_;
    }
    const std::vector<double>& getCachedCos() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return cachedCos_;
    }
    std::vector<glm::vec3> getProjectedVertices() const {
        std::lock_guard<std::mutex> lock(projMutex_);
        return projectedVerts_;
    }
    double getAvgProjScale() const {
        std::lock_guard<std::mutex> lock(projMutex_);
        return avgProjScale_;
    }

    // Advances simulation to next dimension
    void advanceCycle();

    // Computes energy components
    // Vulkan: Pass to shaders as uniforms
    EnergyResult compute() const;

    // Initializes with DimensionalNavigator
    // Throws: std::invalid_argument if navigator is null
    void initializeCalculator(DimensionalNavigator* navigator);

    // Cached simulation data
    struct DimensionData {
        int dimension;
        double observable;
        double potential;
        double darkMatter;
        double darkEnergy;
        std::string toString() const {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(6)
                << "Dimension: " << dimension
                << ", Observable: " << observable
                << ", Potential: " << potential
                << ", Dark Matter: " << darkMatter
                << ", Dark Energy: " << darkEnergy;
            return oss.str();
        }
    };

    // Updates and returns cached data
    // Vulkan: Use for updating uniforms/buffers
    DimensionData updateCache();

    // Computes interaction strength
    double computeInteraction(int vertexIndex, double distance) const;

    // Computes permeation factor
    double computePermeation(int vertexIndex) const;

    // Computes dark energy contribution
    double computeDarkEnergy(double distance) const;

private:
    // Simulation state
    int maxDimensions_;
    std::atomic<int> currentDimension_;
    std::atomic<int> mode_;
    uint64_t maxVertices_;
    std::atomic<double> influence_;
    std::atomic<double> weak_;
    std::atomic<double> collapse_;
    std::atomic<double> twoD_;
    std::atomic<double> threeDInfluence_;
    std::atomic<double> oneDPermeation_;
    std::atomic<double> darkMatterStrength_;
    std::atomic<double> darkEnergyStrength_;
    std::atomic<double> alpha_;
    std::atomic<double> beta_;
    std::atomic<double> carrollFactor_;
    std::atomic<double> meanFieldApprox_;
    std::atomic<double> asymCollapse_;
    std::atomic<double> perspectiveTrans_;
    std::atomic<double> perspectiveFocal_;
    std::atomic<bool> debug_;
    double omega_;
    double invMaxDim_;
    mutable std::vector<DimensionInteraction> interactions_;
    std::vector<std::vector<double>> nCubeVertices_;
    mutable std::vector<glm::vec3> projectedVerts_;
    mutable double avgProjScale_;
    mutable std::mutex projMutex_;
    mutable std::atomic<bool> needsUpdate_;
    std::vector<double> cachedCos_;
    DimensionalNavigator* navigator_;
    mutable std::mutex mutex_;
    mutable std::mutex debugMutex_;

    // Computes collapse factor
    double computeCollapse() const;

    // Initializes n-cube vertices
    void initializeNCube();

    // Updates interaction data
    void updateInteractions() const;

    // Initializes with retry logic
    void initializeWithRetry();

    // Safe exponential function
    double safeExp(double x) const;
};

#endif // UNIVERSAL_EQUATION_HPP