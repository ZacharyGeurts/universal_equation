// universal_equation.hpp
// 2025 Zachary Geurts (enhanced by Grok for foundational cracks)
// Header file for the UniversalEquation class, which simulates quantum-like interactions
// in n-dimensional hypercube lattices, modeling emergent behaviors with tunable parameters.
// Purpose: Provides a framework for physics simulations, data analysis, and visualization
// of high-dimensional systems, incorporating concepts like dark matter, dark energy, and
// dimensional collapse. Now addresses Schrödinger cracks: Carroll rel limit, asym collapse,
// mean-field many-body approx. Designed for thread-safety with atomic variables and mutexes.
// Vulkan Integration: Interfaces with Vulkan via DimensionalNavigator for rendering
// simulation results (e.g., energy distributions, vertex interactions). Ensure proper
// Vulkan pipeline setup to avoid errors like VUID-VkGraphicsPipelineCreateInfo-layout-07988
// and VUID-RuntimeSpirv-OpEntryPoint-08743 by matching shader interfaces and descriptor layouts.
// Usage: Include in projects requiring simulation of multidimensional physical systems.
// Example: Create an instance with UniversalEquation(4, 3, 1.0, ..., 0.0, 0.5, 0.5), initialize with
// DimensionalNavigator, and compute energies for rendering.

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
#include <glm/glm.hpp>  // New: For projected vertices (vec3)
#include <random>  // New: For jitter

// Forward declaration of DimensionalNavigator to avoid circular dependency
// Purpose: Allows integration with Vulkan-based rendering for visualizing simulation results
// Note: Ensure DimensionalNavigator sets up Vulkan pipeline correctly, including descriptor
// set layouts and shader interfaces, to avoid validation errors
class DimensionalNavigator;

// Inspired by the time-dependent Schrödinger equation: i ħ ∂/∂t Ψ = Ĥ Ψ
// This class simulates emergent behaviors in n-dimensional hypercube lattices,
// permeating probabilities through tunable parameters like influence, weak interactions,
// collapse, and dark components. Now enhanced for cracks: carrollFactor for rel, asymCollapse
// for measurement, mean-field many-body approx. Designed for thread-safety with atomic variables and mutexes.
// Designed for data scientists: Provides extensive getters for parameters and results,
// with debug logging for traceability.
// Vulkan Usage: Use with DimensionalNavigator to render simulation outputs (e.g., energy
// distributions, vertex interactions) in a Vulkan pipeline. Ensure fragment shader
// descriptors (e.g., envMap) are declared in the pipeline layout and vertex/fragment
// shader interfaces match to avoid validation errors (e.g., VUID-RuntimeSpirv-OpEntryPoint-08743).

class UniversalEquation {
public:
    // Structure to hold energy computation results
    // Purpose: Encapsulates observable, potential, dark matter, and dark energy values
    // Usage: Returned by compute() to provide simulation results for analysis or rendering
    // Vulkan Note: EnergyResult data can be passed to shaders for visualization (e.g., as uniforms)
    struct EnergyResult {
        double observable;    // Observable energy component (directly measurable phenomena)
        double potential;     // Potential energy component (stored energy in the system)
        double darkMatter;    // Dark matter contribution (invisible mass-like effects)
        double darkEnergy;    // Dark energy contribution (expansive force-like effects)
        // Converts results to a formatted string for logging or display
        // Usage: Call toString() to get a readable summary, e.g., result.toString()
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
    // Purpose: Stores data for each vertex interaction (index, distance, strength)
    // Usage: Accessed via getInteractions() for analysis or rendering of lattice interactions
    // Vulkan Note: Can be used to populate vertex buffers or compute shaders for visualizing
    // lattice interactions
    struct DimensionInteraction {
        int vertexIndex;      // Index of the interacting vertex (relative to reference)
        double distance;      // Euclidean distance to the reference vertex in n-space
        double strength;      // Computed interaction strength (influenced by parameters)
        DimensionInteraction(int idx, double dist, double str)
            : vertexIndex(idx), distance(dist), strength(str) {}
    };

// THESE ARE YOUR DIALS
// Simulation parameters for n-dimensional quantum-like interactions
// Constructor with defaults aligned to physical reality:
// - maxDimensions: Max dimensions (1-20, default 11, M-theory)
// - mode: Initial dimension (1-maxDimensions, default 3, observable universe)
// - influence: Interaction strength (0-10, default 1.0, neutral)
// - weak: Weak force modifier (0-1, default 0.01, ~1/30th electromagnetic)
// - collapse: Quantum collapse factor (0-5, default 5.0, strong collapse)
// - twoD: 2D interaction strength (0-5, default 0.0, minimal)
// - threeDInfluence: 3D interaction strength (0-5, default 5.0, dominant)
// - oneDPermeation: 1D permeation factor (0-5, default 0.0, minimal)
// - darkMatterStrength: Dark matter influence (0-1, default 0.27, cosmic fraction)
// - darkEnergyStrength: Dark energy influence (0-2, default 0.68, cosmic fraction)
// - alpha: Decay factor (0.1-10, default 0.1, fine-structure inspired)
// - beta: Permeation scaling (0-1, default 0.5, balanced)
// - carrollFactor: Relativistic limit (0=Schrödinger, 1=Carroll, default 0.0)
// - meanFieldApprox: Many-body approximation (0=exact, 1=full, default 0.5)
// - asymCollapse: Asymmetric collapse term (0=standard, 1=MDPI 2025, default 0.5)
// - perspectiveTrans: Perspective translation (0-10, default 2.0)
// - perspectiveFocal: Perspective focal length (1-20, default 4.0)
// - debug: Debug logging (default false)
// Usage: UniversalEquation(11, 3, 1.0, ...) for custom values
// Throws: std::exception on initialization failure
// Vulkan: Requires DimensionalNavigator with valid Vulkan instance/surface
UniversalEquation(
    int maxDimensions = 11,             // M-theory's 11 dimensions
    int mode = 3,                      // 3D observable universe
    double influence = 1.0,            // Neutral interaction strength
    double weak = 0.01,                // Weak force (~1/30th electromagnetic)
    double collapse = 5.0,             // Strong quantum collapse
    double twoD = 0.0,                 // Minimal 2D influence
    double threeDInfluence = 5.0,      // Strong 3D dominance
    double oneDPermeation = 0.0,       // Minimal 1D effects
    double darkMatterStrength = 0.27,  // ~27% cosmic energy density
    double darkEnergyStrength = 0.68,  // ~68% cosmic energy density
    double alpha = 0.1,                // Decay factor
    double beta = 0.5,                 // Permeation modifier
    double carrollFactor = 0.0,        // Non-relativistic limit
    double meanFieldApprox = 0.5,      // Many-body approximation
    double asymCollapse = 0.5,         // Asymmetric collapse (MDPI 2025)
    double perspectiveTrans = 2.0,     // Perspective translation
    double perspectiveFocal = 4.0,     // Perspective focal length
    bool debug = false                 // Debug logging
);
    // Setters and Getters for all parameters (thread-safe via atomics)
    // Purpose: Allow modification and retrieval of simulation parameters
    // Usage: Use setters to adjust parameters, e.g., setInfluence(2.5), and getters to query, e.g., getInfluence()
    // Vulkan Note: Parameter changes may require updating Vulkan buffers/uniforms in DimensionalNavigator

    // Sets base interaction strength (clamped 0 to 10)
    void setInfluence(double value);
    double getInfluence() const;

    // Sets weak interaction modifier (clamped 0 to 1)
    void setWeak(double value);
    double getWeak() const;

    // Sets dimensional collapse factor (clamped 0 to 5)
    void setCollapse(double value);
    double getCollapse() const;

    // Sets 2D interaction strength (clamped 0 to 5)
    void setTwoD(double value);
    double getTwoD() const;

    // Sets 3D-specific interaction strength (clamped 0 to 5)
    void setThreeDInfluence(double value);
    double getThreeDInfluence() const;

    // Sets 1D permeation factor (clamped 0 to 5)
    void setOneDPermeation(double value);
    double getOneDPermeation() const;

    // Sets dark matter influence (clamped 0 to 1)
    void setDarkMatterStrength(double value);
    double getDarkMatterStrength() const;

    // Sets dark energy influence (clamped 0 to 2)
    void setDarkEnergyStrength(double value);
    double getDarkEnergyStrength() const;

    // Sets exponential decay factor (clamped 0.1 to 10)
    void setAlpha(double value);
    double getAlpha() const;

    // Sets permeation scaling factor (clamped 0 to 1)
    void setBeta(double value);
    double getBeta() const;

    // Sets Carroll relativistic factor (clamped 0 to 1)
    void setCarrollFactor(double value);
    double getCarrollFactor() const;

    // Sets mean-field approximation strength (clamped 0 to 1)
    void setMeanFieldApprox(double value);
    double getMeanFieldApprox() const;

    // Sets asymmetric collapse factor (clamped 0 to 1)
    void setAsymCollapse(double value);
    double getAsymCollapse() const;

    // Sets perspective translation (clamped 0 to 10)
    void setPerspectiveTrans(double value);
    double getPerspectiveTrans() const;

    // Sets perspective focal length (clamped 1 to 20)
    void setPerspectiveFocal(double value);
    double getPerspectiveFocal() const;

    // Sets debug logging state
    void setDebug(bool value);
    bool getDebug() const;

    // Sets simulation mode (clamped 1 to maxDimensions)
    void setMode(int mode);
    int getMode() const;

    // Sets current dimension (clamped 1 to maxDimensions)
    void setCurrentDimension(int dimension);
    int getCurrentDimension() const;

    // Gets maximum number of dimensions
    int getMaxDimensions() const;

    // Additional getters for internal state (useful for data analysis and rendering)
    // Purpose: Provide access to simulation state for analysis and visualization
    // Usage: Query internal parameters, e.g., getOmega(), getInteractions()
    // Vulkan Note: Use getNCubeVertices() and getInteractions() to populate vertex buffers
    // or compute shaders for rendering lattice structures

    // Gets angular frequency for 2D oscillation
    double getOmega() const;

    // Gets inverse of max dimensions for scaling
    double getInvMaxDim() const;

    // Gets interaction data (thread-safe)
    const std::vector<DimensionInteraction>& getInteractions() const;

    // Gets hypercube vertices (thread-safe)
    const std::vector<std::vector<double>>& getNCubeVertices() const;

    // Gets precomputed cosine values (thread-safe)
    const std::vector<double>& getCachedCos() const;

    // New: Gets projected 3D vertices for rendering deformation
    std::vector<glm::vec3> getProjectedVertices() const;

    // New: Gets average projection scale for modulation
    double getAvgProjScale() const;

    // Advances the simulation to the next dimension (cycles back to 1 if at max)
    // Usage: Iterate through dimensions, e.g., advanceCycle()
    // Vulkan Note: Update Vulkan buffers after advancing to reflect new dimension
    void advanceCycle();

    // Computes energy components for the current dimension
    // Returns: EnergyResult with observable, potential, dark matter, and dark energy
    // Usage: Run simulation and retrieve results, e.g., auto result = compute()
    // Vulkan Note: Pass EnergyResult to shaders (e.g., as uniforms) for visualization
    EnergyResult compute() const;

    // Initializes calculator with a navigator for rendering integration
    // Parameters:
    //   - navigator: Pointer to DimensionalNavigator for visualization
    // Usage: Set up rendering, e.g., initializeCalculator(new DimensionalNavigator(...))
    // Throws std::invalid_argument if navigator is null
    // Vulkan Note: Ensure Vulkan instance, device, and surface are properly set up in
    // DimensionalNavigator to avoid errors like BadMatch. Verify descriptor set layouts
    // and shader interfaces match pipeline requirements.
    void initializeCalculator(DimensionalNavigator* navigator);

    // Structure to cache simulation results for data analysis
    // Purpose: Stores dimension and energy results for easy access
    // Usage: Retrieved via updateCache(), e.g., auto data = updateCache()
    // Vulkan Note: DimensionData can be used to update Vulkan uniforms or buffers
    struct DimensionData {
        int dimension;        // Current dimension
        double observable;    // Observable energy
        double potential;     // Potential energy
        double darkMatter;    // Dark matter contribution
        double darkEnergy;    // Dark energy contribution
        // Converts data to a formatted string for logging or display
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

    // Updates and returns cached simulation data
    // Usage: Retrieve current simulation state, e.g., auto data = updateCache()
    // Vulkan Note: Use DimensionData to update rendering data in Vulkan pipeline
    DimensionData updateCache();

    // Public methods for AMOURANTH access (rendering integration)
    // Purpose: Expose internal calculations for external rendering or analysis
    // Usage: Used by DimensionalNavigator or rendering systems to compute shader inputs
    // Vulkan Note: Ensure shader interfaces using these methods have matching descriptor
    // layouts and vertex/fragment outputs to avoid validation errors

    // Computes interaction strength for a vertex at a given distance
    double computeInteraction(int vertexIndex, double distance) const;

    // Computes permeation factor for a vertex
    double computePermeation(int vertexIndex) const;

    // Computes dark energy contribution based on distance
    double computeDarkEnergy(double distance) const;

private:
    // Private members for internal state (thread-safe where necessary)
    int maxDimensions_;          // Maximum number of dimensions (1 to 20, constant after init)
    std::atomic<int> currentDimension_; // Current dimension for computation
    std::atomic<int> mode_;      // Current mode (aligned with currentDimension_)
    uint64_t maxVertices_;       // Maximum number of vertices (2^20, constant after init)
    std::atomic<double> influence_; // Base interaction strength (0 to 10)
    std::atomic<double> weak_;   // Weak interaction strength (0 to 1)
    std::atomic<double> collapse_; // Dimensional collapse factor (0 to 5)
    std::atomic<double> twoD_;   // 2D influence factor (0 to 5)
    std::atomic<double> threeDInfluence_; // 3D influence factor (0 to 5)
    std::atomic<double> oneDPermeation_; // 1D permeation factor (0 to 5)
    std::atomic<double> darkMatterStrength_; // Dark matter strength (0 to 1)
    std::atomic<double> darkEnergyStrength_; // Dark energy strength (0 to 2)
    std::atomic<double> alpha_;  // Exponential decay factor (0.1 to 10)
    std::atomic<double> beta_;   // Permeation scaling factor (0 to 1)
    std::atomic<double> carrollFactor_; // Carroll rel limit (0 to 1)
    std::atomic<double> meanFieldApprox_; // Many-body mean-field strength (0 to 1)
    std::atomic<double> asymCollapse_; // Asymmetric measurement term (0 to 1)
    std::atomic<double> perspectiveTrans_; // Perspective translation (0 to 10)
    std::atomic<double> perspectiveFocal_; // Perspective focal length (1 to 20)
    std::atomic<bool> debug_;    // Debug output flag
    double omega_;               // Angular frequency for 2D oscillation (constant after init)
    double invMaxDim_;           // Inverse of max dimensions for scaling (constant after init)
    mutable std::vector<DimensionInteraction> interactions_; // Interaction data (protected by mutex_)
    std::vector<std::vector<double>> nCubeVertices_; // Vertex coordinates for n-cube (protected by mutex_)
    mutable std::vector<glm::vec3> projectedVerts_; // New: Projected 3D verts (protected by projMutex_)
    mutable double avgProjScale_; // New: Avg projection scale
    mutable std::mutex projMutex_; // New: Protects projectedVerts_ and avgProjScale_
    mutable std::atomic<bool> needsUpdate_; // Dirty flag for interactions
    std::vector<double> cachedCos_; // Cached cosine values (protected by mutex_)
    DimensionalNavigator* navigator_; // Pointer to DimensionalNavigator (managed externally)
    mutable std::mutex mutex_; // Protects interactions_, nCubeVertices_, cachedCos_
    mutable std::mutex debugMutex_; // Protects debug output for thread-safe logging
    mutable std::mt19937 rng_; // New: Jitter RNG

    // Private methods for internal computations
    // Computes the collapse factor based on dimension and parameters
    // Usage: Called during compute() to apply dimensional convergence effects
    double computeCollapse() const;

    // Initializes n-cube vertices as binary combinations (±1) in n-space
    // Usage: Called during initialization to set up the simulation lattice
    // Parallelized for large dimensions
    void initializeNCube();

    // Updates interaction data (vertex index, distance, strength) for the current dimension
    // Usage: Called when needsUpdate_ is true to refresh simulation state
    // Parallelized for large vertex counts
    void updateInteractions() const;

    // Initializes hypercube and cosines with retry logic for memory errors
    // Usage: Called during constructor and dimension/mode changes
    void initializeWithRetry();

    // Safe exponential function to prevent overflow/underflow
    // Usage: Used in interaction and dark energy calculations
    double safeExp(double x) const;
};

#endif // UNIVERSAL_EQUATION_HPP