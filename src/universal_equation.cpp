// universal_equation.cpp: Core implementation of the UniversalEquation class for quantum simulation on n-dimensional hypercube lattices.
// Models a 19-dimensional reality with stronger influences from 2D and 4D on 3D properties, using weighted dimensional contributions.
// Quantum-specific computations are implemented in universal_equation_quantum.cpp.

// How this ties into the quantum world:
// The UniversalEquation class simulates quantum phenomena on an n-dimensional hypercube lattice, integrating classical and quantum physics.
// It models a system with multiple vertices (representing particles or points in a 1-inch cube of water), with physical properties (mass, volume, density)
// influenced by projections from other dimensions (strongest from 2D and 4D in a 19D reality). The "God wave" models quantum coherence, and NURBS fields
// represent matter and energy distributions. Dimensional interactions are weighted using a Gaussian function centered at 3D, ensuring 2D and 4D have
// greater impact on 3D properties than higher dimensions. Thread-safe, high-precision calculations are optimized with OpenMP.

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
#include <atomic>
#include <latch>
#include <syncstream>
#include <omp.h>
#include <sstream>
#include <iomanip>
#include <limits>

// ANSI color codes
#define RESET "\033[0m"
#define MAGENTA "\033[1;35m" // Bold magenta for errors
#define CYAN "\033[1;36m"    // Bold cyan for debug
#define YELLOW "\033[1;33m"  // Bold yellow for warnings
#define GREEN "\033[1;32m"   // Bold green for info
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

// Constructor: Initializes quantum simulation with dimensional influences and multiple vertices
UniversalEquation::UniversalEquation(
    int maxDimensions, int mode, long double influence, long double weak, long double collapse,
    long double twoD, long double threeDInfluence, long double oneDPermeation, long double nurbMatterStrength,
    long double nurbEnergyStrength, long double alpha, long double beta, long double carrollFactor,
    long double meanFieldApprox, long double asymCollapse, long double perspectiveTrans,
    long double perspectiveFocal, long double spinInteraction, long double emFieldStrength,
    long double renormFactor, long double vacuumEnergy, long double GodWaveFreq, bool debug,
    uint64_t numVertices)
    : maxDimensions_(std::max(1, std::min(maxDimensions <= 0 ? 19 : maxDimensions, 19))),
      mode_(std::clamp(mode <= 0 ? 1 : mode, 1, maxDimensions_)),
      currentDimension_(std::clamp(mode <= 0 ? 1 : mode, 1, maxDimensions_)),
      maxVertices_(std::max<uint64_t>(1ULL, std::min(numVertices, static_cast<uint64_t>(1ULL << 20)))),
      omega_(maxDimensions_ > 0 ? 2.0L * std::numbers::pi_v<long double> / (2 * maxDimensions_ - 1) : 1.0L),
      invMaxDim_(maxDimensions_ > 0 ? 1.0L / maxDimensions_ : 1e-15L),
      totalCharge_(0.0L),
      needsUpdate_(true),
      nCubeVertices_(std::vector<std::vector<long double>>()),
      vertexMomenta_(std::vector<std::vector<long double>>()),
      vertexSpins_(std::vector<long double>()),
      vertexWaveAmplitudes_(std::vector<long double>()),
      interactions_(std::vector<DimensionInteraction>()),
      projectedVerts_(std::vector<glm::vec3>()),
      avgProjScale_(1.0L),
      cachedCos_(maxDimensions_ + 1, 0.0L),
      navigator_(nullptr),
      nurbMatterControlPoints_(5, 1.0L),
      nurbEnergyControlPoints_(5, 1.0L),
      nurbKnots_(9, 0.0L),
      nurbWeights_(5, 1.0L),
      dimensionData_(std::vector<DimensionData>()),
      influence_(std::clamp(influence, 0.0L, 10.0L)),
      weak_(std::clamp(weak, 0.0L, 1.0L)),
      collapse_(std::clamp(collapse, 0.0L, 5.0L)),
      twoD_(std::clamp(twoD, 0.0L, 5.0L)),
      threeDInfluence_(std::clamp(threeDInfluence, 0.0L, 5.0L)),
      oneDPermeation_(std::clamp(oneDPermeation, 0.0L, 5.0L)),
      nurbMatterStrength_(std::clamp(nurbMatterStrength, 0.0L, 1.0L)),
      nurbEnergyStrength_(std::clamp(nurbEnergyStrength, 0.0L, 2.0L)),
      alpha_(std::clamp(alpha, 0.01L, 10.0L)),
      beta_(std::clamp(beta, 0.0L, 1.0L)),
      carrollFactor_(std::clamp(carrollFactor, 0.0L, 1.0L)),
      meanFieldApprox_(std::clamp(meanFieldApprox, 0.0L, 1.0L)),
      asymCollapse_(std::clamp(asymCollapse, 0.0L, 1.0L)),
      perspectiveTrans_(std::clamp(perspectiveTrans, 0.0L, 10.0L)),
      perspectiveFocal_(std::clamp(perspectiveFocal, 1.0L, 20.0L)),
      spinInteraction_(std::clamp(spinInteraction, 0.0L, 1.0L)),
      emFieldStrength_(std::clamp(emFieldStrength, 0.0L, 1.0e7L)),
      renormFactor_(std::clamp(renormFactor, 0.1L, 10.0L)),
      vacuumEnergy_(std::clamp(vacuumEnergy, 0.0L, 1.0L)),
      GodWaveFreq_(std::clamp(GodWaveFreq, 0.1L, 10.0L)),
      debug_(debug) {
    // Validation to ensure currentDimension_ and mode_ are not 0
    if (mode <= 0 || maxDimensions <= 0) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] maxDimensions and mode must be greater than 0" RESET << std::endl;
        throw std::invalid_argument("maxDimensions and mode must be greater than 0");
    }

    // Debug: Log initialization state
    if (debug_.load()) {
        std::osyncstream(std::cout) << GREEN << "[INFO] Starting UniversalEquation initialization with maxDimensions="
                                    << maxDimensions_ << ", mode=" << mode_.load() 
                                    << ", vertices=" << maxVertices_ << ", debug_=" << debug_.load()
                                    << ", nCubeVertices_.data=" << nCubeVertices_.data() << RESET << std::endl;
    }

    // Warning: Check for clamped parameters
    if (debug_.load() && (influence != influence_.load() || weak != weak_.load() || collapse != collapse_.load() ||
                          twoD != twoD_.load() || threeDInfluence != threeDInfluence_.load() ||
                          oneDPermeation != oneDPermeation_.load() || nurbMatterStrength != nurbMatterStrength_.load() ||
                          nurbEnergyStrength != nurbEnergyStrength_.load() || alpha != alpha_.load() ||
                          beta != beta_.load() || carrollFactor != carrollFactor_.load() ||
                          meanFieldApprox != meanFieldApprox_.load() || asymCollapse != asymCollapse_.load() ||
                          perspectiveTrans != perspectiveTrans_.load() || perspectiveFocal != perspectiveFocal_.load() ||
                          spinInteraction != spinInteraction_.load() || emFieldStrength != emFieldStrength_.load() ||
                          renormFactor != renormFactor_.load() || vacuumEnergy != vacuumEnergy_.load() ||
                          GodWaveFreq != GodWaveFreq_.load())) {
        std::osyncstream(std::cout) << YELLOW << "[WARNING] Some input parameters were clamped to valid ranges" RESET << std::endl;
    }

    nurbMatterControlPoints_ = {1.0L, 0.8L, 0.5L, 0.3L, 0.1L};
    nurbEnergyControlPoints_ = {0.1L, 0.5L, 1.0L, 1.5L, 2.0L};
    nurbKnots_ = {0.0L, 0.0L, 0.0L, 0.0L, 0.5L, 1.0L, 1.0L, 1.0L, 1.0L};
    nurbWeights_ = {1.0L, 1.0L, 1.0L, 1.0L, 1.0L};
    try {
        // Ensure all vectors are in a clean state
        nCubeVertices_ = std::vector<std::vector<long double>>();
        vertexMomenta_ = std::vector<std::vector<long double>>();
        vertexSpins_ = std::vector<long double>();
        vertexWaveAmplitudes_ = std::vector<long double>();
        interactions_ = std::vector<DimensionInteraction>();
        projectedVerts_ = std::vector<glm::vec3>();
        dimensionData_ = std::vector<DimensionData>();
        initializeWithRetry();
        if (debug_.load()) {
            std::osyncstream(std::cout) << CYAN << "[DEBUG] Initialized UniversalEquation: maxDimensions=" << maxDimensions_
                                        << ", mode=" << mode_.load() << ", vertices=" << nCubeVertices_.size()
                                        << ", totalCharge=" << totalCharge_.load() << ", nCubeVertices_.data=" << nCubeVertices_.data() << RESET << std::endl;
        }
    } catch (const std::exception& e) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Constructor failed: " << e.what() << RESET << std::endl;
        throw;
    }
}

// Copy constructor
UniversalEquation::UniversalEquation(const UniversalEquation& other)
    : maxDimensions_(other.maxDimensions_),
      mode_(other.mode_.load()),
      currentDimension_(other.currentDimension_.load()),
      maxVertices_(other.maxVertices_),
      omega_(other.omega_),
      invMaxDim_(other.invMaxDim_),
      totalCharge_(other.totalCharge_.load()),
      needsUpdate_(other.needsUpdate_.load()),
      nCubeVertices_(other.nCubeVertices_),
      vertexMomenta_(other.vertexMomenta_),
      vertexSpins_(other.vertexSpins_),
      vertexWaveAmplitudes_(other.vertexWaveAmplitudes_),
      interactions_(other.interactions_),
      projectedVerts_(other.projectedVerts_),
      avgProjScale_(other.avgProjScale_.load()),
      cachedCos_(other.cachedCos_),
      navigator_(nullptr),
      nurbMatterControlPoints_(other.nurbMatterControlPoints_),
      nurbEnergyControlPoints_(other.nurbEnergyControlPoints_),
      nurbKnots_(other.nurbKnots_),
      nurbWeights_(other.nurbWeights_),
      dimensionData_(other.dimensionData_),
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
      debug_(other.debug_.load()) {
    if (debug_.load()) {
        std::osyncstream(std::cout) << GREEN << "[INFO] Starting copy constructor for UniversalEquation, debug_=" << debug_.load() << RESET << std::endl;
    }
    try {
        initializeWithRetry();
        if (debug_.load()) {
            std::osyncstream(std::cout) << CYAN << "[DEBUG] Copy constructor initialized: maxDimensions=" << maxDimensions_
                                        << ", vertices=" << nCubeVertices_.size() << ", debug_=" << debug_.load() << RESET << std::endl;
        }
    } catch (const std::exception& e) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Copy constructor failed: " << e.what() << RESET << std::endl;
        throw;
    }
}

// Copy assignment
UniversalEquation& UniversalEquation::operator=(const UniversalEquation& other) {
    if (this != &other) {
        if (debug_.load()) {
            std::osyncstream(std::cout) << GREEN << "[INFO] Starting copy assignment for UniversalEquation, debug_=" << debug_.load() << RESET << std::endl;
        }
        maxDimensions_ = other.maxDimensions_;
        mode_.store(other.mode_.load());
        currentDimension_.store(other.currentDimension_.load());
        maxVertices_ = other.maxVertices_;
        omega_ = other.omega_;
        invMaxDim_ = other.invMaxDim_;
        totalCharge_.store(other.totalCharge_.load());
        needsUpdate_.store(other.needsUpdate_.load());
        nCubeVertices_ = other.nCubeVertices_;
        vertexMomenta_ = other.vertexMomenta_;
        vertexSpins_ = other.vertexSpins_;
        vertexWaveAmplitudes_ = other.vertexWaveAmplitudes_;
        interactions_ = other.interactions_;
        projectedVerts_ = other.projectedVerts_;
        avgProjScale_.store(other.avgProjScale_.load());
        cachedCos_ = other.cachedCos_;
        navigator_ = nullptr;
        nurbMatterControlPoints_ = other.nurbMatterControlPoints_;
        nurbEnergyControlPoints_ = other.nurbEnergyControlPoints_;
        nurbKnots_ = other.nurbKnots_;
        nurbWeights_ = other.nurbWeights_;
        dimensionData_ = other.dimensionData_;
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
            if (debug_.load()) {
                std::osyncstream(std::cout) << CYAN << "[DEBUG] Copy assignment completed: maxDimensions=" << maxDimensions_
                                            << ", vertices=" << nCubeVertices_.size() << ", debug_=" << debug_.load() << RESET << std::endl;
            }
        } catch (const std::exception& e) {
            std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Copy assignment failed: " << e.what() << RESET << std::endl;
            throw;
        }
    }
    return *this;
}

// Initializes n-cube vertices, momenta, spins, and amplitudes for multiple vertices
void UniversalEquation::initializeNCube() {
    std::latch init_latch(1);
    try {
        // Safely reset vectors by assigning empty vectors
        nCubeVertices_ = std::vector<std::vector<long double>>();
        vertexMomenta_ = std::vector<std::vector<long double>>();
        vertexSpins_ = std::vector<long double>();
        vertexWaveAmplitudes_ = std::vector<long double>();
        interactions_ = std::vector<DimensionInteraction>();
        projectedVerts_ = std::vector<glm::vec3>();

        // Debug logging after vector initialization
        if (debug_.load()) {
            std::osyncstream(std::cout) << GREEN << "[INFO] Initializing n-cube with " << maxVertices_ << " vertices, debug_=" << debug_.load() << RESET << std::endl;
            std::osyncstream(std::cout) << CYAN << "[DEBUG] nCubeVertices_ state before reset: size=" << nCubeVertices_.size()
                                        << ", capacity=" << nCubeVertices_.capacity() << ", data=" << nCubeVertices_.data() << RESET << std::endl;
        }
        if (debug_.load() && maxVertices_ > 1000) {
            std::osyncstream(std::cout) << YELLOW << "[WARNING] High vertex count (" << maxVertices_ << ") may impact performance" RESET << std::endl;
        }

        // Reserve capacity
        nCubeVertices_.reserve(maxVertices_);
        vertexMomenta_.reserve(maxVertices_);
        vertexSpins_.reserve(maxVertices_);
        vertexWaveAmplitudes_.reserve(maxVertices_);
        interactions_.reserve(maxVertices_);
        projectedVerts_.reserve(maxVertices_);
        totalCharge_.store(0.0L);

        // Initialize multiple vertices with slight positional variations
        for (uint64_t i = 0; i < maxVertices_; ++i) {
            std::vector<long double> vertex(currentDimension_.load(), 0.0L);
            std::vector<long double> momentum(currentDimension_.load(), 0.0L);
            for (int j = 0; j < currentDimension_.load(); ++j) {
                vertex[j] = (static_cast<long double>(i) / maxVertices_) * 0.1L;
                momentum[j] = (static_cast<long double>(i % 2) - 0.5L) * 0.01L;
            }
            long double spin = (i % 2 == 0 ? 0.032774L : -0.032774L);
            long double amplitude = oneDPermeation_.load() * (1.0L + 0.1L * (i / static_cast<long double>(maxVertices_)));
            nCubeVertices_.push_back(std::move(vertex));
            vertexMomenta_.push_back(std::move(momentum));
            vertexSpins_.push_back(spin);
            vertexWaveAmplitudes_.push_back(amplitude);
            interactions_.push_back(DimensionInteraction(
                static_cast<int>(i), 0.0L, 0.0L, std::vector<long double>(std::min(3, currentDimension_.load()), 0.0L), 0.0L));
            projectedVerts_.push_back(glm::vec3(0.0f, 0.0f, 0.0f));
            totalCharge_.fetch_add(1.0L / maxVertices_);
        }

        if (debug_.load()) {
            std::osyncstream(std::cout) << CYAN << "[DEBUG] Initialized nCube with " << nCubeVertices_.size() << " vertices, totalCharge="
                                        << totalCharge_.load() << ", nCubeVertices_.data=" << nCubeVertices_.data() << ", debug_=" << debug_.load() << RESET << std::endl;
        }
        init_latch.count_down();
    } catch (const std::exception& e) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] initializeNCube failed: " << e.what() << RESET << std::endl;
        throw;
    }
    init_latch.wait(); // Ensure initialization completes
}

// Evolves system over time with dimensional influences
void UniversalEquation::evolveTimeStep(long double dt) {
    if (dt <= 0.0L || std::isnan(dt) || std::isinf(dt)) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Invalid time step" RESET << std::endl;
        throw std::invalid_argument("Invalid time step");
    }
    if (debug_.load()) {
        std::osyncstream(std::cout) << GREEN << "[INFO] Starting time step evolution with dt=" << dt << RESET << std::endl;
    }
    long double dimInfluence = 0.0L;
    for (int d = 1; d <= maxDimensions_; ++d) {
        dimInfluence += dimensionalWeight(d) * (d == 2 || d == 4 ? 1.5L : 1.0L);
    }
    dimInfluence = safe_div(dimInfluence, maxDimensions_);

    bool anyClamped = false;
    #pragma omp parallel for if(maxVertices_ > 100)
    for (size_t i = 0; i < nCubeVertices_.size(); ++i) {
        auto& vertex = nCubeVertices_[i];
        const auto& momentum = vertexMomenta_[i];
        auto emField = computeEMField(i);
        long double GodWaveAmp = computeGodWaveAmplitude(i, 0.0L);
        long double nurbEnergy = computeNurbEnergy(0.0L);
        for (size_t j = 0; j < vertex.size(); ++j) {
            long double delta = dt * (momentum[j] + emField[j] * vertexSpins_[i] + GodWaveAmp * nurbEnergy * 0.1L * dimInfluence);
            if (std::isnan(delta) || std::isinf(delta)) {
                delta = 0.0L;
                #pragma omp critical
                anyClamped = true;
            }
            vertex[j] += delta;
            if (vertex[j] < -1e3L || vertex[j] > 1e3L) {
                vertex[j] = std::clamp(vertex[j], -1e3L, 1e3L);
                #pragma omp critical
                anyClamped = true;
            }
        }
        long double cos_term = std::cos(dt * emFieldStrength_.load() * dimInfluence);
        long double sin_term = std::sin(dt * emFieldStrength_.load() * dimInfluence);
        long double flip = (vertexSpins_[i] > 0 ? -0.032774L : 0.032774L);
        vertexSpins_[i] = vertexSpins_[i] * cos_term + flip * sin_term;
        if (std::isnan(vertexSpins_[i])) {
            vertexSpins_[i] = 0.032774L;
            #pragma omp critical
            anyClamped = true;
        }
        long double cos_god = std::cos(dt * GodWaveFreq_.load() * dimInfluence);
        long double sin_pert = std::sin(static_cast<long double>(i) * dt * 0.2L) * oneDPermeation_.load();
        vertexWaveAmplitudes_[i] = vertexWaveAmplitudes_[i] * cos_god + sin_pert;
        if (std::isnan(vertexWaveAmplitudes_[i])) {
            vertexWaveAmplitudes_[i] = 0.0L;
            #pragma omp critical
            anyClamped = true;
        }
    }
    if (debug_.load() && anyClamped) {
        std::osyncstream(std::cout) << YELLOW << "[WARNING] Some values were clamped or reset due to NaN/Inf in evolveTimeStep" RESET << std::endl;
    }
    needsUpdate_.store(true);
    updateInteractions();
    if (debug_.load()) {
        std::osyncstream(std::cout) << GREEN << "[INFO] Time step evolution completed" RESET << std::endl;
    }
}

// Updates vertex momenta with dimensional influences
void UniversalEquation::updateMomentum() {
    if (debug_.load()) {
        std::osyncstream(std::cout) << GREEN << "[INFO] Starting momentum update for " << vertexMomenta_.size() << " vertices" RESET << std::endl;
    }
    long double dimInfluence = 0.0L;
    for (int d = 1; d <= maxDimensions_; ++d) {
        dimInfluence += dimensionalWeight(d) * (d == 2 || d == 4 ? 1.5L : 1.0L);
    }
    dimInfluence = safe_div(dimInfluence, maxDimensions_);

    bool anyClamped = false;
    #pragma omp parallel for if(maxVertices_ > 100)
    for (size_t i = 0; i < vertexMomenta_.size(); ++i) {
        auto& momentum = vertexMomenta_[i];
        auto emField = computeEMField(i);
        long double GodWaveAmp = computeGodWaveAmplitude(i, 0.0L);
        long double nurbMatter = computeNurbMatter(0.0L);
        for (size_t j = 0; j < momentum.size(); ++j) {
            long double delta = (std::sin(static_cast<long double>(i + j) * 0.1L) * emFieldStrength_.load() +
                               emField[j] * vertexSpins_[i] + GodWaveAmp * nurbMatter * 0.05L) * dimInfluence;
            if (std::isnan(delta) || std::isinf(delta)) {
                delta = 0.0L;
                #pragma omp critical
                anyClamped = true;
            }
            momentum[j] += delta;
            if (momentum[j] < -0.9L || momentum[j] > 0.9L) {
                momentum[j] = std::clamp(momentum[j], -0.9L, 0.9L);
                #pragma omp critical
                anyClamped = true;
            }
        }
    }
    if (debug_.load() && anyClamped) {
        std::osyncstream(std::cout) << YELLOW << "[WARNING] Some momenta were clamped due to NaN/Inf or bounds in updateMomentum" RESET << std::endl;
    }
    if (debug_.load()) {
        std::osyncstream(std::cout) << GREEN << "[INFO] Momentum update completed" RESET << std::endl;
    }
}

// Initializes with retry logic
void UniversalEquation::initializeWithRetry() {
    std::latch retry_latch(1);
    int attempts = 0;
    const int maxAttempts = 5;
    while (currentDimension_.load() >= 1 && attempts < maxAttempts) {
        try {
            initializeNCube();
            cachedCos_.resize(maxDimensions_ + 1);
            for (int i = 0; i <= maxDimensions_; ++i) {
                cachedCos_[i] = std::cos(omega_ * i);
            }
            updateInteractions();
            if (debug_.load()) {
                std::osyncstream(std::cout) << GREEN << "[INFO] Initialization completed successfully" RESET << std::endl;
            }
            retry_latch.count_down();
            return;
        } catch (const std::bad_alloc& e) {
            std::osyncstream(std::cerr) << YELLOW << "[WARNING] Memory allocation failed for dimension " << currentDimension_.load()
                                        << ". Reducing dimension to " << (currentDimension_.load() - 1)
                                        << ". Attempt " << (attempts + 1) << "/" << maxAttempts << RESET << std::endl;
            if (currentDimension_.load() == 1) {
                std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Failed to allocate memory even at dimension 1" RESET << std::endl;
                throw std::runtime_error("Failed to allocate memory even at dimension 1");
            }
            currentDimension_.store(currentDimension_.load() - 1);
            mode_.store(currentDimension_.load());
            maxVertices_ = std::max<uint64_t>(1ULL, maxVertices_ / 2);
            needsUpdate_.store(true);
            ++attempts;
        }
    }
    std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Max retry attempts reached for initialization" RESET << std::endl;
    throw std::runtime_error("Max retry attempts reached for initialization");
}

// Safe exponential function
long double UniversalEquation::safeExp(long double x) const {
    long double clamped = std::clamp(x, -500.0L, 500.0L);
    long double result = std::exp(clamped);
    if (std::isnan(result) || std::isinf(result)) {
        if (debug_.load()) {
            std::osyncstream(std::cerr) << YELLOW << "[WARNING] safeExp produced invalid result for x=" << x << ", returning 1.0" RESET << std::endl;
        }
        return 1.0L;
    }
    return result;
}

// Initializes with navigator
void UniversalEquation::initializeCalculator(DimensionalNavigator* navigator) {
    if (debug_.load()) {
        std::osyncstream(std::cout) << GREEN << "[INFO] Initializing calculator with navigator=" << navigator << RESET << std::endl;
    }
    if (!navigator) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Navigator pointer cannot be null" RESET << std::endl;
        throw std::invalid_argument("Navigator pointer cannot be null");
    }
    navigator_ = navigator;
    needsUpdate_.store(true);
    initializeWithRetry();
}

// Updates cache with dimensional influences
UniversalEquation::DimensionData UniversalEquation::updateCache() {
    if (debug_.load()) {
        std::osyncstream(std::cout) << GREEN << "[INFO] Updating cache for dimension " << currentDimension_.load() << RESET << std::endl;
    }
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
    if (debug_.load()) {
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Cache updated: observable=" << data.observable << ", potential=" << data.potential << RESET << std::endl;
    }
    return data;
}

// Computes batch of dimension data with weighted contributions
std::vector<UniversalEquation::DimensionData> UniversalEquation::computeBatch(int startDim, int endDim) {
    if (debug_.load()) {
        std::osyncstream(std::cout) << GREEN << "[INFO] Starting batch computation for dimensions " << startDim << " to " << endDim << RESET << std::endl;
    }
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
    for (int d = startDim; d <= endDim; ++d) {
        try {
            UniversalEquation temp(*this);
            temp.currentDimension_.store(d);
            temp.mode_.store(d);
            temp.needsUpdate_.store(true);
            temp.maxVertices_ = maxVertices_;
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
                        tempK.maxVertices_ = maxVertices_;
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
                        if (debug_.load()) {
                            std::osyncstream(std::cerr) << YELLOW << "[WARNING] Inner batch computation failed for dimension " << k
                                                        << ": " << e.what() << RESET << std::endl;
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
            if (debug_.load()) {
                std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Batch computation failed for dimension " << d << ": " << e.what() << RESET << std::endl;
            }
            results[d - startDim] = DimensionData{d, 0.0L, 0.0L, 0.0L, 0.0L, 0.0L, 0.0L, 0.0L, 0.0L};
        }
    }
    currentDimension_.store(savedDim);
    mode_.store(savedMode);
    needsUpdate_.store(true);
    initializeWithRetry();
    if (debug_.load()) {
        std::osyncstream(std::cout) << GREEN << "[INFO] Batch computation completed for dimensions " << startDim << " to " << endDim << RESET << std::endl;
    }
    return results;
}

// Exports to CSV
void UniversalEquation::exportToCSV(const std::string& filename, const std::vector<DimensionData>& data) const {
    if (debug_.load()) {
        std::osyncstream(std::cout) << GREEN << "[INFO] Exporting dimension data to CSV: " << filename << RESET << std::endl;
    }
    std::ofstream ofs(filename);
    if (!ofs) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Cannot open CSV file for writing: " << filename << RESET << std::endl;
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
    if (debug_.load()) {
        std::osyncstream(std::cout) << GREEN << "[INFO] CSV export completed successfully" RESET << std::endl;
    }
}

// Advances simulation cycle
void UniversalEquation::advanceCycle() {
    if (debug_.load()) {
        std::osyncstream(std::cout) << GREEN << "[INFO] Advancing simulation cycle from dimension " << currentDimension_.load() << RESET << std::endl;
    }
    int newDimension = (currentDimension_.load() == maxDimensions_) ? 1 : currentDimension_.load() + 1;
    currentDimension_.store(newDimension);
    mode_.store(newDimension);
    needsUpdate_.store(true);
    initializeWithRetry();
    if (debug_.load()) {
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Advanced to dimension " << currentDimension_.load() << RESET << std::endl;
    }
}