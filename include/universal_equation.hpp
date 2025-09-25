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
// The simulation models quantum-like interactions in higher dimensions, with parameters for influence, weak interactions, collapse, and dark components.
// Thread-safety is ensured through atomic variables and mutexes for shared data access.
// Designed for data scientists: Extensive getters for all parameters and results, with debug logging for traceability.

class UniversalEquation {
public:
    // Structure to hold energy computation results
    struct EnergyResult {
        double observable;    // Observable energy component (directly measurable phenomena)
        double potential;     // Potential energy component (stored energy in the system)
        double darkMatter;    // Dark matter contribution (invisible mass-like effects)
        double darkEnergy;    // Dark energy contribution (expansive force-like effects)
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

    // Structure to represent interactions between vertices in the hypercube lattice
    struct DimensionInteraction {
        int vertexIndex;      // Index of the interacting vertex (relative to reference)
        double distance;      // Euclidean distance to the reference vertex in n-space
        double strength;      // Computed interaction strength (influenced by parameters)
        DimensionInteraction(int idx, double dist, double str)
            : vertexIndex(idx), distance(dist), strength(str) {}
    };

    // Constructor with default parameters for easy instantiation
    // Parameters are clamped to reasonable ranges to prevent invalid states
    UniversalEquation(int maxDimensions = 9, int mode = 1, double influence = 0.05,
                      double weak = 0.01, double collapse = 0.1, double twoD = 0.0,
                      double threeDInfluence = 0.1, double oneDPermeation = 0.1,
                      double darkMatterStrength = 0.27, double darkEnergyStrength = 0.68,
                      double alpha = 2.0, double beta = 0.2, bool debug = false);

    // Setters and Getters for all parameters (thread-safe via atomics)
    // Setters clamp values to valid ranges and mark for update if necessary
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

    // Additional getters for internal state (useful for data analysis)
    double getOmega() const;  // Get angular frequency for 2D oscillation
    double getInvMaxDim() const;  // Get inverse of max dimensions for scaling
    const std::vector<DimensionInteraction>& getInteractions() const;  // Thread-safe access to interactions
    const std::vector<std::vector<double>>& getNCubeVertices() const;  // Get hypercube vertices
    const std::vector<double>& getCachedCos() const;  // Get precomputed cosine values

    // Advance dimension cycle (cycles through dimensions)
    void advanceCycle();

    // Compute energy components (core simulation method)
    EnergyResult compute() const;

    // Initialize calculator with navigator (for integration with rendering)
    void initializeCalculator(DimensionalNavigator* navigator);

    // Update cache and return DimensionData (caches simulation results)
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

    // Public methods for AMOURANTH access (rendering integration)
    double computeInteraction(int vertexIndex, double distance) const;
    double computePermeation(int vertexIndex) const;
    double computeDarkEnergy(double distance) const;

private:
    // Private members with atomics for thread-safety
    int maxDimensions_;          // Maximum number of dimensions (1 to 20, non-atomic as constant)
    std::atomic<int> currentDimension_; // Current dimension for computation
    std::atomic<int> mode_;      // Current mode (aligned with currentDimension_)
    uint64_t maxVertices_;       // Maximum number of vertices (2^20, non-atomic as constant)
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
    double omega_;               // Angular frequency for 2D oscillation (non-atomic, constant after init)
    double invMaxDim_;           // Inverse of max dimensions for scaling (non-atomic, constant after init)
    mutable std::vector<DimensionInteraction> interactions_; // Interaction data (protected by mutex_)
    std::vector<std::vector<double>> nCubeVertices_; // Vertex coordinates for n-cube (protected by mutex_)
    mutable std::atomic<bool> needsUpdate_; // Dirty flag for interactions
    std::vector<double> cachedCos_; // Cached cosine values (protected by mutex_)
    DimensionalNavigator* navigator_; // Pointer to DimensionalNavigator (assumed managed externally, non-atomic)
    mutable std::mutex mutex_; // Protects interactions_, nCubeVertices_, cachedCos_
    mutable std::mutex debugMutex_; // Protects debug output for thread-safe logging

    // Private methods with improved comments
    // Computes the collapse factor based on dimension and parameters
    double computeCollapse() const;

    // Initializes the n-cube vertices for the current dimension (parallelized for large dims)
    void initializeNCube();

    // Updates interactions between vertices (parallelized for large vertex counts)
    void updateInteractions() const;

    // Initializes with retry logic for memory errors, reducing dimension if needed
    void initializeWithRetry();

    // Safe exponential function with clamping to avoid overflow/underflow
    double safeExp(double x) const;
};

#endif // UNIVERSAL_EQUATION_HPP