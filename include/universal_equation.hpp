#ifndef UNIVERSAL_EQUATION_HPP
#define UNIVERSAL_EQUATION_HPP

#include <vector>
#include <cmath>
#include <functional>
#include <algorithm>
#include <iostream>
#include <string>

// UniversalEquation models dimensional interactions from 1D (foundational layer, representing a universal constant or "God" inside/outside all dimensions)
// to maxDimensions (default 9D). 2D acts as a boundary, and higher dimensions embed lower ones, including 1D and 2D.
// Interactions follow a cycle (1D to 9D, back to 1D, then to 2D) with exponential decay and oscillatory dynamics.
// Optimized for frequent calls with precomputed constants and minimal allocations.
class UniversalEquation {
public:
    // Structure to hold energy fluctuation results with interpretation.
    struct EnergyFluctuations {
        double positive;  // Positive energy fluctuation
        double negative;  // Negative energy fluctuation
        std::string interpretation() const {
            return "Positive: " + std::to_string(positive) + ", Negative: " + std::to_string(negative);
        }
    };

    // Structure to hold dimension interaction data (made public for access).
    struct DimensionData {
        int dPrime;       // Interacting dimension
        double distance;  // Dimensional separation
    };

    // Initializes the model with parameters for influence, attenuation, and oscillatory behavior.
    UniversalEquation(int maxDimensions = 9,
                     double influence = 1.0,
                     double weak = 0.5,
                     double collapse = 0.5,
                     double twoD = 0.5,
                     double permeation = 2.0,
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
          alpha_(std::clamp(alpha, 0.1, 10.0)),
          beta_(std::clamp(beta, 0.0, 1.0)),
          debug_(debug),
          omega_(2.0 * M_PI / static_cast<double>(2 * maxDimensions_ - 1)),
          cycleLength_(2.0 * maxDimensions_) {
        dimensionPairs_.reserve(maxDimensions_);
        updateDimensionPairs();
    }

    // Setters
    void setInfluence(double value) { kInfluence_ = std::clamp(value, 0.0, 10.0); }
    void setWeak(double value) { kWeak_ = std::clamp(value, 0.0, 1.0); }
    void setCollapse(double value) { kCollapse_ = std::clamp(value, 0.0, 5.0); }
    void setTwoD(double value) { kTwoD_ = std::clamp(value, 0.0, 5.0); }
    void setPermeation(double value) { kPermeation_ = std::clamp(value, 0.0, 5.0); }
    void setAlpha(double value) { alpha_ = std::clamp(value, 0.1, 10.0); }
    void setBeta(double value) { beta_ = std::clamp(value, 0.0, 1.0); }
    void setDebug(bool enable) { debug_ = enable; }

    // Getters
    double getInfluence() const { return kInfluence_; }
    double getWeak() const { return kWeak_; }
    double getCollapse() const { return kCollapse_; }
    double getTwoD() const { return kTwoD_; }
    double getPermeation() const { return kPermeation_; }
    double getAlpha() const { return alpha_; }
    double getBeta() const { return beta_; }
    int getCurrentDimension() const { return currentDimension_; }
    int getMaxDimensions() const { return maxDimensions_; }
    std::vector<DimensionData> getDimensionPairs() const { return dimensionPairs_; }

    // Advances the dimensional cycle
    void advanceCycle() {
        if (currentDimension_ == maxDimensions_) {
            currentDimension_ = 1;
        } else if (currentDimension_ == 1) {
            currentDimension_ = 2;
        } else {
            currentDimension_++;
        }
        updateDimensionPairs();
    }

    // Sets the active dimension
    void setCurrentDimension(int dimension) {
        if (dimension >= 1 && dimension <= maxDimensions_) {
            currentDimension_ = dimension;
            updateDimensionPairs();
        }
    }

    // Computes energy fluctuations
    EnergyFluctuations compute() const {
        double sphereInfluence = kInfluence_;
        if (currentDimension_ >= 2) {
            sphereInfluence += kTwoD_ * std::cos(omega_ * currentDimension_);
        }

        double totalInfluence = 0.0;
        for (const auto& data : dimensionPairs_) {
            double influence = calculateInfluenceTerm(data.dPrime, data.distance);
            totalInfluence += influence * std::exp(-alpha_ * data.distance) *
                              calculatePermeationFactor(data.dPrime);
        }
        sphereInfluence += totalInfluence;

        double collapse = calculateCollapseTerm();
        EnergyFluctuations result = {sphereInfluence + collapse, std::max(0.0, sphereInfluence - collapse)};
        if (debug_) {
            std::cout << "Compute(D=" << currentDimension_ << "): TotalInfluence=" << totalInfluence
                      << ", Collapse=" << collapse << ", " << result.interpretation() << std::endl;
        }
        return result;
    }

    // Public methods for interaction calculations
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

    double calculatePermeationFactor(int dPrime) const {
        if (currentDimension_ == 2 && dPrime > currentDimension_) return kTwoD_;
        if (dPrime == currentDimension_ + 1 || dPrime == 1) return kPermeation_;
        return 1.0;
    }

private:
    int maxDimensions_;
    int currentDimension_;
    double kInfluence_;
    double kWeak_;
    double kCollapse_;
    double kTwoD_;
    double kPermeation_;
    double alpha_;
    double beta_;
    bool debug_;
    double omega_;
    double cycleLength_;
    std::vector<DimensionData> dimensionPairs_;

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

    void updateDimensionPairs() {
        dimensionPairs_.clear();
        int start = std::max(1, currentDimension_ - 1);
        int end = std::min(maxDimensions_, currentDimension_ + 1);
        for (int dPrime = start; dPrime <= end; ++dPrime) {
            dimensionPairs_.push_back({dPrime, static_cast<double>(std::abs(currentDimension_ - dPrime))});
        }
        for (int priv : {1, 2}) {
            if (priv != currentDimension_ && priv <= maxDimensions_) {
                dimensionPairs_.push_back({priv, static_cast<double>(std::abs(currentDimension_ - priv))});
            }
        }
        if (debug_) {
            std::cout << "Updated pairs for D=" << currentDimension_ << ": ";
            for (const auto& pair : dimensionPairs_) {
                std::cout << "(" << pair.dPrime << ", " << pair.distance << ") ";
            }
            std::cout << std::endl;
        }
    }
};

#endif // UNIVERSAL_EQUATION_HPP