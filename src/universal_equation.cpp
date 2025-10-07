// universal_equation.cpp: Core implementation of the UniversalEquation class for quantum simulation on n-dimensional hypercube lattices.
// Models a 19-dimensional reality with stronger influences from 2D and 4D on 3D properties, using weighted dimensional contributions.
// Quantum-specific computations are implemented in universal_equation_quantum.cpp.
// Uses Logging::Logger for consistent logging across the AMOURANTH RTX Engine.
// Copyright Zachary Geurts 2025 (powered by Grok with Heisenberg swagger)

#include "universal_equation.hpp"
#include <numbers>
#include <cmath>
#include <thread>
#include <fstream>
#include <memory>
#include <algorithm>
#include <stdexcept>
#include <latch>
#include "engine/logging.hpp"

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
    const Logging::Logger& logger,
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
      nurbWeights_(5, 1.0L),
      dimensionData_(),
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
      debug_(debug),
      logger_(logger) {
    // Validation to ensure currentDimension_ and mode_ are not 0
    if (mode <= 0 || maxDimensions <= 0) {
        logger_.log(Logging::LogLevel::Error, "maxDimensions and mode must be greater than 0", std::source_location::current());
        throw std::invalid_argument("maxDimensions and mode must be greater than 0");
    }

    // Debug: Log initialization state
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Starting UniversalEquation initialization with maxDimensions={}, mode={}, vertices={}, debug={}",
                    std::source_location::current(), maxDimensions_, mode_.load(), maxVertices_, debug_.load());
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
        logger_.log(Logging::LogLevel::Warning, "Some input parameters were clamped to valid ranges", std::source_location::current());
    }

    nurbMatterControlPoints_ = {1.0L, 0.8L, 0.5L, 0.3L, 0.1L};
    nurbEnergyControlPoints_ = {0.1L, 0.5L, 1.0L, 1.5L, 2.0L};
    nurbKnots_ = {0.0L, 0.0L, 0.0L, 0.0L, 0.5L, 1.0L, 1.0L, 1.0L, 1.0L};
    nurbWeights_ = {1.0L, 1.0L, 1.0L, 1.0L, 1.0L};
    try {
        initializeWithRetry();
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Initialized UniversalEquation: maxDimensions={}, mode={}, vertices={}, totalCharge={}",
                        std::source_location::current(), maxDimensions_, mode_.load(), nCubeVertices_.size(), totalCharge_.load());
        }
    } catch (const std::exception& e) {
        logger_.log(Logging::LogLevel::Error, "Constructor failed: {}", std::source_location::current(), e.what());
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
      nCubeVertices_(),
      vertexMomenta_(),
      vertexSpins_(),
      vertexWaveAmplitudes_(),
      interactions_(),
      projectedVerts_(),
      avgProjScale_(other.avgProjScale_.load()),
      cachedCos_(other.cachedCos_),
      navigator_(nullptr),
      nurbMatterControlPoints_(other.nurbMatterControlPoints_),
      nurbEnergyControlPoints_(other.nurbEnergyControlPoints_),
      nurbKnots_(other.nurbKnots_),
      nurbWeights_(other.nurbWeights_),
      dimensionData_(),
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
      logger_(other.logger_) {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Starting copy constructor for UniversalEquation, debug={}", 
                    std::source_location::current(), debug_.load());
    }
    try {
        // Deep copy vectors to avoid memory issues
        nCubeVertices_.reserve(other.nCubeVertices_.size());
        vertexMomenta_.reserve(other.vertexMomenta_.size());
        vertexSpins_ = other.vertexSpins_;
        vertexWaveAmplitudes_ = other.vertexWaveAmplitudes_;
        interactions_ = other.interactions_;
        projectedVerts_ = other.projectedVerts_;
        dimensionData_ = other.dimensionData_;
        for (const auto& vertex : other.nCubeVertices_) {
            nCubeVertices_.emplace_back(vertex);
        }
        for (const auto& momentum : other.vertexMomenta_) {
            vertexMomenta_.emplace_back(momentum);
        }
        initializeWithRetry();
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Copy constructor initialized: maxDimensions={}, vertices={}", 
                        std::source_location::current(), maxDimensions_, nCubeVertices_.size());
        }
    } catch (const std::exception& e) {
        logger_.log(Logging::LogLevel::Error, "Copy constructor failed: {}", std::source_location::current(), e.what());
        throw;
    }
}

// Copy assignment
UniversalEquation& UniversalEquation::operator=(const UniversalEquation& other) {
    if (this != &other) {
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Info, "Starting copy assignment for UniversalEquation, debug={}", 
                        std::source_location::current(), debug_.load());
        }
        maxDimensions_ = other.maxDimensions_;
        mode_.store(other.mode_.load());
        currentDimension_.store(other.currentDimension_.load());
        maxVertices_ = other.maxVertices_;
        omega_ = other.omega_;
        invMaxDim_ = other.invMaxDim_;
        totalCharge_.store(other.totalCharge_.load());
        needsUpdate_.store(other.needsUpdate_.load());
        nCubeVertices_.clear();
        vertexMomenta_.clear();
        vertexSpins_ = other.vertexSpins_;
        vertexWaveAmplitudes_ = other.vertexWaveAmplitudes_;
        interactions_ = other.interactions_;
        projectedVerts_ = other.projectedVerts_;
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
            nCubeVertices_.reserve(other.nCubeVertices_.size());
            vertexMomenta_.reserve(other.vertexMomenta_.size());
            for (const auto& vertex : other.nCubeVertices_) {
                nCubeVertices_.emplace_back(vertex);
            }
            for (const auto& momentum : other.vertexMomenta_) {
                vertexMomenta_.emplace_back(momentum);
            }
            initializeWithRetry();
            if (debug_.load()) {
                logger_.log(Logging::LogLevel::Debug, "Copy assignment completed: maxDimensions={}, vertices={}", 
                            std::source_location::current(), maxDimensions_, nCubeVertices_.size());
            }
        } catch (const std::exception& e) {
            logger_.log(Logging::LogLevel::Error, "Copy assignment failed: {}", std::source_location::current(), e.what());
            throw;
        }
    }
    return *this;
}

// Initializes n-cube vertices, momenta, spins, and amplitudes for multiple vertices
void UniversalEquation::initializeNCube() {
    std::latch init_latch(1);
    try {
        // Clear vectors to ensure clean state
        nCubeVertices_.clear();
        vertexMomenta_.clear();
        vertexSpins_.clear();
        vertexWaveAmplitudes_.clear();
        interactions_.clear();
        projectedVerts_.clear();

        // Debug logging after vector initialization
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Info, "Initializing n-cube with {} vertices, debug={}", 
                        std::source_location::current(), maxVertices_, debug_.load());
        }
        if (debug_.load() && maxVertices_ > 1000) {
            logger_.log(Logging::LogLevel::Warning, "High vertex count ({}) may impact performance", 
                        std::source_location::current(), maxVertices_);
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
            logger_.log(Logging::LogLevel::Debug, "Initialized nCube with {} vertices, totalCharge={}",
                        std::source_location::current(), nCubeVertices_.size(), totalCharge_.load());
        }
        init_latch.count_down();
    } catch (const std::exception& e) {
        logger_.log(Logging::LogLevel::Error, "initializeNCube failed: {}", std::source_location::current(), e.what());
        throw;
    }
    init_latch.wait(); // Ensure initialization completes
}

// Evolves system over time with dimensional influences
void UniversalEquation::evolveTimeStep(long double dt) {
    if (dt <= 0.0L || std::isnan(dt) || std::isinf(dt)) {
        logger_.log(Logging::LogLevel::Error, "Invalid time step", std::source_location::current());
        throw std::invalid_argument("Invalid time step");
    }
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Starting time step evolution with dt={}", std::source_location::current(), dt);
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
        logger_.log(Logging::LogLevel::Warning, "Some values were clamped or reset due to NaN/Inf in evolveTimeStep", 
                    std::source_location::current());
    }
    needsUpdate_.store(true);
    updateInteractions();
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Time step evolution completed", std::source_location::current());
    }
}

// Updates vertex momenta with dimensional influences
void UniversalEquation::updateMomentum() {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Starting momentum update for {} vertices", 
                    std::source_location::current(), vertexMomenta_.size());
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
        logger_.log(Logging::LogLevel::Warning, "Some momenta were clamped due to NaN/Inf or bounds in updateMomentum", 
                    std::source_location::current());
    }
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Momentum update completed", std::source_location::current());
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
                logger_.log(Logging::LogLevel::Info, "Initialization completed successfully", std::source_location::current());
            }
            retry_latch.count_down();
            return;
        } catch (const std::bad_alloc& e) {
            logger_.log(Logging::LogLevel::Warning, "Memory allocation failed for dimension {}. Reducing dimension to {}. Attempt {}/{}", 
                        std::source_location::current(), currentDimension_.load(), currentDimension_.load() - 1, attempts + 1, maxAttempts);
            if (currentDimension_.load() == 1) {
                logger_.log(Logging::LogLevel::Error, "Failed to allocate memory even at dimension 1", std::source_location::current());
                throw std::runtime_error("Failed to allocate memory even at dimension 1");
            }
            currentDimension_.store(currentDimension_.load() - 1);
            mode_.store(currentDimension_.load());
            maxVertices_ = std::max<uint64_t>(1ULL, maxVertices_ / 2);
            needsUpdate_.store(true);
            ++attempts;
        }
    }
    logger_.log(Logging::LogLevel::Error, "Max retry attempts reached for initialization", std::source_location::current());
    throw std::runtime_error("Max retry attempts reached for initialization");
}

// Safe exponential function
long double UniversalEquation::safeExp(long double x) const {
    long double clamped = std::clamp(x, -500.0L, 500.0L);
    long double result = std::exp(clamped);
    if (std::isnan(result) || std::isinf(result)) {
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Warning, "safeExp produced invalid result for x={}, returning 1.0", 
                        std::source_location::current(), x);
        }
        return 1.0L;
    }
    return result;
}

// Initializes with navigator
void UniversalEquation::initializeCalculator(DimensionalNavigator* navigator) {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Initializing calculator with navigator={}", 
                    std::source_location::current(), static_cast<void*>(navigator));
    }
    if (!navigator) {
        logger_.log(Logging::LogLevel::Error, "Navigator pointer cannot be null", std::source_location::current());
        throw std::invalid_argument("Navigator pointer cannot be null");
    }
    navigator_ = navigator;
    needsUpdate_.store(true);
    initializeWithRetry();
}

// Updates cache with dimensional influences
UniversalEquation::DimensionData UniversalEquation::updateCache() {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Updating cache for dimension {}", 
                    std::source_location::current(), currentDimension_.load());
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
        logger_.log(Logging::LogLevel::Debug, "Cache updated: observable={}, potential={}", 
                    std::source_location::current(), data.observable, data.potential);
    }
    return data;
}

// Computes batch of dimension data with weighted contributions
std::vector<UniversalEquation::DimensionData> UniversalEquation::computeBatch(int startDim, int endDim) {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Starting batch computation for dimensions {} to {}", 
                    std::source_location::current(), startDim, endDim);
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
            UniversalEquation temp(logger_, maxDimensions_, d, influence_.load(), weak_.load(), collapse_.load(),
                                  twoD_.load(), threeDInfluence_.load(), oneDPermeation_.load(),
                                  nurbMatterStrength_.load(), nurbEnergyStrength_.load(), alpha_.load(),
                                  beta_.load(), carrollFactor_.load(), meanFieldApprox_.load(),
                                  asymCollapse_.load(), perspectiveTrans_.load(), perspectiveFocal_.load(),
                                  spinInteraction_.load(), emFieldStrength_.load(), renormFactor_.load(),
                                  vacuumEnergy_.load(), GodWaveFreq_.load(), debug_.load(), maxVertices_);
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
                        UniversalEquation tempK(logger_, maxDimensions_, k, influence_.load(), weak_.load(), collapse_.load(),
                                               twoD_.load(), threeDInfluence_.load(), oneDPermeation_.load(),
                                               nurbMatterStrength_.load(), nurbEnergyStrength_.load(), alpha_.load(),
                                               beta_.load(), carrollFactor_.load(), meanFieldApprox_.load(),
                                               asymCollapse_.load(), perspectiveTrans_.load(), perspectiveFocal_.load(),
                                               spinInteraction_.load(), emFieldStrength_.load(), renormFactor_.load(),
                                               vacuumEnergy_.load(), GodWaveFreq_.load(), debug_.load(), maxVertices_);
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
                            logger_.log(Logging::LogLevel::Warning, "Inner batch computation failed for dimension {}: {}", 
                                        std::source_location::current(), k, e.what());
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
                logger_.log(Logging::LogLevel::Error, "Batch computation failed for dimension {}: {}", 
                            std::source_location::current(), d, e.what());
            }
            results[d - startDim] = DimensionData{d, 0.0L, 0.0L, 0.0L, 0.0L, 0.0L, 0.0L, 0.0L, 0.0L};
        }
    }
    currentDimension_.store(savedDim);
    mode_.store(savedMode);
    needsUpdate_.store(true);
    initializeWithRetry();
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Batch computation completed for dimensions {} to {}", 
                    std::source_location::current(), startDim, endDim);
    }
    return results;
}

// Exports to CSV
void UniversalEquation::exportToCSV(const std::string& filename, const std::vector<DimensionData>& data) const {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Exporting dimension data to CSV: {}", std::source_location::current(), filename);
    }
    std::ofstream ofs(filename);
    if (!ofs) {
        logger_.log(Logging::LogLevel::Error, "Cannot open CSV file for writing: {}", std::source_location::current(), filename);
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
        logger_.log(Logging::LogLevel::Info, "CSV export completed successfully", std::source_location::current());
    }
}

// Advances simulation cycle
void UniversalEquation::advanceCycle() {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Advancing simulation cycle from dimension {}", 
                    std::source_location::current(), currentDimension_.load());
    }
    int newDimension = (currentDimension_.load() == maxDimensions_) ? 1 : currentDimension_.load() + 1;
    currentDimension_.store(newDimension);
    mode_.store(newDimension);
    needsUpdate_.store(true);
    initializeWithRetry();
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Advanced to dimension {}", std::source_location::current(), currentDimension_.load());
    }
}