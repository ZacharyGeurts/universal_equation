// universal_equation.cpp: Core implementation of the UniversalEquation class for quantum simulation on n-dimensional hypercube lattices.
// Models a 19-dimensional reality with stronger influences from 2D and 4D on 3D properties, using weighted dimensional contributions.
// Quantum-specific computations are implemented in universal_equation_quantum.cpp.

// How this ties into the quantum world:
// The UniversalEquation class simulates quantum phenomena on an n-dimensional hypercube lattice, integrating classical and quantum physics.
// It models a 1-inch cube of water as a single vertex, with physical properties (mass, volume, density) influenced by projections from 
// other dimensions (strongest from 2D and 4D in a 19D reality). The "God wave" models quantum coherence, and NURBS fields represent 
// matter and energy distributions. Dimensional interactions are weighted using a Gaussian function centered at 3D, ensuring 2D and 4D 
// have greater impact on 3D properties than higher dimensions. Thread-safe, high-precision calculations are optimized with OpenMP.

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

// ANSI color codes
#define RESET "\033[0m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"
#define BOLD "\033[1m"

// Helper: Safe division to prevent NaN/Inf
inline long double safe_div(long double a, long double b) {
    return (b != 0.0L && !std::isnan(b) && !std::isinf(b)) ? a / b : 0.0L;
}

// Helper: Gaussian weight for dimensional influence, centered at 3D
inline long double dimensionalWeight(int dim, int centerDim = 3, long double sigma = 1.0L) {
    long double diff = static_cast<long double>(dim - centerDim);
    return std::exp(-diff * diff / (2.0L * sigma * sigma));
}

// Constructor: Initializes quantum simulation with dimensional influences
UniversalEquation::UniversalEquation(
    int maxDimensions, int mode, long double influence, long double weak, long double collapse,
    long double twoD, long double threeDInfluence, long double oneDPermeation, long double nurbMatterStrength,
    long double nurbEnergyStrength, long double alpha, long double beta, long double carrollFactor,
    long double meanFieldApprox, long double asymCollapse, long double perspectiveTrans,
    long double perspectiveFocal, long double spinInteraction, long double emFieldStrength,
    long double renormFactor, long double vacuumEnergy, long double GodWaveFreq, bool debug)
    : maxDimensions_(std::max(1, std::min(maxDimensions == 0 ? 19 : maxDimensions, 19))),
      mode_(std::clamp(mode, 1, maxDimensions_)),
      currentDimension_(std::clamp(mode, 1, maxDimensions_)),
      maxVertices_(1ULL), // Single vertex for water cube
      omega_(maxDimensions_ > 0 ? 2.0L * std::numbers::pi_v<long double> / (2 * maxDimensions_ - 1) : 1.0L),
      invMaxDim_(maxDimensions_ > 0 ? 1.0L / maxDimensions_ : 1e-15L),
      totalCharge_(0.0L),
      needsUpdate_(true),
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
      nCubeVertices_(),
      vertexMomenta_(),
      vertexSpins_(),
      vertexWaveAmplitudes_(),
      interactions_(),
      projectedVerts_(),
      avgProjScale_(1.0L),
      cachedCos_(maxDimensions_ + 1, 0.0L),
      navigator_(nullptr),
      nurbMatterControlPoints_(5, 1.0L),
      nurbEnergyControlPoints_(5, 1.0L),
      nurbKnots_(9, 0.0L),
      nurbWeights_(5, 1.0L) {
    nurbMatterControlPoints_ = {1.0L, 0.8L, 0.5L, 0.3L, 0.1L};
    nurbEnergyControlPoints_ = {0.1L, 0.5L, 1.0L, 1.5L, 2.0L};
    nurbKnots_ = {0.0L, 0.0L, 0.0L, 0.0L, 0.5L, 1.0L, 1.0L, 1.0L, 1.0L};
    nurbWeights_ = {1.0L, 1.0L, 1.0L, 1.0L, 1.0L};
    try {
        // Debug check for nCubeVertices_ before initialization
        if (debug_) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cout << BOLD << CYAN << "[DEBUG] Before initializeWithRetry: nCubeVertices_ size=" << nCubeVertices_.size()
                      << ", capacity=" << nCubeVertices_.capacity() << "\n" << RESET;
            if (!nCubeVertices_.empty()) {
                std::cerr << MAGENTA << "[ERROR] nCubeVertices_ is not empty before initialization\n" << RESET;
            }
        }
        initializeWithRetry();
        if (debug_) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cout << BOLD << CYAN << "[DEBUG] Initialized: maxDimensions=" << maxDimensions_ << ", mode=" << mode_.load()
                      << ", vertices=" << nCubeVertices_.size() << ", totalCharge=" << totalCharge_ << "\n" << RESET;
        }
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cerr << MAGENTA << "[ERROR] Constructor failed: " << e.what() << "\n" << RESET;
        throw;
    }
}

// Copy constructor
UniversalEquation::UniversalEquation(const UniversalEquation& other)
    : maxDimensions_(other.maxDimensions_),
      mode_(other.mode_.load()),
      currentDimension_(other.currentDimension_.load()),
      maxVertices_(1ULL), // Single vertex
      omega_(other.omega_),
      invMaxDim_(other.invMaxDim_),
      totalCharge_(other.totalCharge_),
      needsUpdate_(other.needsUpdate_.load()),
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
      nCubeVertices_(),
      vertexMomenta_(),
      vertexSpins_(),
      vertexWaveAmplitudes_(),
      interactions_(),
      projectedVerts_(),
      avgProjScale_(other.avgProjScale_),
      cachedCos_(other.cachedCos_.size(), 0.0L),
      navigator_(nullptr),
      nurbMatterControlPoints_(other.nurbMatterControlPoints_),
      nurbEnergyControlPoints_(other.nurbEnergyControlPoints_),
      nurbKnots_(other.nurbKnots_),
      nurbWeights_(other.nurbWeights_) {
    try {
        // Lock other object's mutex for safe copying
        std::lock_guard<std::mutex> lock(other.mutex_);
        // Reserve space for single vertex
        nCubeVertices_.reserve(1);
        vertexMomenta_.reserve(1);
        vertexSpins_.reserve(1);
        vertexWaveAmplitudes_.reserve(1);
        interactions_.reserve(1);
        projectedVerts_.reserve(1);
        // Copy single vertex data
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
        }
        cachedCos_ = other.cachedCos_;
        initializeWithRetry();
        if (debug_) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cout << BOLD << CYAN << "[DEBUG] Copy constructor initialized: maxDimensions=" << maxDimensions_
                      << ", vertices=" << nCubeVertices_.size() << "\n" << RESET;
        }
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cerr << MAGENTA << "[ERROR] Copy constructor failed: " << e.what() << "\n" << RESET;
        throw;
    }
}

// Copy assignment
UniversalEquation& UniversalEquation::operator=(const UniversalEquation& other) {
    if (this != &other) {
        std::unique_lock<std::mutex> lockThis(mutex_);
        std::lock_guard<std::mutex> lockOther(other.mutex_);
        maxDimensions_ = other.maxDimensions_;
        mode_.store(other.mode_.load());
        currentDimension_.store(other.currentDimension_.load());
        maxVertices_ = 1ULL; // Single vertex
        omega_ = other.omega_;
        invMaxDim_ = other.invMaxDim_;
        totalCharge_ = other.totalCharge_;
        needsUpdate_.store(other.needsUpdate_.load());
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
            // Clear existing data
            nCubeVertices_.clear();
            vertexMomenta_.clear();
            vertexSpins_.clear();
            vertexWaveAmplitudes_.clear();
            interactions_.clear();
            projectedVerts_.clear();
            // Reserve space for single vertex
            nCubeVertices_.reserve(1);
            vertexMomenta_.reserve(1);
            vertexSpins_.reserve(1);
            vertexWaveAmplitudes_.reserve(1);
            interactions_.reserve(1);
            projectedVerts_.reserve(1);
            cachedCos_.reserve(other.cachedCos_.size());
            // Copy single vertex data
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
            }
            cachedCos_ = other.cachedCos_;
            nurbMatterControlPoints_ = other.nurbMatterControlPoints_;
            nurbEnergyControlPoints_ = other.nurbEnergyControlPoints_;
            nurbKnots_ = other.nurbKnots_;
            nurbWeights_ = other.nurbWeights_;
            initializeWithRetry();
            if (debug_) {
                std::lock_guard<std::mutex> lock(debugMutex_);
                std::cout << BOLD << CYAN << "[DEBUG] Copy assignment completed: maxDimensions=" << maxDimensions_
                          << ", vertices=" << nCubeVertices_.size() << "\n" << RESET;
            }
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cerr << MAGENTA << "[ERROR] Copy assignment failed: " << e.what() << "\n" << RESET;
            throw;
        }
    }
    return *this;
}

// Initializes n-cube vertices, momenta, spins, and amplitudes
void UniversalEquation::initializeNCube() {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        // Clear vectors with explicit destruction to avoid invalid memory access
        nCubeVertices_.clear();
        vertexMomenta_.clear();
        vertexSpins_.clear();
        vertexWaveAmplitudes_.clear();
        interactions_.clear();
        projectedVerts_.clear();

        // Reserve space for single vertex
        uint64_t numVertices = 1ULL; // Single vertex for water cube
        nCubeVertices_.reserve(numVertices);
        vertexMomenta_.reserve(numVertices);
        vertexSpins_.reserve(numVertices);
        vertexWaveAmplitudes_.reserve(numVertices);
        interactions_.reserve(numVertices);
        projectedVerts_.reserve(numVertices);

        // Initialize single vertex
        int dim = currentDimension_.load();
        if (dim < 1 || dim > maxDimensions_) {
            throw std::runtime_error("Invalid dimension: " + std::to_string(dim));
        }
        std::vector<long double> vertex(dim, 0.0L); // Center at origin
        std::vector<long double> momentum(dim, 0.0L);
        long double spin = 0.032774L;
        long double amplitude = oneDPermeation_.load();

        // Push back initialized data
        nCubeVertices_.push_back(std::move(vertex));
        vertexMomenta_.push_back(std::move(momentum));
        vertexSpins_.push_back(spin);
        vertexWaveAmplitudes_.push_back(amplitude);
        totalCharge_ = 1.0L;

        // Verify vector integrity
        if (nCubeVertices_.size() != 1 || nCubeVertices_[0].size() != static_cast<size_t>(dim)) {
            throw std::runtime_error("Failed to initialize nCubeVertices correctly");
        }

        if (debug_) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cout << BOLD << CYAN << "[DEBUG] Initialized nCube with " << nCubeVertices_.size() 
                      << " vertices, dimension=" << dim << ", totalCharge=" << totalCharge_ << "\n" << RESET;
        }
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cerr << MAGENTA << "[ERROR] initializeNCube failed: " << e.what() << "\n" << RESET;
        throw;
    }
}

// Evolves system over time with dimensional influences
void UniversalEquation::evolveTimeStep(long double dt) {
    if (dt <= 0.0L || std::isnan(dt) || std::isinf(dt)) {
        throw std::invalid_argument("Invalid time step");
    }
    std::lock_guard<std::mutex> lock(mutex_);
    long double dimInfluence = 0.0L;
    for (int d = 1; d <= maxDimensions_; ++d) {
        dimInfluence += dimensionalWeight(d) * (d == 2 || d == 4 ? 1.5L : 1.0L);
    }
    dimInfluence = safe_div(dimInfluence, maxDimensions_);
    for (size_t i = 0; i < nCubeVertices_.size(); ++i) { // No OpenMP for single vertex
        auto& vertex = nCubeVertices_[i];
        const auto& momentum = vertexMomenta_[i];
        auto emField = computeEMField(i);
        long double GodWaveAmp = computeGodWaveAmplitude(i, 0.0L);
        long double nurbEnergy = computeNurbEnergy(0.0L);
        for (size_t j = 0; j < vertex.size(); ++j) {
            long double delta = dt * (momentum[j] + emField[j] * vertexSpins_[i] + GodWaveAmp * nurbEnergy * 0.1L * dimInfluence);
            if (std::isnan(delta) || std::isinf(delta)) delta = 0.0L;
            vertex[j] += delta;
            vertex[j] = std::clamp(vertex[j], -1e3L, 1e3L);
        }
        long double cos_term = std::cos(dt * emFieldStrength_.load() * dimInfluence);
        long double sin_term = std::sin(dt * emFieldStrength_.load() * dimInfluence);
        long double flip = (vertexSpins_[i] > 0 ? -0.032774L : 0.032774L);
        vertexSpins_[i] = vertexSpins_[i] * cos_term + flip * sin_term;
        if (std::isnan(vertexSpins_[i])) vertexSpins_[i] = 0.032774L;
        long double cos_god = std::cos(dt * GodWaveFreq_.load() * dimInfluence);
        long double sin_pert = std::sin(static_cast<long double>(i) * dt * 0.2L) * oneDPermeation_.load();
        vertexWaveAmplitudes_[i] = vertexWaveAmplitudes_[i] * cos_god + sin_pert;
        if (std::isnan(vertexWaveAmplitudes_[i])) vertexWaveAmplitudes_[i] = 0.0L;
    }
    needsUpdate_.store(false);
    updateInteractions();
}

// Updates vertex momenta with dimensional influences
void UniversalEquation::updateMomentum() {
    std::lock_guard<std::mutex> lock(mutex_);
    long double dimInfluence = 0.0L;
    for (int d = 1; d <= maxDimensions_; ++d) {
        dimInfluence += dimensionalWeight(d) * (d == 2 || d == 4 ? 1.5L : 1.0L);
    }
    dimInfluence = safe_div(dimInfluence, maxDimensions_);
    for (size_t i = 0; i < vertexMomenta_.size(); ++i) { // No OpenMP for single vertex
        auto& momentum = vertexMomenta_[i];
        auto emField = computeEMField(i);
        long double GodWaveAmp = computeGodWaveAmplitude(i, 0.0L);
        long double nurbMatter = computeNurbMatter(0.0L);
        for (size_t j = 0; j < momentum.size(); ++j) {
            long double delta = (std::sin(static_cast<long double>(i + j) * 0.1L) * emFieldStrength_.load() +
                               emField[j] * vertexSpins_[i] + GodWaveAmp * nurbMatter * 0.05L) * dimInfluence;
            if (std::isnan(delta) || std::isinf(delta)) delta = 0.0L;
            momentum[j] += delta;
            momentum[j] = std::clamp(momentum[j], -0.9L, 0.9L);
        }
    }
}

// Initializes with retry logic
void UniversalEquation::initializeWithRetry() {
    int attempts = 0;
    const int maxAttempts = 5;
    while (currentDimension_.load() >= 1 && attempts < maxAttempts) {
        try {
            initializeNCube();
            std::lock_guard<std::mutex> lock(mutex_);
            cachedCos_.resize(maxDimensions_ + 1);
            for (int i = 0; i <= maxDimensions_; ++i) {
                cachedCos_[i] = std::cos(omega_ * i);
            }
            updateInteractions();
            return;
        } catch (const std::bad_alloc& e) {
            std::lock_guard<std::mutex> lock(debugMutex_);
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
    long double clamped = std::clamp(x, -500.0L, 500.0L);
    long double result = std::exp(clamped);
    if (std::isnan(result) || std::isinf(result)) {
        if (debug_) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cerr << MAGENTA << "[ERROR] safeExp produced invalid result for x=" << x << "\n" << RESET;
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
    needsUpdate_.store(true);
    initializeWithRetry();
}

// Updates cache with dimensional influences
UniversalEquation::DimensionData UniversalEquation::updateCache() {
    long double dimInfluence = 0.0L;
    for (int d = 1; d <= maxDimensions_; ++d) {
        dimInfluence += dimensionalWeight(d) * (d == 2 || d == 4 ? 1.5L : 1.0L);
    }
    dimInfluence = safe_div(dimInfluence, maxDimensions_);
    auto result = compute();
    DimensionData data{
        currentDimension_.load(),
        result.observable * dimInfluence,
        result.potential * dimInfluence,
        result.nurbMatter * dimInfluence,
        result.nurbEnergy * dimInfluence,
        result.spinEnergy * dimInfluence,
        result.momentumEnergy * dimInfluence,
        result.fieldEnergy * dimInfluence,
        result.GodWaveEnergy * dimInfluence
    };
    return data;
}

// Computes batch of dimension data with weighted contributions
std::vector<UniversalEquation::DimensionData> UniversalEquation::computeBatch(int startDim, int endDim) {
    startDim = std::clamp(startDim, 1, maxDimensions_);
    endDim = std::clamp(endDim == -1 ? maxDimensions_ : endDim, startDim, maxDimensions_);
    int savedDim = currentDimension_.load();
    int savedMode = mode_.load();
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
    #pragma omp parallel for schedule(dynamic)
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
                        DimensionData kData = tempK.updateCache();
                        long double weight = weights[k];
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
                            std::lock_guard<std::mutex> lock(debugMutex_);
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
                std::lock_guard<std::mutex> lock(debugMutex_);
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