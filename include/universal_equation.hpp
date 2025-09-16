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
        int vertexIndex;
        double distance;
        double strength;
    };

    UniversalEquation(int maxDimensions = 9, int mode = 1, double influence = 1.0,
                     double weak = 0.5, double collapse = 0.5, double twoD = 0.5,
                     double threeDInfluence = 1.5, double oneDPermeation = 2.0,
                     double darkMatterStrength = 0.27, double darkEnergyStrength = 0.68,
                     double alpha = 5.0, double beta = 0.2, bool debug = false)
        : maxDimensions_(std::max(1, std::min(9, maxDimensions))),
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
          omega_(2.0 * M_PI / (2 * maxDimensions_ - 1)),  // Precomputed omega (angular frequency for oscillations)
          invMaxDim_(maxDimensions_ > 0 ? 1.0 / maxDimensions_ : 1e-15),  // Precomputed inverse max dimensions for scaling
          interactions_() {
        initializeNCube();
        updateInteractions();
        if (debug_) {
            std::cout << "Initialized: maxDimensions=" << maxDimensions_ << ", mode=" << mode_
                      << ", currentDimension=" << currentDimension_ << "\n";
        }
    }

    void setInfluence(double value) { influence_ = std::clamp(value, 0.0, 10.0); updateInteractions(); }
    double getInfluence() const { return influence_; }

    void setWeak(double value) { weak_ = std::clamp(value, 0.0, 1.0); updateInteractions(); }
    double getWeak() const { return weak_; }

    void setCollapse(double value) { collapse_ = std::clamp(value, 0.0, 5.0); }
    double getCollapse() const { return collapse_; }

    void setTwoD(double value) { twoD_ = std::clamp(value, 0.0, 5.0); updateInteractions(); }
    double getTwoD() const { return twoD_; }

    void setThreeDInfluence(double value) { threeDInfluence_ = std::clamp(value, 0.0, 5.0); updateInteractions(); }
    double getThreeDInfluence() const { return threeDInfluence_; }

    void setOneDPermeation(double value) { oneDPermeation_ = std::clamp(value, 0.0, 5.0); updateInteractions(); }
    double getOneDPermeation() const { return oneDPermeation_; }

    void setDarkMatterStrength(double value) { darkMatterStrength_ = std::clamp(value, 0.0, 1.0); updateInteractions(); }
    double getDarkMatterStrength() const { return darkMatterStrength_; }

    void setDarkEnergyStrength(double value) { darkEnergyStrength_ = std::clamp(value, 0.0, 2.0); updateInteractions(); }
    double getDarkEnergyStrength() const { return darkEnergyStrength_; }

    void setAlpha(double value) { alpha_ = std::clamp(value, 0.1, 10.0); updateInteractions(); }
    double getAlpha() const { return alpha_; }

    void setBeta(double value) { beta_ = std::clamp(value, 0.0, 1.0); updateInteractions(); }
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
        if (dimension >= 1 && dimension <= maxDimensions_) {
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

    void advanceCycle() {
        if (currentDimension_ == maxDimensions_) {
            currentDimension_ = 1;
            mode_ = 1;
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
        double observable = influence_;
        if (currentDimension_ >= 2) {
            observable += twoD_ * std::cos(omega_ * currentDimension_);
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
            interactionSum += influence * std::exp(-alpha_ * interaction.distance) * permeation * darkMatter;
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

        if (debug_) {
            std::cout << "Compute(D=" << currentDimension_ << "): " << result.toString() << "\n";
        }
        return result;
    }

    // Moved to public for access in renderMode1
    double computePermeation(int vertexIndex) const {
        if (vertexIndex == 1 || currentDimension_ == 1) return oneDPermeation_;
        if (currentDimension_ == 2 && (vertexIndex % maxDimensions_ + 1) > 2) return twoD_;
        if (currentDimension_ == 3 && ((vertexIndex % maxDimensions_ + 1) == 2 || (vertexIndex % maxDimensions_ + 1) == 4)) return threeDInfluence_;
        std::vector<double> vertex = nCubeVertices_[vertexIndex % nCubeVertices_.size()];
        double vertexMagnitude = 0.0;
        for (int i = 0; i < currentDimension_; ++i) {
            vertexMagnitude += vertex[i] * vertex[i];
        }
        vertexMagnitude = std::sqrt(vertexMagnitude);
        double result = 1.0 + beta_ * vertexMagnitude / currentDimension_;
        if (debug_) {
            std::cout << "Permeation(vertex=" << vertexIndex << "): " << result << "\n";
        }
        return result;
    }

    // Moved to public for access in renderMode1
    double computeDarkEnergy(double distance) const {
        double d = std::min(distance, 10.0);
        double result = darkEnergyStrength_ * std::exp(d * invMaxDim_);
        if (debug_) {
            std::cout << "DarkEnergy(dist=" << distance << "): " << result << "\n";
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
    std::vector<std::vector<double>> nCubeVertices_;

    void initializeNCube() {
        nCubeVertices_.clear();
        int numVertices = 1 << currentDimension_;
        for (int i = 0; i < numVertices; ++i) {
            std::vector<double> vertex(currentDimension_, 0.0);
            for (int j = 0; j < currentDimension_; ++j) {
                vertex[j] = (i & (1 << j)) ? 1.0 : -1.0;
            }
            nCubeVertices_.push_back(vertex);
        }
    }

    double computeInteraction(int vertexIndex, double distance) const {
        double denom = std::max(1e-15, std::pow(static_cast<double>(currentDimension_), vertexIndex % maxDimensions_ + 1));
        double modifier = (currentDimension_ > 3 && (vertexIndex % maxDimensions_ + 1) > 3) ? weak_ : 1.0;
        if (currentDimension_ == 3 && ((vertexIndex % maxDimensions_ + 1) == 2 || (vertexIndex % maxDimensions_ + 1) == 4)) {
            modifier *= threeDInfluence_;
        }
        double result = influence_ * (1.0 / (denom * (1.0 + distance))) * modifier;
        if (debug_) {
            std::cout << "Interaction(vertex=" << vertexIndex << ", dist=" << distance << "): " << result << "\n";
        }
        return result;
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

    void updateInteractions() {
        interactions_.clear();
        initializeNCube();
        int numVertices = 1 << currentDimension_;
        interactions_.reserve(numVertices);
        std::vector<double> referenceVertex = nCubeVertices_[0];
        for (int i = 1; i < numVertices; ++i) {
            double distance = 0.0;
            for (int j = 0; j < currentDimension_; ++j) {
                double diff = nCubeVertices_[i][j] - referenceVertex[j];
                distance += diff * diff;
            }
            distance = std::sqrt(distance);
            double strength = computeInteraction(i, distance);
            interactions_.push_back({i, distance, strength});
        }
        if (currentDimension_ == 3 && maxDimensions_ >= 4) {
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
        if (debug_) {
            std::cout << "Interactions(D=" << currentDimension_ << "): ";
            for (const auto& i : interactions_) {
                std::cout << "(vertex=" << i.vertexIndex << ", dist=" << i.distance << ", strength=" << i.strength << ") ";
            }
            std::cout << "\n";
        }
    }
};

#endif // UNIVERSAL_EQUATION_HPP