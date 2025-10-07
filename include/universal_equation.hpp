#ifndef UNIVERSAL_EQUATION_HPP
#define UNIVERSAL_EQUATION_HPP

#include <vector>
#include <string>
#include <cstdint>
#include <cmath>
#include <limits>
#include <algorithm>
#include <stdexcept>
#include <atomic>
#include <latch>
#include <omp.h>
#include <sstream>
#include <iomanip>
#include <numbers>
#include <glm/glm.hpp>
#include <span>
#include "engine/logging.hpp"
#include "engine/core.hpp" // Include core.hpp for DimensionalNavigator

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
        int maxDimensions = 19,
        int mode = 3,
        long double influence = 2.0L,
        long double weak = 0.1L,
        long double collapse = 5.0L,
        long double twoD = 1.5L,
        long double threeDInfluence = 5.0L,
        long double oneDPermeation = 1.0L,
        long double nurbMatterStrength = 0.5L,
        long double nurbEnergyStrength = 1.0L,
        long double alpha = 0.01L,
        long double beta = 0.5L,
        long double carrollFactor = 0.1L,
        long double meanFieldApprox = 0.5L,
        long double asymCollapse = 0.5L,
        long double perspectiveTrans = 2.0L,
        long double perspectiveFocal = 4.0L,
        long double spinInteraction = 1.0L,
        long double emFieldStrength = 1.0e6L,
        long double renormFactor = 1.0L,
        long double vacuumEnergy = 0.5L,
        long double GodWaveFreq = 2.0L,
        bool debug = true,
        uint64_t numVertices = 1ULL);

    UniversalEquation(const UniversalEquation& other);
    UniversalEquation& operator=(const UniversalEquation& other);
    virtual ~UniversalEquation() = default;

    // Setters for configuration parameters
    void setInfluence(long double value) {
        long double clamped = std::clamp(value, 0.0L, 10.0L);
        influence_.store(clamped);
        needsUpdate_.store(true);
        logger_.log(Logging::LogLevel::Debug, "Set influence: value={}", std::source_location::current(), clamped);
    }
    void setWeak(long double value) {
        long double clamped = std::clamp(value, 0.0L, 1.0L);
        weak_.store(clamped);
        needsUpdate_.store(true);
        logger_.log(Logging::LogLevel::Debug, "Set weak: value={}", std::source_location::current(), clamped);
    }
    void setCollapse(long double value) {
        long double clamped = std::clamp(value, 0.0L, 5.0L);
        collapse_.store(clamped);
        needsUpdate_.store(true);
        logger_.log(Logging::LogLevel::Debug, "Set collapse: value={}", std::source_location::current(), clamped);
    }
    void setTwoD(long double value) {
        long double clamped = std::clamp(value, 0.0L, 5.0L);
        twoD_.store(clamped);
        needsUpdate_.store(true);
        logger_.log(Logging::LogLevel::Debug, "Set twoD: value={}", std::source_location::current(), clamped);
    }
    void setThreeDInfluence(long double value) {
        long double clamped = std::clamp(value, 0.0L, 5.0L);
        threeDInfluence_.store(clamped);
        needsUpdate_.store(true);
        logger_.log(Logging::LogLevel::Debug, "Set threeDInfluence: value={}", std::source_location::current(), clamped);
    }
    void setOneDPermeation(long double value) {
        long double clamped = std::clamp(value, 0.0L, 5.0L);
        oneDPermeation_.store(clamped);
        needsUpdate_.store(true);
        logger_.log(Logging::LogLevel::Debug, "Set oneDPermeation: value={}", std::source_location::current(), clamped);
    }
    void setNurbMatterStrength(long double value) {
        long double clamped = std::clamp(value, 0.0L, 1.0L);
        nurbMatterStrength_.store(clamped);
        needsUpdate_.store(true);
        logger_.log(Logging::LogLevel::Debug, "Set nurbMatterStrength: value={}", std::source_location::current(), clamped);
    }
    void setNurbEnergyStrength(long double value) {
        long double clamped = std::clamp(value, 0.0L, 2.0L);
        nurbEnergyStrength_.store(clamped);
        needsUpdate_.store(true);
        logger_.log(Logging::LogLevel::Debug, "Set nurbEnergyStrength: value={}", std::source_location::current(), clamped);
    }
    void setAlpha(long double value) {
        long double clamped = std::clamp(value, 0.01L, 10.0L);
        alpha_.store(clamped);
        needsUpdate_.store(true);
        logger_.log(Logging::LogLevel::Debug, "Set alpha: value={}", std::source_location::current(), clamped);
    }
    void setBeta(long double value) {
        long double clamped = std::clamp(value, 0.0L, 1.0L);
        beta_.store(clamped);
        needsUpdate_.store(true);
        logger_.log(Logging::LogLevel::Debug, "Set beta: value={}", std::source_location::current(), clamped);
    }
    void setCarrollFactor(long double value) {
        long double clamped = std::clamp(value, 0.0L, 1.0L);
        carrollFactor_.store(clamped);
        needsUpdate_.store(true);
        logger_.log(Logging::LogLevel::Debug, "Set carrollFactor: value={}", std::source_location::current(), clamped);
    }
    void setMeanFieldApprox(long double value) {
        long double clamped = std::clamp(value, 0.0L, 1.0L);
        meanFieldApprox_.store(clamped);
        needsUpdate_.store(true);
        logger_.log(Logging::LogLevel::Debug, "Set meanFieldApprox: value={}", std::source_location::current(), clamped);
    }
    void setAsymCollapse(long double value) {
        long double clamped = std::clamp(value, 0.0L, 1.0L);
        asymCollapse_.store(clamped);
        needsUpdate_.store(true);
        logger_.log(Logging::LogLevel::Debug, "Set asymCollapse: value={}", std::source_location::current(), clamped);
    }
    void setPerspectiveTrans(long double value) {
        long double clamped = std::clamp(value, 0.0L, 10.0L);
        perspectiveTrans_.store(clamped);
        needsUpdate_.store(true);
        logger_.log(Logging::LogLevel::Debug, "Set perspectiveTrans: value={}", std::source_location::current(), clamped);
    }
    void setPerspectiveFocal(long double value) {
        long double clamped = std::clamp(value, 1.0L, 20.0L);
        perspectiveFocal_.store(clamped);
        needsUpdate_.store(true);
        logger_.log(Logging::LogLevel::Debug, "Set perspectiveFocal: value={}", std::source_location::current(), clamped);
    }
    void setSpinInteraction(long double value) {
        long double clamped = std::clamp(value, 0.0L, 1.0L);
        spinInteraction_.store(clamped);
        needsUpdate_.store(true);
        logger_.log(Logging::LogLevel::Debug, "Set spinInteraction: value={}", std::source_location::current(), clamped);
    }
    void setEMFieldStrength(long double value) {
        long double clamped = std::clamp(value, 0.0L, 1.0e7L);
        emFieldStrength_.store(clamped);
        needsUpdate_.store(true);
        logger_.log(Logging::LogLevel::Debug, "Set emFieldStrength: value={}", std::source_location::current(), clamped);
    }
    void setRenormFactor(long double value) {
        long double clamped = std::clamp(value, 0.1L, 10.0L);
        renormFactor_.store(clamped);
        needsUpdate_.store(true);
        logger_.log(Logging::LogLevel::Debug, "Set renormFactor: value={}", std::source_location::current(), clamped);
    }
    void setVacuumEnergy(long double value) {
        long double clamped = std::clamp(value, 0.0L, 1.0L);
        vacuumEnergy_.store(clamped);
        needsUpdate_.store(true);
        logger_.log(Logging::LogLevel::Debug, "Set vacuumEnergy: value={}", std::source_location::current(), clamped);
    }
    void setGodWaveFreq(long double value) {
        long double clamped = std::clamp(value, 0.1L, 10.0L);
        GodWaveFreq_.store(clamped);
        needsUpdate_.store(true);
        logger_.log(Logging::LogLevel::Debug, "Set GodWaveFreq: value={}", std::source_location::current(), clamped);
    }
    void setCurrentDimension(int value) {
        int clamped = std::clamp(value, 1, maxDimensions_);
        currentDimension_.store(clamped);
        mode_.store(clamped);
        needsUpdate_.store(true);
        logger_.log(Logging::LogLevel::Debug, "Set currentDimension: value={}", std::source_location::current(), clamped);
    }
    void setDebug(bool value) {
        debug_.store(value);
        logger_.log(Logging::LogLevel::Debug, "Set debug: value={}", std::source_location::current(), value);
    }
    void setCurrentVertices(uint64_t value) {
        uint64_t clamped = std::min(std::max(value, static_cast<uint64_t>(1)), maxVertices_);
        currentVertices_.store(clamped);
        needsUpdate_.store(true);
        logger_.log(Logging::LogLevel::Debug, "Set currentVertices: value={}", std::source_location::current(), clamped);
    }
    void setNavigator(DimensionalNavigator* nav) {
        navigator_ = nav;
        needsUpdate_.store(true);
        logger_.log(Logging::LogLevel::Debug, "Set navigator: value={}", std::source_location::current(), static_cast<void*>(nav));
    }

    // Setters for simulation state
    void setNCubeVertex(int vertexIndex, const std::vector<long double>& vertex) {
        validateVertexIndex(vertexIndex);
        std::latch latch(1);
        if (vertex.size() != static_cast<size_t>(currentDimension_.load())) {
            logger_.log(Logging::LogLevel::Error, "Invalid vertex dimension: vertexIndex={}, size={}, expected={}", 
                        std::source_location::current(), vertexIndex, vertex.size(), currentDimension_.load());
            throw std::invalid_argument("Invalid vertex dimension");
        }
        nCubeVertices_[vertexIndex] = vertex;
        needsUpdate_.store(true);
        logger_.log(Logging::LogLevel::Debug, "Set nCubeVertex: vertexIndex={}", 
                    std::source_location::current(), vertexIndex);
        latch.count_down();
        latch.wait();
    }
    void setVertexMomentum(int vertexIndex, const std::vector<long double>& momentum) {
        validateVertexIndex(vertexIndex);
        std::latch latch(1);
        if (momentum.size() != static_cast<size_t>(currentDimension_.load())) {
            logger_.log(Logging::LogLevel::Error, "Invalid momentum dimension: vertexIndex={}, size={}, expected={}", 
                        std::source_location::current(), vertexIndex, momentum.size(), currentDimension_.load());
            throw std::invalid_argument("Invalid momentum dimension");
        }
        vertexMomenta_[vertexIndex] = momentum;
        needsUpdate_.store(true);
        logger_.log(Logging::LogLevel::Debug, "Set vertexMomentum: vertexIndex={}", 
                    std::source_location::current(), vertexIndex);
        latch.count_down();
        latch.wait();
    }
    void setVertexSpin(int vertexIndex, long double spin) {
        validateVertexIndex(vertexIndex);
        std::latch latch(1);
        if (std::isnan(spin) || std::isinf(spin)) {
            logger_.log(Logging::LogLevel::Warning, "Invalid spin value for vertexIndex={}: spin={}", 
                        std::source_location::current(), vertexIndex, spin);
            vertexSpins_[vertexIndex] = 0.032774L;
        } else {
            vertexSpins_[vertexIndex] = spin;
        }
        needsUpdate_.store(true);
        logger_.log(Logging::LogLevel::Debug, "Set vertexSpin: vertexIndex={}, spin={}", 
                    std::source_location::current(), vertexIndex, vertexSpins_[vertexIndex]);
        latch.count_down();
        latch.wait();
    }
    void setVertexWaveAmplitude(int vertexIndex, long double amplitude) {
        validateVertexIndex(vertexIndex);
        std::latch latch(1);
        if (std::isnan(amplitude) || std::isinf(amplitude)) {
            logger_.log(Logging::LogLevel::Warning, "Invalid wave amplitude for vertexIndex={}: amplitude={}", 
                        std::source_location::current(), vertexIndex, amplitude);
            vertexWaveAmplitudes_[vertexIndex] = 0.0L;
        } else {
            vertexWaveAmplitudes_[vertexIndex] = amplitude;
        }
        needsUpdate_.store(true);
        logger_.log(Logging::LogLevel::Debug, "Set vertexWaveAmplitude: vertexIndex={}, amplitude={}", 
                    std::source_location::current(), vertexIndex, vertexWaveAmplitudes_[vertexIndex]);
        latch.count_down();
        latch.wait();
    }
    void setProjectedVertex(int vertexIndex, const glm::vec3& vertex) {
        validateVertexIndex(vertexIndex);
        std::latch latch(1);
        if (reinterpret_cast<std::uintptr_t>(&vertex) % alignof(glm::vec3) != 0) {
            logger_.log(Logging::LogLevel::Error, "Misaligned projected vertex for vertexIndex={}: address={}", 
                        std::source_location::current(), vertexIndex, reinterpret_cast<std::uintptr_t>(&vertex));
            throw std::runtime_error("Misaligned projected vertex");
        }
        projectedVerts_[vertexIndex] = vertex;
        needsUpdate_.store(true);
        logger_.log(Logging::LogLevel::Debug, "Set projectedVertex: vertexIndex={}", 
                    std::source_location::current(), vertexIndex);
        latch.count_down();
        latch.wait();
    }
    void setNCubeVertices(const std::vector<std::vector<long double>>& vertices) {
        std::latch latch(1);
        if (vertices.size() > maxVertices_) {
            logger_.log(Logging::LogLevel::Error, "Vertex count exceeds maxVertices_: size={}, maxVertices_={}", 
                        std::source_location::current(), vertices.size(), maxVertices_);
            throw std::invalid_argument("Vertex count exceeds maxVertices_");
        }
        for (size_t i = 0; i < vertices.size(); ++i) {
            if (vertices[i].size() != static_cast<size_t>(currentDimension_.load())) {
                logger_.log(Logging::LogLevel::Error, "Invalid dimension for vertex {}: size={}, expected={}", 
                            std::source_location::current(), i, vertices[i].size(), currentDimension_.load());
                throw std::invalid_argument("Invalid vertex dimension");
        }
        nCubeVertices_ = vertices;
        currentVertices_.store(std::min(vertices.size(), static_cast<size_t>(maxVertices_)));
        needsUpdate_.store(true);
        logger_.log(Logging::LogLevel::Debug, "Set nCubeVertices: size={}, currentVertices={}", 
                    std::source_location::current(), nCubeVertices_.size(), currentVertices_.load());
        latch.count_down();
        latch.wait();
    }
    void setVertexMomenta(const std::vector<std::vector<long double>>& momenta) {
        std::latch latch(1);
        if (momenta.size() > maxVertices_) {
            logger_.log(Logging::LogLevel::Error, "Momentum count exceeds maxVertices_: size={}, maxVertices_={}", 
                        std::source_location::current(), momenta.size(), maxVertices_);
            throw std::invalid_argument("Momentum count exceeds maxVertices_");
        }
        for (size_t i = 0; i < momenta.size(); ++i) {
            if (momenta[i].size() != static_cast<size_t>(currentDimension_.load())) {
                logger_.log(Logging::LogLevel::Error, "Invalid dimension for momentum {}: size={}, expected={}", 
                            std::source_location::current(), i, momenta[i].size(), currentDimension_.load());
                throw std::invalid_argument("Invalid momentum dimension");
            }
        }
        vertexMomenta_ = momenta;
        currentVertices_.store(std::min(momenta.size(), static_cast<size_t>(maxVertices_)));
        needsUpdate_.store(true);
        logger_.log(Logging::LogLevel::Debug, "Set vertexMomenta: size={}, currentVertices={}", 
                    std::source_location::current(), vertexMomenta_.size(), currentVertices_.load());
        latch.count_down();
        latch.wait();
    }
    void setVertexSpins(const std::vector<long double>& spins) {
        std::latch latch(1);
        if (spins.size() > maxVertices_) {
            logger_.log(Logging::LogLevel::Error, "Spin count exceeds maxVertices_: size={}, maxVertices_={}", 
                        std::source_location::current(), spins.size(), maxVertices_);
            throw std::invalid_argument("Spin count exceeds maxVertices_");
        }
        vertexSpins_.resize(spins.size());
        for (size_t i = 0; i < spins.size(); ++i) {
            if (std::isnan(spins[i]) || std::isinf(spins[i])) {
                logger_.log(Logging::LogLevel::Warning, "Invalid spin for index {}: spin={}", 
                            std::source_location::current(), i, spins[i]);
                vertexSpins_[i] = 0.032774L;
            } else {
                vertexSpins_[i] = spins[i];
            }
        }
        currentVertices_.store(std::min(spins.size(), static_cast<size_t>(maxVertices_)));
        needsUpdate_.store(true);
        logger_.log(Logging::LogLevel::Debug, "Set vertexSpins: size={}, currentVertices={}", 
                    std::source_location::current(), vertexSpins_.size(), currentVertices_.load());
        latch.count_down();
        latch.wait();
    }
    void setVertexWaveAmplitudes(const std::vector<long double>& amplitudes) {
        std::latch latch(1);
        if (amplitudes.size() > maxVertices_) {
            logger_.log(Logging::LogLevel::Error, "Amplitude count exceeds maxVertices_: size={}, maxVertices_={}", 
                        std::source_location::current(), amplitudes.size(), maxVertices_);
            throw std::invalid_argument("Amplitude count exceeds maxVertices_");
        }
        vertexWaveAmplitudes_.resize(amplitudes.size());
        for (size_t i = 0; i < amplitudes.size(); ++i) {
            if (std::isnan(amplitudes[i]) || std::isinf(amplitudes[i])) {
                logger_.log(Logging::LogLevel::Warning, "Invalid amplitude for index {}: amplitude={}", 
                            std::source_location::current(), i, amplitudes[i]);
                vertexWaveAmplitudes_[i] = 0.0L;
            } else {
                vertexWaveAmplitudes_[i] = amplitudes[i];
            }
        }
        currentVertices_.store(std::min(amplitudes.size(), static_cast<size_t>(maxVertices_)));
        needsUpdate_.store(true);
        logger_.log(Logging::LogLevel::Debug, "Set vertexWaveAmplitudes: size={}, currentVertices={}", 
                    std::source_location::current(), vertexWaveAmplitudes_.size(), currentVertices_.load());
        latch.count_down();
        latch.wait();
    }
    void setProjectedVertices(const std::vector<glm::vec3>& vertices) {
        std::latch latch(1);
        if (vertices.size() > maxVertices_) {
            logger_.log(Logging::LogLevel::Error, "Projected vertex count exceeds maxVertices_: size={}, maxVertices_={}", 
                        std::source_location::current(), vertices.size(), maxVertices_);
            throw std::invalid_argument("Projected vertex count exceeds maxVertices_");
        }
        if (!vertices.empty() && reinterpret_cast<std::uintptr_t>(vertices.data()) % alignof(glm::vec3) != 0) {
            logger_.log(Logging::LogLevel::Error, "Misaligned projected vertices: address={}", 
                        std::source_location::current(), reinterpret_cast<std::uintptr_t>(vertices.data()));
            throw std::runtime_error("Misaligned projected vertices");
        }
        projectedVerts_ = vertices;
        currentVertices_.store(std::min(vertices.size(), static_cast<size_t>(maxVertices_)));
        needsUpdate_.store(true);
        logger_.log(Logging::LogLevel::Debug, "Set projectedVertices: size={}, currentVertices={}", 
                    std::source_location::current(), projectedVerts_.size(), currentVertices_.load());
        latch.count_down();
        latch.wait();
    }
    void setTotalCharge(long double value) {
        if (std::isnan(value) || std::isinf(value)) {
            logger_.log(Logging::LogLevel::Warning, "Invalid total charge: value={}", 
                        std::source_location::current(), value);
            totalCharge_.store(0.0L);
        } else {
            totalCharge_.store(value);
        }
        logger_.log(Logging::LogLevel::Debug, "Set totalCharge: value={}", 
                    std::source_location::current(), totalCharge_.load());
    }

    // Getters for configuration parameters
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

    // Getters for simulation state
    std::span<const std::vector<long double>> getNCubeVertices() const { return nCubeVertices_; }
    std::span<const std::vector<long double>> getVertexMomenta() const { return vertexMomenta_; }
    std::span<const long double> getVertexSpins() const { return vertexSpins_; }
    std::span<const long double> getVertexWaveAmplitudes() const { return vertexWaveAmplitudes_; }
    std::span<const DimensionInteraction> getInteractions() const { return interactions_; }
    std::span<const glm::vec3> getProjectedVertices() const {
        if (!projectedVerts_.empty() && reinterpret_cast<std::uintptr_t>(projectedVerts_.data()) % alignof(glm::vec3) != 0) {
            logger_.log(Logging::LogLevel::Error, "Misaligned projectedVerts_: address={}", 
                        std::source_location::current(), reinterpret_cast<std::uintptr_t>(projectedVerts_.data()));
            throw std::runtime_error("Misaligned projectedVerts_");
        }
        return projectedVerts_;
    }
    const std::vector<long double>& getNCubeVertex(int vertexIndex) const {
        validateVertexIndex(vertexIndex);
        logger_.log(Logging::LogLevel::Debug, "Get nCubeVertex: vertexIndex={}", 
                    std::source_location::current(), vertexIndex);
        return nCubeVertices_[vertexIndex];
    }
    const std::vector<long double>& getVertexMomentum(int vertexIndex) const {
        validateVertexIndex(vertexIndex);
        logger_.log(Logging::LogLevel::Debug, "Get vertexMomentum: vertexIndex={}", 
                    std::source_location::current(), vertexIndex);
        return vertexMomenta_[vertexIndex];
    }
    long double getVertexSpin(int vertexIndex) const {
        validateVertexIndex(vertexIndex);
        logger_.log(Logging::LogLevel::Debug, "Get vertexSpin: vertexIndex={}, spin={}", 
                    std::source_location::current(), vertexIndex, vertexSpins_[vertexIndex]);
        return vertexSpins_[vertexIndex];
    }
    long double getVertexWaveAmplitude(int vertexIndex) const {
        validateVertexIndex(vertexIndex);
        logger_.log(Logging::LogLevel::Debug, "Get vertexWaveAmplitude: vertexIndex={}, amplitude={}", 
                    std::source_location::current(), vertexIndex, vertexWaveAmplitudes_[vertexIndex]);
        return vertexWaveAmplitudes_[vertexIndex];
    }
    const glm::vec3& getProjectedVertex(int vertexIndex) const {
        validateVertexIndex(vertexIndex);
        logger_.log(Logging::LogLevel::Debug, "Get projectedVertex: vertexIndex={}", 
                    std::source_location::current(), vertexIndex);
        return projectedVerts_[vertexIndex];
    }

    // Core methods
    EnergyResult compute();
    void initializeNCube();
    void initializeCalculator();
    void updateInteractions() const;
    void initializeWithRetry();
    long double safeExp(long double x) const {
        if (std::isnan(x) || std::isinf(x)) {
            logger_.log(Logging::LogLevel::Warning, "Invalid input to safeExp: x={}", std::source_location::current(), x);
            return 0.0L;
        }
        if (x > 100.0L) {
            logger_.log(Logging::LogLevel::Warning, "Clamping large exponent in safeExp: x={}", std::source_location::current(), x);
            x = 100.0L;
        }
        return std::exp(x);
    }
    inline long double safe_div(long double a, long double b) const {
        if (b == 0.0L || std::isnan(b) || std::isinf(b)) {
            logger_.log(Logging::LogLevel::Warning, "Invalid divisor in safe_div: a={}, b={}", 
                        std::source_location::current(), a, b);
            return 0.0L;
        }
        long double result = a / b;
        if (std::isnan(result) || std::isinf(result)) {
            logger_.log(Logging::LogLevel::Warning, "Invalid result in safe_div: a={}, b={}, result={}", 
                        std::source_location::current(), a, b, result);
            return 0.0L;
        }
        return result;
    }
    inline void validateVertexIndex(int vertexIndex, const std::source_location& loc = std::source_location::current()) const {
        if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= nCubeVertices_.size()) {
            logger_.log(Logging::LogLevel::Error, "Invalid vertexIndex: vertexIndex={}, size={}", 
                        loc, vertexIndex, nCubeVertices_.size());
            throw std::out_of_range("Invalid vertex index");
        }
    }

    // Stubs for unimplemented methods
    void evolveTimeStep(long double dt) {
        logger_.log(Logging::LogLevel::Info, "evolveTimeStep called: dt={}", std::source_location::current(), dt);
    }
    void updateMomentum() {
        logger_.log(Logging::LogLevel::Info, "updateMomentum called", std::source_location::current());
    }
    void advanceCycle() {
        logger_.log(Logging::LogLevel::Info, "advanceCycle called", std::source_location::current());
    }
    std::vector<DimensionData> computeBatch(int startDim, int endDim) {
        logger_.log(Logging::LogLevel::Info, "computeBatch called: startDim={}, endDim={}", 
                    std::source_location::current(), startDim, endDim);
        return {};
    }
    void exportToCSV(const std::string& filename, [[maybe_unused]] const std::vector<DimensionData>& data) const {
        logger_.log(Logging::LogLevel::Info, "exportToCSV called: filename={}", std::source_location::current(), filename);
    }
    DimensionData updateCache() {
        logger_.log(Logging::LogLevel::Info, "updateCache called", std::source_location::current());
        return {};
    }
    long double computeVertexVolume(int vertexIndex) const {
        validateVertexIndex(vertexIndex);
        return 1.0L; // Stub
    }
    long double computeVertexMass(int vertexIndex) const {
        validateVertexIndex(vertexIndex);
        return 1.0L; // Stub
    }
    long double computeVertexDensity(int vertexIndex) const {
        validateVertexIndex(vertexIndex);
        return 1.0L; // Stub
    }
    std::vector<long double> computeCenterOfMass() const {
        return std::vector<long double>(getCurrentDimension(), 0.0L); // Stub
    }
    long double computeTotalSystemVolume() const {
        return 1.0L; // Stub
    }
    long double computeGravitationalPotential(int vertexIndex1, int vertexIndex2 = -1) const {
        validateVertexIndex(vertexIndex1);
        if (vertexIndex2 != -1) validateVertexIndex(vertexIndex2);
        return 0.0L; // Stub
    }
    std::vector<long double> computeGravitationalAcceleration(int vertexIndex) const {
        validateVertexIndex(vertexIndex);
        return std::vector<long double>(getCurrentDimension(), 0.0L); // Stub
    }
    std::vector<long double> computeClassicalEMField(int vertexIndex) const {
        validateVertexIndex(vertexIndex);
        return std::vector<long double>(std::min(3, getCurrentDimension()), 0.0L); // Stub
    }
    void updateOrbitalVelocity(long double dt) {
        logger_.log(Logging::LogLevel::Info, "updateOrbitalVelocity called: dt={}", std::source_location::current(), dt);
    }
    void updateOrbitalPositions(long double dt) {
        logger_.log(Logging::LogLevel::Info, "updateOrbitalPositions called: dt={}", std::source_location::current(), dt);
    }
    long double computeSystemEnergy() const {
        return 0.0L; // Stub
    }
    long double computePythagoreanScaling(int vertexIndex) const {
        validateVertexIndex(vertexIndex);
        return 1.0L; // Stub
    }
    long double computeNurbMatter(int vertexIndex) const {
        validateVertexIndex(vertexIndex);
        return 0.0L; // Stub
    }
    long double computeNurbEnergy(int vertexIndex) const {
        validateVertexIndex(vertexIndex);
        return 0.0L; // Stub
    }
    std::vector<long double> computeVectorPotential(int vertexIndex) const {
        validateVertexIndex(vertexIndex);
        return std::vector<long double>(std::min(3, getCurrentDimension()), 0.0L); // Stub
    }
    long double computeInteraction(int vertexIndex, [[maybe_unused]] long double distance) const {
        validateVertexIndex(vertexIndex);
        return 0.0L; // Stub
    }
    long double computeSpinEnergy(int vertexIndex) const {
        validateVertexIndex(vertexIndex);
        return 0.0L; // Stub
    }
    long double computeEMField(int vertexIndex) const {
        validateVertexIndex(vertexIndex);
        return 0.0L; // Stub
    }
    long double computeGodWave(int vertexIndex) const {
        validateVertexIndex(vertexIndex);
        return 0.0L; // Stub
    }
    long double computeGodWaveAmplitude(int vertexIndex, [[maybe_unused]] long double time) const {
        validateVertexIndex(vertexIndex);
        return 0.0L; // Stub
    }

private:
    // Thread-safe configuration parameters
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
    const uint64_t maxVertices_;
    const int maxDimensions_;
    const long double omega_;
    const long double invMaxDim_;

    // Simulation state
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

#endif // UNIVERSAL_EQUATION_HPP