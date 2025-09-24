// ue engine
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

// Forward declaration to avoid circular dependency
class DimensionalNavigator;

// Inspired by the time-dependent Schrödinger equation, the quiet architect of quantum reality:
// i ħ ∂/∂t Ψ = Ĥ Ψ
// This class simulates emergent behaviors in n-dimensional hypercube lattices, permeating probabilities through tunable parameters.

class UniversalEquation {
public:
    // Structure to hold energy computation results
    struct EnergyResult {
        double observable;    // Observable energy component
        double potential;     // Potential energy component
        double darkMatter;    // Dark matter contribution
        double darkEnergy;    // Dark energy contribution
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

    // Structure to represent interactions between vertices
    struct DimensionInteraction {
        int vertexIndex;      // Index of the vertex
        double distance;      // Distance to reference vertex
        double strength;      // Interaction strength
        DimensionInteraction(int idx, double dist, double str)
            : vertexIndex(idx), distance(dist), strength(str) {}
    };

    // Constructor with default parameters
    UniversalEquation(int maxDimensions = 9, int mode = 1, double influence = 0.05,
                     double weak = 0.01, double collapse = 0.1, double twoD = 0.0,
                     double threeDInfluence = 0.1, double oneDPermeation = 0.1,
                     double darkMatterStrength = 0.27, double darkEnergyStrength = 0.68,
                     double alpha = 2.0, double beta = 0.2, bool debug = false);

    // Setters and Getters (thread-safe)
    void setInfluence(double value);
    double getInfluence() const;
    void setWeak(double value);
    double getWeak() const;
    void setCollapse(double value);
    double getCollapse() const;
    void setTwoD(double value);
    double getTwoD() const;
    void setThreeDInfluence(double value);
    double getThreeDInfluence() const;
    void setOneDPermeation(double value);
    double getOneDPermeation() const;
    void setDarkMatterStrength(double value);
    double getDarkMatterStrength() const;
    void setDarkEnergyStrength(double value);
    double getDarkEnergyStrength() const;
    void setAlpha(double value);
    double getAlpha() const;
    void setBeta(double value);
    double getBeta() const;
    void setDebug(bool value);
    bool getDebug() const;
    void setMode(int mode);
    int getMode() const;
    void setCurrentDimension(int dimension);
    int getCurrentDimension() const;
    int getMaxDimensions() const;

    // Get interactions (thread-safe)
    const std::vector<DimensionInteraction>& getInteractions() const;

    // Advance dimension cycle
    void advanceCycle();

    // Compute energy components
    EnergyResult compute() const;

    // Initialize calculator with navigator
    void initializeCalculator(DimensionalNavigator* navigator);

    // Update cache and return DimensionData
    struct DimensionData {
        int dimension;
        double observable, potential, darkMatter, darkEnergy;
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
    DimensionData updateCache();

    // Public methods for AMOURANTH access
    double computeInteraction(int vertexIndex, double distance) const;
    double computePermeation(int vertexIndex) const;
    double computeDarkEnergy(double distance) const;

private:
    int maxDimensions_;          // Maximum number of dimensions (1 to 20)
    std::atomic<int> currentDimension_; // Current dimension for computation
    std::atomic<int> mode_;      // Current mode (aligned with currentDimension_)
    uint64_t maxVertices_;       // Maximum number of vertices (2^20)
    std::atomic<double> influence_; // Influence factor (0 to 10)
    std::atomic<double> weak_;   // Weak interaction strength (0 to 1)
    std::atomic<double> collapse_; // Collapse factor (0 to 5)
    std::atomic<double> twoD_;   // 2D influence factor (0 to 5)
    std::atomic<double> threeDInfluence_; // 3D influence factor (0 to 5)
    std::atomic<double> oneDPermeation_; // 1D permeation factor (0 to 5)
    std::atomic<double> darkMatterStrength_; // Dark matter strength (0 to 1)
    std::atomic<double> darkEnergyStrength_; // Dark energy strength (0 to 2)
    std::atomic<double> alpha_;  // Exponential decay factor (0.1 to 10)
    std::atomic<double> beta_;   // Permeation scaling factor (0 to 1)
    std::atomic<bool> debug_;    // Debug output flag
    double omega_;               // Angular frequency for 2D oscillation
    double invMaxDim_;           // Inverse of max dimensions for scaling
    mutable std::vector<DimensionInteraction> interactions_; // Interaction data
    std::vector<std::vector<double>> nCubeVertices_; // Vertex coordinates for n-cube
    mutable std::atomic<bool> needsUpdate_; // Dirty flag for interactions
    std::vector<double> cachedCos_; // Cached cosine values
    DimensionalNavigator* navigator_; // Pointer to DimensionalNavigator (assumed managed externally)
    mutable std::mutex mutex_; // Protects interactions_, nCubeVertices_, cachedCos_
    mutable std::mutex debugMutex_; // Protects debug output

    // Private methods
    double computeCollapse() const;
    void initializeNCube();
    void updateInteractions() const;
    void initializeWithRetry();
    double safeExp(double x) const;
};

#endif // UNIVERSAL_EQUATION_HPP