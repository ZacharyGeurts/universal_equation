#include "universal_equation.hpp"
#include <cmath>
#include <limits>
#include <iostream>
#include <algorithm>
#include <stdexcept>

std::string UniversalEquation::EnergyResult::toString() const {
    return "Observable: " + std::to_string(observable) +
           ", Potential: " + std::to_string(potential) +
           ", Dark Matter: " + std::to_string(darkMatter) +
           ", Dark Energy: " + std::to_string(darkEnergy);
}

UniversalEquation::UniversalEquation(int maxDimensions, int mode, double influence,
                                   double weak, double collapse, double twoD,
                                   double threeDInfluence, double oneDPermeation,
                                   double darkMatterStrength, double darkEnergyStrength,
                                   double alpha, double beta, bool debug)
    : maxDimensions_(std::max(1, std::min(maxDimensions == 0 ? 20 : maxDimensions, 20))),
      currentDimension_(std::clamp(mode, 1, maxDimensions_)),
      mode_(currentDimension_),
      maxVertices_(1ULL << std::min(maxDimensions_, 20)),
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
      needsUpdate_(true) {
    initializeWithRetry();
    if (debug_) {
        std::cout << "Initialized: maxDimensions=" << maxDimensions_ << ", mode=" << mode_
                  << ", currentDimension=" << currentDimension_ << ", maxVertices=" << maxVertices_ << "\n";
    }
}

void UniversalEquation::setInfluence(double value) {
    influence_ = std::clamp(value, 0.0, 10.0);
    needsUpdate_ = true;
}

double UniversalEquation::getInfluence() const { return influence_; }

void UniversalEquation::setWeak(double value) {
    weak_ = std::clamp(value, 0.0, 1.0);
    needsUpdate_ = true;
}

double UniversalEquation::getWeak() const { return weak_; }

void UniversalEquation::setCollapse(double value) {
    collapse_ = std::clamp(value, 0.0, 5.0);
}

double UniversalEquation::getCollapse() const { return collapse_; }

void UniversalEquation::setTwoD(double value) {
    twoD_ = std::clamp(value, 0.0, 5.0);
    needsUpdate_ = true;
}

double UniversalEquation::getTwoD() const { return twoD_; }

void UniversalEquation::setThreeDInfluence(double value) {
    threeDInfluence_ = std::clamp(value, 0.0, 5.0);
    needsUpdate_ = true;
}

double UniversalEquation::getThreeDInfluence() const { return threeDInfluence_; }

void UniversalEquation::setOneDPermeation(double value) {
    oneDPermeation_ = std::clamp(value, 0.0, 5.0);
    needsUpdate_ = true;
}

double UniversalEquation::getOneDPermeation() const { return oneDPermeation_; }

void UniversalEquation::setDarkMatterStrength(double value) {
    darkMatterStrength_ = std::clamp(value, 0.0, 1.0);
    needsUpdate_ = true;
}

double UniversalEquation::getDarkMatterStrength() const { return darkMatterStrength_; }

void UniversalEquation::setDarkEnergyStrength(double value) {
    darkEnergyStrength_ = std::clamp(value, 0.0, 2.0);
    needsUpdate_ = true;
}

double UniversalEquation::getDarkEnergyStrength() const { return darkEnergyStrength_; }

void UniversalEquation::setAlpha(double value) {
    alpha_ = std::clamp(value, 0.1, 10.0);
    needsUpdate_ = true;
}

double UniversalEquation::getAlpha() const { return alpha_; }

void UniversalEquation::setBeta(double value) {
    beta_ = std::clamp(value, 0.0, 1.0);
    needsUpdate_ = true;
}

double UniversalEquation::getBeta() const { return beta_; }

void UniversalEquation::setDebug(bool value) { debug_ = value; }

bool UniversalEquation::getDebug() const { return debug_; }

void UniversalEquation::setMode(int mode) {
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

int UniversalEquation::getMode() const { return mode_; }

void UniversalEquation::setCurrentDimension(int dimension) {
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

int UniversalEquation::getCurrentDimension() const { return currentDimension_; }

int UniversalEquation::getMaxDimensions() const { return maxDimensions_; }

const std::vector<UniversalEquation::DimensionInteraction>& UniversalEquation::getInteractions() const {
    if (needsUpdate_) {
        updateInteractions();
    }
    return interactions_;
}

void UniversalEquation::advanceCycle() {
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

UniversalEquation::EnergyResult UniversalEquation::compute() const {
    if (needsUpdate_) {
        updateInteractions();
    }
    double observable = influence_;
    if (currentDimension_ >= 2) {
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

    if (debug_) {
        std::cout << "Compute(D=" << currentDimension_ << "): " << result.toString() << "\n";
    }
    return result;
}

double UniversalEquation::computeInteraction(int vertexIndex, double distance) const {
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

double UniversalEquation::computePermeation(int vertexIndex) const {
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

double UniversalEquation::computeDarkEnergy(double distance) const {
    double d = std::min(distance, 10.0);
    double result = darkEnergyStrength_ * safeExp(d * invMaxDim_);
    if (debug_ && interactions_.size() <= 100) {
        std::cout << "DarkEnergy(dist=" << distance << "): " << result << "\n";
    }
    return result;
}

double UniversalEquation::computeCollapse() const {
    if (currentDimension_ == 1) return 0.0;
    double phase = static_cast<double>(currentDimension_) / (2 * maxDimensions_);
    double osc = std::abs(cachedCos_[static_cast<size_t>(2.0 * M_PI * phase * cachedCos_.size()) % cachedCos_.size()]);
    double result = std::max(0.0, collapse_ * currentDimension_ * safeExp(-beta_ * (currentDimension_ - 1)) * (0.8 * osc + 0.2));
    if (debug_ && interactions_.size() <= 100) {
        std::cout << "Collapse(D=" << currentDimension_ << "): " << result << "\n";
    }
    return result;
}

void UniversalEquation::initializeNCube() {
    nCubeVertices_.clear();
    uint64_t numVertices = std::min(static_cast<uint64_t>(1ULL << currentDimension_), maxVertices_);
    nCubeVertices_.reserve(numVertices);
    for (uint64_t i = 0; i < numVertices; ++i) {
        std::vector<double> vertex(currentDimension_, 0.0);
        for (int j = 0; j < currentDimension_; ++j) {
            vertex[j] = (i & (1ULL << j)) ? 1.0 : -1.0;
        }
        nCubeVertices_.push_back(std::move(vertex));
    }
    if (debug_ && nCubeVertices_.size() <= 100) {
        std::cout << "Initialized nCube with " << nCubeVertices_.size() << " vertices for dimension " << currentDimension_ << "\n";
    }
}

void UniversalEquation::updateInteractions() const {
    interactions_.clear();
    interactions_.reserve(std::min(static_cast<uint64_t>(1ULL << currentDimension_), maxVertices_));
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
    needsUpdate_ = false;
    if (debug_ && interactions_.size() <= 100) {
        std::cout << "Interactions(D=" << currentDimension_ << "): ";
        for (const auto& i : interactions_) {
            std::cout << "(vertex=" << i.vertexIndex << ", dist=" << i.distance << ", strength=" << i.strength << ") ";
        }
        std::cout << "\n";
    }
}

void UniversalEquation::initializeWithRetry() {
    while (currentDimension_ >= 1) {
        try {
            initializeNCube();
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

double UniversalEquation::safeExp(double x) const {
    return std::exp(std::clamp(x, -709.0, 709.0));
}
