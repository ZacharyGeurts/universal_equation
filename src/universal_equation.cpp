// universal_equation.cpp: Core implementation of the UniversalEquation class for quantum simulation on n-dimensional hypercube lattices.
// Models a 19-dimensional reality with stronger influences from 2D and 4D on 3D properties, using weighted dimensional contributions.
// Quantum-specific computations are implemented in universal_equation_quantum.cpp.
// Uses Logging::Logger for consistent, thread-safe logging across the AMOURANTH RTX Engine.
// Thread-safety note: Uses std::atomic for scalar members and std::latch for parallel vector updates to avoid mutexes.
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
    : influence_(std::clamp(influence, 0.0L, 10.0L)),
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
      currentDimension_(std::clamp(mode <= 0 ? 1 : mode, 1, maxDimensions <= 0 ? 19 : maxDimensions)),
      mode_(std::clamp(mode <= 0 ? 1 : mode, 1, maxDimensions <= 0 ? 19 : maxDimensions)),
      debug_(debug),
      needsUpdate_(true),
      totalCharge_(0.0L),
      avgProjScale_(1.0L),
      maxVertices_(std::max<uint64_t>(1ULL, std::min(numVertices, static_cast<uint64_t>(1ULL << 20)))),
      maxDimensions_(std::max(1, std::min(maxDimensions <= 0 ? 19 : maxDimensions, 19))),
      omega_(maxDimensions_ > 0 ? 2.0L * std::numbers::pi_v<long double> / (2 * maxDimensions_ - 1) : 1.0L),
      invMaxDim_(maxDimensions_ > 0 ? 1.0L / maxDimensions_ : 1e-15L),
      nCubeVertices_(),
      vertexMomenta_(),
      vertexSpins_(),
      vertexWaveAmplitudes_(),
      interactions_(),
      projectedVerts_(),
      cachedCos_(maxDimensions_ + 1, 0.0L),
      nurbMatterControlPoints_(5, 1.0L),
      nurbEnergyControlPoints_(5, 1.0L),
      nurbKnots_(9, 0.0L),
      nurbWeights_(5, 1.0L),
      dimensionData_(),
      navigator_(nullptr),
      logger_(logger) {
    logger_.log(Logging::LogLevel::Info, "Constructing UniversalEquation: maxVertices={}, maxDimensions={}, mode={}", 
                std::source_location::current(), getMaxVertices(), getMaxDimensions(), getMode());
    if (getMaxVertices() > 1'000'000) {
        logger_.log(Logging::LogLevel::Warning, "High vertex count ({}) may cause memory issues", 
                    std::source_location::current(), getMaxVertices());
    }
    if (mode <= 0 || maxDimensions <= 0) {
        logger_.log(Logging::LogLevel::Error, "maxDimensions and mode must be greater than 0: maxDimensions={}, mode={}", 
                    std::source_location::current(), maxDimensions, mode);
        throw std::invalid_argument("maxDimensions and mode must be greater than 0");
    }
    if (debug_.load() && (influence != getInfluence() || weak != getWeak() || collapse != getCollapse() ||
                          twoD != getTwoD() || threeDInfluence != getThreeDInfluence() ||
                          oneDPermeation != getOneDPermeation() || nurbMatterStrength != getNurbMatterStrength() ||
                          nurbEnergyStrength != getNurbEnergyStrength() || alpha != getAlpha() ||
                          beta != getBeta() || carrollFactor != getCarrollFactor() ||
                          meanFieldApprox != getMeanFieldApprox() || asymCollapse != getAsymCollapse() ||
                          perspectiveTrans != getPerspectiveTrans() || perspectiveFocal != getPerspectiveFocal() ||
                          spinInteraction != getSpinInteraction() || emFieldStrength != getEMFieldStrength() ||
                          renormFactor != getRenormFactor() || vacuumEnergy != getVacuumEnergy() ||
                          GodWaveFreq != getGodWaveFreq())) {
        logger_.log(Logging::LogLevel::Warning, "Some input parameters were clamped to valid ranges", 
                    std::source_location::current());
    }
    nurbMatterControlPoints_ = {1.0L, 0.8L, 0.5L, 0.3L, 0.1L};
    nurbEnergyControlPoints_ = {0.1L, 0.5L, 1.0L, 1.5L, 2.0L};
    nurbKnots_ = {0.0L, 0.0L, 0.0L, 0.0L, 0.5L, 1.0L, 1.0L, 1.0L, 1.0L};
    nurbWeights_ = {1.0L, 1.0L, 1.0L, 1.0L, 1.0L};
    try {
        initializeWithRetry();
        logger_.log(Logging::LogLevel::Info, "UniversalEquation initialized: vertices={}, totalCharge={}", 
                    std::source_location::current(), nCubeVertices_.size(), getTotalCharge());
    } catch (const std::exception& e) {
        logger_.log(Logging::LogLevel::Error, "Constructor failed: {}", std::source_location::current(), e.what());
        throw;
    }
}

// Copy constructor
UniversalEquation::UniversalEquation(const UniversalEquation& other)
    : influence_(other.getInfluence()),
      weak_(other.getWeak()),
      collapse_(other.getCollapse()),
      twoD_(other.getTwoD()),
      threeDInfluence_(other.getThreeDInfluence()),
      oneDPermeation_(other.getOneDPermeation()),
      nurbMatterStrength_(other.getNurbMatterStrength()),
      nurbEnergyStrength_(other.getNurbEnergyStrength()),
      alpha_(other.getAlpha()),
      beta_(other.getBeta()),
      carrollFactor_(other.getCarrollFactor()),
      meanFieldApprox_(other.getMeanFieldApprox()),
      asymCollapse_(other.getAsymCollapse()),
      perspectiveTrans_(other.getPerspectiveTrans()),
      perspectiveFocal_(other.getPerspectiveFocal()),
      spinInteraction_(other.getSpinInteraction()),
      emFieldStrength_(other.getEMFieldStrength()),
      renormFactor_(other.getRenormFactor()),
      vacuumEnergy_(other.getVacuumEnergy()),
      GodWaveFreq_(other.getGodWaveFreq()),
      currentDimension_(other.getCurrentDimension()),
      mode_(other.getMode()),
      debug_(other.getDebug()),
      needsUpdate_(other.getNeedsUpdate()),
      totalCharge_(other.getTotalCharge()),
      avgProjScale_(other.avgProjScale_.load()),
      maxVertices_(other.getMaxVertices()),
      maxDimensions_(other.getMaxDimensions()),
      omega_(other.getOmega()),
      invMaxDim_(other.getInvMaxDim()),
      nCubeVertices_(),
      vertexMomenta_(),
      vertexSpins_(other.getVertexSpins()),
      vertexWaveAmplitudes_(other.getVertexWaveAmplitudes()),
      interactions_(other.getInteractions()),
      projectedVerts_(other.getProjectedVertices()),
      cachedCos_(other.getCachedCos()),
      nurbMatterControlPoints_(other.getNurbMatterControlPoints()),
      nurbEnergyControlPoints_(other.getNurbEnergyControlPoints()),
      nurbKnots_(other.getNurbKnots()),
      nurbWeights_(other.getNurbWeights()),
      dimensionData_(other.getDimensionData()),
      navigator_(nullptr),
      logger_(other.logger_) {
    logger_.log(Logging::LogLevel::Info, "Copy constructing UniversalEquation: vertices={}", 
                std::source_location::current(), other.nCubeVertices_.size());
    try {
        nCubeVertices_.reserve(other.nCubeVertices_.size());
        vertexMomenta_.reserve(other.nCubeVertices_.size());
        for (const auto& vertex : other.nCubeVertices_) {
            nCubeVertices_.emplace_back(vertex);
        }
        for (const auto& momentum : other.vertexMomenta_) {
            vertexMomenta_.emplace_back(momentum);
        }
        initializeWithRetry();
        logger_.log(Logging::LogLevel::Debug, "Copy constructor completed: vertices={}", 
                    std::source_location::current(), nCubeVertices_.size());
    } catch (const std::exception& e) {
        logger_.log(Logging::LogLevel::Error, "Copy constructor failed: {}", std::source_location::current(), e.what());
        throw;
    }
}

// Copy assignment
UniversalEquation& UniversalEquation::operator=(const UniversalEquation& other) {
    if (this != &other) {
        logger_.log(Logging::LogLevel::Info, "Assigning UniversalEquation: vertices={}", 
                    std::source_location::current(), other.nCubeVertices_.size());
        setInfluence(other.getInfluence());
        setWeak(other.getWeak());
        setCollapse(other.getCollapse());
        setTwoD(other.getTwoD());
        setThreeDInfluence(other.getThreeDInfluence());
        setOneDPermeation(other.getOneDPermeation());
        setNurbMatterStrength(other.getNurbMatterStrength());
        setNurbEnergyStrength(other.getNurbEnergyStrength());
        setAlpha(other.getAlpha());
        setBeta(other.getBeta());
        setCarrollFactor(other.getCarrollFactor());
        setMeanFieldApprox(other.getMeanFieldApprox());
        setAsymCollapse(other.getAsymCollapse());
        setPerspectiveTrans(other.getPerspectiveTrans());
        setPerspectiveFocal(other.getPerspectiveFocal());
        setSpinInteraction(other.getSpinInteraction());
        setEMFieldStrength(other.getEMFieldStrength());
        setRenormFactor(other.getRenormFactor());
        setVacuumEnergy(other.getVacuumEnergy());
        setGodWaveFreq(other.getGodWaveFreq());
        setCurrentDimension(other.getCurrentDimension());
        setDebug(other.getDebug());
        setTotalCharge(other.getTotalCharge());
        avgProjScale_.store(other.avgProjScale_.load());
        nCubeVertices_.clear();
        vertexMomenta_.clear();
        setVertexSpins(other.getVertexSpins());
        setVertexWaveAmplitudes(other.getVertexWaveAmplitudes());
        interactions_ = other.getInteractions();
        setProjectedVertices(other.getProjectedVertices());
        cachedCos_ = other.getCachedCos();
        nurbMatterControlPoints_ = other.getNurbMatterControlPoints();
        nurbEnergyControlPoints_ = other.getNurbEnergyControlPoints();
        nurbKnots_ = other.getNurbKnots();
        nurbWeights_ = other.getNurbWeights();
        dimensionData_ = other.getDimensionData();
        navigator_ = nullptr;
        try {
            nCubeVertices_.reserve(other.nCubeVertices_.size());
            vertexMomenta_.reserve(other.nCubeVertices_.size());
            for (const auto& vertex : other.nCubeVertices_) {
                nCubeVertices_.emplace_back(vertex);
            }
            for (const auto& momentum : other.vertexMomenta_) {
                vertexMomenta_.emplace_back(momentum);
            }
            initializeWithRetry();
            logger_.log(Logging::LogLevel::Debug, "Copy assignment completed: vertices={}", 
                        std::source_location::current(), nCubeVertices_.size());
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
        logger_.log(Logging::LogLevel::Info, "Initializing n-cube: maxVertices={}, currentDimension={}", 
                    std::source_location::current(), getMaxVertices(), getCurrentDimension());
        if (getDebug() && getMaxVertices() > 1'000'000) {
            logger_.log(Logging::LogLevel::Warning, "High vertex count ({}) may impact memory usage", 
                        std::source_location::current(), getMaxVertices());
        }

        // Clear vectors
        nCubeVertices_.clear();
        vertexMomenta_.clear();
        vertexSpins_.clear();
        vertexWaveAmplitudes_.clear();
        interactions_.clear();
        projectedVerts_.clear();
        logger_.log(Logging::LogLevel::Debug, "Cleared all vectors", std::source_location::current());

        // Reserve capacity with checks
        logger_.log(Logging::LogLevel::Debug, "Reserving {} elements for nCubeVertices_", 
                    std::source_location::current(), getMaxVertices());
        nCubeVertices_.reserve(getMaxVertices());
        if (nCubeVertices_.capacity() < getMaxVertices()) {
            logger_.log(Logging::LogLevel::Error, "Failed to reserve {} elements for nCubeVertices_, actual capacity={}", 
                        std::source_location::current(), getMaxVertices(), nCubeVertices_.capacity());
            throw std::bad_alloc();
        }
        vertexMomenta_.reserve(getMaxVertices());
        vertexSpins_.reserve(getMaxVertices());
        vertexWaveAmplitudes_.reserve(getMaxVertices());
        interactions_.reserve(getMaxVertices());
        projectedVerts_.reserve(getMaxVertices());
        setTotalCharge(0.0L);

        // Initialize vertices
        for (uint64_t i = 0; i < getMaxVertices(); ++i) {
            std::vector<long double> vertex(getCurrentDimension(), 0.0L);
            std::vector<long double> momentum(getCurrentDimension(), 0.0L);
            for (int j = 0; j < getCurrentDimension(); ++j) {
                vertex[j] = (static_cast<long double>(i) / getMaxVertices()) * 0.1L;
                momentum[j] = (static_cast<long double>(i % 2) - 0.5L) * 0.01L;
            }
            long double spin = (i % 2 == 0 ? 0.032774L : -0.032774L);
            long double amplitude = getOneDPermeation() * (1.0L + 0.1L * (i / static_cast<long double>(getMaxVertices())));
            nCubeVertices_.push_back(std::move(vertex));
            vertexMomenta_.push_back(std::move(momentum));
            vertexSpins_.push_back(spin);
            vertexWaveAmplitudes_.push_back(amplitude);
            interactions_.push_back(DimensionInteraction(
                static_cast<int>(i), 0.0L, 0.0L, std::vector<long double>(std::min(3, getCurrentDimension()), 0.0L), 0.0L));
            projectedVerts_.push_back(glm::vec3(0.0f, 0.0f, 0.0f));
            totalCharge_.fetch_add(1.0L / getMaxVertices());
            if (getDebug() && (i % 1000 == 0 || i == getMaxVertices() - 1)) {
                logger_.log(Logging::LogLevel::Debug, "Initialized vertex {}/{}, nCubeVertices_.size()={}", 
                            std::source_location::current(), i, getMaxVertices(), nCubeVertices_.size());
            }
        }

        // Verify alignment for Vulkan
        if (!projectedVerts_.empty() && reinterpret_cast<std::uintptr_t>(projectedVerts_.data()) % alignof(glm::vec3) != 0) {
            logger_.log(Logging::LogLevel::Error, "projectedVerts_ is misaligned: address={}, alignment={}", 
                        std::source_location::current(), reinterpret_cast<std::uintptr_t>(projectedVerts_.data()), alignof(glm::vec3));
            throw std::runtime_error("Misaligned projectedVerts_");
        }

        logger_.log(Logging::LogLevel::Info, "n-cube initialized: vertices={}, totalCharge={}", 
                    std::source_location::current(), nCubeVertices_.size(), getTotalCharge());
        init_latch.count_down();
    } catch (const std::exception& e) {
        logger_.log(Logging::LogLevel::Error, "initializeNCube failed: {}", std::source_location::current(), e.what());
        throw;
    }
    init_latch.wait();
}

// Updates interactions and projects vertices to 3D space for visualization
void UniversalEquation::updateInteractions() const {
    logger_.log(Logging::LogLevel::Info, "Starting interaction update: vertices={}, dimension={}", 
                std::source_location::current(), nCubeVertices_.size(), getCurrentDimension());
    interactions_.clear();
    projectedVerts_.clear();
    logger_.log(Logging::LogLevel::Debug, "Cleared interactions_ and projectedVerts_", 
                std::source_location::current());

    size_t d = static_cast<size_t>(getCurrentDimension());
    uint64_t numVertices = std::min(static_cast<uint64_t>(nCubeVertices_.size()), getMaxVertices());
    numVertices = std::min(numVertices, static_cast<uint64_t>(1024));
    logger_.log(Logging::LogLevel::Debug, "Processing {} vertices (maxVertices_={}, capped at 1024)", 
                std::source_location::current(), numVertices, getMaxVertices());

    // Per-thread local storage
    std::vector<std::vector<DimensionInteraction>> localInteractions(omp_get_max_threads());
    std::vector<std::vector<glm::vec3>> localProjected(omp_get_max_threads());
    for (int t = 0; t < omp_get_max_threads(); ++t) {
        localInteractions[t].reserve((numVertices - 1) / omp_get_max_threads() + 1);
        localProjected[t].reserve((numVertices - 1) / omp_get_max_threads() + 1);
        logger_.log(Logging::LogLevel::Debug, "Thread {}: reserved {} elements for localInteractions and localProjected", 
                    std::source_location::current(), t, (numVertices - 1) / omp_get_max_threads() + 1);
    }

    // Compute reference vertex projection
    std::vector<long double> referenceVertex(d, 0.0L);
    for (size_t i = 0; i < numVertices && i < nCubeVertices_.size(); ++i) {
        validateVertexIndex(static_cast<int>(i));
        for (size_t j = 0; j < d; ++j) {
            referenceVertex[j] += nCubeVertices_[i][j];
        }
    }
    for (size_t j = 0; j < d; ++j) {
        referenceVertex[j] = safe_div(referenceVertex[j], static_cast<long double>(numVertices));
    }
    long double trans = getPerspectiveTrans();
    long double focal = getPerspectiveFocal();
    size_t depthIdx = d > 0 ? d - 1 : 0;
    long double depthRef = referenceVertex[depthIdx] + trans;
    if (depthRef <= 0.0L) {
        depthRef = 0.001L;
        logger_.log(Logging::LogLevel::Warning, "Clamped depthRef to 0.001: original={}", 
                    std::source_location::current(), referenceVertex[depthIdx] + trans);
    }
    long double scaleRef = safe_div(focal, depthRef);
    glm::vec3 projRefVec(0.0f);
    size_t projDim = std::min<size_t>(3, d);
    for (size_t k = 0; k < projDim; ++k) {
        projRefVec[k] = static_cast<float>(referenceVertex[k] * scaleRef);
    }
    projectedVerts_.push_back(projRefVec);
    logger_.log(Logging::LogLevel::Debug, "Added reference projection: projectedVerts_.size()={}", 
                std::source_location::current(), projectedVerts_.size());

    // Parallel compute interactions
    std::latch latch(omp_get_max_threads());
    #pragma omp parallel
    {
        int thread_id = omp_get_thread_num();
        logger_.log(Logging::LogLevel::Debug, "Thread {}: processing vertices 1 to {}", 
                    std::source_location::current(), thread_id, numVertices - 1);
        #pragma omp for schedule(dynamic)
        for (uint64_t i = 1; i < numVertices; ++i) {
            if (i >= nCubeVertices_.size()) {
                logger_.log(Logging::LogLevel::Warning, "Thread {}: skipping vertex {} (exceeds nCubeVertices_.size()={})", 
                            std::source_location::current(), thread_id, i, nCubeVertices_.size());
                continue;
            }
            validateVertexIndex(static_cast<int>(i));
            const auto& v = nCubeVertices_[i];
            long double depthI = v[depthIdx] + trans;
            if (depthI <= 0.0L) {
                depthI = 0.001L;
                logger_.log(Logging::LogLevel::Warning, "Thread {}: clamped depthI to 0.001 for vertex {}", 
                            std::source_location::current(), thread_id, i);
            }
            long double scaleI = safe_div(focal, depthI);
            long double distance = 0.0L;
            for (size_t j = 0; j < d; ++j) {
                long double diff = v[j] - referenceVertex[j];
                distance += diff * diff;
            }
            distance = std::sqrt(distance);
            long double strength = computeInteraction(static_cast<int>(i), distance);
            auto vecPot = computeVectorPotential(static_cast<int>(i));
            long double GodWaveAmp = computeGodWave(static_cast<int>(i));
            glm::vec3 projIVec(0.0f);
            for (size_t k = 0; k < projDim; ++k) {
                projIVec[k] = static_cast<float>(v[k] * scaleI);
            }
            localInteractions[thread_id].emplace_back(static_cast<int>(i), distance, strength, vecPot, GodWaveAmp);
            localProjected[thread_id].push_back(projIVec);
        }
        latch.count_down();
    }
    latch.wait();

    // Merge local results
    size_t totalInteractions = 0;
    size_t totalProjected = 0;
    for (int t = 0; t < omp_get_max_threads(); ++t) {
        totalInteractions += localInteractions[t].size();
        totalProjected += localProjected[t].size();
        interactions_.insert(interactions_.end(), localInteractions[t].begin(), localInteractions[t].end());
        projectedVerts_.insert(projectedVerts_.end(), localProjected[t].begin(), localProjected[t].end());
    }
    logger_.log(Logging::LogLevel::Info, "Interactions updated: interactions_.size()={}, projectedVerts_.size()={}", 
                std::source_location::current(), interactions_.size(), projectedVerts_.size());
    if (totalInteractions != interactions_.size() || totalProjected != projectedVerts_.size()) {
        logger_.log(Logging::LogLevel::Error, "Mismatch in merged sizes: expected interactions={}, projected={}, actual interactions_.size()={}, projectedVerts_.size()={}", 
                    std::source_location::current(), totalInteractions, totalProjected, interactions_.size(), projectedVerts_.size());
        throw std::runtime_error("Mismatch in merged vector sizes");
    }

    // Verify alignment for Vulkan
    if (!projectedVerts_.empty() && reinterpret_cast<std::uintptr_t>(projectedVerts_.data()) % alignof(glm::vec3) != 0) {
        logger_.log(Logging::LogLevel::Error, "projectedVerts_ is misaligned: address={}, alignment={}", 
                    std::source_location::current(), reinterpret_cast<std::uintptr_t>(projectedVerts_.data()), alignof(glm::vec3));
        throw std::runtime_error("Misaligned projectedVerts_");
    }
}

// Computes energy contributions across all vertices
UniversalEquation::EnergyResult UniversalEquation::compute() {
    logger_.log(Logging::LogLevel::Info, "Starting compute: vertices={}, dimension={}", 
                std::source_location::current(), nCubeVertices_.size(), getCurrentDimension());
    if (getNeedsUpdate()) {
        updateInteractions();
        needsUpdate_.store(false);
    }

    EnergyResult result{0.0L, 0.0L, 0.0L, 0.0L, 0.0L, 0.0L, 0.0L, 0.0L};
    std::vector<long double> potentials(getMaxVertices(), 0.0L);
    std::vector<long double> nurbMatters(getMaxVertices(), 0.0L);
    std::vector<long double> nurbEnergies(getMaxVertices(), 0.0L);
    std::vector<long double> spinEnergies(getMaxVertices(), 0.0L);
    std::vector<long double> momentumEnergies(getMaxVertices(), 0.0L);
    std::vector<long double> fieldEnergies(getMaxVertices(), 0.0L);
    std::vector<long double> GodWaveEnergies(getMaxVertices(), 0.0L);

    std::latch latch(omp_get_max_threads());
    #pragma omp parallel
    {
        int thread_id = omp_get_thread_num();
        logger_.log(Logging::LogLevel::Debug, "Thread {}: computing energies for vertices", 
                    std::source_location::current(), thread_id);
        #pragma omp for schedule(dynamic)
        for (uint64_t i = 0; i < getMaxVertices(); ++i) {
            if (i >= nCubeVertices_.size()) continue;
            validateVertexIndex(static_cast<int>(i));
            potentials[i] = computeGravitationalPotential(static_cast<int>(i));
            nurbMatters[i] = computeNurbMatter(static_cast<int>(i));
            nurbEnergies[i] = computeNurbEnergy(static_cast<int>(i));
            spinEnergies[i] = computeSpinEnergy(static_cast<int>(i));
            momentumEnergies[i] = 0.0L;
            for (const auto& m : vertexMomenta_[i]) {
                momentumEnergies[i] += m * m;
            }
            momentumEnergies[i] *= 0.5L;
            fieldEnergies[i] = computeEMField(static_cast<int>(i));
            GodWaveEnergies[i] = computeGodWave(static_cast<int>(i));
        }
        latch.count_down();
    }
    latch.wait();

    for (uint64_t i = 0; i < getMaxVertices() && i < nCubeVertices_.size(); ++i) {
        result.observable += potentials[i] + nurbMatters[i] + nurbEnergies[i] + spinEnergies[i] + momentumEnergies[i] + fieldEnergies[i] + GodWaveEnergies[i];
        result.potential += potentials[i];
        result.nurbMatter += nurbMatters[i];
        result.nurbEnergy += nurbEnergies[i];
        result.spinEnergy += spinEnergies[i];
        result.momentumEnergy += momentumEnergies[i];
        result.fieldEnergy += fieldEnergies[i];
        result.GodWaveEnergy += GodWaveEnergies[i];
    }
    result.observable = safe_div(result.observable, static_cast<long double>(getMaxVertices()));
    logger_.log(Logging::LogLevel::Info, "Compute completed: {}", 
                std::source_location::current(), result.toString());
    return result;
}

// Initializes with retry logic
void UniversalEquation::initializeWithRetry() {
    std::latch retry_latch(1);
    int attempts = 0;
    const int maxAttempts = 5;
    uint64_t currentVertices = getMaxVertices();
    while (getCurrentDimension() >= 1 && attempts < maxAttempts) {
        try {
            if (nCubeVertices_.size() > currentVertices) {
                nCubeVertices_.resize(currentVertices);
                vertexMomenta_.resize(currentVertices);
                vertexSpins_.resize(currentVertices);
                vertexWaveAmplitudes_.resize(currentVertices);
                interactions_.resize(currentVertices);
                projectedVerts_.resize(currentVertices);
            }
            initializeNCube();
            cachedCos_.resize(getMaxDimensions() + 1);
            for (int i = 0; i <= getMaxDimensions(); ++i) {
                cachedCos_[i] = std::cos(getOmega() * i);
            }
            updateInteractions();
            logger_.log(Logging::LogLevel::Info, "Initialization completed successfully", 
                        std::source_location::current());
            retry_latch.count_down();
            return;
        } catch (const std::bad_alloc& e) {
            logger_.log(Logging::LogLevel::Warning, "Memory allocation failed for dimension {}. Reducing dimension to {}. Attempt {}/{}", 
                        std::source_location::current(), getCurrentDimension(), getCurrentDimension() - 1, attempts + 1, maxAttempts);
            if (getCurrentDimension() == 1) {
                logger_.log(Logging::LogLevel::Error, "Failed to allocate memory even at dimension 1", 
                            std::source_location::current());
                throw std::runtime_error("Failed to allocate memory even at dimension 1");
            }
            setCurrentDimension(getCurrentDimension() - 1);
            currentVertices = std::max<uint64_t>(1ULL, currentVertices / 2);
            needsUpdate_.store(true);
            ++attempts;
        }
    }
    logger_.log(Logging::LogLevel::Error, "Max retry attempts reached for initialization", 
                std::source_location::current());
    throw std::runtime_error("Max retry attempts reached for initialization");
}

// Initializes with navigator
void UniversalEquation::initializeCalculator() {
    logger_.log(Logging::LogLevel::Info, "Initializing calculator", std::source_location::current());
    try {
        if (navigator_) {
            // Assume navigator_ is set externally with a concrete implementation
            navigator_->initialize(getCurrentDimension(), getMaxVertices());
            logger_.log(Logging::LogLevel::Debug, "DimensionalNavigator initialized: navigator_={}", 
                        std::source_location::current(), static_cast<void*>(navigator_));
        } else {
            logger_.log(Logging::LogLevel::Warning, "DimensionalNavigator not set, skipping initialization", 
                        std::source_location::current());
        }
        needsUpdate_.store(true);
        initializeWithRetry();
    } catch (const std::exception& e) {
        logger_.log(Logging::LogLevel::Error, "initializeCalculator failed: {}", 
                    std::source_location::current(), e.what());
        throw;
    }
}