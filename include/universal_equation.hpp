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

// Forward declaration for Vulkan rendering integration
class DimensionalNavigator;

// UniversalEquation simulates quantum-like interactions in n-dimensional hypercube lattices,
// offering a powerful tool for data scientists to explore complex physical systems.
// Think of it as a computational sandbox for modeling particle interactions in higher dimensions,
// with outputs (energy components, vertex positions) you can analyze in Python, R, or Excel.
// Physics Highlights:
// - Models ultra-relativistic quantum effects (Carroll-Schrödinger limit, see "Carroll–Schrödinger equation as the ultra-relativistic limit of the tachyon equation", Sci Rep 2025).
// - Addresses the quantum measurement problem with a deterministic collapse term (inspired by "A Solution to the Quantum Measurement Problem", MDPI 2025).
// - Simplifies many-body interactions using mean-field approximations (arXiv:2508.00118, "Dynamical mean field theory with quantum computing").
// - Visualizes n-dimensional structures in 3D (Noll 1967).
// Why Data Scientists Will Love It:
// - Generate rich datasets (energies, interactions) across dimensions with computeBatch() and exportToCSV().
// - Tweak 18 parameters (e.g., darkMatterStrength, influence) to test hypotheses or simulate physical scenarios.
// - Thread-safe (mutexes) and parallelized (OpenMP) for fast, reliable computations.
// - Outputs 3D-projected vertices for visualization, bridging data analysis with visual insights.
// Usage: Create with UniversalEquation(maxDimensions, mode, ...), compute energies, export to CSV.
// Example: UniversalEquation eq(5, 3, 1.0); auto data = eq.computeBatch(1, 5); eq.exportToCSV("data.csv", data);
// Updated: Added copy constructor/assignment for computeBatch, enhanced comments for data scientists.
// Updated: Optimized mutex granularity, OpenMP scheduling, memory pooling for high dimensions.
// Zachary Geurts 2025 (enhanced by Grok with love for data exploration)

class UniversalEquation {
public:
    // EnergyResult holds the output of energy computations, perfect for data analysis.
    // Contains four energy components: observable (measurable energy), potential (stored energy),
    // darkMatter (mass-like effects), and darkEnergy (expansion-like effects).
    // Use toString() to print or log results for quick inspection.
    struct EnergyResult {
        double observable;    // Measurable energy, influenced by interactions and collapse
        double potential;     // Stored energy, adjusted by collapse term
        double darkMatter;    // Invisible mass-like effects, mimicking cosmological dark matter
        double darkEnergy;    // Expansive force-like effects, simulating universe expansion
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

    // DimensionInteraction represents interactions between hypercube vertices.
    // Useful for analyzing spatial relationships or network-like structures in n-dimensional space.
    // Access via getInteractions() to study vertex distances and interaction strengths.
    struct DimensionInteraction {
        int vertexIndex;      // Index of the interacting vertex in the hypercube
        double distance;      // Euclidean distance in n-dimensional space (or projected 3D space)
        double strength;      // Strength of the interaction, influenced by parameters like influence_
        DimensionInteraction(int idx, double dist, double str)
            : vertexIndex(idx), distance(dist), strength(str) {}
    };

    // Constructor initializes the simulation with customizable parameters.
    // Parameters control dimensionality, interaction strengths, and physical effects.
    // All parameters are optional with defaults, clamped to safe ranges for stability.
    // Example: UniversalEquation eq(5, 3, 2.0) for a 5D max simulation with stronger interactions.
    // For data scientists: Tweak parameters like darkEnergyStrength or collapse to explore different physical scenarios.
    // Throws std::invalid_argument for invalid inputs (e.g., maxDimensions < 1).
    UniversalEquation(
        int maxDimensions = 11,           // Max dimensions (1-20), controls hypercube size
        int mode = 3,                     // Starting dimension/mode (1-maxDimensions)
        double influence = 1.0,           // Overall interaction strength (0-10)
        double weak = 0.01,               // Weak interaction modifier for high dimensions (0-1)
        double collapse = 5.0,            // Strength of measurement collapse term (0-5)
        double twoD = 0.0,                // 2D interaction strength (0-5)
        double threeDInfluence = 5.0,     // 3D interaction strength (0-5)
        double oneDPermeation = 0.0,      // 1D interaction strength (0-5)
        double darkMatterStrength = 0.27, // Dark matter effect strength (0-1)
        double darkEnergyStrength = 0.68, // Dark energy effect strength (0-2)
        double alpha = 0.1,               // Exponential decay factor for interactions (0.1-10)
        double beta = 0.5,                // Vertex magnitude scaling factor (0-1)
        double carrollFactor = 0.0,       // Relativistic adjustment (0-1, see Carroll-Schrödinger)
        double meanFieldApprox = 0.5,     // Mean-field approximation strength (0-1)
        double asymCollapse = 0.5,        // Asymmetric collapse term (0-1, for measurement problem)
        double perspectiveTrans = 2.0,    // Translation for 3D projection (0-10)
        double perspectiveFocal = 4.0,    // Focal length for 3D projection (1-20)
        bool debug = false                // Enable verbose logging for debugging
    );

    // Copy constructor for thread-local copies in computeBatch.
    UniversalEquation(const UniversalEquation& other);

    // Copy assignment operator for thread-local copies in computeBatch.
    UniversalEquation& operator=(const UniversalEquation& other);

    // Parameter setters (thread-safe with std::atomic, clamped to safe ranges).
    // Use these to dynamically adjust the simulation during runtime.
    // Example: setDarkEnergyStrength(1.0) to amplify expansion effects.
    // Most setters mark needsUpdate_ = true to trigger recomputation of internal state.
    void setInfluence(double value) { influence_ = std::clamp(value, 0.0, 10.0); needsUpdate_ = true; } // Set interaction strength
    void setWeak(double value) { weak_ = std::clamp(value, 0.0, 1.0); needsUpdate_ = true; } // Set weak interaction modifier
    void setCollapse(double value) { collapse_ = std::clamp(value, 0.0, 5.0); needsUpdate_ = true; } // Set collapse term strength
    void setTwoD(double value) { twoD_ = std::clamp(value, 0.0, 5.0); needsUpdate_ = true; } // Set 2D interaction strength
    void setThreeDInfluence(double value) { threeDInfluence_ = std::clamp(value, 0.0, 5.0); needsUpdate_ = true; } // Set 3D interaction strength
    void setOneDPermeation(double value) { oneDPermeation_ = std::clamp(value, 0.0, 5.0); needsUpdate_ = true; } // Set 1D interaction strength
    void setDarkMatterStrength(double value) { darkMatterStrength_ = std::clamp(value, 0.0, 1.0); needsUpdate_ = true; } // Set dark matter effect
    void setDarkEnergyStrength(double value) { darkEnergyStrength_ = std::clamp(value, 0.0, 2.0); needsUpdate_ = true; } // Set dark energy effect
    void setAlpha(double value) { alpha_ = std::clamp(value, 0.1, 10.0); needsUpdate_ = true; } // Set exponential decay factor
    void setBeta(double value) { beta_ = std::clamp(value, 0.0, 1.0); needsUpdate_ = true; } // Set vertex magnitude scaling
    void setCarrollFactor(double value) { carrollFactor_ = std::clamp(value, 0.0, 1.0); needsUpdate_ = true; } // Set relativistic adjustment
    void setMeanFieldApprox(double value) { meanFieldApprox_ = std::clamp(value, 0.0, 1.0); needsUpdate_ = true; } // Set mean-field strength
    void setAsymCollapse(double value) { asymCollapse_ = std::clamp(value, 0.0, 1.0); needsUpdate_ = true; } // Set asymmetric collapse term
    void setPerspectiveTrans(double value) { perspectiveTrans_ = std::clamp(value, 0.0, 10.0); needsUpdate_ = true; } // Set projection translation
    void setPerspectiveFocal(double value) { perspectiveFocal_ = std::clamp(value, 1.0, 20.0); needsUpdate_ = true; } // Set projection focal length
    void setDebug(bool value) { debug_ = value; } // Enable/disable debug logging
    void setMode(int mode) { mode_ = std::clamp(mode, 1, maxDimensions_); needsUpdate_ = true; } // Set simulation mode
    void setCurrentDimension(int dimension) { currentDimension_ = std::clamp(dimension, 1, maxDimensions_); needsUpdate_ = true; } // Set current dimension

    // Parameter getters (thread-safe with std::atomic).
    // Use to query current parameter values for logging or analysis.
    // Example: double strength = getDarkEnergyStrength(); to check current dark energy effect.
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

    // Additional getters for internal simulation state (thread-safe).
    // Use these to access computed data for analysis or visualization.
    // Example: auto vertices = getProjectedVertices(); for 3D visualization.
    double getOmega() const { return omega_; } // Angular frequency for oscillations
    double getInvMaxDim() const { return invMaxDim_; } // Inverse of max dimensions for scaling
    uint64_t getMaxVertices() const { return maxVertices_; } // Max number of hypercube vertices
    size_t getCachedCosSize() const { std::lock_guard<std::mutex> lock(mutex_); return cachedCos_.size(); } // Size of cached cosine values
    const std::vector<DimensionInteraction>& getInteractions() const { // Interaction data for vertices
        std::lock_guard<std::mutex> lock(mutex_);
        if (needsUpdate_) updateInteractions();
        return interactions_;
    }
    const std::vector<std::vector<double>>& getNCubeVertices() const { // Raw n-dimensional vertex coordinates
        std::lock_guard<std::mutex> lock(mutex_);
        return nCubeVertices_;
    }
    const std::vector<double>& getCachedCos() const { // Cached cosine values for computations
        std::lock_guard<std::mutex> lock(mutex_);
        return cachedCos_;
    }
    std::vector<glm::vec3> getProjectedVertices() const { // 3D-projected vertices for visualization
        std::lock_guard<std::mutex> lock(projMutex_);
        return projectedVerts_;
    }
    double getAvgProjScale() const { // Average projection scale for visualization
        std::lock_guard<std::mutex> lock(projMutex_);
        return avgProjScale_;
    }

    // Advances the simulation to the next dimension, cycling from maxDimensions_ back to 1.
    // Use in iterative simulations to explore energy changes across dimensions.
    // Example: equation.advanceCycle(); equation.compute(); to analyze the next dimension.
    void advanceCycle();

    // Computes energy components for the current dimension, returning an EnergyResult.
    // Incorporates relativistic effects, collapse terms, and cosmological influences.
    // Use for single-point analysis or to feed data to Vulkan shaders for visualization.
    // Example: auto result = equation.compute(); to get energy values.
    EnergyResult compute() const;

    // Initializes the simulation with a DimensionalNavigator for Vulkan rendering.
    // Required for visualization; skip if only analyzing data.
    // Throws std::invalid_argument if navigator is null.
    // Example: equation.initializeCalculator(navigator); for rendering integration.
    void initializeCalculator(DimensionalNavigator* navigator);

    // Cached simulation data, mirroring EnergyResult with dimension info.
    // Use to store and analyze results from updateCache() or computeBatch().
    struct DimensionData {
        int dimension;         // Dimension of the computation
        double observable;    // Measurable energy
        double potential;     // Stored energy
        double darkMatter;    // Invisible mass-like effects
        double darkEnergy;    // Expansive force-like effects
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

    // Updates and returns cached simulation data.
    // Use for Vulkan rendering (updates uniforms/buffers) or single-dimension analysis.
    // Example: auto data = equation.updateCache(); to get current state.
    DimensionData updateCache();

    // Computes batch of dimension data from startDim to endDim (default: 1 to maxDimensions_).
    // Parallelized with OpenMP for fast processing, ideal for generating large datasets.
    // Example: auto data = equation.computeBatch(1, 5); to analyze dimensions 1-5.
    std::vector<DimensionData> computeBatch(int startDim = 1, int endDim = -1);

    // Exports batch data to a CSV file for analysis in Python, R, or Excel.
    // Columns: Dimension, Observable, Potential, DarkMatter, DarkEnergy.
    // Example: equation.exportToCSV("results.csv", data); to save results.
    void exportToCSV(const std::string& filename, const std::vector<DimensionData>& data) const;

    // Computes interaction strength for a vertex at a given distance.
    // Used internally; access via getInteractions() for analysis.
    double computeInteraction(int vertexIndex, double distance) const;

    // Computes permeation factor for a vertex, adjusting interaction strength.
    // Used internally; influences energy calculations.
    double computePermeation(int vertexIndex) const;

    // Computes dark energy contribution for a given distance.
    // Used internally; models expansive forces in the simulation.
    double computeDarkEnergy(double distance) const;

private:
    // Simulation state variables
    int maxDimensions_;                            // Maximum number of dimensions (1-20)
    std::atomic<int> currentDimension_;            // Current dimension being simulated
    std::atomic<int> mode_;                        // Current simulation mode
    uint64_t maxVertices_;                         // Maximum number of hypercube vertices (2^maxDimensions_)
    std::atomic<double> influence_;                // Interaction strength
    std::atomic<double> weak_;                     // Weak interaction modifier
    std::atomic<double> collapse_;                 // Collapse term strength
    std::atomic<double> twoD_;                     // 2D interaction strength
    std::atomic<double> threeDInfluence_;          // 3D interaction strength
    std::atomic<double> oneDPermeation_;           // 1D interaction strength
    std::atomic<double> darkMatterStrength_;       // Dark matter effect strength
    std::atomic<double> darkEnergyStrength_;       // Dark energy effect strength
    std::atomic<double> alpha_;                    // Exponential decay factor
    std::atomic<double> beta_;                     // Vertex magnitude scaling factor
    std::atomic<double> carrollFactor_;            // Relativistic adjustment factor
    std::atomic<double> meanFieldApprox_;          // Mean-field approximation strength
    std::atomic<double> asymCollapse_;             // Asymmetric collapse term
    std::atomic<double> perspectiveTrans_;         // Translation for 3D projection
    std::atomic<double> perspectiveFocal_;         // Focal length for 3D projection
    std::atomic<bool> debug_;                      // Debug logging flag
    double omega_;                                 // Angular frequency for oscillations
    double invMaxDim_;                             // Inverse of max dimensions for scaling
    mutable std::vector<DimensionInteraction> interactions_; // Vertex interaction data
    std::vector<std::vector<double>> nCubeVertices_; // Raw n-dimensional vertex coordinates
    mutable std::vector<glm::vec3> projectedVerts_; // 3D-projected vertex coordinates
    mutable double avgProjScale_;                  // Average projection scale
    mutable std::mutex projMutex_;                 // Mutex for projectedVerts_ and avgProjScale_
    mutable std::atomic<bool> needsUpdate_;        // Flag to trigger state recomputation
    std::vector<double> cachedCos_;                // Cached cosine values for performance
    DimensionalNavigator* navigator_;               // Pointer to Vulkan rendering component
    mutable std::mutex mutex_;                     // Mutex for interactions_, nCubeVertices_, cachedCos_
    mutable std::mutex debugMutex_;                // Mutex for thread-safe debug logging

    // Computes collapse factor with deterministic asymmetric term.
    // Used internally to model quantum measurement effects.
    double computeCollapse() const;

    // Initializes n-cube vertices with memory pooling for efficiency.
    // Called during initialization and dimension changes.
    void initializeNCube();

    // Updates interaction data with perspective projection and LOD for high dimensions.
    // Thread-safe with mutexes, optimized with OpenMP.
    void updateInteractions() const;

    // Initializes with retry logic for memory errors.
    // Handles high-dimensional vertex allocation gracefully.
    void initializeWithRetry();

    // Safe exponential function to prevent overflow/underflow.
    // Used in energy and interaction calculations.
    double safeExp(double x) const;
};

#endif // UNIVERSAL_EQUATION_HPP