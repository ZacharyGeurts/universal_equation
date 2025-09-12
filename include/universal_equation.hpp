#ifndef UNIVERSAL_EQUATION_HPP
#define UNIVERSAL_EQUATION_HPP

#include <vector>
#include <cmath>
#include <functional>
#include <algorithm>
#include <iostream>
#include <string>

// UniversalEquation models dimensional interactions from 1D (foundational layer, representing a universal constant or "God" inside/outside all dimensions)
// to maxDimensions (default 9D). 2D acts as a boundary, and higher dimensions embed lower ones.
// Incorporates dark matter as a stabilizing force and dark energy as an expansion driver.
// Interactions follow a cycle (1D to 9D, back to 1D, then to 2D) with exponential decay and oscillatory dynamics.
// Optimized for performance with precomputed constants, minimal allocations, and modular design.
class UniversalEquation {
public:
    // Structure to hold energy fluctuation results with interpretation.
    struct EnergyFluctuations {
        double positive;  // Positive energy fluctuation (e.g., observable energy)
        double negative;  // Negative energy fluctuation (e.g., potential energy sinks)
        double darkMatterContribution;  // Contribution from dark matter stabilization
        double darkEnergyContribution;  // Contribution from dark energy expansion
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

    // Constructor initializes the model with parameters for influence, dark matter, dark energy, and oscillatory behavior.
    UniversalEquation(int maxDimensions = 9,
                     double influence = 1.0,
                     double weak = 0.5,
                     double collapse = 0.5,
                     double twoD = 0.5,
                     double permeation = 2.0,
                     double darkMatterStrength = 0.3,  // Dark matter coupling strength
                     double darkEnergyScale = 0.7,     // Dark energy expansion scale
                     double alpha = 5.0,
                     double beta = 0.2,
                     bool debug = false)
        : maxDimensions_(std::max(1, maxDimensions)),
          currentDimension_(1),
          kInfluence_(std::clamp(influence, 0.0, 10.0)),
          kWeak_(std::clamp(weak, 0.0, 1.0)),
          kCollapse_(std::clamp(collapse, 0.0, 5.0)),
          kTwoD_(std::clamp(twoD, 0.0, 5.0)),
          kPermeation_(std::clamp(permeation, 0.0, 5.0)),
          kDarkMatter_(std::clamp(darkMatterStrength, 0.0, 1.0)),  // Constrain dark matter strength
          kDarkEnergy_(std::clamp(darkEnergyScale, 0.0, 2.0)),     // Constrain dark energy scale
          alpha_(std::clamp(alpha, 0.1, 10.0)),
          beta_(std::clamp(beta, 0.0, 1.0)),
          debug_(debug),
          omega_(2.0 * M_PI / static_cast<double>(2 * maxDimensions_ - 1)),
          cycleLength_(2.0 * maxDimensions_),
          invMaxDim_(1.0 / maxDimensions_) {  // Precompute inverse for efficiency
        dimensionPairs_.reserve(maxDimensions_);  // Pre-allocate to avoid reallocations
        updateDimensionPairs();
        if (debug_) {
            std::cout << "Initialized UniversalEquation with maxDimensions=" << maxDimensions_
                      << ", darkMatterStrength=" << kDarkMatter_ 
                      << ", darkEnergyScale=" << kDarkEnergy_ << std::endl;
        }
    }

    // Setters for parameters, ensuring physical constraints.
    void setInfluence(double value) { kInfluence_ = std::clamp(value, 0.0, 10.0); }
    void setWeak(double value) { kWeak_ = std::clamp(value, 0.0, 1.0); }
    void setCollapse(double value) { kCollapse_ = std::clamp(value, 0.0, 5.0); }
    void setTwoD(double value) { kTwoD_ = std::clamp(value, 0.0, 5.0); }
    void setPermeation(double value) { kPermeation_ = std::clamp(value, 0.0, 5.0); }
    void setDarkMatterStrength(double value) { 
        kDarkMatter_ = std::clamp(value, 0.0, 1.0); 
        updateDimensionPairs();  // Recalculate dark matter densities
    }
    void setDarkEnergyScale(double value) { 
        kDarkEnergy_ = std::clamp(value, 0.0, 2.0); 
        updateDimensionPairs();  // Recalculate distances
    }
    void setAlpha(double value) { alpha_ = std::clamp(value, 0.1, 10.0); }
    void setBeta(double value) { beta_ = std::clamp(value, 0.0, 1.0); }
    void setDebug(bool enable) { debug_ = enable; }

    // Getters for accessing parameters and state.
    double getInfluence() const { return kInfluence_; }
    double getWeak() const { return kWeak_; }
    double getCollapse() const { return kCollapse_; }
    double getTwoD() const { return kTwoD_; }
    double getPermeation() const { return kPermeation_; }
    double getDarkMatterStrength() const { return kDarkMatter_; }
    double getDarkEnergyScale() const { return kDarkEnergy_; }
    double getAlpha() const { return alpha_; }
    double getBeta() const { return beta_; }
    int getCurrentDimension() const { return currentDimension_; }
    int getMaxDimensions() const { return maxDimensions_; }
    std::vector<DimensionData> getDimensionPairs() const { return dimensionPairs_; }

    // Advances the dimensional cycle (1D -> 9D -> 1D -> 2D).
    void advanceCycle() {
        if (currentDimension_ == maxDimensions_) {
            currentDimension_ = 1;
        } else if (currentDimension_ == 1) {
            currentDimension_ = 2;
        } else {
            currentDimension_++;
        }
        updateDimensionPairs();
        if (debug_) {
            std::cout << "Advanced to dimension: " << currentDimension_ << std::endl;
        }
    }

    // Sets the active dimension, ensuring it's within bounds.
    void setCurrentDimension(int dimension) {
        if (dimension >= 1 && dimension <= maxDimensions_) {
            currentDimension_ = dimension;
            updateDimensionPairs();
            if (debug_) {
                std::cout << "Set current dimension to: " << currentDimension_ << std::endl;
            }
        }
    }

    // Computes energy fluctuations, incorporating dark matter and dark energy.
    EnergyFluctuations compute() const {
        // Base influence includes 2D boundary oscillation
        double sphereInfluence = kInfluence_;
        if (currentDimension_ >= 2) {
            sphereInfluence += kTwoD_ * std::cos(omega_ * currentDimension_);
        }

        // Sum contributions from dimensional interactions
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

        // Apply collapse term
        double collapse = calculateCollapseTerm();
        EnergyFluctuations result = {
            sphereInfluence + collapse,                    // Positive energy
            std::max(0.0, sphereInfluence - collapse),     // Negative energy
            totalDarkMatter,                               // Dark matter contribution
            totalDarkEnergy                                // Dark energy contribution
        };

        if (debug_) {
            std::cout << "Compute(D=" << currentDimension_ << "): TotalInfluence=" << totalInfluence
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
        if (dPrime == currentDimension_ + 1 || dPrime == 1) return kPermeation_;
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
    int maxDimensions_;           // Maximum number of dimensions
    int currentDimension_;        // Current active dimension
    double kInfluence_;           // Base influence strength
    double kWeak_;                // Weak interaction modifier for higher dimensions
    double kCollapse_;            // Collapse strength
    double kTwoD_;                // 2D boundary strength
    double kPermeation_;          // Permeation factor for specific interactions
    double kDarkMatter_;          // Dark matter coupling strength
    double kDarkEnergy_;          // Dark energy expansion scale
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

    // Updates dimension pairs with dark matter and dark energy effects.
    void updateDimensionPairs() {
        dimensionPairs_.clear();
        int start = std::max(1, currentDimension_ - 1);
        int end = std::min(maxDimensions_, currentDimension_ + 1);
        for (int dPrime = start; dPrime <= end; ++dPrime) {
            double baseDistance = static_cast<double>(std::abs(currentDimension_ - dPrime));
            double adjustedDistance = baseDistance * (1.0 + kDarkEnergy_ * invMaxDim_);
            double darkMatterDensity = calculateDarkMatterDensity(dPrime);
            dimensionPairs_.push_back({dPrime, adjustedDistance, darkMatterDensity});
        }
        for (int priv : {1, 2}) {
            if (priv != currentDimension_ && priv <= maxDimensions_) {
                double baseDistance = static_cast<double>(std::abs(currentDimension_ - priv));
                double adjustedDistance = baseDistance * (1.0 + kDarkEnergy_ * invMaxDim_);
                double darkMatterDensity = calculateDarkMatterDensity(priv);
                dimensionPairs_.push_back({priv, adjustedDistance, darkMatterDensity});
            }
        }
        if (debug_) {
            std::cout << "Updated pairs for D=" << currentDimension_ << ": ";
            for (const auto& pair : dimensionPairs_) {
                std::cout << "(D'=" << pair.dPrime << ", dist=" << pair.distance 
                          << ", DM=" << pair.darkMatterDensity << ") ";
            }
            std::cout << std::endl;
        }
    }

    // Calculates dark matter density, stronger in higher dimensions.
    double calculateDarkMatterDensity(int dPrime) const {
        double density = kDarkMatter_ * (1.0 + static_cast<double>(dPrime) * invMaxDim_);
        if (debug_) {
            std::cout << "DarkMatterDensity(D'=" << dPrime << "): " << density << std::endl;
        }
        return density;
    }
};

#endif // UNIVERSAL_EQUATION_HPP