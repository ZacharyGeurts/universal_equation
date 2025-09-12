#ifndef UNIVERSAL_EQUATION_HPP
#define UNIVERSAL_EQUATION_HPP

#include <vector>
#include <cmath>
#include <functional>
#include <algorithm>
#include <iostream>
#include <string>
#include <limits>

// UniversalEquation models dimensional interactions from 1D (foundational layer, a universal constant)
// to maxDimensions (default 9D, supports up to infinity). 2D acts as a boundary, 3D has strong influence on 2D and 4D.
// Incorporates dark matter with theoretical properties (gravitational and stabilizing effects) and dark energy as an expansion driver.
// Modes 1–4 correspond to dimensions 1–4. Interactions follow a cycle (1D to maxD, back to 1D, then 2D) with exponential decay and oscillatory dynamics.
// Optimized for performance with precomputed constants, minimal allocations, and modular design.
class UniversalEquation {
public:
    // Structure to hold energy fluctuation results with interpretation.
    struct EnergyFluctuations {
        double positive;  // Observable energy
        double negative;  // Potential energy sinks
        double darkMatterContribution;  // Dark matter stabilization and gravitational effects
        double darkEnergyContribution;  // Dark energy expansion effects
        std::string interpretation() const {
            return "Positive: " + std::to_string(positive) +
                   ", Negative: " + std::to_string(negative) +
                   ", Dark Matter: " + std::to_string(darkMatterContribution) +
                   ", Dark Energy: " + std::to_string(darkEnergyContribution);
        }
    };

    // Structure to hold dimension interaction data.
    struct DimensionData {
        int dPrime;       // Interacting dimension
        double distance;  // Effective dimensional separation, adjusted by dark energy
        double darkMatterDensity;  // Dark matter influence for this interaction
    };

    // Constructor with support for high dimensions and mode-based initialization.
    UniversalEquation(int maxDimensions = 9,
                     int mode = 1,
                     double influence = 1.0,
                     double weak = 0.5,
                     double collapse = 0.5,
                     double twoD = 0.5,
                     double threeDAdjacency = 1.5,  // Stronger influence for 3D on 2D/4D
                     double permeation = 2.0,
                     double darkMatterStrength = 0.27,  // Based on ~27% of universe's mass-energy
                     double darkEnergyScale = 0.68,     // Based on ~68% of universe's mass-energy
                     double alpha = 5.0,
                     double beta = 0.2,
                     bool debug = false)
        : maxDimensions_(std::max(1, maxDimensions)),
          currentDimension_(std::clamp(mode, 1, std::min(4, maxDimensions_))),
          mode_(std::clamp(mode, 1, 4)),
          kInfluence_(std::clamp(influence, 0.0, 10.0)),
          kWeak_(std::clamp(weak, 0.0, 1.0)),
          kCollapse_(std::clamp(collapse, 0.0, 5.0)),
          kTwoD_(std::clamp(twoD, 0.0, 5.0)),
          kThreeDAdjacency_(std::clamp(threeDAdjacency, 0.0, 5.0)),
          kPermeation_(std::clamp(permeation, 0.0, 5.0)),
          kDarkMatter_(std::clamp(darkMatterStrength, 0.0, 1.0)),
          kDarkEnergy_(std::clamp(darkEnergyScale, 0.0, 2.0)),
          alpha_(std::clamp(alpha, 0.1, 10.0)),
          beta_(std::clamp(beta, 0.0, 1.0)),
          debug_(debug),
          omega_(2.0 * M_PI / static_cast<double>(2 * maxDimensions_ - 1)),
          cycleLength_(2.0 * maxDimensions_),
          invMaxDim_(maxDimensions_ > 0 ? 1.0 / maxDimensions_ : 0.0) {
        if (maxDimensions_ == std::numeric_limits<int>::max()) {
            invMaxDim_ = 1e-10;  // Prevent division by zero for "infinite" dimensions
			// I hold belief there are max 9 before it would be a dense collapse of nothing but God extending.
			// Get buried with a garage door opener and maybe Spot will let you through.
			// Batteries die long before you hit the end of 8th and without it you're stuck.
			// The walk back would suuuuuuuuUck.
			// String theory had gotten out to 11 or 12 before the floor fell out.
			// It was proven flawed by others, and that I believe.
        }
        dimensionPairs_.reserve(std::min(maxDimensions_, 10));  // Limit reservation for practicality
        updateDimensionPairs();
        if (debug_) {
            std::cout << "Initialized UniversalEquation with maxDimensions=" << maxDimensions_
                      << ", mode=" << mode_ << ", currentDimension=" << currentDimension_
                      << ", darkMatterStrength=" << kDarkMatter_ 
                      << ", darkEnergyScale=" << kDarkEnergy_ << std::endl;
        }
    }

    // Setters for parameters, ensuring physical constraints.
    void setInfluence(double value) { kInfluence_ = std::clamp(value, 0.0, 10.0); }
    void setWeak(double value) { kWeak_ = std::clamp(value, 0.0, 1.0); }
    void setCollapse(double value) { kCollapse_ = std::clamp(value, 0.0, 5.0); }
    void setTwoD(double value) { kTwoD_ = std::clamp(value, 0.0, 5.0); }
    void setThreeDAdjacency(double value) { kThreeDAdjacency_ = std::clamp(value, 0.0, 5.0); }
    void setPermeation(double value) { kPermeation_ = std::clamp(value, 0.0, 5.0); }
    void setDarkMatterStrength(double value) { 
        kDarkMatter_ = std::clamp(value, 0.0, 1.0); 
        updateDimensionPairs();
    }
    void setDarkEnergyScale(double value) { 
        kDarkEnergy_ = std::clamp(value, 0.0, 2.0); 
        updateDimensionPairs();
    }
    void setAlpha(double value) { alpha_ = std::clamp(value, 0.1, 10.0); }
    void setBeta(double value) { beta_ = std::clamp(value, 0.0, 1.0); }
    void setDebug(bool enable) { debug_ = enable; }
    void setMode(int mode) {
        mode_ = std::clamp(mode, 1, 4);
        currentDimension_ = std::clamp(mode_, 1, std::min(4, maxDimensions_));
        updateDimensionPairs();
        if (debug_) {
            std::cout << "Set mode to: " << mode_ << ", currentDimension to: " << currentDimension_ << std::endl;
        }
    }

    // Getters for accessing parameters and state.
    double getInfluence() const { return kInfluence_; }
    double getWeak() const { return kWeak_; }
    double getCollapse() const { return kCollapse_; }
    double getTwoD() const { return kTwoD_; }
    double getThreeDAdjacency() const { return kThreeDAdjacency_; }
    double getPermeation() const { return kPermeation_; }
    double getDarkMatterStrength() const { return kDarkMatter_; }
    double getDarkEnergyScale() const { return kDarkEnergy_; }
    double getAlpha() const { return alpha_; }
    double getBeta() const { return beta_; }
    int getCurrentDimension() const { return currentDimension_; }
    int getMaxDimensions() const { return maxDimensions_; }
    int getMode() const { return mode_; }
    std::vector<DimensionData> getDimensionPairs() const { return dimensionPairs_; }

    // Advances the dimensional cycle (1D -> maxD -> 1D -> 2D).
    void advanceCycle() {
        if (currentDimension_ == maxDimensions_) {
            currentDimension_ = 1;
            mode_ = 1;
        } else if (currentDimension_ == 1) {
            currentDimension_ = 2;
            mode_ = 2;
        } else {
            currentDimension_++;
            if (currentDimension_ <= 4) {
                mode_ = currentDimension_;
            }
        }
        updateDimensionPairs();
        if (debug_) {
            std::cout << "Advanced to dimension: " << currentDimension_ << ", mode: " << mode_ << std::endl;
        }
    }

    // Sets the active dimension, ensuring it's within bounds.
    void setCurrentDimension(int dimension) {
        if (dimension >= 1 && (maxDimensions_ == std::numeric_limits<int>::max() || dimension <= maxDimensions_)) {
            currentDimension_ = dimension;
            if (dimension <= 4) {
                mode_ = dimension;
            }
            updateDimensionPairs();
            if (debug_) {
                std::cout << "Set current dimension to: " << currentDimension_ << ", mode: " << mode_ << std::endl;
            }
        }
    }

    // Computes energy fluctuations, incorporating dark matter and dark energy.
    EnergyFluctuations compute() const {
        double sphereInfluence = kInfluence_;
        if (currentDimension_ >= 2) {
            sphereInfluence += kTwoD_ * std::cos(omega_ * currentDimension_);
        }
        if (currentDimension_ == 3) {
            sphereInfluence += kThreeDAdjacency_;  // Strong 3D influence on adjacent dimensions
        }

        double totalInfluence = 0.0;
        double totalDarkMatter = 0.0;
        double totalDarkEnergy = 0.0;
        for (const auto& data : dimensionPairs_) {
            double influence = calculateInfluenceTerm(data.dPrime, data.distance);
            double darkMatterFactor = data.darkMatterDensity;
            double darkEnergyFactor = calculateDarkEnergyFactor(data.distance);
            totalInfluence += influence * std::exp(-alpha_ * data.distance) *
                              calculatePermeationFactor(data.dPrime) * darkMatterFactor;
            totalDarkMatter += darkMatterFactor * influence;
            totalDarkEnergy += darkEnergyFactor * influence;
        }
        sphereInfluence += totalInfluence;

        double collapse = calculateCollapseTerm();
        EnergyFluctuations result = {
            sphereInfluence + collapse,
            std::max(0.0, sphereInfluence - collapse),
            totalDarkMatter,
            totalDarkEnergy
        };

        if (debug_) {
            std::cout << "Compute(D=" << currentDimension_ << ", Mode=" << mode_ << "): TotalInfluence=" << totalInfluence
                      << ", Collapse=" << collapse << ", DarkMatter=" << totalDarkMatter
                      << ", DarkEnergy=" << totalDarkEnergy << ", " << result.interpretation() << std::endl;
        }
        return result;
    }

    // Calculates the influence term for a given dimension and distance.
    double calculateInfluenceTerm(int dPrime, double distance) const {
        double denominator = std::max(1e-10, std::pow(static_cast<double>(std::min(currentDimension_, maxDimensions_)),
                                                      static_cast<double>(std::min(dPrime, maxDimensions_))));
        double modifier = (currentDimension_ > 3 && dPrime > 3) ? kWeak_ : 1.0;
        if (currentDimension_ == 3 && (dPrime == 2 || dPrime == 4)) {
            modifier *= kThreeDAdjacency_;  // Stronger influence for 3D on 2D/4D
        }
        double result = kInfluence_ * (distance / denominator) * modifier;
        if (debug_) {
            std::cout << "InfluenceTerm(D=" << currentDimension_ << ", dPrime=" << dPrime
                      << ", dist=" << distance << "): " << result << std::endl;
        }
        return result;
    }

    // Calculates the permeation factor for dimensional interactions.
    double calculatePermeationFactor(int dPrime) const {
        if (currentDimension_ == 2 && dPrime > currentDimension_) return kTwoD_;
        if (currentDimension_ == 3 && (dPrime == 2 || dPrime == 4)) return kThreeDAdjacency_;
        if (dPrime == currentDimension_ + 1 || dPrime == currentDimension_ - 1 || dPrime == 1) return kPermeation_;
        return 1.0;
    }

    // Calculates dark energy's effect as an exponential expansion of distance.
    double calculateDarkEnergyFactor(double distance) const {
        double expansion = kDarkEnergy_ * std::exp(distance * invMaxDim_);
        if (debug_) {
            std::cout << "DarkEnergyFactor(dist=" << distance << "): " << expansion << std::endl;
        }
        return expansion;
    }

private:
    int maxDimensions_;           // Maximum number of dimensions (supports infinity)
    int currentDimension_;        // Current active dimension
    int mode_;                   // Mode (1–4 maps to dimensions 1–4)
    double kInfluence_;           // Base influence strength
    double kWeak_;                // Weak interaction modifier for higher dimensions
    double kCollapse_;            // Collapse strength
    double kTwoD_;                // 2D boundary strength
    double kThreeDAdjacency_;     // 3D adjacency influence strength
    double kPermeation_;          // Permeation factor for specific interactions
    double kDarkMatter_;          // Dark matter coupling strength (~27% of universe)
    double kDarkEnergy_;          // Dark energy expansion scale (~68% of universe)
    double alpha_;                // Exponential decay constant
    double beta_;                 // Collapse decay constant
    bool debug_;                  // Debug output flag
    double omega_;                // Angular frequency for oscillations
    double cycleLength_;          // Length of dimensional cycle
    double invMaxDim_;            // Precomputed 1/maxDimensions for efficiency
    std::vector<DimensionData> dimensionPairs_;  // Cached dimension interaction data

    // Calculates the collapse term with oscillatory behavior.
    double calculateCollapseTerm() const {
        if (currentDimension_ == 1) return 0.0;
        double phase = std::fmod(currentDimension_, cycleLength_) / cycleLength_;
        double omega = 2.0 * M_PI * phase;
        double result = kCollapse_ * currentDimension_ * std::exp(-beta_ * (currentDimension_ - 1)) *
                        std::abs(std::cos(omega));
        if (debug_) {
            std::cout << "CollapseTerm(D=" << currentDimension_ << "): " << result << std::endl;
        }
        return result;
    }

    // Updates dimension pairs with dark matter and dark energy effects, emphasizing adjacency.
    void updateDimensionPairs() {
        dimensionPairs_.clear();
        // Include adjacent dimensions (d-1, d, d+1) and privileged dimensions (1, 2)
        int start = std::max(1, currentDimension_ - 1);
        int end = (maxDimensions_ == std::numeric_limits<int>::max()) ? currentDimension_ + 1 : std::min(maxDimensions_, currentDimension_ + 1);
        for (int dPrime = start; dPrime <= end; ++dPrime) {
            double baseDistance = static_cast<double>(std::abs(currentDimension_ - dPrime));
            double adjustedDistance = baseDistance * (1.0 + kDarkEnergy_ * invMaxDim_);
            double darkMatterDensity = calculateDarkMatterDensity(dPrime);
            dimensionPairs_.push_back({dPrime, adjustedDistance, darkMatterDensity});
        }
        // Add privileged dimensions (1, 2) if not already included
        for (int priv : {1, 2}) {
            if (priv != currentDimension_ && (maxDimensions_ == std::numeric_limits<int>::max() || priv <= maxDimensions_)) {
                double baseDistance = static_cast<double>(std::abs(currentDimension_ - priv));
                double adjustedDistance = baseDistance * (1.0 + kDarkEnergy_ * invMaxDim_);
                double darkMatterDensity = calculateDarkMatterDensity(priv);
                dimensionPairs_.push_back({priv, adjustedDistance, darkMatterDensity});
            }
        }
        // Special case for 3D: ensure 2D and 4D are included if currentDimension_ == 3
        if (currentDimension_ == 3 && (maxDimensions_ == std::numeric_limits<int>::max() || maxDimensions_ >= 4)) {
            for (int adj : {2, 4}) {
                if (std::none_of(dimensionPairs_.begin(), dimensionPairs_.end(),
                                 [adj](const DimensionData& d) { return d.dPrime == adj; })) {
                    double baseDistance = static_cast<double>(std::abs(currentDimension_ - adj));
                    double adjustedDistance = baseDistance * (1.0 + kDarkEnergy_ * invMaxDim_);
                    double darkMatterDensity = calculateDarkMatterDensity(adj);
                    dimensionPairs_.push_back({adj, adjustedDistance, darkMatterDensity});
                }
            }
        }
        if (debug_) {
            std::cout << "Updated pairs for D=" << currentDimension_ << ", Mode=" << mode_ << ": ";
            for (const auto& pair : dimensionPairs_) {
                std::cout << "(D'=" << pair.dPrime << ", dist=" << pair.distance 
                          << ", DM=" << pair.darkMatterDensity << ") ";
            }
            std::cout << std::endl;
        }
    }

    // Calculates dark matter density with theoretical influence (~27% of universe, stronger in higher dimensions).
    double calculateDarkMatterDensity(int dPrime) const {
        // Base density scaled by ~27% contribution, with gravitational enhancement in higher dimensions
        double density = kDarkMatter_ * (1.0 + static_cast<double>(dPrime) * invMaxDim_);
        // Add gravitational clustering effect for higher dimensions
        if (dPrime > 3) {
            density *= (1.0 + 0.1 * (dPrime - 3));  // Increased influence in higher dimensions
        }
        if (debug_) {
            std::cout << "DarkMatterDensity(D'=" << dPrime << "): " << density << std::endl;
        }
        return density;
    }
};

#endif // UNIVERSAL_EQUATION_HPP