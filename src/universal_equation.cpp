// UniversalEquation.cpp: Core implementation of the UniversalEquation class for quantum simulation on n-dimensional hypercube lattices.
// This file handles class structure, initialization, time evolution, and utility functions.
// Quantum-specific computations are implemented in universal_equation_quantum.cpp.

// How this ties into the quantum world:
// The UniversalEquation class simulates quantum phenomena by modeling particles and fields on an n-dimensional hypercube lattice, 
// integrating concepts from quantum mechanics and quantum electrodynamics (QED). It represents quantum states through vertex-based 
// wave amplitudes and spins, evolving them over time with interactions governed by electromagnetic fields, Lorentz factors, and 
// NURBS-based matter and energy fields. The "God wave" introduces a phase-coherent oscillation to mimic quantum coherence and 
// entanglement effects. Deterministic wave function collapse is modeled via the computeCollapse method, balancing symmetric and 
// asymmetric terms influenced by lattice dimensionality. The system incorporates relativistic effects through the Lorentz factor and 
// supports high-dimensional simulations with thread-safe, high-precision calculations optimized via OpenMP.

// Copyright Zachary Geurts 2025 (powered by Grok with Heisenberg swagger)

#include "universal_equation.hpp"
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

// Constructor: Initializes quantum simulation with NURBS fields and God wave dynamics.
UniversalEquation::UniversalEquation(
    int maxDimensions, int mode, long double influence, long double weak, long double collapse,
    long double twoD, long double threeDInfluence, long double oneDPermeation, long double nurbMatterStrength,
    long double nurbEnergyStrength, long double alpha, long double beta, long double carrollFactor,
    long double meanFieldApprox, long double asymCollapse, long double perspectiveTrans,
    long double perspectiveFocal, long double spinInteraction, long double emFieldStrength,
    long double renormFactor, long double vacuumEnergy, long double GodWaveFreq, bool debug)
    : maxDimensions_(std::max(1, std::min(maxDimensions == 0 ? 20 : maxDimensions, 20))),
      currentDimension_(std::clamp(mode, 1, maxDimensions_)),
      mode_(std::clamp(mode, 1, maxDimensions_)),
      maxVertices_(1ULL << std::min(maxDimensions_, 20)),
      influence_(std::clamp(influence, 0.0L, 10.0L)),
      weak_(std::clamp(weak, 0.0L, 1.0L)),
      collapse_(std::clamp(collapse, 0.0L, 5.0L)),
      twoD_(std::clamp(twoD, 0.0L, 5.0L)),
      threeDInfluence_(std::clamp(threeDInfluence, 0.0L, 5.0L)),
      oneDPermeation_(std::clamp(oneDPermeation, 0.0L, 5.0L)),
      nurbMatterStrength_(std::clamp(nurbMatterStrength, 0.0L, 1.0L)),
      nurbEnergyStrength_(std::clamp(nurbEnergyStrength, 0.0L, 2.0L)),
      alpha_(std::clamp(alpha, 0.1L, 10.0L)),
      beta_(std::clamp(beta, 0.0L, 1.0L)),
      carrollFactor_(std::clamp(carrollFactor, 0.0L, 1.0L)),
      meanFieldApprox_(std::clamp(meanFieldApprox, 0.0L, 1.0L)),
      asymCollapse_(std::clamp(asymCollapse, 0.0L, 1.0L)),
      perspectiveTrans_(std::clamp(perspectiveTrans, 0.0L, 10.0L)),
      perspectiveFocal_(std::clamp(perspectiveFocal, 1.0L, 20.0L)),
      spinInteraction_(std::clamp(spinInteraction, 0.0L, 1.0L)),
      emFieldStrength_(std::clamp(emFieldStrength, 0.0L, 1.0L)),
      renormFactor_(std::clamp(renormFactor, 0.1L, 10.0L)),
      vacuumEnergy_(std::clamp(vacuumEnergy, 0.0L, 1.0L)),
      GodWaveFreq_(std::clamp(GodWaveFreq, 0.1L, 10.0L)),
      debug_(debug),
      omega_(maxDimensions_ > 0 ? 2.0L * M_PIl / (2 * maxDimensions_ - 1) : 1.0L),
      invMaxDim_(maxDimensions_ > 0 ? 1.0L / maxDimensions_ : 1e-15L),
      totalCharge_(0.0L),
      nCubeVertices_(),
      vertexMomenta_(),
      vertexSpins_(),
      vertexWaveAmplitudes_(),
      projectedVerts_(),
      avgProjScale_(1.0L),
      needsUpdate_(true),
      cachedCos_(maxDimensions_ + 1, 0.0L),
      navigator_(nullptr),
      nurbMatterControlPoints_(5, 1.0L),
      nurbEnergyControlPoints_(5, 1.0L),
      nurbKnots_(9, 0.0L),
      nurbWeights_(5, 1.0L) {
    // Validate input parameters
    if (std::isnan(influence) || std::isinf(influence) || std::isnan(alpha) || std::isinf(alpha)) {
        throw std::invalid_argument("Invalid input parameters: NaN or Inf detected");
    }

    // Initialize NURBS control points and knots
    nurbMatterControlPoints_ = {1.0L, 0.8L, 0.5L, 0.3L, 0.1L};
    nurbEnergyControlPoints_ = {0.1L, 0.5L, 1.0L, 1.5L, 2.0L};
    nurbKnots_ = {0.0L, 0.0L, 0.0L, 0.0L, 0.5L, 1.0L, 1.0L, 1.0L, 1.0L};
    nurbWeights_ = {1.0L, 1.0L, 1.0L, 1.0L, 1.0L};

    try {
        initializeWithRetry();
        if (debug_) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cout << "[DEBUG] Initialized: maxDimensions=" << maxDimensions_ << ", mode=" << mode_.load()
                      << ", vertices=" << nCubeVertices_.size() << ", totalCharge=" << totalCharge_ << "\n";
        }
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cerr << "[ERROR] Constructor failed: " << e.what() << "\n";
        throw;
    }
}

// Copy constructor: Thread-safe deep copy
UniversalEquation::UniversalEquation(const UniversalEquation& other)
    : maxDimensions_(other.maxDimensions_),
      currentDimension_(other.currentDimension_.load()),
      mode_(other.mode_.load()),
      maxVertices_(other.maxVertices_),
      influence_(other.influence_.load()),
      weak_(other.weak_.load()),
      collapse_(other.collapse_.load()),
      twoD_(other.twoD_.load()),
      threeDInfluence_(other.threeDInfluence_.load()),
      oneDPermeation_(other.oneDPermeation_.load()),
      nurbMatterStrength_(other.nurbMatterStrength_.load()),
      nurbEnergyStrength_(other.nurbEnergyStrength_.load()),
      alpha_(other.alpha_.load()),
      beta_(other.beta_.load()),
      carrollFactor_(other.carrollFactor_.load()),
      meanFieldApprox_(other.meanFieldApprox_.load()),
      asymCollapse_(other.asymCollapse_.load()),
      perspectiveTrans_(other.perspectiveTrans_.load()),
      perspectiveFocal_(other.perspectiveFocal_.load()),
      spinInteraction_(other.spinInteraction_.load()),
      emFieldStrength_(other.emFieldStrength_.load()),
      renormFactor_(other.renormFactor_.load()),
      vacuumEnergy_(other.vacuumEnergy_.load()),
      GodWaveFreq_(other.GodWaveFreq_.load()),
      debug_(other.debug_.load()),
      omega_(other.omega_),
      invMaxDim_(other.invMaxDim_),
      totalCharge_(other.totalCharge_),
      interactions_(other.interactions_),
      nCubeVertices_(other.nCubeVertices_),
      vertexMomenta_(other.vertexMomenta_),
      vertexSpins_(other.vertexSpins_),
      vertexWaveAmplitudes_(other.vertexWaveAmplitudes_),
      projectedVerts_(other.projectedVerts_),
      avgProjScale_(other.avgProjScale_),
      needsUpdate_(other.needsUpdate_.load()),
      cachedCos_(other.cachedCos_),
      navigator_(other.navigator_),
      nurbMatterControlPoints_(other.nurbMatterControlPoints_),
      nurbEnergyControlPoints_(other.nurbEnergyControlPoints_),
      nurbKnots_(other.nurbKnots_),
      nurbWeights_(other.nurbWeights_) {
    try {
        initializeWithRetry();
        if (debug_) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cout << "[DEBUG] Copy constructor initialized: maxDimensions=" << maxDimensions_
                      << ", vertices=" << nCubeVertices_.size() << "\n";
        }
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cerr << "[ERROR] Copy constructor failed: " << e.what() << "\n";
        throw;
    }
}

// Copy assignment: Thread-safe state transfer
UniversalEquation& UniversalEquation::operator=(const UniversalEquation& other) {
    if (this != &other) {
        std::lock_guard<std::mutex> lock(mutex_);
        maxDimensions_ = other.maxDimensions_;
        currentDimension_.store(other.currentDimension_.load());
        mode_.store(other.mode_.load());
        maxVertices_ = other.maxVertices_;
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
        omega_ = other.omega_;
        invMaxDim_ = other.invMaxDim_;
        totalCharge_ = other.totalCharge_;
        interactions_ = other.interactions_;
        nCubeVertices_ = other.nCubeVertices_;
        vertexMomenta_ = other.vertexMomenta_;
        vertexSpins_ = other.vertexSpins_;
        vertexWaveAmplitudes_ = other.vertexWaveAmplitudes_;
        projectedVerts_ = other.projectedVerts_;
        avgProjScale_ = other.avgProjScale_;
        needsUpdate_.store(other.needsUpdate_.load());
        cachedCos_ = other.cachedCos_;
        navigator_ = other.navigator_;
        nurbMatterControlPoints_ = other.nurbMatterControlPoints_;
        nurbEnergyControlPoints_ = other.nurbEnergyControlPoints_;
        nurbKnots_ = other.nurbKnots_;
        nurbWeights_ = other.nurbWeights_;
        try {
            initializeWithRetry();
            if (debug_) {
                std::lock_guard<std::mutex> lock(debugMutex_);
                std::cout << "[DEBUG] Copy assignment completed: maxDimensions=" << maxDimensions_
                          << ", vertices=" << nCubeVertices_.size() << "\n";
            }
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cerr << "[ERROR] Copy assignment failed: " << e.what() << "\n";
            throw;
        }
    }
    return *this;
}

// Initializes n-cube vertices, momenta, spins, and amplitudes
void UniversalEquation::initializeNCube() {
    std::lock_guard<std::mutex> lock(mutex_);
    // Clear vectors safely
    nCubeVertices_.resize(0);
    vertexMomenta_.resize(0);
    vertexSpins_.resize(0);
    vertexWaveAmplitudes_.resize(0);
    uint64_t numVertices = std::min(static_cast<uint64_t>(1ULL << currentDimension_.load()), maxVertices_);
    // Cap vertices to prevent memory exhaustion
    numVertices = std::min(numVertices, static_cast<uint64_t>(1ULL << 10)); // Limit to 1024 vertices
    nCubeVertices_.reserve(numVertices);
    vertexMomenta_.reserve(numVertices);
    vertexSpins_.reserve(numVertices);
    vertexWaveAmplitudes_.reserve(numVertices);
    totalCharge_ = 0.0L;

    if (debug_) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Initializing nCube with " << numVertices << " vertices for dimension "
                  << currentDimension_.load() << "\n";
    }

    try {
        if (numVertices > 1000) {
            #pragma omp parallel
            {
                std::vector<std::vector<long double>> localVertices;
                std::vector<std::vector<long double>> localMomenta;
                std::vector<long double> localSpins, localAmplitudes;
                localVertices.reserve(numVertices / omp_get_num_threads() + 1);
                localMomenta.reserve(numVertices / omp_get_num_threads() + 1);
                localSpins.reserve(numVertices / omp_get_num_threads() + 1);
                localAmplitudes.reserve(numVertices / omp_get_num_threads() + 1);
                #pragma omp for schedule(dynamic)
                for (uint64_t i = 0; i < numVertices; ++i) {
                    std::vector<long double> vertex(currentDimension_.load(), 0.0L);
                    std::vector<long double> momentum(currentDimension_.load(), 0.0L);
                    for (int j = 0; j < currentDimension_.load(); ++j) {
                        vertex[j] = (i & (1ULL << j)) ? 1.0L : -1.0L;
                        momentum[j] = std::sin(static_cast<long double>(i + j) * 0.1L) * 0.1L;
                    }
                    long double spin = (i % 2 == 0) ? 0.5L : -0.5L;
                    long double amplitude = std::sin(static_cast<long double>(i) * 0.2L) * oneDPermeation_.load();
                    localVertices.push_back(std::move(vertex));
                    localMomenta.push_back(std::move(momentum));
                    localSpins.push_back(spin);
                    localAmplitudes.push_back(amplitude);
                }
                #pragma omp critical
                {
                    nCubeVertices_.insert(nCubeVertices_.end(), localVertices.begin(), localVertices.end());
                    vertexMomenta_.insert(vertexMomenta_.end(), localMomenta.begin(), localMomenta.end());
                    vertexSpins_.insert(vertexSpins_.end(), localSpins.begin(), localSpins.end());
                    vertexWaveAmplitudes_.insert(vertexWaveAmplitudes_.end(), localAmplitudes.begin(), localAmplitudes.end());
                    totalCharge_ += static_cast<long double>(localSpins.size());
                }
            }
        } else {
            for (uint64_t i = 0; i < numVertices; ++i) {
                std::vector<long double> vertex(currentDimension_.load(), 0.0L);
                std::vector<long double> momentum(currentDimension_.load(), 0.0L);
                for (int j = 0; j < currentDimension_.load(); ++j) {
                    vertex[j] = (i & (1ULL << j)) ? 1.0L : -1.0L;
                    momentum[j] = std::sin(static_cast<long double>(i + j) * 0.1L) * 0.1L;
                }
                long double spin = (i % 2 == 0) ? 0.5L : -0.5L;
                long double amplitude = std::sin(static_cast<long double>(i) * 0.2L) * oneDPermeation_.load();
                nCubeVertices_.push_back(std::move(vertex));
                vertexMomenta_.push_back(std::move(momentum));
                vertexSpins_.push_back(spin);
                vertexWaveAmplitudes_.push_back(amplitude);
                totalCharge_ += 1.0L;
            }
        }
        totalCharge_ = numVertices > 0 ? totalCharge_ / numVertices : 0.0L;
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cerr << "[ERROR] initializeNCube failed: " << e.what() << "\n";
        throw;
    }

    // Validate vector sizes
    if (nCubeVertices_.size() != numVertices || vertexMomenta_.size() != numVertices ||
        vertexSpins_.size() != numVertices || vertexWaveAmplitudes_.size() != numVertices) {
        throw std::runtime_error("Vector size mismatch in initializeNCube");
    }

    if (debug_ && nCubeVertices_.size() <= 100) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Initialized nCube with " << nCubeVertices_.size() << " vertices, totalCharge="
                  << totalCharge_ << "\n";
    }
}

// Evolves system over time
void UniversalEquation::evolveTimeStep(long double dt) {
    if (dt <= 0.0L || std::isnan(dt) || std::isinf(dt)) {
        throw std::invalid_argument("Invalid time step");
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (nCubeVertices_.size() != vertexMomenta_.size() || nCubeVertices_.size() != vertexWaveAmplitudes_.size()) {
        throw std::runtime_error("Mismatch between vertices, momenta, and amplitudes");
    }
    #pragma omp parallel for schedule(dynamic)
    for (size_t i = 0; i < nCubeVertices_.size(); ++i) {
        auto& vertex = nCubeVertices_[i];
        const auto& momentum = vertexMomenta_[i];
        auto emField = computeEMField(i);
        long double GodWaveAmp = computeGodWaveAmplitude(i, 0.0L);
        long double nurbEnergy = computeNurbEnergy(0.0L);
        for (size_t j = 0; j < vertex.size(); ++j) {
            vertex[j] += dt * (momentum[j] + emField[j] * vertexSpins_[i] + GodWaveAmp * nurbEnergy * 0.1L);
            vertex[j] = std::clamp(vertex[j], -1e6L, 1e6L); // Prevent overflow
        }
        vertexSpins_[i] = vertexSpins_[i] * std::cos(dt * emFieldStrength_.load()) +
                          (vertexSpins_[i] > 0 ? -0.5L : 0.5L) * std::sin(dt * emFieldStrength_.load());
        vertexWaveAmplitudes_[i] = vertexWaveAmplitudes_[i] * std::cos(dt * GodWaveFreq_.load()) +
                                   std::sin(static_cast<long double>(i) * dt * 0.2L) * oneDPermeation_.load();
    }
    needsUpdate_ = true;
    updateInteractions();
}

// Updates vertex momenta
void UniversalEquation::updateMomentum() {
    std::lock_guard<std::mutex> lock(mutex_);
    #pragma omp parallel for schedule(dynamic)
    for (size_t i = 0; i < vertexMomenta_.size(); ++i) {
        auto& momentum = vertexMomenta_[i];
        auto emField = computeEMField(i);
        long double GodWaveAmp = computeGodWaveAmplitude(i, 0.0L);
        long double nurbMatter = computeNurbMatter(0.0L);
        for (size_t j = 0; j < momentum.size(); ++j) {
            momentum[j] += std::sin(static_cast<long double>(i + j) * 0.1L) * emFieldStrength_.load() +
                           emField[j] * vertexSpins_[i] + GodWaveAmp * nurbMatter * 0.05L;
            momentum[j] = std::clamp(momentum[j], -0.999L, 0.999L);
        }
    }
}

// Initializes with retry logic
void UniversalEquation::initializeWithRetry() {
    int attempts = 0;
    const int maxAttempts = 3;
    while (currentDimension_.load() >= 1 && attempts < maxAttempts) {
        try {
            initializeNCube();
            {
                std::lock_guard<std::mutex> lock(mutex_);
                cachedCos_.resize(maxDimensions_ + 1);
                if (maxDimensions_ > 10) {
                    #pragma omp parallel for schedule(dynamic)
                    for (int i = 0; i <= maxDimensions_; ++i) {
                        cachedCos_[i] = std::cos(omega_ * i);
                    }
                } else {
                    for (int i = 0; i <= maxDimensions_; ++i) {
                        cachedCos_[i] = std::cos(omega_ * i);
                    }
                }
            }
            updateInteractions();
            return;
        } catch (const std::bad_alloc& e) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cerr << "[ERROR] Memory allocation failed for " << (1ULL << currentDimension_.load())
                      << " vertices. Reducing dimension to " << (currentDimension_.load() - 1)
                      << ". Attempt " << (attempts + 1) << "/" << maxAttempts << ".\n";
            if (currentDimension_.load() == 1) {
                throw std::runtime_error("Failed to allocate memory even at dimension 1");
            }
            currentDimension_.store(currentDimension_.load() - 1);
            mode_.store(currentDimension_.load());
            maxVertices_ = 1ULL << std::min(currentDimension_.load(), 20);
            needsUpdate_ = true;
            ++attempts;
        }
    }
    if (attempts >= maxAttempts) {
        throw std::runtime_error("Max retry attempts reached for initialization");
    }
}

// Safe exponential function
long double UniversalEquation::safeExp(long double x) const {
    long double clamped = std::clamp(x, -500.0L, 500.0L); // Tighter bounds
    long double result = std::exp(clamped);
    if (std::isnan(result) || std::isinf(result)) {
        if (debug_) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cerr << "[ERROR] safeExp produced invalid result for x=" << x << "\n";
        }
        return 1.0L;
    }
    return result;
}

// Initializes with navigator
void UniversalEquation::initializeCalculator(DimensionalNavigator* navigator) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!navigator) {
        throw std::invalid_argument("Navigator pointer cannot be null");
    }
    navigator_ = navigator;
    needsUpdate_ = true;
    initializeWithRetry();
}

// Updates cache
UniversalEquation::DimensionData UniversalEquation::updateCache() {
    auto result = compute();
    DimensionData data{
        currentDimension_.load(),
        result.observable,
        result.potential,
        result.nurbMatter,
        result.nurbEnergy,
        result.spinEnergy,
        result.momentumEnergy,
        result.fieldEnergy,
        result.GodWaveEnergy
    };
    return data;
}

// Computes batch of dimension data
std::vector<UniversalEquation::DimensionData> UniversalEquation::computeBatch(int startDim, int endDim) {
    startDim = std::clamp(startDim, 1, maxDimensions_);
    endDim = std::clamp(endDim == -1 ? maxDimensions_ : endDim, startDim, maxDimensions_);
    int savedDim = currentDimension_.load();
    int savedMode = mode_.load();
    std::vector<DimensionData> results(endDim - startDim + 1);
    #pragma omp parallel for schedule(dynamic)
    for (int d = startDim; d <= endDim; ++d) {
        UniversalEquation temp(*this);
        temp.currentDimension_.store(d);
        temp.mode_.store(d);
        temp.needsUpdate_.store(true);
        try {
            temp.initializeWithRetry();
            results[d - startDim] = temp.updateCache();
        } catch (const std::exception& e) {
            if (debug_) {
                std::lock_guard<std::mutex> lock(debugMutex_);
                std::cerr << "[ERROR] Batch computation failed for dimension " << d << ": " << e.what() << "\n";
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

// Exports to CSV
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

// Advances simulation cycle
void UniversalEquation::advanceCycle() {
    int newDimension = (currentDimension_.load() == maxDimensions_) ? 1 : currentDimension_.load() + 1;
    currentDimension_.store(newDimension);
    mode_.store(newDimension);
    needsUpdate_.store(true);
    initializeWithRetry();
}