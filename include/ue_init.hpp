#ifndef UE_INIT_HPP
#define UE_INIT_HPP

// UniversalEquation, October 2025
// Core physics simulation for AMOURANTH RTX Engine.
// Manages N-dimensional calculations, nurb matter, and energy dynamics.
// Dependencies: C++20, GLM, Logging
// Zachary Geurts 2025

#include "engine/logging.hpp"
//#include "engine/core.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <cstdint>
#include <atomic>
#include <numbers>
#include <format>
#include <span>
#include <sstream>
#include <iomanip>

// Forward declarations
class AMOURANTH;
class DimensionalNavigator;

class UniversalEquation {
public:
    struct EnergyResult {
        long double observable;
        long double potential;
        long double nurbMatter;
        long double nurbEnergy;
        long double spinEnergy;
        long double momentumEnergy;
        long double fieldEnergy;
        long double GodWaveEnergy;
        std::string toString() const {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(10)
                << "Observable: " << observable
                << ", Potential: " << potential
                << ", NURB Matter: " << nurbMatter
                << ", NURB Energy: " << nurbEnergy
                << ", Spin Energy: " << spinEnergy
                << ", Momentum Energy: " << momentumEnergy
                << ", Field Energy: " << fieldEnergy
                << ", God Wave Energy: " << GodWaveEnergy;
            return oss.str();
        }
    };

    struct DimensionInteraction {
        int vertexIndex;
        long double distance;
        long double strength;
        std::vector<long double> vectorPotential;
        long double waveAmplitude;
        DimensionInteraction() : vertexIndex(0), distance(0.0L), strength(0.0L), vectorPotential(), waveAmplitude(0.0L) {}
        DimensionInteraction(int idx, long double dist, long double str, const std::vector<long double>& vecPot, long double amp)
            : vertexIndex(idx), distance(dist), strength(str), vectorPotential(vecPot), waveAmplitude(amp) {}
    };

    struct DimensionData {
        int dimension;
        long double observable;
        long double potential;
        long double nurbMatter;
        long double nurbEnergy;
        long double spinEnergy;
        long double momentumEnergy;
        long double fieldEnergy;
        long double GodWaveEnergy;
        std::string toString() const {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(10)
                << "Dimension: " << dimension << ", " << EnergyResult{observable, potential, nurbMatter, nurbEnergy, spinEnergy, momentumEnergy, fieldEnergy, GodWaveEnergy}.toString();
            return oss.str();
        }
    };

    UniversalEquation(
        const Logging::Logger& logger,
        int maxDimensions,
        int mode,
        long double influence,
        long double weak,
        long double collapse,
        long double twoD,
        long double threeDInfluence,
        long double oneDPermeation,
        long double nurbMatterStrength,
        long double nurbEnergyStrength,
        long double alpha,
        long double beta,
        long double carrollFactor,
        long double meanFieldApprox,
        long double asymCollapse,
        long double perspectiveTrans,
        long double perspectiveFocal,
        long double spinInteraction,
        long double emFieldStrength,
        long double renormFactor,
        long double vacuumEnergy,
        long double GodWaveFreq,
        bool debug,
        uint64_t numVertices
    );

    UniversalEquation(
        const Logging::Logger& logger,
        int maxDimensions,
        int mode,
        long double influence,
        long double weak,
        bool debug,
        uint64_t numVertices = 30000
    );

    UniversalEquation(const UniversalEquation& other);
    UniversalEquation& operator=(const UniversalEquation& other);
    virtual ~UniversalEquation();

    void initializeCalculator(AMOURANTH* amouranth);
    EnergyResult compute();
    void initializeNCube();
    void updateInteractions() const;
    void initializeWithRetry();
    void setCurrentDimension(int dimension);
    void setMode(int mode);
    void setInfluence(long double value);
    void setWeak(long double value);
    void setCollapse(long double value);
    void setTwoD(long double value);
    void setThreeDInfluence(long double value);
    void setOneDPermeation(long double value);
    void setNurbMatterStrength(long double value);
    void setNurbEnergyStrength(long double value);
    void setAlpha(long double value);
    void setBeta(long double value);
    void setCarrollFactor(long double value);
    void setMeanFieldApprox(long double value);
    void setAsymCollapse(long double value);
    void setPerspectiveTrans(long double value);
    void setPerspectiveFocal(long double value);
    void setSpinInteraction(long double value);
    void setEMFieldStrength(long double value);
    void setRenormFactor(long double value);
    void setVacuumEnergy(long double value);
    void setGodWaveFreq(long double value);
    void setDebug(bool value);
    void setCurrentVertices(uint64_t value);
    void setNavigator(DimensionalNavigator* nav);
    void setNCubeVertex(int vertexIndex, const std::vector<long double>& vertex);
    void setVertexMomentum(int vertexIndex, const std::vector<long double>& momentum);
    void setVertexSpin(int vertexIndex, long double spin);
    void setVertexWaveAmplitude(int vertexIndex, long double amplitude);
    void setProjectedVertex(int vertexIndex, const glm::vec3& vertex);
    void setNCubeVertices(const std::vector<std::vector<long double>>& vertices);
    void setVertexMomenta(const std::vector<std::vector<long double>>& momenta);
    void setVertexSpins(const std::vector<long double>& spins);
    void setVertexWaveAmplitudes(const std::vector<long double>& amplitudes);
    void setProjectedVertices(const std::vector<glm::vec3>& vertices);
    void setTotalCharge(long double value);
    void setMaterialDensity(long double density);

    long double getInfluence() const { return influence_.load(); }
    long double getWeak() const { return weak_.load(); }
    long double getCollapse() const { return collapse_.load(); }
    long double getTwoD() const { return twoD_.load(); }
    long double getThreeDInfluence() const { return threeDInfluence_.load(); }
    long double getOneDPermeation() const { return oneDPermeation_.load(); }
    long double getNurbMatterStrength() const { return nurbMatterStrength_.load(); }
    long double getNurbEnergyStrength() const { return nurbEnergyStrength_.load(); }
    long double getAlpha() const { return alpha_.load(); }
    long double getBeta() const { return beta_.load(); }
    long double getCarrollFactor() const { return carrollFactor_.load(); }
    long double getMeanFieldApprox() const { return meanFieldApprox_.load(); }
    long double getAsymCollapse() const { return asymCollapse_.load(); }
    long double getPerspectiveTrans() const { return perspectiveTrans_.load(); }
    long double getPerspectiveFocal() const { return perspectiveFocal_.load(); }
    long double getSpinInteraction() const { return spinInteraction_.load(); }
    long double getEMFieldStrength() const { return emFieldStrength_.load(); }
    long double getRenormFactor() const { return renormFactor_.load(); }
    long double getVacuumEnergy() const { return vacuumEnergy_.load(); }
    long double getGodWaveFreq() const { return GodWaveFreq_.load(); }
    int getCurrentDimension() const { return currentDimension_.load(); }
    int getMaxDimensions() const { return maxDimensions_; }
    int getMode() const { return mode_.load(); }
    bool getDebug() const { return debug_.load(); }
    bool getNeedsUpdate() const { return needsUpdate_.load(); }
    long double getTotalCharge() const { return totalCharge_.load(); }
    uint64_t getMaxVertices() const { return maxVertices_; }
    uint64_t getCurrentVertices() const { return currentVertices_.load(); }
    long double getOmega() const { return omega_; }
    long double getInvMaxDim() const { return invMaxDim_; }
    std::span<const long double> getCachedCos() const { return cachedCos_; }
    std::span<const long double> getNurbMatterControlPoints() const { return nurbMatterControlPoints_; }
    std::span<const long double> getNurbEnergyControlPoints() const { return nurbEnergyControlPoints_; }
    std::span<const long double> getNurbKnots() const { return nurbKnots_; }
    std::span<const long double> getNurbWeights() const { return nurbWeights_; }
    const DimensionData& getDimensionData() const { return dimensionData_; }
    DimensionalNavigator* getNavigator() const { return navigator_; }
    float getSimulationTime() const { return simulationTime_.load(); }
    std::span<const std::vector<long double>> getNCubeVertices() const { return nCubeVertices_; }
    std::span<const std::vector<long double>> getVertexMomenta() const { return vertexMomenta_; }
    std::span<const long double> getVertexSpins() const { return vertexSpins_; }
    std::span<const long double> getVertexWaveAmplitudes() const { return vertexWaveAmplitudes_; }
    std::span<const DimensionInteraction> getInteractions() const { return interactions_; }
    std::span<const glm::vec3> getProjectedVertices() const { return projectedVerts_; }
    const std::vector<long double>& getNCubeVertex(int vertexIndex) const;
    const std::vector<long double>& getVertexMomentum(int vertexIndex) const;
    long double getVertexSpin(int vertexIndex) const;
    long double getVertexWaveAmplitude(int vertexIndex) const;
    const glm::vec3& getProjectedVertex(int vertexIndex) const;

    void evolveTimeStep(long double dt);
    void updateMomentum();
    void advanceCycle();
    std::vector<DimensionData> computeBatch(int startDim, int endDim);
    void exportToCSV(const std::string& filename, const std::vector<DimensionData>& data) const;
    DimensionData updateCache();
    long double computeNurbMatter(int vertexIndex) const;
    long double computeNurbEnergy(int vertexIndex) const;
    std::vector<long double> computeVectorPotential(int vertexIndex) const;
    long double computeInteraction(int vertexIndex, long double distance) const;
    long double computeSpinEnergy(int vertexIndex) const;
    long double computeEMField(int vertexIndex) const;
    long double computeGodWave(int vertexIndex) const;
    long double computeGodWaveAmplitude(int vertexIndex, long double time) const;
    long double safeExp(long double x) const;
    long double safe_div(long double a, long double b) const;
    void validateVertexIndex(int vertexIndex, const std::source_location& loc = std::source_location::current()) const;
	void validateProjectedVertices() const;

    // Methods implemented in universal_equation_physical.cpp
    long double computeVertexVolume(int vertexIndex) const;
    long double computeVertexMass(int vertexIndex) const;
    long double computeVertexDensity(int vertexIndex) const;
    std::vector<long double> computeCenterOfMass() const;
    long double computeTotalSystemVolume() const;
    long double computeGravitationalPotential(int vertexIndex1, int vertexIndex2 = -1) const;
    std::vector<long double> computeGravitationalAcceleration(int vertexIndex) const;
    std::vector<long double> computeClassicalEMField(int vertexIndex) const;
    void updateOrbitalVelocity(long double dt);
    void updateOrbitalPositions(long double dt);
    long double computeSystemEnergy() const;
    long double computePythagoreanScaling(int vertexIndex) const;
    long double computeCircleArea(long double radius) const;
    long double computeSphereVolume(long double radius) const;
    long double computeKineticEnergy(int vertexIndex) const;
    long double computeGravitationalForceMagnitude(int vertexIndex1, int vertexIndex2) const;
    long double computeCoulombForceMagnitude(int vertexIndex1, int vertexIndex2) const;
    long double computePressure(int vertexIndex) const;
    long double computeBuoyantForce(int vertexIndex) const;
    long double computeCentripetalAcceleration(int vertexIndex, long double radius) const;
    bool computeSphereCollision(int vertexIndex1, int vertexIndex2) const;
    void resolveSphereCollision(int vertexIndex1, int vertexIndex2);
    bool computeAABBCollision2D(int vertexIndex1, int vertexIndex2) const;
    long double computeEffectiveMass(int vertexIndex1, int vertexIndex2) const;
    std::vector<long double> computeTorque(int vertexIndex, const std::vector<long double>& pivot) const;
    std::vector<long double> computeDragForce(int vertexIndex) const;
    std::vector<long double> computeSpringForce(int vertexIndex1, int vertexIndex2, long double springConstant, long double restLength) const;

private:
    std::atomic<long double> influence_;
    std::atomic<long double> weak_;
    std::atomic<long double> collapse_;
    std::atomic<long double> twoD_;
    std::atomic<long double> threeDInfluence_;
    std::atomic<long double> oneDPermeation_;
    std::atomic<long double> nurbMatterStrength_;
    std::atomic<long double> nurbEnergyStrength_;
    std::atomic<long double> alpha_;
    std::atomic<long double> beta_;
    std::atomic<long double> carrollFactor_;
    std::atomic<long double> meanFieldApprox_;
    std::atomic<long double> asymCollapse_;
    std::atomic<long double> perspectiveTrans_;
    std::atomic<long double> perspectiveFocal_;
    std::atomic<long double> spinInteraction_;
    std::atomic<long double> emFieldStrength_;
    std::atomic<long double> renormFactor_;
    std::atomic<long double> vacuumEnergy_;
    std::atomic<long double> GodWaveFreq_;
    std::atomic<int> currentDimension_;
    std::atomic<int> mode_;
    std::atomic<bool> debug_;
    std::atomic<bool> needsUpdate_;
    std::atomic<long double> totalCharge_;
    std::atomic<long double> avgProjScale_;
    std::atomic<uint64_t> currentVertices_;
    std::atomic<float> simulationTime_;
    std::atomic<long double> materialDensity_;
    const uint64_t maxVertices_;
    const int maxDimensions_;
    const long double omega_;
    const long double invMaxDim_;
    mutable std::vector<std::vector<long double>> nCubeVertices_;
    mutable std::vector<std::vector<long double>> vertexMomenta_;
    mutable std::vector<long double> vertexSpins_;
    mutable std::vector<long double> vertexWaveAmplitudes_;
    mutable std::vector<DimensionInteraction> interactions_;
    mutable std::vector<glm::vec3> projectedVerts_;
    std::vector<long double> cachedCos_;
    std::vector<long double> nurbMatterControlPoints_;
    std::vector<long double> nurbEnergyControlPoints_;
    std::vector<long double> nurbKnots_;
    std::vector<long double> nurbWeights_;
    DimensionData dimensionData_;
    DimensionalNavigator* navigator_;
    const Logging::Logger& logger_;
};

#endif // UE_INIT_HPP