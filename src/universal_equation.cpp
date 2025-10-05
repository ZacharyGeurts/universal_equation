// universal_equation.cpp: Core implementation of the UniversalEquation class for quantum simulation on n-dimensional hypercube lattices.
// Models a 19-dimensional reality with stronger influences from 2D and 4D on 3D properties, using weighted dimensional contributions.
// Quantum-specific computations are implemented in universal_equation_quantum.cpp.
// Physical computations are implemented in universal_equation_physical.cpp.

// Copyright Zachary Geurts 2025 (powered by Grok with Heisenberg swagger)

#include "universal_equation.hpp"
#include <numbers>
#include <cmath>
#include <thread>
#include <fstream>
#include <memory>
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <mutex>
#include <atomic>
#include <omp.h>
#include <sstream>
#include <iomanip>
#include <limits>
#include <shared_mutex>
#include <numeric>
#include <pthread.h>

// ANSI color codes
#define RESET "\033[0m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"
#define BOLD "\033[1m"

// Helper: Safe division to prevent NaN/Inf
inline long double safe_div(long double a, long double b) noexcept {
    return (b != 0.0L && !std::isnan(b) && !std::isinf(b)) ? a / b : 0.0L;
}

// Helper: Gaussian weight for dimensional influence, centered at 3D
inline long double dimensionalWeight(int dim, int centerDim = 3, long double sigma = 1.0L) noexcept {
    long double diff = static_cast<long double>(dim - centerDim);
    return std::exp(-diff * diff / (2.0L * sigma * sigma));
}

// Constructor
UniversalEquation::UniversalEquation(
    int maxDimensions, int mode, long double influence, long double weak, long double collapse,
    long double twoD, long double threeDInfluence, long double oneDPermeation, long double nurbMatterStrength,
    long double nurbEnergyStrength, long double alpha, long double beta, long double carrollFactor,
    long double meanFieldApprox, long double asymCollapse, long double perspectiveTrans,
    long double perspectiveFocal, long double spinInteraction, long double emFieldStrength,
    long double renormFactor, long double vacuumEnergy, long double GodWaveFreq, bool debug)
    : maxDimensions_{std::max(1, std::min(maxDimensions == 0 ? 19 : maxDimensions, 19))},
      mode_{std::clamp(mode, 1, maxDimensions_)},
      currentDimension_{std::clamp(mode, 1, maxDimensions_)},
      maxVertices_{1ULL},
      omega_{maxDimensions_ > 0 ? 2.0L * std::numbers::pi_v<long double> / (2 * maxDimensions_ - 1) : 1.0L},
      invMaxDim_{maxDimensions_ > 0 ? 1.0L / maxDimensions_ : 1e-15L},
      totalCharge_{0.0L},
      needsUpdate_{true},
      mutex_{},
      nCubeVertices_{},
      vertexMomenta_{},
      vertexSpins_{},
      vertexWaveAmplitudes_{},
      interactions_{},
      projectedVerts_{},
      avgProjScale_{1.0L},
      projMutex_{},
      debugMutex_{},
      cachedCos_(maxDimensions_ + 1, 0.0L),
      navigator_{nullptr},
      nurbMatterControlPoints_{{1.0L, 0.8L, 0.5L, 0.3L, 0.1L}},
      nurbEnergyControlPoints_{{0.1L, 0.5L, 1.0L, 1.5L, 2.0L}},
      nurbKnots_{{0.0L, 0.0L, 0.0L, 0.0L, 0.5L, 1.0L, 1.0L, 1.0L, 1.0L}},
      nurbWeights_{{1.0L, 1.0L, 1.0L, 1.0L, 1.0L}},
      dimensionData_{},
      influence_{std::clamp(influence, 0.0L, 10.0L)},
      weak_{std::clamp(weak, 0.0L, 1.0L)},
      collapse_{std::clamp(collapse, 0.0L, 5.0L)},
      twoD_{std::clamp(twoD, 0.0L, 5.0L)},
      threeDInfluence_{std::clamp(threeDInfluence, 0.0L, 5.0L)},
      oneDPermeation_{std::clamp(oneDPermeation, 0.0L, 5.0L)},
      nurbMatterStrength_{std::clamp(nurbMatterStrength, 0.0L, 1.0L)},
      nurbEnergyStrength_{std::clamp(nurbEnergyStrength, 0.0L, 2.0L)},
      alpha_{std::clamp(alpha, 0.01L, 10.0L)},
      beta_{std::clamp(beta, 0.0L, 1.0L)},
      carrollFactor_{std::clamp(carrollFactor, 0.0L, 1.0L)},
      meanFieldApprox_{std::clamp(meanFieldApprox, 0.0L, 1.0L)},
      asymCollapse_{std::clamp(asymCollapse, 0.0L, 1.0L)},
      perspectiveTrans_{std::clamp(perspectiveTrans, 0.0L, 10.0L)},
      perspectiveFocal_{std::clamp(perspectiveFocal, 1.0L, 20.0L)},
      spinInteraction_{std::clamp(spinInteraction, 0.0L, 1.0L)},
      emFieldStrength_{std::clamp(emFieldStrength, 0.0L, 1.0e7L)},
      renormFactor_{std::clamp(renormFactor, 0.1L, 10.0L)},
      vacuumEnergy_{std::clamp(vacuumEnergy, 0.0L, 1.0L)},
      GodWaveFreq_{std::clamp(GodWaveFreq, 0.1L, 10.0L)},
      debug_{debug} {
    try {
        if (debug_) {
            std::lock_guard lock(debugMutex_);
            std::cout << BOLD << CYAN << "[DEBUG] Before initializeWithRetry: nCubeVertices_ size=" << nCubeVertices_.size()
                      << ", capacity=" << nCubeVertices_.capacity() << "\n" << RESET;
            std::cout << BOLD << CYAN << "[DEBUG] nurbMatterControlPoints_ size=" << nurbMatterControlPoints_.size()
                      << ", nurbEnergyControlPoints_ size=" << nurbEnergyControlPoints_.size()
                      << ", nurbKnots_ size=" << nurbKnots_.size()
                      << ", nurbWeights_ size=" << nurbWeights_.size() << "\n" << RESET;
            if (!nCubeVertices_.empty()) {
                std::cerr << MAGENTA << "[ERROR] nCubeVertices_ is not empty before initialization\n" << RESET;
            }
        }
        initializeWithRetry();
        if (debug_) {
            std::lock_guard lock(debugMutex_);
            std::cout << BOLD << CYAN << "[DEBUG] Initialized: maxDimensions=" << maxDimensions_ << ", mode=" << mode_.load()
                      << ", vertices=" << nCubeVertices_.size() << ", totalCharge=" << totalCharge_ << "\n" << RESET;
        }
    } catch (const std::exception& e) {
        std::lock_guard lock(debugMutex_);
        std::cerr << MAGENTA << "[ERROR] Constructor failed: " << e.what() << "\n" << RESET;
        throw;
    }
}

// Copy constructor
UniversalEquation::UniversalEquation(const UniversalEquation& other)
    : maxDimensions_{other.maxDimensions_},
      mode_{other.mode_.load()},
      currentDimension_{other.currentDimension_.load()},
      maxVertices_{1ULL},
      omega_{other.omega_},
      invMaxDim_{other.invMaxDim_},
      totalCharge_{other.totalCharge_},
      needsUpdate_{other.needsUpdate_.load()},
      mutex_{},
      nCubeVertices_{},
      vertexMomenta_{},
      vertexSpins_{},
      vertexWaveAmplitudes_{},
      interactions_{},
      projectedVerts_{},
      avgProjScale_{other.avgProjScale_},
      projMutex_{},
      debugMutex_{},
      cachedCos_(other.cachedCos_.size(), 0.0L),
      navigator_{nullptr},
      nurbMatterControlPoints_{other.nurbMatterControlPoints_},
      nurbEnergyControlPoints_{other.nurbEnergyControlPoints_},
      nurbKnots_{other.nurbKnots_},
      nurbWeights_{other.nurbWeights_},
      dimensionData_{},
      influence_{other.influence_.load()},
      weak_{other.weak_.load()},
      collapse_{other.collapse_.load()},
      twoD_{other.twoD_.load()},
      threeDInfluence_{other.threeDInfluence_.load()},
      oneDPermeation_{other.oneDPermeation_.load()},
      nurbMatterStrength_{other.nurbMatterStrength_.load()},
      nurbEnergyStrength_{other.nurbEnergyStrength_.load()},
      alpha_{other.alpha_.load()},
      beta_{other.beta_.load()},
      carrollFactor_{other.carrollFactor_.load()},
      meanFieldApprox_{other.meanFieldApprox_.load()},
      asymCollapse_{other.asymCollapse_.load()},
      perspectiveTrans_{other.perspectiveTrans_.load()},
      perspectiveFocal_{other.perspectiveFocal_.load()},
      spinInteraction_{other.spinInteraction_.load()},
      emFieldStrength_{other.emFieldStrength_.load()},
      renormFactor_{other.renormFactor_.load()},
      vacuumEnergy_{other.vacuumEnergy_.load()},
      GodWaveFreq_{other.GodWaveFreq_.load()},
      debug_{other.debug_.load()} {
    try {
        std::lock_guard lock(other.mutex_);
        nCubeVertices_.reserve(1);
        vertexMomenta_.reserve(1);
        vertexSpins_.reserve(1);
        vertexWaveAmplitudes_.reserve(1);
        interactions_.reserve(1);
        projectedVerts_.reserve(1);
        dimensionData_.reserve(1);
        if (!other.nCubeVertices_.empty()) {
            if (other.nCubeVertices_[0].size() != static_cast<size_t>(currentDimension_.load())) {
                throw std::runtime_error("Invalid dimension in copied nCubeVertices");
            }
            nCubeVertices_.push_back(other.nCubeVertices_[0]);
            vertexMomenta_.push_back(other.vertexMomenta_[0]);
            vertexSpins_.push_back(other.vertexSpins_[0]);
            vertexWaveAmplitudes_.push_back(other.vertexWaveAmplitudes_[0]);
            if (!other.interactions_.empty()) {
                interactions_.push_back(other.interactions_[0]);
            }
            if (!other.projectedVerts_.empty()) {
                projectedVerts_.push_back(other.projectedVerts_[0]);
            }
            if (!other.dimensionData_.empty()) {
                dimensionData_.push_back(other.dimensionData_[0]);
            }
        }
        cachedCos_ = other.cachedCos_;
        initializeWithRetry();
        if (debug_) {
            std::lock_guard lock(debugMutex_);
            std::cout << BOLD << CYAN << "[DEBUG] Copy constructor initialized: maxDimensions=" << maxDimensions_
                      << ", vertices=" << nCubeVertices_.size() << "\n" << RESET;
        }
    } catch (const std::exception& e) {
        std::lock_guard lock(debugMutex_);
        std::cerr << MAGENTA << "[ERROR] Copy constructor failed: " << e.what() << "\n" << RESET;
        throw;
    }
}

// Copy assignment
UniversalEquation& UniversalEquation::operator=(const UniversalEquation& other) {
    if (this != &other) {
        std::unique_lock lockThis(mutex_);
        std::lock_guard lockOther(other.mutex_);
        maxDimensions_ = other.maxDimensions_;
        mode_.store(other.mode_.load());
        currentDimension_.store(other.currentDimension_.load());
        maxVertices_ = 1ULL;
        omega_ = other.omega_;
        invMaxDim_ = other.invMaxDim_;
        totalCharge_ = other.totalCharge_;
        needsUpdate_.store(other.needsUpdate_.load());
        nCubeVertices_.clear();
        vertexMomenta_.clear();
        vertexSpins_.clear();
        vertexWaveAmplitudes_.clear();
        interactions_.clear();
        projectedVerts_.clear();
        dimensionData_.clear();
        nCubeVertices_.reserve(1);
        vertexMomenta_.reserve(1);
        vertexSpins_.reserve(1);
        vertexWaveAmplitudes_.reserve(1);
        interactions_.reserve(1);
        projectedVerts_.reserve(1);
        dimensionData_.reserve(1);
        cachedCos_.reserve(other.cachedCos_.size());
        if (!other.nCubeVertices_.empty()) {
            if (other.nCubeVertices_[0].size() != static_cast<size_t>(currentDimension_.load())) {
                throw std::runtime_error("Invalid dimension in copied nCubeVertices");
            }
            nCubeVertices_.push_back(other.nCubeVertices_[0]);
            vertexMomenta_.push_back(other.vertexMomenta_[0]);
            vertexSpins_.push_back(other.vertexSpins_[0]);
            vertexWaveAmplitudes_.push_back(other.vertexWaveAmplitudes_[0]);
            if (!other.interactions_.empty()) {
                interactions_.push_back(other.interactions_[0]);
            }
            if (!other.projectedVerts_.empty()) {
                projectedVerts_.push_back(other.projectedVerts_[0]);
            }
            if (!other.dimensionData_.empty()) {
                dimensionData_.push_back(other.dimensionData_[0]);
            }
        }
        cachedCos_ = other.cachedCos_;
        nurbMatterControlPoints_ = other.nurbMatterControlPoints_;
        nurbEnergyControlPoints_ = other.nurbEnergyControlPoints_;
        nurbKnots_ = other.nurbKnots_;
        nurbWeights_ = other.nurbWeights_;
        influence_.store(other.influence_.load());
        weak_.store(other.weak_.load());
        collapse_.store(other.collapse_.load());
        twoD_.store(other.twoD_.load());
        threeDInfluence_.store(other.threeDInfluence_.load());
        oneDPermeation_.store(other.oneDPermeation_.load());
        nurbMatterStrength_.store(other.nurbMatterStrength_.load());
        nurbEnergyStrength_.store(other.nurbEnergyStrength_.load());
        alpha_.store(other.alpha_.load());
        beta_.store(other.beta_.load());
        carrollFactor_.store(other.carrollFactor_.load());
        meanFieldApprox_.store(other.meanFieldApprox_.load());
        asymCollapse_.store(other.asymCollapse_.load());
        perspectiveTrans_.store(other.perspectiveTrans_.load());
        perspectiveFocal_.store(other.perspectiveFocal_.load());
        spinInteraction_.store(other.spinInteraction_.load());
        emFieldStrength_.store(other.emFieldStrength_.load());
        renormFactor_.store(other.renormFactor_.load());
        vacuumEnergy_.store(other.vacuumEnergy_.load());
        GodWaveFreq_.store(other.GodWaveFreq_.load());
        debug_.store(other.debug_.load());
        try {
            initializeWithRetry();
            if (debug_) {
                std::lock_guard lock(debugMutex_);
                std::cout << BOLD << CYAN << "[DEBUG] Copy assignment completed: maxDimensions=" << maxDimensions_
                          << ", vertices=" << nCubeVertices_.size() << "\n" << RESET;
            }
        } catch (const std::exception& e) {
            std::lock_guard lock(debugMutex_);
            std::cerr << MAGENTA << "[ERROR] Copy assignment failed: " << e.what() << "\n" << RESET;
            throw;
        }
    }
    return *this;
}

// Initialize n-cube vertices, momenta, spins, amplitudes, and dimension data
void UniversalEquation::initializeNCube() {
    if (debug_) {
        std::lock_guard lock(debugMutex_);
        std::cout << BOLD << CYAN << "[DEBUG] Entering initializeNCube: thread_id=" << std::this_thread::get_id()
                  << ", mutex_address=" << &mutex_ << "\n" << RESET;
    }
    std::lock_guard lock(mutex_);
    try {
        // Log initial state for debugging
        if (debug_) {
            std::lock_guard lock(debugMutex_);
            std::cout << BOLD << CYAN << "[DEBUG] initializeNCube: nCubeVertices_ size=" << nCubeVertices_.size()
                      << ", capacity=" << nCubeVertices_.capacity() << "\n" << RESET;
            std::cout << BOLD << CYAN << "[DEBUG] nurbMatterControlPoints_ size=" << nurbMatterControlPoints_.size()
                      << ", nurbEnergyControlPoints_ size=" << nurbEnergyControlPoints_.size()
                      << ", nurbKnots_ size=" << nurbKnots_.size()
                      << ", nurbWeights_ size=" << nurbWeights_.size() << "\n" << RESET;
        }

        // Validate NURBS vectors
        if (nurbMatterControlPoints_.size() < 4 || nurbEnergyControlPoints_.size() < 4 ||
            nurbKnots_.size() < 8 || nurbWeights_.size() < 4) {
            throw std::runtime_error("Invalid NURBS vector sizes: matter=" + std::to_string(nurbMatterControlPoints_.size()) +
                                     ", energy=" + std::to_string(nurbEnergyControlPoints_.size()) +
                                     ", knots=" + std::to_string(nurbKnots_.size()) +
                                     ", weights=" + std::to_string(nurbWeights_.size()));
        }

        // Clear vectors safely
        nCubeVertices_.clear();
        vertexMomenta_.clear();
        vertexSpins_.clear();
        vertexWaveAmplitudes_.clear();
        interactions_.clear();
        projectedVerts_.clear();
        dimensionData_.clear();

        // Verify cleared state
        if (!nCubeVertices_.empty() || !vertexMomenta_.empty() || !vertexSpins_.empty() ||
            !vertexWaveAmplitudes_.empty() || !interactions_.empty() || !projectedVerts_.empty() ||
            !dimensionData_.empty()) {
            throw std::runtime_error("Failed to clear vectors in initializeNCube");
        }

        // Reserve space for single vertex
        constexpr uint64_t numVertices = 1ULL;
        nCubeVertices_.reserve(numVertices);
        vertexMomenta_.reserve(numVertices);
        vertexSpins_.reserve(numVertices);
        vertexWaveAmplitudes_.reserve(numVertices);
        interactions_.reserve(numVertices);
        projectedVerts_.reserve(numVertices);
        dimensionData_.reserve(numVertices);

        // Initialize single vertex
        const int dim = currentDimension_.load();
        if (dim < 1 || dim > maxDimensions_) {
            throw std::runtime_error("Invalid dimension: " + std::to_string(dim));
        }
        std::vector<long double> vertex(dim, 0.0L); // Center at origin
        std::vector<long double> momentum(dim, 0.0L);
        const long double spin = 0.032774L;
        const long double amplitude = oneDPermeation_.load();

        // Populate vectors before computing NURBS and God wave
        nCubeVertices_.push_back(std::move(vertex));
        vertexMomenta_.push_back(std::move(momentum));
        vertexSpins_.push_back(spin);
        vertexWaveAmplitudes_.push_back(amplitude);

        // Compute NURBS-related values with debug logging
        long double nurbMatter, nurbEnergy, godWaveAmplitude;
        try {
            if (debug_) {
                std::lock_guard lock(debugMutex_);
                std::cout << BOLD << CYAN << "[DEBUG] Computing computeNurbMatter(0.0L)\n" << RESET;
            }
            nurbMatter = computeNurbMatter(0.0L);
            if (debug_) {
                std::lock_guard lock(debugMutex_);
                std::cout << BOLD << CYAN << "[DEBUG] Computing computeNurbEnergy(0.0L)\n" << RESET;
            }
            nurbEnergy = computeNurbEnergy(0.0L);
            if (debug_) {
                std::lock_guard lock(debugMutex_);
                std::cout << BOLD << CYAN << "[DEBUG] Computing computeGodWaveAmplitude(0, 0.0L), vertexWaveAmplitudes_ size=" << vertexWaveAmplitudes_.size() << "\n" << RESET;
            }
            godWaveAmplitude = computeGodWaveAmplitude(0, 0.0L);
        } catch (const std::exception& e) {
            std::lock_guard lock(debugMutex_);
            std::cerr << MAGENTA << "[ERROR] Failed to compute NURBS values: " << e.what() << "\n" << RESET;
            throw;
        }

        // Push back initialized data
        dimensionData_.push_back(DimensionData{
            dim,
            0.0L,
            0.0L,
            nurbMatter,
            nurbEnergy,
            spin * spinInteraction_.load(),
            0.0L,
            0.0L,
            godWaveAmplitude
        });
        totalCharge_ = 1.0L;

        // Initialize projectedVerts_ for rendering
        projectedVerts_.push_back(glm::vec3{
            dim >= 1 ? static_cast<float>(nCubeVertices_[0][0]) : 0.0f,
            dim >= 2 ? static_cast<float>(nCubeVertices_[0][1]) : 0.0f,
            dim >= 3 ? static_cast<float>(nCubeVertices_[0][2]) : 0.0f
        });

        // Verify vector integrity
        if (nCubeVertices_.size() != numVertices || nCubeVertices_[0].size() != static_cast<size_t>(dim)) {
            throw std::runtime_error("Failed to initialize nCubeVertices correctly");
        }

        if (debug_) {
            std::lock_guard lock(debugMutex_);
            std::cout << BOLD << CYAN << "[DEBUG] Initialized nCube with " << nCubeVertices_.size()
                      << " vertices, dimension=" << dim << ", totalCharge=" << totalCharge_ << "\n" << RESET;
        }
    } catch (const std::exception& e) {
        std::lock_guard lock(debugMutex_);
        std::cerr << MAGENTA << "[ERROR] initializeNCube failed: " << e.what() << "\n" << RESET;
        throw;
    }
}

// Evolve system over time with dimensional influences
void UniversalEquation::evolveTimeStep(long double dt) {
    if (dt <= 0.0L || std::isnan(dt) || std::isinf(dt)) {
        throw std::invalid_argument("Invalid time step");
    }
    std::lock_guard lock(mutex_);
    long double dimInfluence = 0.0L;
    for (int d = 1; d <= maxDimensions_; ++d) {
        dimInfluence += dimensionalWeight(d) * (d == 2 || d == 4 ? 1.5L : 1.0L);
    }
    dimInfluence = safe_div(dimInfluence, maxDimensions_);
    for (size_t i = 0; i < nCubeVertices_.size(); ++i) {
        auto& vertex = nCubeVertices_[i];
        const auto& momentum = vertexMomenta_[i];
        auto emField = computeEMField(i);
        const long double GodWaveAmp = computeGodWaveAmplitude(i, 0.0L);
        const long double nurbEnergy = computeNurbEnergy(0.0L);
        for (size_t j = 0; j < vertex.size(); ++j) {
            long double delta = dt * (momentum[j] + emField[j] * vertexSpins_[i] + GodWaveAmp * nurbEnergy * 0.1L * dimInfluence);
            if (std::isnan(delta) || std::isinf(delta)) delta = 0.0L;
            vertex[j] += delta;
            vertex[j] = std::clamp(vertex[j], -1e3L, 1e3L);
        }
        const long double cos_term = std::cos(dt * emFieldStrength_.load() * dimInfluence);
        const long double sin_term = std::sin(dt * emFieldStrength_.load() * dimInfluence);
        const long double flip = vertexSpins_[i] > 0 ? -0.032774L : 0.032774L;
        vertexSpins_[i] = vertexSpins_[i] * cos_term + flip * sin_term;
        if (std::isnan(vertexSpins_[i])) vertexSpins_[i] = 0.032774L;
        const long double cos_god = std::cos(dt * GodWaveFreq_.load() * dimInfluence);
        const long double sin_pert = std::sin(static_cast<long double>(i) * dt * 0.2L) * oneDPermeation_.load();
        vertexWaveAmplitudes_[i] = vertexWaveAmplitudes_[i] * cos_god + sin_pert;
        if (std::isnan(vertexWaveAmplitudes_[i])) vertexWaveAmplitudes_[i] = 0.0L;

        // Update dimensionData_ for rendering
        dimensionData_[i] = DimensionData{
            currentDimension_.load(),
            0.0L,
            0.0L,
            computeNurbMatter(0.0L),
            nurbEnergy,
            vertexSpins_[i] * spinInteraction_.load(),
            0.0L,
            0.0L,
            GodWaveAmp
        };
        // Update projectedVerts_ for rendering
        projectedVerts_[i] = glm::vec3{
            vertex.size() >= 1 ? static_cast<float>(vertex[0]) : 0.0f,
            vertex.size() >= 2 ? static_cast<float>(vertex[1]) : 0.0f,
            vertex.size() >= 3 ? static_cast<float>(vertex[2]) : 0.0f
        };
    }
    needsUpdate_.store(false);
    updateInteractions();
}

// Update vertex momenta with dimensional influences
void UniversalEquation::updateMomentum() {
    std::lock_guard lock(mutex_);
    long double dimInfluence = 0.0L;
    for (int d = 1; d <= maxDimensions_; ++d) {
        dimInfluence += dimensionalWeight(d) * (d == 2 || d == 4 ? 1.5L : 1.0L);
    }
    dimInfluence = safe_div(dimInfluence, maxDimensions_);
    for (size_t i = 0; i < vertexMomenta_.size(); ++i) {
        auto& momentum = vertexMomenta_[i];
        auto emField = computeEMField(i);
        const long double GodWaveAmp = computeGodWaveAmplitude(i, 0.0L);
        const long double nurbMatter = computeNurbMatter(0.0L);
        for (size_t j = 0; j < momentum.size(); ++j) {
            long double delta = (std::sin(static_cast<long double>(i + j) * 0.1L) * emFieldStrength_.load() +
                               emField[j] * vertexSpins_[i] + GodWaveAmp * nurbMatter * 0.05L) * dimInfluence;
            if (std::isnan(delta) || std::isinf(delta)) delta = 0.0L;
            momentum[j] += delta;
            momentum[j] = std::clamp(momentum[j], -0.9L, 0.9L);
        }
    }
}

// Initialize with retry logic
void UniversalEquation::initializeWithRetry() {
    int attempts = 0;
    constexpr int maxAttempts = 5;
    while (currentDimension_.load() >= 1 && attempts < maxAttempts) {
        try {
            initializeNCube();
            std::lock_guard lock(mutex_);
            cachedCos_.resize(maxDimensions_ + 1);
            for (int i = 0; i <= maxDimensions_; ++i) {
                cachedCos_[i] = std::cos(omega_ * i);
            }
            updateInteractions();
            return;
        } catch (const std::bad_alloc& e) {
            std::lock_guard lock(debugMutex_);
            std::cerr << MAGENTA << "[ERROR] Memory allocation failed for dimension " << currentDimension_.load()
                      << ". Reducing dimension to " << (currentDimension_.load() - 1)
                      << ". Attempt " << (attempts + 1) << "/" << maxAttempts << ".\n" << RESET;
            if (currentDimension_.load() == 1) {
                throw std::runtime_error("Failed to allocate memory even at dimension 1");
            }
            currentDimension_.store(currentDimension_.load() - 1);
            mode_.store(currentDimension_.load());
            maxVertices_ = 1ULL;
            needsUpdate_.store(true);
            ++attempts;
        }
    }
    if (attempts >= maxAttempts) {
        throw std::runtime_error("Max retry attempts reached for initialization");
    }
}

// Safe exponential function
long double UniversalEquation::safeExp(long double x) const {
    const long double clamped = std::clamp(x, -500.0L, 500.0L);
    const long double result = std::exp(clamped);
    if (std::isnan(result) || std::isinf(result)) {
        if (debug_) {
            std::lock_guard lock(debugMutex_);
            std::cerr << MAGENTA << "[ERROR] safeExp produced invalid result for x=" << x << "\n" << RESET;
        }
        return 1.0L;
    }
    return result;
}

// Initialize with navigator
void UniversalEquation::initializeCalculator(DimensionalNavigator* navigator) {
    std::lock_guard lock(mutex_);
    if (!navigator) {
        throw std::invalid_argument("Navigator pointer cannot be null");
    }
    if (debug_) {
        std::lock_guard lock(debugMutex_);
        std::cout << BOLD << CYAN << "[DEBUG] initializeCalculator: navigator=" << navigator << "\n" << RESET;
    }
    navigator_ = navigator;
    needsUpdate_.store(true);
    initializeWithRetry();
}

// Update cache with dimensional influences
UniversalEquation::DimensionData UniversalEquation::updateCache() {
    long double dimInfluence = 0.0L;
    for (int d = 1; d <= maxDimensions_; ++d) {
        dimInfluence += dimensionalWeight(d) * (d == 2 || d == 4 ? 1.5L : 1.0L);
    }
    const long double scaledInfluence = safe_div(dimInfluence, maxDimensions_);
    const auto result = compute();
    return DimensionData{
        currentDimension_.load(),
        result.observable * scaledInfluence,
        result.potential * scaledInfluence,
        result.nurbMatter * scaledInfluence,
        result.nurbEnergy * scaledInfluence,
        result.spinEnergy * scaledInfluence,
        result.momentumEnergy * scaledInfluence,
        result.fieldEnergy * scaledInfluence,
        result.GodWaveEnergy * scaledInfluence
    };
}

// Compute batch of dimension data with weighted contributions
std::vector<UniversalEquation::DimensionData> UniversalEquation::computeBatch(int startDim, int endDim) {
    startDim = std::clamp(startDim, 1, maxDimensions_);
    endDim = std::clamp(endDim == -1 ? maxDimensions_ : endDim, startDim, maxDimensions_);
    const int savedDim = currentDimension_.load();
    const int savedMode = mode_.load();
    std::vector<DimensionData> results(endDim - startDim + 1);
    std::vector<long double> weights(maxDimensions_ + 1, 0.0L);
    long double totalWeight = 0.0L;
    for (int d = 1; d <= maxDimensions_; ++d) {
        weights[d] = dimensionalWeight(d) * (d == 2 || d == 4 ? 1.5L : 1.0L);
        totalWeight += weights[d];
    }
    for (int d = 1; d <= maxDimensions_; ++d) {
        weights[d] = safe_div(weights[d], totalWeight);
    }
    // Temporarily disable OpenMP to avoid thread priority conflicts
    // #pragma omp parallel for schedule(dynamic)
    for (int d = startDim; d <= endDim; ++d) {
        try {
            UniversalEquation temp(*this);
            temp.currentDimension_.store(d);
            temp.mode_.store(d);
            temp.needsUpdate_.store(true);
            temp.maxVertices_ = 1ULL;
            temp.initializeWithRetry();
            DimensionData data = temp.updateCache();
            if (d == 3) {
                long double weightedObservable = 0.0L;
                long double weightedPotential = 0.0L;
                long double weightedNurbMatter = 0.0L;
                long double weightedNurbEnergy = 0.0L;
                long double weightedSpinEnergy = 0.0L;
                long double weightedMomentumEnergy = 0.0L;
                long double weightedFieldEnergy = 0.0L;
                long double weightedGodWaveEnergy = 0.0L;
                for (int k = 1; k <= maxDimensions_; ++k) {
                    try {
                        UniversalEquation tempK(*this);
                        tempK.currentDimension_.store(k);
                        tempK.mode_.store(k);
                        tempK.needsUpdate_.store(true);
                        tempK.maxVertices_ = 1ULL;
                        tempK.initializeWithRetry();
                        const DimensionData kData = tempK.updateCache();
                        const long double weight = weights[k];
                        weightedObservable += weight * kData.observable;
                        weightedPotential += weight * kData.potential;
                        weightedNurbMatter += weight * kData.nurbMatter;
                        weightedNurbEnergy += weight * kData.nurbEnergy;
                        weightedSpinEnergy += weight * kData.spinEnergy;
                        weightedMomentumEnergy += weight * kData.momentumEnergy;
                        weightedFieldEnergy += weight * kData.fieldEnergy;
                        weightedGodWaveEnergy += weight * kData.GodWaveEnergy;
                    } catch (const std::exception& e) {
                        if (debug_) {
                            std::lock_guard lock(debugMutex_);
                            std::cerr << MAGENTA << "[ERROR] Inner batch computation failed for dimension " << k << ": " << e.what() << "\n" << RESET;
                        }
                    }
                }
                data.observable = weightedObservable;
                data.potential = weightedPotential;
                data.nurbMatter = weightedNurbMatter;
                data.nurbEnergy = weightedNurbEnergy;
                data.spinEnergy = weightedSpinEnergy;
                data.momentumEnergy = weightedMomentumEnergy;
                data.fieldEnergy = weightedFieldEnergy;
                data.GodWaveEnergy = weightedGodWaveEnergy;
            }
            results[d - startDim] = data;
        } catch (const std::exception& e) {
            if (debug_) {
                std::lock_guard lock(debugMutex_);
                std::cerr << MAGENTA << "[ERROR] Batch computation failed for dimension " << d << ": " << e.what() << "\n" << RESET;
            }
            results[d - startDim] = DimensionData{d, 0.0L, 0.0L, 0.0L, 0.0L, 0.0L, 0.0L, 0.0L, 0.0L};
        }
    }
    currentDimension_.store(savedDim);
    mode_.store(savedMode);
    needsUpdate_.store(true);
    initializeWithRetry();
    return results;
}

// Export to CSV
void UniversalEquation::exportToCSV(const std::string& filename, const std::vector<DimensionData>& data) const {
    std::ofstream ofs(filename);
    if (!ofs) {
        throw std::runtime_error("Cannot open CSV file for writing: " + filename);
    }
    ofs << "Dimension,Observable,Potential,NURBMatter,NURBEnergy,SpinEnergy,MomentumEnergy,FieldEnergy,GodWaveEnergy\n";
    for (const auto& d : data) {
        ofs << d.dimension << ","
            << std::fixed << std::setprecision(10) << d.observable << ","
            << d.potential << "," << d.nurbMatter << "," << d.nurbEnergy << ","
            << d.spinEnergy << "," << d.momentumEnergy << "," << d.fieldEnergy << ","
            << d.GodWaveEnergy << "\n";
    }
}

// Advance simulation cycle
void UniversalEquation::advanceCycle() {
    const int newDimension = (currentDimension_.load() == maxDimensions_) ? 1 : currentDimension_.load() + 1;
    currentDimension_.store(newDimension);
    mode_.store(newDimension);
    needsUpdate_.store(true);
    initializeWithRetry();
}