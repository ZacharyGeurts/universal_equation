// UniversalEquation.hpp
#ifndef UNIVERSAL_EQUATION_HPP
#define UNIVERSAL_EQUATION_HPP

#include <vector>
#include <string>
#include <cstdint>
#include <cmath>
#include <limits>
#include <algorithm>
#include <stdexcept>
#include <mutex>
#include <atomic>
#include "PushConstants.hpp"

// Forward declaration to avoid circular dependency
class DimensionalNavigator;

class UniversalEquation {
public:
    // Structure to hold energy computation results
    struct EnergyResult {
        float observable;    // Observable energy component
        float potential;     // Potential energy component
        float darkMatter;    // Dark matter contribution
        float darkEnergy;    // Dark energy contribution
        std::string toString() const {
            return "Observable: " + std::to_string(observable) +
                   ", Potential: " + std::to_string(potential) +
                   ", Dark Matter: " + std::to_string(darkMatter) +
                   ", Dark Energy: " + std::to_string(darkEnergy);
        }
    };

    // Structure to represent interactions between vertices
    struct DimensionInteraction {
        int32_t vertexIndex;  // Index of the vertex
        float distance;       // Distance to reference vertex
        float strength;       // Interaction strength
        DimensionInteraction(int32_t idx, float dist, float str)
            : vertexIndex(idx), distance(dist), strength(str) {}
    };

    // Constructor with default parameters
    UniversalEquation(int32_t maxDimensions = 9, int32_t mode = 1, float influence = 0.05f,
                     float weak = 0.01f, float collapse = 0.1f, float twoD = 0.0f,
                     float threeDInfluence = 0.1f, float oneDPermeation = 0.1f,
                     float darkMatterStrength = 0.27f, float darkEnergyStrength = 0.68f,
                     float alpha = 2.0f, float beta = 0.2f, bool debug = false);

    // Populate PushConstants for shader use
    void populatePushConstants(PushConstants& constants) const;

    // Setters and Getters (thread-safe)
    void setInfluence(float value) { influence_.store(value, std::memory_order_relaxed); }
    float getInfluence() const { return influence_.load(std::memory_order_relaxed); }
    void setWeak(float value) { weak_.store(value, std::memory_order_relaxed); }
    float getWeak() const { return weak_.load(std::memory_order_relaxed); }
    void setCollapse(float value) { collapse_.store(value, std::memory_order_relaxed); }
    float getCollapse() const { return collapse_.load(std::memory_order_relaxed); }
    void setTwoD(float value) { twoD_.store(value, std::memory_order_relaxed); }
    float getTwoD() const { return twoD_.load(std::memory_order_relaxed); }
    void setThreeDInfluence(float value) { threeDInfluence_.store(value, std::memory_order_relaxed); }
    float getThreeDInfluence() const { return threeDInfluence_.load(std::memory_order_relaxed); }
    void setOneDPermeation(float value) { oneDPermeation_.store(value, std::memory_order_relaxed); }
    float getOneDPermeation() const { return oneDPermeation_.load(std::memory_order_relaxed); }
    void setDarkMatterStrength(float value) { darkMatterStrength_.store(value, std::memory_order_relaxed); }
    float getDarkMatterStrength() const { return darkMatterStrength_.load(std::memory_order_relaxed); }
    void setDarkEnergyStrength(float value) { darkEnergyStrength_.store(value, std::memory_order_relaxed); }
    float getDarkEnergyStrength() const { return darkEnergyStrength_.load(std::memory_order_relaxed); }
    void setAlpha(float value) { alpha_.store(value, std::memory_order_relaxed); }
    float getAlpha() const { return alpha_.load(std::memory_order_relaxed); }
    void setBeta(float value) { beta_.store(value, std::memory_order_relaxed); }
    float getBeta() const { return beta_.load(std::memory_order_relaxed); }
    void setDebug(bool value) { debug_.store(value, std::memory_order_relaxed); }
    bool getDebug() const { return debug_.load(std::memory_order_relaxed); }
    void setMode(int32_t mode) { mode_.store(mode, std::memory_order_relaxed); }
    int32_t getMode() const { return mode_.load(std::memory_order_relaxed); }
    void setCurrentDimension(int32_t dimension) { currentDimension_.store(dimension, std::memory_order_relaxed); }
    int32_t getCurrentDimension() const { return currentDimension_.load(std::memory_order_relaxed); }
    int32_t getMaxDimensions() const { return maxDimensions_; }

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
        int32_t dimension;
        float observable, potential, darkMatter, darkEnergy;
    };
    DimensionData updateCache();

    // Public methods for AMOURANTH access
    float computeInteraction(int32_t vertexIndex, float distance) const;
    float computePermeation(int32_t vertexIndex) const;
    float computeDarkEnergy(float distance) const;

private:
    int32_t maxDimensions_;          // Maximum number of dimensions (1 to 20)
    std::atomic<int32_t> currentDimension_; // Current dimension for computation
    std::atomic<int32_t> mode_;      // Current mode (aligned with currentDimension_)
    uint64_t maxVertices_;           // Maximum number of vertices (2^20)
    std::atomic<float> influence_;   // Influence factor (0 to 10)
    std::atomic<float> weak_;        // Weak interaction strength (0 to 1)
    std::atomic<float> collapse_;    // Collapse factor (0 to 5)
    std::atomic<float> twoD_;        // 2D influence factor (0 to 5)
    std::atomic<float> threeDInfluence_; // 3D influence factor (0 to 5)
    std::atomic<float> oneDPermeation_; // 1D permeation factor (0 to 5)
    std::atomic<float> darkMatterStrength_; // Dark matter strength (0 to 1)
    std::atomic<float> darkEnergyStrength_; // Dark energy strength (0 to 2)
    std::atomic<float> alpha_;       // Exponential decay factor (0.1 to 10)
    std::atomic<float> beta_;        // Permeation scaling factor (0 to 1)
    std::atomic<bool> debug_;        // Debug output flag
    float omega_;                    // Angular frequency for 2D oscillation
    float invMaxDim_;                // Inverse of max dimensions for scaling
    mutable std::vector<DimensionInteraction> interactions_; // Interaction data
    std::vector<std::vector<float>> nCubeVertices_; // Vertex coordinates for n-cube
    mutable std::atomic<bool> needsUpdate_; // Dirty flag for interactions
    std::vector<float> cachedCos_;   // Cached cosine values
    DimensionalNavigator* navigator_; // Pointer to DimensionalNavigator (assumed managed externally)
    mutable std::mutex mutex_;       // Protects interactions_, nCubeVertices_, cachedCos_
    mutable std::mutex debugMutex_;  // Protects debug output

    // Private methods
    float computeCollapse() const;
    void initializeNCube();
    void updateInteractions() const;
    void initializeWithRetry();
    float safeExp(float x) const;
};

#endif // UNIVERSAL_EQUATION_HPP