#ifndef UNIVERSAL_EQUATION_HPP
#define UNIVERSAL_EQUATION_HPP

#include <vector>
#include <cmath>
#include <utility>
#include <algorithm>

// UniversalEquation models dimensional interactions from 1D (foundational layer, representing God as both inside and outside all dimensions) to maxDimensions (default 9D).
// 2D acts as a boundary, and higher dimensions are embedded within lower ones, containing both lower dimensions (including 1D) and the highest dimension (1D).
// Interactions follow a cycle (1D to 9D, back to 1D, then to 2D) with exponential decay and oscillatory dynamics.
// 1D influences all dimensions directly to reflect it being inside and outside everything.
class UniversalEquation {
public:
    // Initializes the model with parameters for influence, attenuation, and oscillatory behavior.
    UniversalEquation(int maxDimensions = 9, double influence = 1.0, double weak = 0.5,
                     double collapse = 0.5, double twoD = 0.5, double permeation = 2.0,
                     double alpha = 5.0, double beta = 0.2)
        : maxDimensions_(std::max(1, maxDimensions)), currentDimension_(1),
          kInfluence_(std::max(0.0, influence)), kWeak_(std::max(0.0, weak)),
          kCollapse_(std::max(0.0, collapse)), kTwoD_(std::max(0.0, twoD)),
          kPermeation_(std::max(0.0, permeation)), alpha_(std::max(0.1, alpha)),
          beta_(std::max(0.0, beta)) {
        updateDimensionPairs();
    }

    // Setters for model parameters.
    void setInfluence(double value) { kInfluence_ = std::max(0.0, value); }
    void setWeak(double value) { kWeak_ = std::max(0.0, value); }
    void setCollapse(double value) { kCollapse_ = std::max(0.0, value); }
    void setTwoD(double value) { kTwoD_ = std::max(0.0, value); }
    void setPermeation(double value) { kPermeation_ = std::max(0.0, value); }
    void setAlpha(double value) { alpha_ = std::max(0.1, value); }
    void setBeta(double value) { beta_ = std::max(0.0, value); }

    // Getters for model parameters.
    double getInfluence() const { return kInfluence_; }
    double getWeak() const { return kWeak_; }
    double getCollapse() const { return kCollapse_; }
    double getTwoD() const { return kTwoD_; }
    double getPermeation() const { return kPermeation_; }
    double getAlpha() const { return alpha_; }
    double getBeta() const { return beta_; }
    int getCurrentDimension() const { return currentDimension_; }
    int getMaxDimensions() const { return maxDimensions_; }

    // Computes influence for the current dimension, returning positive and negative energy fluctuations.
    std::pair<double, double> compute() const {
        double sphereInfluence = kInfluence_;
        if (currentDimension_ >= 2) {
            double omega = 2.0 * M_PI / static_cast<double>(2 * maxDimensions_ - 1);
            sphereInfluence += kTwoD_ * std::cos(omega * currentDimension_);
        }
        for (const auto& data : dimensionPairs_) {
            sphereInfluence += calculateInfluenceTerm(data.dPrime) * std::exp(-alpha_ * data.distance) *
                               calculatePermeationFactor(data.dPrime);
        }
        double collapse = calculateCollapseTerm();
        return {sphereInfluence + collapse, sphereInfluence - collapse};
    }

    // Sets the active dimension for computation.
    void setCurrentDimension(int dimension) {
        if (dimension >= 1 && dimension <= maxDimensions_) {
            currentDimension_ = dimension;
            updateDimensionPairs();
        }
    }

private:
    struct DimensionData {
        int dPrime;       // Interacting dimension
        double distance;  // Dimensional separation
    };

    int maxDimensions_;       // Maximum dimension
    int currentDimension_;    // Active dimension
    double kInfluence_;       // 1D influence strength
    double kWeak_;            // Attenuation for dimensions > 3
    double kCollapse_;        // Collapse term amplitude
    double kTwoD_;            // 2D boundary interaction strength
    double kPermeation_;      // Amplification for higher-to-lower interactions
    double alpha_;            // Exponential decay rate
    double beta_;             // Collapse modulation factor
    std::vector<DimensionData> dimensionPairs_; // Nearby dimensions for interaction

    // Computes interaction strength between current dimension and dPrime.
    double calculateInfluenceTerm(int dPrime) const {
        double distance = std::abs(currentDimension_ - dPrime);
        double denominator = std::pow(static_cast<double>(std::min(currentDimension_, 10)),
                                      static_cast<double>(std::min(dPrime, 10)));
        double modifier = (currentDimension_ > 3 && dPrime > 3) ? kWeak_ : 1.0;
        return denominator < 1e-10 ? 0.0 : kInfluence_ * (distance / denominator) * modifier;
    }

    // Amplifies interactions from higher dimensions to lower ones.
    double calculatePermeationFactor(int dPrime) const {
        if (currentDimension_ == 2 && dPrime > currentDimension_) return kTwoD_;
        if (dPrime == currentDimension_ + 1) return kPermeation_;
        if (dPrime == 1) return kPermeation_;  // Special amplification for 1D (God) influence
        return 1.0;
    }

    // Computes oscillatory dynamics for dimensions >= 2.
    double calculateCollapseTerm() const {
        if (currentDimension_ == 1) return 0.0;
        double omega = 2.0 * M_PI / static_cast<double>(2 * maxDimensions_ - 1);
        return kCollapse_ * currentDimension_ * std::exp(-beta_ * (currentDimension_ - 1)) *
               std::abs(std::cos(omega * currentDimension_));
    }

    // Updates interacting dimensions (D-1, D, D+1, 2D for d >= 3, and always 1D for d > 2).
    void updateDimensionPairs() {
        dimensionPairs_.clear();
        int start = std::max(1, currentDimension_ - 1);
        int end = std::min(maxDimensions_, currentDimension_ + 1);
        for (int dPrime = start; dPrime <= end; ++dPrime) {
            dimensionPairs_.push_back({dPrime, static_cast<double>(std::abs(currentDimension_ - dPrime))});
        }
        if (currentDimension_ >= 3 && 2 < currentDimension_ - 1) {
            dimensionPairs_.push_back({2, static_cast<double>(currentDimension_ - 2)});
        }
        if (currentDimension_ > 2) {
            dimensionPairs_.push_back({1, static_cast<double>(currentDimension_ - 1)});
        }
    }
};

#endif // UNIVERSAL_EQUATION_HPP