#ifndef UNIVERSAL_EQUATION_HPP
#define UNIVERSAL_EQUATION_HPP

#include <vector>
#include <string>
#include <cstdint> // Added for uint64_t

class UniversalEquation {
public:
    // Structure to hold energy computation results
    struct EnergyResult {
        double observable;    // Observable energy component
        double potential;     // Potential energy component
        double darkMatter;    // Dark matter contribution
        double darkEnergy;    // Dark energy contribution
        std::string toString() const;
    };

    // Structure to represent interactions between vertices
    struct DimensionInteraction {
        int vertexIndex;      // Index of the vertex
        double distance;      // Distance to reference vertex
        double strength;      // Interaction strength
        DimensionInteraction(int idx, double dist, double str) 
            : vertexIndex(idx), distance(dist), strength(str) {}
    };

    // Constructor
    UniversalEquation(int maxDimensions = 9, int mode = 1, double influence = 0.05,
                     double weak = 0.01, double collapse = 0.1, double twoD = 0.0,
                     double threeDInfluence = 0.1, double oneDPermeation = 0.1,
                     double darkMatterStrength = 0.27, double darkEnergyStrength = 0.68,
                     double alpha = 2.0, double beta = 0.2, bool debug = false);

    // Setters
    void setInfluence(double value);
    double getInfluence() const;
    void setWeak(double value);
    double getWeak() const;
    void setCollapse(double value);
    double getCollapse() const;
    void setTwoD(double value);
    double getTwoD() const;
    void setThreeDInfluence(double value);
    double getThreeDInfluence() const;
    void setOneDPermeation(double value);
    double getOneDPermeation() const;
    void setDarkMatterStrength(double value);
    double getDarkMatterStrength() const;
    void setDarkEnergyStrength(double value);
    double getDarkEnergyStrength() const;
    void setAlpha(double value);
    double getAlpha() const;
    void setBeta(double value);
    double getBeta() const;
    void setDebug(bool value);
    bool getDebug() const;
    void setMode(int mode);
    int getMode() const;
    void setCurrentDimension(int dimension);
    int getCurrentDimension() const;
    int getMaxDimensions() const;

    // Get interactions
    const std::vector<DimensionInteraction>& getInteractions() const;

    // Advance dimension cycle
    void advanceCycle();

    // Compute energy components
    EnergyResult compute() const;

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
    mutable std::vector<DimensionInteraction> interactions_; // Interaction data
    std::vector<std::vector<double>> nCubeVertices_;        // Vertex coordinates for n-cube
    mutable bool needsUpdate_;                              // Dirty flag for interactions
    std::vector<double> cachedCos_;                         // Cached cosine values

    // Private methods
    double computeInteraction(int vertexIndex, double distance) const;
    double computePermeation(int vertexIndex) const;
    double computeDarkEnergy(double distance) const;
    double computeCollapse() const;
    void initializeNCube();
    void updateInteractions() const;
    void initializeWithRetry();
    double safeExp(double x) const;
};

#endif // UNIVERSAL_EQUATION_HPP