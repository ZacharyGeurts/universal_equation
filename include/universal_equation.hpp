#ifndef UNIVERSAL_EQUATION_HPP
#define UNIVERSAL_EQUATION_HPP

#include <vector>
#include <cmath>
#include <string>
#include <limits>
#include <iostream>
#include <algorithm>
#include <stdexcept>

class UniversalEquation {
public:
    // Structure to hold energy computation results
    struct EnergyResult {
        double observable;    // Observable energy component
        double potential;     // Potential energy component
        double darkMatter;    // Dark matter contribution
        double darkEnergy;    // Dark energy contribution
        std::string toString() const {
            return "Observable: " + std::to_string(observable) +
                   ", Potential: " + std::to_string(potential) +
                   ", Dark Matter: " + std::to_string(darkMatter) +
                   ", Dark Energy: " + std::to_string(darkEnergy);
        }
    };

    // Structure to represent interactions between vertices
    struct DimensionInteraction {
        int vertexIndex;      // Index of the vertex
        double distance;      // Distance to reference vertex
        double strength;      // Interaction strength
    };

    // Constructor with parameters for the physical model
    // Parameters are clamped to safe ranges to prevent undefined behavior
    UniversalEquation(int maxDimensions = 9, int mode = 1, double influence = 0.05,
                     double weak = 0.01, double collapse = 0.1, double twoD = 0.0,
                     double threeDInfluence = 0.1, double oneDPermeation = 0.1,
                     double darkMatterStrength = 0.27, double darkEnergyStrength = 0.68,
                     double alpha = 2.0, double beta = 0.2, bool debug = false)
        : maxDimensions_(std::max(1, std::min(maxDimensions == 0 ? 20 : maxDimensions, 20))), // Cap at 20 dimensions
          currentDimension_(std::clamp(mode, 1, maxDimensions_)),
          mode_(currentDimension_),
          maxVertices_(1ULL << std::min(maxDimensions_, 20)), // Cap at 2^20 (~1M) vertices for memory safety
          influence_(std::clamp(influence, 0.0, 10.0)),
          weak_(std::clamp(weak, 0.0, 1.0)),
          collapse_(std::clamp(collapse, 0.0, 5.0)),
          twoD_(std::clamp(twoD, 0.0, 5.0)),
          threeDInfluence_(std::clamp(threeDInfluence, 0.0, 5.0)),
          oneDPermeation_(std::clamp(oneDPermeation, 0.0, 5.0)),
          darkMatterStrength_(std::clamp(darkMatterStrength, 0.0, 1.0)),
          darkEnergyStrength_(std::clamp(darkEnergyStrength, 0.0, 2.0)),
          alpha_(std::clamp(alpha, 0.1, 10.0)),
          beta_(std::clamp(beta, 0.0, 1.0)),
          debug_(debug),
          omega_(maxDimensions_ > 0 ? 2.0 * M_PI / (2 * maxDimensions_ - 1) : 1.0),
          invMaxDim_(maxDimensions_ > 0 ? 1.0 / maxDimensions_ : 1e-15),
          interactions_(),
          nCubeVertices_(),
          needsUpdate_(true) { // Initialize dirty flag for interactions
        // Initialize n-cube and interactions with retry logic for memory allocation
        initializeWithRetry();
        if (debug_) {
            std::cout << "Initialized: maxDimensions=" << maxDimensions_ << ", mode=" << mode_
                      << ", currentDimension=" << currentDimension_ << ", maxVertices=" << maxVertices_ << "\n";
        }
    }

    // Setters with clamping and interaction updates
    void setInfluence(double value) {
        influence_ = std::clamp(value, 0.0, 10.0);
        needsUpdate_ = true; // Mark interactions as needing update
    }
    double getInfluence() const { return influence_; }

    void setWeak(double value) {
        weak_ = std::clamp(value, 0.0, 1.0);
        needsUpdate_ = true;
    }
    double getWeak() const { return weak_; }

    void setCollapse(double value) {
        collapse_ = std::clamp(value, 0.0, 5.0);
        // No need to update interactions, as collapse_ only affects compute()
    }
    double getCollapse() const { return collapse_; }

    void setTwoD(double value) {
        twoD_ = std::clamp(value, 0.0, 5.0);
        needsUpdate_ = true;
    }
    double getTwoD() const { return twoD_; }

    void setThreeDInfluence(double value) {
        threeDInfluence_ = std::clamp(value, 0.0, 5.0);
        needsUpdate_ = true;
    }
    double getThreeDInfluence() const { return threeDInfluence_; }

    void setOneDPermeation(double value) {
        oneDPermeation_ = std::clamp(value, 0.0, 5.0);
        needsUpdate_ = true;
    }
    double getOneDPermeation() const { return oneDPermeation_; }

    void setDarkMatterStrength(double value) {
        darkMatterStrength_ = std::clamp(value, 0.0, 1.0);
        needsUpdate_ = true;
    }
    double getDarkMatterStrength() const { return darkMatterStrength_; }

    void setDarkEnergyStrength(double value) {
        darkEnergyStrength_ = std::clamp(value, 0.0, 2.0);
        needsUpdate_ = true;
    }
    double getDarkEnergyStrength() const { return darkEnergyStrength_; }

    void setAlpha(double value) {
        alpha_ = std::clamp(value, 0.1, 10.0);
        needsUpdate_ = true;
    }
    double getAlpha() const { return alpha_; }

    void setBeta(double value) {
        beta_ = std::clamp(value, 0.0, 1.0);
        needsUpdate_ = true;
    }
    double getBeta() const { return beta_; }

    void setDebug(bool value) { debug_ = value; }
    bool getDebug() const { return debug_; }

    // Set computation mode and update dimension
    void setMode(int mode) {
        mode = std::clamp(mode, 1, maxDimensions_);
        if (mode_ != mode || currentDimension_ != mode) {
            mode_ = mode;
            currentDimension_ = mode;
            needsUpdate_ = true;
            initializeWithRetry();
            if (debug_) {
                std::cout << "Mode set to: " << mode_ << ", dimension: " << currentDimension_ << "\n";
            }
        }
    }
    int getMode() const { return mode_; }

    // Set current dimension explicitly
    void setCurrentDimension(int dimension) {
        if (dimension >= 1 && dimension <= maxDimensions_ && dimension != currentDimension_) {
            currentDimension_ = dimension;
            mode_ = dimension;
            needsUpdate_ = true;
            initializeWithRetry();
            if (debug_) {
                std::cout << "Dimension set to: " << currentDimension_ << ", mode: " << mode_ << "\n";
            }
        }
    }
    int getCurrentDimension() const { return currentDimension_; }

    int getMaxDimensions() const { return maxDimensions_; }

    // Get copy of interactions
    const std::vector<DimensionInteraction>& getInteractions() const {
        if (needsUpdate_) {
            updateInteractions();
        }
        return interactions_;
    }

    // Advance to next dimension in cycle
    void advanceCycle() {
        if (currentDimension_ == maxDimensions_) {
            currentDimension_ = 1;
            mode_ = 1;
        } else {
            currentDimension_++;
            mode_ = currentDimension_;
        }
        needsUpdate_ = true;
        initializeWithRetry();
        if (debug_) {
            std::cout << "Cycle advanced: dimension=" << currentDimension_ << ", mode=" << mode_ << "\n";
        }
    }

    // Compute energy components
    EnergyResult compute() const {
        if (needsUpdate_) {
            updateInteractions();
        }
        double observable = influence_;
        if (currentDimension_ >= 2) {
            // Cache cos computation
            observable += twoD_ * cachedCos_[currentDimension_ % cachedCos_.size()];
        }
        if (currentDimension_ == 3) {
            observable += threeDInfluence_;
        }

        double totalDarkMatter = 0.0, totalDarkEnergy = 0.0, interactionSum = 0.0;
        for (const auto& interaction : interactions_) {
            double influence = interaction.strength;
            double permeation = computePermeation(interaction.vertexIndex);
            double darkMatter = darkMatterStrength_;
            double darkEnergy = computeDarkEnergy(interaction.distance);
            // Cache exp computation for performance
            interactionSum += influence * safeExp(-alpha_ * interaction.distance) * permeation * darkMatter;
            totalDarkMatter += darkMatter * influence * permeation;
            totalDarkEnergy += darkEnergy * influence * permeation;
        }
        observable += interactionSum;

        double collapse = computeCollapse();
        EnergyResult result = {
            observable + collapse,
            std::max(0.0, observable - collapse),
            totalDarkMatter,
            totalDarkEnergy
        };

        if (debug_) { // Limit debug output for large datasets
            std::cout << "Compute(D=" << currentDimension_ << "): " << result.toString() << "\n";
        }
        return result;
    }

private:
    int maxDimensions_;          // Maximum number of dimensions (1 to 20)
    int currentDimension_;       // Current dimension for computation
    int mode_;                  // Current mode (aligned with currentDimension_)
    uint64_t maxVertices_;      // Maximum number of vertices (2^20)
    double influence_;          // Influence factor (0 to 10)
    double weak_;               // Weak interaction strength (0 to 1)
    double collapse_;           // Collapse factor (0 to 5)
    double twoD_;               // 2D influence factor (0 to 5)
    double threeDInfluence_;    // 3D influence factor (0 to 5)
    double oneDPermeation_;     // 1D permeation factor (0 to 5)
    double darkMatterStrength_; // Dark matter strength (0 to 1)
    double darkEnergyStrength_; // Dark energy strength (0 to 2)
    double alpha_;              // Exponential decay factor (0.1 to 10)
    double beta_;               // Permeation scaling factor (0 to 1)
    bool debug_;                // Debug output flag
    double omega_;              // Angular frequency for 2D oscillation
    double invMaxDim_;          // Inverse of max dimensions for scaling
    mutable std::vector<DimensionInteraction> interactions_; // Interaction data (mutable for lazy update)
    std::vector<std::vector<double>> nCubeVertices_;        // Vertex coordinates for n-cube
    mutable bool needsUpdate_;                              // Dirty flag for interactions
    std::vector<double> cachedCos_;                         // Cached cosine values for performance

    // Compute interaction strength for a vertex
    double computeInteraction(int vertexIndex, double distance) const {
        double denom = std::max(1e-15, std::pow(static_cast<double>(currentDimension_), (vertexIndex % maxDimensions_ + 1)));
        double modifier = (currentDimension_ > 3 && (vertexIndex % maxDimensions_ + 1) > 3) ? weak_ : 1.0;
        if (currentDimension_ == 3 && ((vertexIndex % maxDimensions_ + 1) == 2 || (vertexIndex % maxDimensions_ + 1) == 4)) {
            modifier *= threeDInfluence_;
        }
        double result = influence_ * (1.0 / (denom * (1.0 + distance))) * modifier;
        if (debug_ && interactions_.size() <= 100) {
            std::cout << "Interaction(vertex=" << vertexIndex << ", dist=" << distance << "): " << result << "\n";
        }
        return result;
    }

    // Compute permeation factor for a vertex
    double computePermeation(int vertexIndex) const {
        if (vertexIndex < 0 || nCubeVertices_.empty()) {
            if (debug_) {
                std::cerr << "Error: Invalid vertex index " << vertexIndex << " or empty vertex list\n";
            }
            throw std::out_of_range("Invalid vertex index or empty vertex list");
        }
        if (vertexIndex == 1 || currentDimension_ == 1) return oneDPermeation_;
        if (currentDimension_ == 2 && (vertexIndex % maxDimensions_ + 1) > 2) return twoD_;
        if (currentDimension_ == 3 && ((vertexIndex % maxDimensions_ + 1) == 2 || (vertexIndex % maxDimensions_ + 1) == 4)) {
            return threeDInfluence_;
        }

        size_t safeIndex = static_cast<size_t>(vertexIndex) % nCubeVertices_.size();
        const auto& vertex = nCubeVertices_[safeIndex];
        double vertexMagnitude = 0.0;
        for (int i = 0; i < std::min(currentDimension_, static_cast<int>(vertex.size())); ++i) {
            vertexMagnitude += vertex[i] * vertex[i];
        }
        vertexMagnitude = std::sqrt(vertexMagnitude);
        double result = 1.0 + beta_ * vertexMagnitude / std::max(1, currentDimension_);
        if (debug_ && interactions_.size() <= 100) {
            std::cout << "Permeation(vertex=" << vertexIndex << "): " << result << "\n";
        }
        return result;
    }

    // Compute dark energy contribution
    double computeDarkEnergy(double distance) const {
        double d = std::min(distance, 10.0);
        double result = darkEnergyStrength_ * safeExp(d * invMaxDim_);
        if (debug_ && interactions_.size() <= 100) {
            std::cout << "DarkEnergy(dist=" << distance << "): " << result << "\n";
        }
        return result;
    }

    // Compute collapse factor
    double computeCollapse() const {
        if (currentDimension_ == 1) return 0.0;
        double phase = static_cast<double>(currentDimension_) / (2 * maxDimensions_);
        double osc = std::abs(cachedCos_[static_cast<size_t>(2.0 * M_PI * phase * cachedCos_.size()) % cachedCos_.size()]);
        double result = std::max(0.0, collapse_ * currentDimension_ * safeExp(-beta_ * (currentDimension_ - 1)) * (0.8 * osc + 0.2));
        if (debug_ && interactions_.size() <= 100) {
            std::cout << "Collapse(D=" << currentDimension_ << "): " << result << "\n";
        }
        return result;
    }

    // Initialize n-cube vertices
    void initializeNCube() {
        nCubeVertices_.clear();
        uint64_t numVertices = std::min(static_cast<uint64_t>(1ULL << currentDimension_), maxVertices_);
        nCubeVertices_.reserve(numVertices); // Preallocate to avoid reallocations
        for (uint64_t i = 0; i < numVertices; ++i) {
            std::vector<double> vertex(currentDimension_, 0.0);
            for (int j = 0; j < currentDimension_; ++j) {
                vertex[j] = (i & (1ULL << j)) ? 1.0 : -1.0;
            }
            nCubeVertices_.push_back(std::move(vertex)); // Use move to avoid copying
        }
        if (debug_ && nCubeVertices_.size() <= 100) {
            std::cout << "Initialized nCube with " << nCubeVertices_.size() << " vertices for dimension " << currentDimension_ << "\n";
        }
    }

    // Update interactions between vertices
    void updateInteractions() const {
        interactions_.clear();
        interactions_.reserve(std::min(static_cast<uint64_t>(1ULL << currentDimension_), maxVertices_)); // Preallocate
        const auto& referenceVertex = nCubeVertices_[0];
        uint64_t numVertices = std::min(static_cast<uint64_t>(1ULL << currentDimension_), maxVertices_);
        for (uint64_t i = 1; i < numVertices; ++i) {
            double distance = 0.0;
            for (int j = 0; j < currentDimension_; ++j) {
                double diff = nCubeVertices_[i][j] - referenceVertex[j];
                distance += diff * diff;
            }
            distance = std::sqrt(distance);
            double strength = computeInteraction(static_cast<int>(i), distance);
            interactions_.push_back({static_cast<int>(i), distance, strength});
        }
        if (currentDimension_ == 3) {
            for (int adj : {2, 4}) {
                size_t vertexIndex = static_cast<size_t>(adj - 1);
                if (vertexIndex < nCubeVertices_.size() &&
                    std::none_of(interactions_.begin(), interactions_.end(),
                                 [adj](const auto& i) { return i.vertexIndex == adj; })) {
                    double distance = 0.0;
                    for (int j = 0; j < currentDimension_; ++j) {
                        double diff = nCubeVertices_[vertexIndex][j] - referenceVertex[j];
                        distance += diff * diff;
                    }
                    distance = std::sqrt(distance);
                    double strength = computeInteraction(static_cast<int>(vertexIndex), distance);
                    interactions_.push_back({static_cast<int>(vertexIndex), distance, strength});
                }
            }
        }
        needsUpdate_ = false; // Clear dirty flag
        if (debug_ && interactions_.size() <= 100) {
            std::cout << "Interactions(D=" << currentDimension_ << "): ";
            for (const auto& i : interactions_) {
                std::cout << "(vertex=" << i.vertexIndex << ", dist=" << i.distance << ", strength=" << i.strength << ") ";
            }
            std::cout << "\n";
        }
    }

    // Initialize with retry logic for memory allocation failures
    void initializeWithRetry() {
        while (currentDimension_ >= 1) {
            try {
                initializeNCube();
                // Precompute cosine values for performance
                cachedCos_.resize(maxDimensions_ + 1);
                for (int i = 0; i <= maxDimensions_; ++i) {
                    cachedCos_[i] = std::cos(omega_ * i);
                }
                updateInteractions();
                return;
            } catch (const std::bad_alloc& e) {
                std::cerr << "Error: Failed to allocate memory for " << (1ULL << currentDimension_)
                          << " vertices. Reducing dimension to " << (currentDimension_ - 1) << ".\n";
                if (currentDimension_ == 1) {
                    throw std::runtime_error("Failed to allocate memory even at dimension 1");
                }
                currentDimension_--;
                mode_ = currentDimension_;
                maxVertices_ = 1ULL << std::min(currentDimension_, 20);
                needsUpdate_ = true;
            }
        }
    }

    // Safe exponential function to prevent overflow/underflow
    double safeExp(double x) const {
        return std::exp(std::clamp(x, -709.0, 709.0)); // Safe range for double
    }
};

#endif // UNIVERSAL_EQUATION_HPP