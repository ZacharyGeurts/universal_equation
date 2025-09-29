#ifndef UE_INIT_HPP
#define UE_INIT_HPP

#include <vector>
#include <cmath>
// used by AMOURANTH RTX September 2025
// this is not part of universal_equation and is for visualization
// Zachary Geurts 2025

// Structure to hold data for each rendered dimension, used for caching physics simulation results.
// Moved from core.hpp to decouple physics computations from rendering logic.
struct DimensionData {
    int dimension;           // Dimension index (1 to kMaxRenderedDimensions)
    double observable;       // Observable energy component
    double potential;        // Potential energy component
    double darkMatter;       // Dark matter influence
    double darkEnergy;       // Dark energy influence
};

// Structure to hold energy computation results
struct EnergyResult {
    double observable = 0.0;
    double potential = 0.0;
    double darkMatter = 0.0;
    double darkEnergy = 0.0;
};

// Structure to hold dimension interaction data
struct DimensionInteraction {
    int dimension;
    double strength;
    double phase;
};

// Class for computing multidimensional physics simulations
class UniversalEquation {
public:
    UniversalEquation() : currentDimension_(1), influence_(1.0), alpha_(0.0072973525693), debug_(false) {}

    EnergyResult compute() const {
        EnergyResult result;
        result.observable = influence_ * std::cos(wavePhase_);
        result.potential = influence_ * std::sin(wavePhase_);
        result.darkMatter = influence_ * 0.27;
        result.darkEnergy = influence_ * 0.68;
        return result;
    }

    void setCurrentDimension(int dimension) { currentDimension_ = dimension; }
    void setInfluence(double influence) { influence_ = influence; }
    void advanceCycle() { wavePhase_ += 0.1; }
    double getInfluence() const { return influence_; }
    bool getDebug() const { return debug_; }
    double computeInteraction(int vertexIndex, double distance) const {
        return influence_ * std::cos(wavePhase_ + vertexIndex * 0.1) / (distance + 1e-6);
    }
    double computePermeation(int vertexIndex) const {
        return influence_ * std::sin(wavePhase_ + vertexIndex * 0.1);
    }
    double computeDarkEnergy(double distance) const {
        return influence_ * 0.68 / (distance + 1e-6);
    }
    double getAlpha() const { return alpha_; }
    const std::vector<DimensionInteraction>& getInteractions() const { return interactions_; }

private:
    int currentDimension_;
    double influence_;
    double alpha_;
    bool debug_;
    double wavePhase_ = 0.0;
    std::vector<DimensionInteraction> interactions_;
};

#endif // UE_INIT_HPP