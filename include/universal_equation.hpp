#ifndef UNIVERSAL_EQUATION_HPP
#define UNIVERSAL_EQUATION_HPP

#include <vector>
#include <cmath>
#include <string>
#include <limits>
#include <iostream>
#include <algorithm>

class UniversalEquation {
public:
    struct EnergyResult {
        double observable;
        double potential;
        double darkMatter;
        double darkEnergy;
        std::string toString() const {
            return "Observable: " + std::to_string(observable) +
                   ", Potential: " + std::to_string(potential) +
                   ", Dark Matter: " + std::to_string(darkMatter) +
                   ", Dark Energy: " + std::to_string(darkEnergy);
        }
    };

    struct DimensionInteraction {
        int dimension;
        double distance;
        double darkMatterDensity;
    };

    UniversalEquation(int maxDimensions = 9, int mode = 1, double influence = 1.0,
                     double weak = 0.5, double collapse = 0.5, double twoD = 0.5,
                     double threeDInfluence = 1.5, double oneDPermeation = 2.0,
                     double darkMatterStrength = 0.27, double darkEnergyStrength = 0.68,
                     double alpha = 5.0, double beta = 0.2, bool debug = false)
        : maxDimensions_(std::max(1, maxDimensions)),
          currentDimension_(std::clamp(mode, 1, maxDimensions_)),
          mode_(std::clamp(mode, 1, maxDimensions_)),
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
          omega_(2.0 * M_PI / (2 * maxDimensions_ - 1)),
          invMaxDim_(maxDimensions_ > 0 ? 1.0 / maxDimensions_ : 1e-15),
          interactions_() {
        if (maxDimensions_ == std::numeric_limits<int>::max()) {
            maxDimensions_ = 9;
            invMaxDim_ = 1.0 / maxDimensions_;
        }
        updateInteractions();
        if (debug_) {
            std::cout << "Initialized: maxDimensions=" << maxDimensions_ << ", mode=" << mode_
                      << ", currentDimension=" << currentDimension_ << "\n";
        }
    }

    // --- Getter and Setter Methods: ---
    void setInfluence(double value) { influence_ = std::clamp(value, 0.0, 10.0); }
    double getInfluence() const { return influence_; }

    void setWeak(double value) { weak_ = std::clamp(value, 0.0, 1.0); }
    double getWeak() const { return weak_; }

    void setCollapse(double value) { collapse_ = std::clamp(value, 0.0, 5.0); }
    double getCollapse() const { return collapse_; }

    void setTwoD(double value) { twoD_ = std::clamp(value, 0.0, 5.0); }
    double getTwoD() const { return twoD_; }

    void setThreeDInfluence(double value) { threeDInfluence_ = std::clamp(value, 0.0, 5.0); }
    double getThreeDInfluence() const { return threeDInfluence_; }

    void setOneDPermeation(double value) { oneDPermeation_ = std::clamp(value, 0.0, 5.0); }
    double getOneDPermeation() const { return oneDPermeation_; }

    void setDarkMatterStrength(double value) {
        darkMatterStrength_ = std::clamp(value, 0.0, 1.0);
        updateInteractions();
    }
    double getDarkMatterStrength() const { return darkMatterStrength_; }

    void setDarkEnergyStrength(double value) {
        darkEnergyStrength_ = std::clamp(value, 0.0, 2.0);
        updateInteractions();
    }
    double getDarkEnergyStrength() const { return darkEnergyStrength_; }

    void setAlpha(double value) { alpha_ = std::clamp(value, 0.1, 10.0); }
    double getAlpha() const { return alpha_; }

    void setBeta(double value) { beta_ = std::clamp(value, 0.0, 1.0); }
    double getBeta() const { return beta_; }

    void setDebug(bool value) { debug_ = value; }
    bool getDebug() const { return debug_; }

    void setMode(int mode) {
        mode_ = std::clamp(mode, 1, maxDimensions_);
        currentDimension_ = mode_;
        updateInteractions();
        if (debug_) {
            std::cout << "Mode set to: " << mode_ << ", dimension: " << currentDimension_ << "\n";
        }
    }
    int getMode() const { return mode_; }

    void setCurrentDimension(int dimension) {
        if (dimension >= 1 && (dimension <= maxDimensions_)) {
            currentDimension_ = dimension;
            mode_ = dimension;
            updateInteractions();
            if (debug_) {
                std::cout << "Dimension set to: " << currentDimension_ << ", mode: " << mode_ << "\n";
            }
        }
    }
    int getCurrentDimension() const { return currentDimension_; }

    int getMaxDimensions() const { return maxDimensions_; }

    std::vector<DimensionInteraction> getInteractions() const { return interactions_; }

    // --- End of Getter and Setter Methods ---

    void advanceCycle() {
        if (currentDimension_ == maxDimensions_) {
            currentDimension_ = 1;
            mode_ = 1;
        } else if (currentDimension_ == 1) {
            currentDimension_ = 2;
            mode_ = 2;
        } else {
            currentDimension_++;
            mode_ = currentDimension_;
        }
        updateInteractions();
        if (debug_) {
            std::cout << "Cycle advanced: dimension=" << currentDimension_ << ", mode=" << mode_ << "\n";
        }
    }

    EnergyResult compute() const {
        double totalInfluence = influence_;
        if (currentDimension_ >= 2) {
            totalInfluence += twoD_ * std::cos(omega_ * currentDimension_);
        }
        if (currentDimension_ == 3) {
            totalInfluence += threeDInfluence_;
        }

        double totalDarkMatter = 0.0, totalDarkEnergy = 0.0, interactionSum = 0.0;
        for (const auto& interaction : interactions_) {
            double influence = computeInteraction(interaction.dimension, interaction.distance);
            double darkMatter = interaction.darkMatterDensity;
            double darkEnergy = computeDarkEnergy(interaction.distance);
            interactionSum += influence * std::exp(-alpha_ * interaction.distance) *
                              computePermeation(interaction.dimension) * darkMatter;
            totalDarkMatter += darkMatter * influence;
            totalDarkEnergy += darkEnergy * influence;
        }
        totalInfluence += interactionSum;

        double collapse = computeCollapse();
        EnergyResult result = {
            totalInfluence + collapse,
            std::max(0.0, totalInfluence - collapse),
            totalDarkMatter,
            totalDarkEnergy
        };

        if (debug_) {
            std::cout << "Compute(D=" << currentDimension_ << "): " << result.toString() << "\n";
        }
        return result;
    }

private:
    int maxDimensions_, currentDimension_, mode_;
    double influence_, weak_, collapse_, twoD_, threeDInfluence_, oneDPermeation_;
    double darkMatterStrength_, darkEnergyStrength_, alpha_, beta_;
    bool debug_;
    double omega_, invMaxDim_;
    std::vector<DimensionInteraction> interactions_;

    double computeInteraction(int dimension, double distance) const {
        double denom = std::max(1e-15, std::pow(static_cast<double>(currentDimension_), dimension));
        double modifier = (currentDimension_ > 3 && dimension > 3) ? weak_ : 1.0;
        if (currentDimension_ == 3 && (dimension == 2 || dimension == 4)) {
            modifier *= threeDInfluence_;
        }
        double result = influence_ * (1.0 / (denom * (1.0 + distance))) * modifier;
        if (debug_) {
            std::cout << "Interaction(D=" << dimension << ", dist=" << distance << "): " << result << "\n";
        }
        return result;
    }

    double computePermeation(int dimension) const {
        if (dimension == 1 || currentDimension_ == 1) return 1.0;
        if (currentDimension_ == 2 && dimension > 2) return twoD_;
        if (currentDimension_ == 3 && (dimension == 2 || dimension == 4)) return threeDInfluence_;
        return 1.0;
    }

    double computeCollapse() const {
        if (currentDimension_ == 1) return 0.0;
        double phase = static_cast<double>(currentDimension_) / (2 * maxDimensions_);
        double osc = std::abs(std::cos(2.0 * M_PI * phase));
        double result = std::max(0.0, collapse_ * currentDimension_ * std::exp(-beta_ * (currentDimension_ - 1)) * (0.8 * osc + 0.2));
        if (debug_) {
            std::cout << "Collapse(D=" << currentDimension_ << "): " << result << "\n";
        }
        return result;
    }

    double computeDarkEnergy(double distance) const {
        double d = std::min(distance, 10.0);
        double result = darkEnergyStrength_ * std::exp(d * invMaxDim_);
        if (debug_) {
            std::cout << "DarkEnergy(dist=" << distance << "): " << result << "\n";
        }
        return result;
    }

    double computeDarkMatterDensity(int dimension) const {
        double density = darkMatterStrength_ * (1.0 + dimension * invMaxDim_);
        if (dimension > 3) {
            density *= (1.0 + 0.1 * (dimension - 3));
        }
        if (debug_) {
            std::cout << "DarkMatter(D=" << dimension << "): " << density << "\n";
        }
        return std::max(1e-15, density);
    }

    void updateInteractions() {
        interactions_.clear();
        int interactionLimit = (maxDimensions_ == 9 && maxDimensions_ == std::numeric_limits<int>::max()) ? 9 : maxDimensions_;
        interactions_.reserve(interactionLimit + 2);
        int start = std::max(1, currentDimension_ - 1);
        int end = std::min(interactionLimit, currentDimension_ + 1);
        for (int d = start; d <= end; ++d) {
            double distance = std::abs(currentDimension_ - d) * (1.0 + darkEnergyStrength_ * invMaxDim_);
            interactions_.push_back({d, distance, computeDarkMatterDensity(d)});
        }
        for (int priv : {1, 2}) {
            if (priv != currentDimension_ && priv <= maxDimensions_) {
                double distance = std::abs(currentDimension_ - priv) * (1.0 + darkEnergyStrength_ * invMaxDim_);
                interactions_.push_back({priv, distance, computeDarkMatterDensity(priv)});
            }
        }
        if (currentDimension_ == 3 && maxDimensions_ >= 4) {
            for (int adj : {2, 4}) {
                if (std::none_of(interactions_.begin(), interactions_.end(),
                                 [adj](const auto& i) { return i.dimension == adj; })) {
                    double distance = std::abs(currentDimension_ - adj) * (1.0 + darkEnergyStrength_ * invMaxDim_);
                    interactions_.push_back({adj, distance, computeDarkMatterDensity(adj)});
                }
            }
        }
        if (debug_) {
            std::cout << "Interactions(D=" << currentDimension_ << "): ";
            for (const auto& i : interactions_) {
                std::cout << "(D=" << i.dimension << ", dist=" << i.distance << ", DM=" << i.darkMatterDensity << ") ";
            }
            std::cout << "\n";
        }
    }
};

#endif // UNIVERSAL_EQUATION_HPP