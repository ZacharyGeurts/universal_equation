// universal_equation.hpp
// This header defines the UniversalEquation class, a precise mathematical model for calculating
// interactions between dimensions (1D to maxDimensions, default 9D). The model represents 1D as an
// infinite foundational layer, 2D as the boundary enclosing all dimensions, and higher dimensions (3D to 9D)
// embedded within lower ones. Interactions follow a pattern (1D to 9D, back to 1D, then to 2D) with a cycle
// length of 2*maxDimensions-1. Influence between dimensions decays exponentially with distance, and a collapse
// term introduces oscillatory dynamics. The model outputs dual values (positive and negative) representing
// energy fluctuations.

#ifndef UNIVERSAL_EQUATION_HPP
#define UNIVERSAL_EQUATION_HPP

#include <vector>
#include <cmath>
#include <utility>

class UniversalEquation {
public:
    // Constructor: Initializes the dimensional interaction model with configurable parameters.
    // Parameters control influence strength, dimensional interactions, and oscillatory behavior.
    UniversalEquation(
        int maxDimensions = 9,     // Maximum dimension (e.g., 9D)
        double influence = 1.0,    // Strength of 1D's foundational influence
        double weak = 0.5,         // Attenuation factor for interactions between dimensions > 3
        double collapse = 0.5,     // Amplitude of the collapse term
        double twoD = 0.5,         // Strength of 2D's boundary interaction
        double permeation = 2.0,   // Amplification for higher dimensions interacting with lower ones
        double alpha = 5.0,        // Exponential decay rate for interdimensional influence
        double beta = 0.2          // Modulation factor for collapse term scaling
    )
        : maxDimensions_(std::max(1, maxDimensions)), // Ensure at least 1D
          currentDimension_(1),
          kInfluence_(std::max(0.0, influence)),      // Non-negative influence
          kWeak_(std::max(0.0, weak)),                // Non-negative attenuation
          kCollapse_(std::max(0.0, collapse)),        // Non-negative collapse amplitude
          kTwoD_(std::max(0.0, twoD)),                // Non-negative 2D interaction
          kPermeation_(std::max(0.0, permeation)),    // Non-negative permeation
          alpha_(std::max(0.1, alpha)),              // Positive decay rate
          beta_(std::max(0.0, beta)) {               // Non-negative modulation
        initializeDimensions();
    }

    // Setters: Adjust model parameters for interdimensional interactions.
    void setInfluence(double value) { kInfluence_ = std::max(0.0, value); }
    void setWeak(double value) { kWeak_ = std::max(0.0, value); }
    void setCollapse(double value) { kCollapse_ = std::max(0.0, value); }
    void setTwoD(double value) { kTwoD_ = std::max(0.0, value); }
    void setPermeation(double value) { kPermeation_ = std::max(0.0, value); }
    void setAlpha(double value) { alpha_ = std::max(0.1, value); }
    void setBeta(double value) { beta_ = std::max(0.0, value); }

    // Getters: Retrieve current parameter values.
    double getInfluence() const { return kInfluence_; }
    double getWeak() const { return kWeak_; }
    double getCollapse() const { return kCollapse_; }
    double getTwoD() const { return kTwoD_; }
    double getPermeation() const { return kPermeation_; }
    double getAlpha() const { return alpha_; }
    double getBeta() const { return beta_; }

    // DimensionData: Stores data for interacting dimensions.
    struct DimensionData {
        int dPrime;       // Interacting dimension
        double distance;  // Dimensional separation
    };

    // calculateInfluenceTerm: Computes the interaction strength between dimensions d and dPrime.
    // 1D has a dominant influence when interacting with itself. Other interactions decay with distance.
    double calculateInfluenceTerm(int d, int dPrime) const {
        if (d == 1 && dPrime == 1) return kInfluence_;
        double distance = std::abs(d - dPrime);
        double denominator = std::pow(std::min(d, 10), std::min(dPrime, 10)); // Cap for stability
        double modifier = (d > 3 && dPrime > 3) ? kWeak_ : 1.0;
        if (denominator < 1e-10) return 0.0; // Prevent numerical instability
        return kInfluence_ * (distance / denominator) * modifier;
    }

    // calculatePermeationFactor: Amplifies interactions from a higher dimension to the one below
    // or from higher dimensions to 2D (the boundary).
    double calculatePermeationFactor(int d, int dPrime) const {
        if (d >= 3 && dPrime == d - 1) return kPermeation_;
        if (dPrime == 2 && d >= 3) return kTwoD_;
        return 1.0;
    }

    // calculateCollapseTerm: Models oscillatory dynamics for dimensions >= 2, modulated by dimension size.
    double calculateCollapseTerm(int d) const {
        double omega = 2.0 * M_PI / (2 * maxDimensions_ - 1); // Dynamic cycle length
        if (d == 1) return 0.0;
        return kCollapse_ * d * std::exp(-beta_ * (d - 1)) * std::abs(std::cos(omega * d));
    }

    // compute: Calculates the total dimensional influence for the current dimension, returning
    // dual values (positive and negative) representing energy fluctuations.
    std::pair<double, double> compute() const {
        double sphereInfluence = kInfluence_;
        if (currentDimension_ >= 2) {
            double omega = 2.0 * M_PI / (2 * maxDimensions_ - 1);
            sphereInfluence += kTwoD_ * std::cos(omega * currentDimension_);
        }
        for (const auto& data : dimensionPairs_) {
            sphereInfluence += calculateInfluenceTerm(currentDimension_, data.dPrime) *
                               std::exp(-alpha_ * data.distance) *
                               calculatePermeationFactor(currentDimension_, data.dPrime);
        }
        double collapse = calculateCollapseTerm(currentDimension_);
        return {sphereInfluence + collapse, sphereInfluence - collapse};
    }

    // setCurrentDimension: Sets the active dimension for computation.
    void setCurrentDimension(int dimension) {
        if (dimension >= 1 && dimension <= maxDimensions_) {
            currentDimension_ = dimension;
            updateDimensionPairs();
        }
    }

    // getCurrentDimension: Returns the active dimension.
    int getCurrentDimension() const { return currentDimension_; }

    // getMaxDimensions: Returns the maximum dimension.
    int getMaxDimensions() const { return maxDimensions_; }

private:
    // Model parameters
    int maxDimensions_;       // Maximum dimension
    int currentDimension_;    // Active dimension
    double kInfluence_;       // 1D influence strength
    double kWeak_;            // Attenuation for dimensions > 3
    double kCollapse_;        // Collapse term amplitude
    double kTwoD_;            // 2D boundary interaction strength
    double kPermeation_;      // Amplification for higher-to-lower dimension interactions
    double alpha_;            // Exponential decay rate
    double beta_;             // Collapse modulation factor
    std::vector<DimensionData> dimensionPairs_; // Nearby dimensions for interaction

    // initializeDimensions: Initializes the dimensional interaction data.
    void initializeDimensions() {
        updateDimensionPairs();
    }

    // updateDimensionPairs: Updates the list of interacting dimensions (D-1, D, D+1, and 2D for d >= 3).
    void updateDimensionPairs() {
        dimensionPairs_.clear();
        int start = std::max(1, currentDimension_ - 1);
        int end = std::min(maxDimensions_, currentDimension_ + 1);
        for (int dPrime = start; dPrime <= end; ++dPrime) {
            dimensionPairs_.push_back({dPrime, static_cast<double>(std::abs(currentDimension_ - dPrime))});
        }
        if (currentDimension_ >= 3 && currentDimension_ <= maxDimensions_) {
            dimensionPairs_.push_back({2, static_cast<double>(currentDimension_ - 2)});
        }
    }
};

#endif // UNIVERSAL_EQUATION_HPP
