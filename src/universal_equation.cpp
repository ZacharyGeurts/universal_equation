// universal_equation.cpp
// Core implementation of UniversalEquation for the AMOURANTH RTX Engine.
// Manages N-dimensional calculations, NURBS-based matter/energy dynamics, and core simulation logic.
// Integrates with physical computations from universal_equation_physical.cpp.
// Copyright Zachary Geurts 2025 (powered by Grok with Science B*! precision)

#include "ue_init.hpp"
#include "engine/core.hpp"
#include <numbers>
#include <cmath>
#include <thread>
#include <fstream>
#include <memory>
#include <algorithm>
#include <stdexcept>
#include <latch>
#include <omp.h>

UniversalEquation::UniversalEquation(
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
) : influence_(std::clamp(influence, 0.0L, 10.0L)),
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
    currentVertices_(0),
    simulationTime_(0.0f),
    materialDensity_(1000.0L), // Default to water density
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
    logger_.log(Logging::LogLevel::Info, "Constructing UniversalEquation: maxVertices={}, maxDimensions={}, mode={}, GodWaveFreq={}",
                std::source_location::current(), getMaxVertices(), getMaxDimensions(), getMode(), getGodWaveFreq());
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

UniversalEquation::UniversalEquation(
    const Logging::Logger& logger,
    int maxDimensions,
    int mode,
    long double influence,
    long double weak,
    bool debug,
    uint64_t numVertices
) : UniversalEquation(
        logger, maxDimensions, mode, influence, weak, 5.0L, 1.5L, 5.0L, 1.0L, 0.5L, 1.0L, 0.01L, 0.5L, 0.1L,
        0.5L, 0.5L, 2.0L, 4.0L, 1.0L, 1.0e6L, 1.0L, 0.5L, 2.0L, debug, numVertices) {
    logger_.log(Logging::LogLevel::Debug, "Initialized UniversalEquation with simplified constructor, GodWaveFreq={}",
                std::source_location::current(), getGodWaveFreq());
}

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
      currentVertices_(other.getCurrentVertices()),
      simulationTime_(other.getSimulationTime()),
      materialDensity_(other.materialDensity_.load()),
      maxVertices_(other.getMaxVertices()),
      maxDimensions_(other.getMaxDimensions()),
      omega_(other.getOmega()),
      invMaxDim_(other.getInvMaxDim()),
      nCubeVertices_(),
      vertexMomenta_(),
      vertexSpins_(other.getVertexSpins().begin(), other.getVertexSpins().end()),
      vertexWaveAmplitudes_(other.getVertexWaveAmplitudes().begin(), other.getVertexWaveAmplitudes().end()),
      interactions_(other.getInteractions().begin(), other.getInteractions().end()),
      projectedVerts_(other.getProjectedVertices().begin(), other.getProjectedVertices().end()),
      cachedCos_(other.getCachedCos().begin(), other.getCachedCos().end()),
      nurbMatterControlPoints_(other.getNurbMatterControlPoints().begin(), other.getNurbMatterControlPoints().end()),
      nurbEnergyControlPoints_(other.getNurbEnergyControlPoints().begin(), other.getNurbEnergyControlPoints().end()),
      nurbKnots_(other.getNurbKnots().begin(), other.getNurbKnots().end()),
      nurbWeights_(other.getNurbWeights().begin(), other.getNurbWeights().end()),
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
        validateProjectedVertices();
        logger_.log(Logging::LogLevel::Debug, "Copy constructor completed: vertices={}",
                    std::source_location::current(), nCubeVertices_.size());
    } catch (const std::exception& e) {
        logger_.log(Logging::LogLevel::Error, "Copy constructor failed: {}", std::source_location::current(), e.what());
        throw;
    }
}

UniversalEquation& UniversalEquation::operator=(const UniversalEquation& other) {
    if (this != &other) {
        logger_.log(Logging::LogLevel::Info, "Assigning UniversalEquation: vertices={}",
                    std::source_location::current(), other.nCubeVertices_.size());
        influence_.store(other.getInfluence());
        weak_.store(other.getWeak());
        collapse_.store(other.getCollapse());
        twoD_.store(other.getTwoD());
        threeDInfluence_.store(other.getThreeDInfluence());
        oneDPermeation_.store(other.getOneDPermeation());
        nurbMatterStrength_.store(other.getNurbMatterStrength());
        nurbEnergyStrength_.store(other.getNurbEnergyStrength());
        alpha_.store(other.getAlpha());
        beta_.store(other.getBeta());
        carrollFactor_.store(other.getCarrollFactor());
        meanFieldApprox_.store(other.getMeanFieldApprox());
        asymCollapse_.store(other.getAsymCollapse());
        perspectiveTrans_.store(other.getPerspectiveTrans());
        perspectiveFocal_.store(other.getPerspectiveFocal());
        spinInteraction_.store(other.getSpinInteraction());
        emFieldStrength_.store(other.getEMFieldStrength());
        renormFactor_.store(other.getRenormFactor());
        vacuumEnergy_.store(other.getVacuumEnergy());
        GodWaveFreq_.store(other.getGodWaveFreq());
        currentDimension_.store(other.getCurrentDimension());
        mode_.store(other.getMode());
        debug_.store(other.getDebug());
        needsUpdate_.store(other.getNeedsUpdate());
        totalCharge_.store(other.getTotalCharge());
        avgProjScale_.store(other.avgProjScale_.load());
        currentVertices_.store(other.getCurrentVertices());
        simulationTime_.store(other.getSimulationTime());
        materialDensity_.store(other.materialDensity_.load());
        nCubeVertices_.clear();
        vertexMomenta_.clear();
        vertexSpins_.assign(other.getVertexSpins().begin(), other.getVertexSpins().end());
        vertexWaveAmplitudes_.assign(other.getVertexWaveAmplitudes().begin(), other.getVertexWaveAmplitudes().end());
        interactions_.assign(other.getInteractions().begin(), other.getInteractions().end());
        projectedVerts_.assign(other.getProjectedVertices().begin(), other.getProjectedVertices().end());
        cachedCos_.assign(other.getCachedCos().begin(), other.getCachedCos().end());
        nurbMatterControlPoints_.assign(other.getNurbMatterControlPoints().begin(), other.getNurbMatterControlPoints().end());
        nurbEnergyControlPoints_.assign(other.getNurbEnergyControlPoints().begin(), other.getNurbEnergyControlPoints().end());
        nurbKnots_.assign(other.getNurbKnots().begin(), other.getNurbKnots().end());
        nurbWeights_.assign(other.getNurbWeights().begin(), other.getNurbWeights().end());
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
            validateProjectedVertices();
            logger_.log(Logging::LogLevel::Debug, "Copy assignment completed: vertices={}",
                        std::source_location::current(), nCubeVertices_.size());
        } catch (const std::exception& e) {
            logger_.log(Logging::LogLevel::Error, "Copy assignment failed: {}", std::source_location::current(), e.what());
            throw;
        }
    }
    return *this;
}

UniversalEquation::~UniversalEquation() {
    logger_.log(Logging::LogLevel::Debug, "Destroying UniversalEquation: vertices={}",
                std::source_location::current(), nCubeVertices_.size());
}

void UniversalEquation::initializeNCube() {
    std::latch init_latch(1);
    try {
        logger_.log(Logging::LogLevel::Info, "Initializing n-cube: maxVertices={}, currentDimension={}",
                    std::source_location::current(), getMaxVertices(), getCurrentDimension());
        if (getDebug() && getMaxVertices() > 1'000'000) {
            logger_.log(Logging::LogLevel::Warning, "High vertex count ({}) may impact memory usage",
                        std::source_location::current(), getMaxVertices());
        }

        nCubeVertices_.clear();
        vertexMomenta_.clear();
        vertexSpins_.clear();
        vertexWaveAmplitudes_.clear();
        interactions_.clear();
        projectedVerts_.clear();
        logger_.log(Logging::LogLevel::Debug, "Cleared all vectors", std::source_location::current());

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

        for (uint64_t i = 0; i < getMaxVertices(); ++i) {
            std::vector<long double> vertex(getCurrentDimension(), 0.0L);
            std::vector<long double> momentum(getCurrentDimension(), 0.0L);
            for (int j = 0; j < getCurrentDimension(); ++j) {
                vertex[j] = (static_cast<long double>(i) / getMaxVertices()) * 0.0254L; // Scale to 1-inch cube
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

void UniversalEquation::validateProjectedVertices() const {
    if (projectedVerts_.empty()) {
        logger_.log(Logging::LogLevel::Warning, "projectedVerts_ is empty, initializing with default values",
                    std::source_location::current());
        projectedVerts_.resize(nCubeVertices_.size(), glm::vec3(0.0f, 0.0f, 0.0f));
    }
    if (projectedVerts_.size() != nCubeVertices_.size()) {
        logger_.log(Logging::LogLevel::Error, "projectedVerts_ size mismatch: projectedVerts_.size()={}, nCubeVertices_.size()={}",
                    std::source_location::current(), projectedVerts_.size(), nCubeVertices_.size());
        throw std::runtime_error("projectedVerts_ size does not match nCubeVertices_");
    }
    if (!projectedVerts_.empty() && reinterpret_cast<std::uintptr_t>(projectedVerts_.data()) % alignof(glm::vec3) != 0) {
        logger_.log(Logging::LogLevel::Error, "projectedVerts_ is misaligned: address={}, alignment={}",
                    std::source_location::current(), reinterpret_cast<std::uintptr_t>(projectedVerts_.data()), alignof(glm::vec3));
        throw std::runtime_error("Misaligned projectedVerts_");
    }
}

long double UniversalEquation::computeKineticEnergy(int vertexIndex) const {
    validateVertexIndex(vertexIndex);
    long double kineticEnergy = 0.0L;
    const auto& momentum = vertexMomenta_[vertexIndex];
    for (size_t j = 0; j < static_cast<size_t>(getCurrentDimension()); ++j) {
        kineticEnergy += momentum[j] * momentum[j]; // Sum of squared momentum components
    }
    kineticEnergy *= 0.5L * materialDensity_.load(); // KE = (1/2) * mass * v^2, assuming density as mass proxy
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Computed kinetic energy for vertex {}: result={}",
                    std::source_location::current(), vertexIndex, kineticEnergy);
    }
    return kineticEnergy;
}

void UniversalEquation::updateInteractions() const {
    logger_.log(Logging::LogLevel::Info, "Starting interaction update: vertices={}, dimension={}",
                std::source_location::current(), nCubeVertices_.size(), getCurrentDimension());
    interactions_.clear();
    projectedVerts_.clear();
    logger_.log(Logging::LogLevel::Debug, "Cleared interactions_ and projectedVerts_",
                std::source_location::current());

    size_t d = static_cast<size_t>(getCurrentDimension());
    uint64_t numVertices = std::min(static_cast<uint64_t>(nCubeVertices_.size()), getMaxVertices());
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Processing {} vertices (maxVertices_={})",
                    std::source_location::current(), numVertices, getMaxVertices());
    }

    std::vector<std::vector<DimensionInteraction>> localInteractions(omp_get_max_threads());
    std::vector<std::vector<glm::vec3>> localProjected(omp_get_max_threads());
    for (int t = 0; t < omp_get_max_threads(); ++t) {
        localInteractions[t].reserve(numVertices / omp_get_max_threads() + 1);
        localProjected[t].reserve(numVertices / omp_get_max_threads() + 1);
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Thread {}: reserved {} elements for localInteractions and localProjected",
                        std::source_location::current(), t, numVertices / omp_get_max_threads() + 1);
        }
    }

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

    std::latch latch(omp_get_max_threads());
    #pragma omp parallel
    {
        int thread_id = omp_get_thread_num();
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Thread {}: processing vertices 0 to {}",
                        std::source_location::current(), thread_id, numVertices);
        }
        #pragma omp for schedule(dynamic)
        for (uint64_t i = 0; i < numVertices; ++i) {
            if (i >= nCubeVertices_.size()) {
                if (debug_.load()) {
                    logger_.log(Logging::LogLevel::Warning, "Thread {}: skipping vertex {} (exceeds nCubeVertices_.size()={})",
                                std::source_location::current(), thread_id, i, nCubeVertices_.size());
                }
                continue;
            }
            validateVertexIndex(static_cast<int>(i));
            const auto& v = nCubeVertices_[i];
            long double depthI = v[depthIdx] + trans;
            if (depthI <= 0.0L) {
                depthI = 0.001L;
                if (debug_.load()) {
                    logger_.log(Logging::LogLevel::Warning, "Thread {}: clamped depthI to 0.001 for vertex {}",
                                std::source_location::current(), thread_id, i);
                }
            }
            long double scaleI = safe_div(focal, depthI);
            long double distance = 0.0L;
            for (size_t j = 0; j < d; ++j) {
                long double diff = v[j] - referenceVertex[j];
                distance += diff * diff;
            }
            distance = std::sqrt(distance);
            if (distance <= 0.0L || std::isnan(distance) || std::isinf(distance)) {
                distance = 1e-10L;
                if (debug_.load()) {
                    logger_.log(Logging::LogLevel::Warning, "Thread {}: invalid distance for vertex {}, using default={}",
                                std::source_location::current(), thread_id, i, distance);
                }
            }
            long double strength = computeInteraction(static_cast<int>(i), distance);
            auto vecPot = computeVectorPotential(static_cast<int>(i));
            long double GodWaveAmp = computeGodWave(static_cast<int>(i));
            glm::vec3 projIVec(0.0f);
            size_t projDim = std::min<size_t>(3, d);
            for (size_t k = 0; k < projDim; ++k) {
                projIVec[k] = static_cast<float>(v[k] * scaleI);
            }
            localInteractions[thread_id].emplace_back(static_cast<int>(i), distance, strength, vecPot, GodWaveAmp);
            localProjected[thread_id].push_back(projIVec);
        }
        latch.count_down();
    }
    latch.wait();

    // Pre-resize global vectors to avoid reallocation during insertion
    size_t totalInteractions = 0;
    size_t totalProjected = 0;
    for (int t = 0; t < omp_get_max_threads(); ++t) {
        totalInteractions += localInteractions[t].size();
        totalProjected += localProjected[t].size();
    }
    interactions_.reserve(totalInteractions);
    projectedVerts_.reserve(totalProjected);
    for (int t = 0; t < omp_get_max_threads(); ++t) {
        interactions_.insert(interactions_.end(), localInteractions[t].begin(), localInteractions[t].end());
        projectedVerts_.insert(projectedVerts_.end(), localProjected[t].begin(), localProjected[t].end());
    }
    logger_.log(Logging::LogLevel::Info, "Interactions updated: interactions_.size()={}, projectedVerts_.size()={}",
                std::source_location::current(), interactions_.size(), projectedVerts_.size());
    if (totalInteractions != totalProjected || totalInteractions != interactions_.size() || totalProjected != projectedVerts_.size()) {
        logger_.log(Logging::LogLevel::Error, "Mismatch in merged sizes: expected interactions={}, projected={}, actual interactions_.size()={}, projectedVerts_.size()={}",
                    std::source_location::current(), totalInteractions, totalProjected, interactions_.size(), projectedVerts_.size());
        throw std::runtime_error("Mismatch in merged vector sizes");
    }
    validateProjectedVertices();
}

UniversalEquation::EnergyResult UniversalEquation::compute() {
    logger_.log(Logging::LogLevel::Info, "Starting compute: vertices={}, dimension={}",
                std::source_location::current(), nCubeVertices_.size(), getCurrentDimension());
    if (getNeedsUpdate()) {
        updateInteractions();
        needsUpdate_.store(false);
    }

    EnergyResult result{0.0L, 0.0L, 0.0L, 0.0L, 0.0L, 0.0L, 0.0L, 0.0L};
    uint64_t numVertices = std::min(static_cast<uint64_t>(nCubeVertices_.size()), getMaxVertices());
    std::vector<long double> potentials(numVertices, 0.0L);
    std::vector<long double> nurbMatters(numVertices, 0.0L);
    std::vector<long double> nurbEnergies(numVertices, 0.0L);
    std::vector<long double> spinEnergies(numVertices, 0.0L);
    std::vector<long double> momentumEnergies(numVertices, 0.0L);
    std::vector<long double> fieldEnergies(numVertices, 0.0L);
    std::vector<long double> GodWaveEnergies(numVertices, 0.0L);

    // Ensure vectors are properly sized
    if (nCubeVertices_.size() != numVertices || vertexMomenta_.size() != numVertices ||
        vertexSpins_.size() != numVertices || vertexWaveAmplitudes_.size() != numVertices) {
        logger_.log(Logging::LogLevel::Error, "Vector size mismatch: nCubeVertices_={}, vertexMomenta_={}, vertexSpins_={}, vertexWaveAmplitudes_={}",
                    std::source_location::current(), nCubeVertices_.size(), vertexMomenta_.size(),
                    vertexSpins_.size(), vertexWaveAmplitudes_.size());
        throw std::runtime_error("Vector size mismatch in compute");
    }

    std::latch latch(omp_get_max_threads());
    #pragma omp parallel
    {
        int thread_id = omp_get_thread_num();
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Thread {}: computing energies for vertices",
                        std::source_location::current(), thread_id);
        }
        #pragma omp for schedule(dynamic) nowait
        for (uint64_t i = 0; i < numVertices; ++i) {
            if (i >= nCubeVertices_.size()) {
                if (debug_.load()) {
                    logger_.log(Logging::LogLevel::Warning, "Thread {}: skipping vertex {} (exceeds nCubeVertices_.size()={})",
                                std::source_location::current(), thread_id, i, nCubeVertices_.size());
                }
                continue;
            }
            validateVertexIndex(static_cast<int>(i));

            // Compute total gravitational potential for vertex i by summing over sampled vertices
            long double totalPotential = 0.0L;
            uint64_t sampleStep = std::max<uint64_t>(1, numVertices / 100); // Sample ~100 pairs per vertex
            for (uint64_t j = 0; j < numVertices && j < nCubeVertices_.size(); j += sampleStep) {
                if (static_cast<int>(j) == static_cast<int>(i)) continue; // Skip self-interaction
                try {
                    totalPotential += computeGravitationalPotential(static_cast<int>(i), static_cast<int>(j));
                } catch (const std::out_of_range& e) {
                    if (debug_.load()) {
                        logger_.log(Logging::LogLevel::Warning, "Thread {}: skipping invalid vertex pair ({}, {}): {}",
                                    std::source_location::current(), thread_id, i, j, e.what());
                    }
                    continue;
                }
            }
            // Scale up the sampled potential to approximate full sum
            totalPotential *= static_cast<long double>(sampleStep);
            if (std::isnan(totalPotential) || std::isinf(totalPotential)) {
                if (debug_.load()) {
                    logger_.log(Logging::LogLevel::Warning, "Thread {}: invalid totalPotential for vertex {}: {}, resetting to 0",
                                std::source_location::current(), thread_id, i, totalPotential);
                }
                totalPotential = 0.0L;
            }
            potentials[i] = totalPotential;

            nurbMatters[i] = computeNurbMatter(static_cast<int>(i));
            nurbEnergies[i] = computeNurbEnergy(static_cast<int>(i));
            spinEnergies[i] = computeSpinEnergy(static_cast<int>(i));
            momentumEnergies[i] = computeKineticEnergy(static_cast<int>(i));
            fieldEnergies[i] = computeEMField(static_cast<int>(i));
            GodWaveEnergies[i] = computeGodWave(static_cast<int>(i));
        }
        latch.count_down();
    }
    latch.wait();

    for (uint64_t i = 0; i < numVertices && i < nCubeVertices_.size(); ++i) {
        result.observable += potentials[i] + nurbMatters[i] + nurbEnergies[i] + spinEnergies[i] + momentumEnergies[i] + fieldEnergies[i] + GodWaveEnergies[i];
        result.potential += potentials[i];
        result.nurbMatter += nurbMatters[i];
        result.nurbEnergy += nurbEnergies[i];
        result.spinEnergy += spinEnergies[i];
        result.momentumEnergy += momentumEnergies[i];
        result.fieldEnergy += fieldEnergies[i];
        result.GodWaveEnergy += GodWaveEnergies[i];
    }
    result.observable = safe_div(result.observable, static_cast<long double>(numVertices));
    logger_.log(Logging::LogLevel::Info, "Compute completed: {}",
                std::source_location::current(), result.toString());
    return result;
}

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
            validateProjectedVertices();
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

void UniversalEquation::initializeCalculator(AMOURANTH* amouranth) {
    logger_.log(Logging::LogLevel::Info, "Initializing calculator with AMOURANTH={}",
                std::source_location::current(), static_cast<void*>(amouranth));
    try {
        if (amouranth) {
            navigator_ = amouranth->getNavigator();
            if (navigator_) {
                navigator_->initialize(getCurrentDimension(), getMaxVertices());
                logger_.log(Logging::LogLevel::Debug, "DimensionalNavigator initialized: navigator_={}",
                            std::source_location::current(), static_cast<void*>(navigator_));
            } else {
                logger_.log(Logging::LogLevel::Warning, "AMOURANTH provided but navigator_ is null",
                            std::source_location::current());
            }
        } else {
            logger_.log(Logging::LogLevel::Warning, "AMOURANTH is null, skipping navigator initialization",
                        std::source_location::current());
        }
        needsUpdate_.store(true);
        initializeWithRetry();
        validateProjectedVertices();
    } catch (const std::exception& e) {
        logger_.log(Logging::LogLevel::Error, "initializeCalculator failed: {}", std::source_location::current(), e.what());
        throw;
    }
}

void UniversalEquation::setGodWaveFreq(long double value) {
    GodWaveFreq_.store(std::clamp(value, 0.1L, 10.0L));
    needsUpdate_.store(true);
    logger_.log(Logging::LogLevel::Debug, "Set GodWaveFreq: value={}", std::source_location::current(), GodWaveFreq_.load());
}

long double UniversalEquation::computeNurbMatter(int vertexIndex) const {
    validateVertexIndex(vertexIndex);
    long double result = getNurbMatterStrength() * vertexWaveAmplitudes_[vertexIndex] * 0.5L;
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Computed NURB matter for vertex {}: result={}",
                    std::source_location::current(), vertexIndex, result);
    }
    return result;
}

long double UniversalEquation::computeNurbEnergy(int vertexIndex) const {
    validateVertexIndex(vertexIndex);
    long double result = getNurbEnergyStrength() * vertexWaveAmplitudes_[vertexIndex] * 0.3L;
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Computed NURB energy for vertex {}: result={}",
                    std::source_location::current(), vertexIndex, result);
    }
    return result;
}

long double UniversalEquation::computeSpinEnergy(int vertexIndex) const {
    validateVertexIndex(vertexIndex);
    long double result = getSpinInteraction() * vertexSpins_[vertexIndex] * 0.2L;
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Computed spin energy for vertex {}: result={}",
                    std::source_location::current(), vertexIndex, result);
    }
    return result;
}

long double UniversalEquation::computeEMField(int vertexIndex) const {
    validateVertexIndex(vertexIndex);
    long double result = getEMFieldStrength() * vertexWaveAmplitudes_[vertexIndex] * 0.01L;
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Computed EM field for vertex {}: result={}",
                    std::source_location::current(), vertexIndex, result);
    }
    return result;
}

long double UniversalEquation::computeGodWave(int vertexIndex) const {
    validateVertexIndex(vertexIndex);
    long double result = getGodWaveFreq() * vertexWaveAmplitudes_[vertexIndex] * 0.1L;
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Computed God wave for vertex {}: result={}",
                    std::source_location::current(), vertexIndex, result);
    }
    return result;
}

long double UniversalEquation::computeInteraction(int vertexIndex, long double distance) const {
    validateVertexIndex(vertexIndex);
    long double result = getInfluence() * safe_div(1.0L, distance + 1e-10L);
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Computed interaction for vertex {}: distance={}, result={}",
                    std::source_location::current(), vertexIndex, distance, result);
    }
    return result;
}

std::vector<long double> UniversalEquation::computeVectorPotential(int vertexIndex) const {
    validateVertexIndex(vertexIndex);
    std::vector<long double> result(std::min(3, getCurrentDimension()), 0.0L);
    for (int i = 0; i < std::min(3, getCurrentDimension()); ++i) {
        result[i] = vertexMomenta_[vertexIndex][i] * getWeak();
    }
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Computed vector potential for vertex {}: result size={}",
                    std::source_location::current(), vertexIndex, result.size());
    }
    return result;
}

long double UniversalEquation::safeExp(long double x) const {
    if (std::isnan(x) || std::isinf(x)) {
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Warning, "Invalid input to safeExp: x={}", std::source_location::current(), x);
        }
        return 0.0L;
    }
    if (x > 100.0L) {
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Warning, "Clamping large exponent in safeExp: x={}", std::source_location::current(), x);
        }
        x = 100.0L;
    }
    long double result = std::exp(x);
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Computed safeExp: x={}, result={}",
                    std::source_location::current(), x, result);
    }
    return result;
}

long double UniversalEquation::safe_div(long double a, long double b) const {
    if (b == 0.0L || std::isnan(b) || std::isinf(b)) {
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Warning, "Invalid divisor in safe_div: a={}, b={}",
                        std::source_location::current(), a, b);
        }
        return 0.0L;
    }
    long double result = a / b;
    if (std::isnan(result) || std::isinf(result)) {
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Warning, "Invalid result in safe_div: a={}, b={}, result={}",
                        std::source_location::current(), a, b, result);
        }
        return 0.0L;
    }
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Computed safe_div: a={}, b={}, result={}",
                    std::source_location::current(), a, b, result);
    }
    return result;
}

void UniversalEquation::validateVertexIndex(int vertexIndex, const std::source_location& loc) const {
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= nCubeVertices_.size()) {
        logger_.log(Logging::LogLevel::Error, "Invalid vertexIndex: vertexIndex={}, size={}",
                    loc, vertexIndex, nCubeVertices_.size());
        throw std::out_of_range("Invalid vertex index");
    }
}

void UniversalEquation::setCurrentDimension(int dimension) {
    currentDimension_.store(std::clamp(dimension, 1, maxDimensions_));
    needsUpdate_.store(true);
    logger_.log(Logging::LogLevel::Debug, "Set currentDimension: value={}", std::source_location::current(), dimension);
}

void UniversalEquation::setMode(int mode) {
    mode_.store(std::clamp(mode, 1, maxDimensions_));
    needsUpdate_.store(true);
    logger_.log(Logging::LogLevel::Debug, "Set mode: value={}", std::source_location::current(), mode);
}

void UniversalEquation::setInfluence(long double value) {
    influence_.store(std::clamp(value, 0.0L, 10.0L));
    needsUpdate_.store(true);
    logger_.log(Logging::LogLevel::Debug, "Set influence: value={}", std::source_location::current(), value);
}

void UniversalEquation::setWeak(long double value) {
    weak_.store(std::clamp(value, 0.0L, 1.0L));
    needsUpdate_.store(true);
    logger_.log(Logging::LogLevel::Debug, "Set weak: value={}", std::source_location::current(), value);
}

void UniversalEquation::setCollapse(long double value) {
    collapse_.store(std::clamp(value, 0.0L, 5.0L));
    needsUpdate_.store(true);
    logger_.log(Logging::LogLevel::Debug, "Set collapse: value={}", std::source_location::current(), value);
}

void UniversalEquation::setTwoD(long double value) {
    twoD_.store(std::clamp(value, 0.0L, 5.0L));
    needsUpdate_.store(true);
    logger_.log(Logging::LogLevel::Debug, "Set twoD: value={}", std::source_location::current(), value);
}

void UniversalEquation::setThreeDInfluence(long double value) {
    threeDInfluence_.store(std::clamp(value, 0.0L, 5.0L));
    needsUpdate_.store(true);
    logger_.log(Logging::LogLevel::Debug, "Set threeDInfluence: value={}", std::source_location::current(), value);
}

void UniversalEquation::setOneDPermeation(long double value) {
    oneDPermeation_.store(std::clamp(value, 0.0L, 5.0L));
    needsUpdate_.store(true);
    logger_.log(Logging::LogLevel::Debug, "Set oneDPermeation: value={}", std::source_location::current(), value);
}

void UniversalEquation::setNurbMatterStrength(long double value) {
    nurbMatterStrength_.store(std::clamp(value, 0.0L, 1.0L));
    needsUpdate_.store(true);
    logger_.log(Logging::LogLevel::Debug, "Set nurbMatterStrength: value={}", std::source_location::current(), value);
}

void UniversalEquation::setNurbEnergyStrength(long double value) {
    nurbEnergyStrength_.store(std::clamp(value, 0.0L, 2.0L));
    needsUpdate_.store(true);
    logger_.log(Logging::LogLevel::Debug, "Set nurbEnergyStrength: value={}", std::source_location::current(), value);
}

void UniversalEquation::setAlpha(long double value) {
    alpha_.store(std::clamp(value, 0.01L, 10.0L));
    needsUpdate_.store(true);
    logger_.log(Logging::LogLevel::Debug, "Set alpha: value={}", std::source_location::current(), value);
}

void UniversalEquation::setBeta(long double value) {
    beta_.store(std::clamp(value, 0.0L, 1.0L));
    needsUpdate_.store(true);
    logger_.log(Logging::LogLevel::Debug, "Set beta: value={}", std::source_location::current(), value);
}

void UniversalEquation::setCarrollFactor(long double value) {
    carrollFactor_.store(std::clamp(value, 0.0L, 1.0L));
    needsUpdate_.store(true);
    logger_.log(Logging::LogLevel::Debug, "Set carrollFactor: value={}", std::source_location::current(), value);
}

void UniversalEquation::setMeanFieldApprox(long double value) {
    meanFieldApprox_.store(std::clamp(value, 0.0L, 1.0L));
    needsUpdate_.store(true);
    logger_.log(Logging::LogLevel::Debug, "Set meanFieldApprox: value={}", std::source_location::current(), value);
}

void UniversalEquation::setAsymCollapse(long double value) {
    asymCollapse_.store(std::clamp(value, 0.0L, 1.0L));
    needsUpdate_.store(true);
    logger_.log(Logging::LogLevel::Debug, "Set asymCollapse: value={}", std::source_location::current(), value);
}

void UniversalEquation::setPerspectiveTrans(long double value) {
    perspectiveTrans_.store(std::clamp(value, 0.0L, 10.0L));
    needsUpdate_.store(true);
    logger_.log(Logging::LogLevel::Debug, "Set perspectiveTrans: value={}", std::source_location::current(), value);
}

void UniversalEquation::setPerspectiveFocal(long double value) {
    perspectiveFocal_.store(std::clamp(value, 1.0L, 20.0L));
    needsUpdate_.store(true);
    logger_.log(Logging::LogLevel::Debug, "Set perspectiveFocal: value={}", std::source_location::current(), value);
}

void UniversalEquation::setSpinInteraction(long double value) {
    spinInteraction_.store(std::clamp(value, 0.0L, 1.0L));
    needsUpdate_.store(true);
    logger_.log(Logging::LogLevel::Debug, "Set spinInteraction: value={}", std::source_location::current(), value);
}

void UniversalEquation::setEMFieldStrength(long double value) {
    emFieldStrength_.store(std::clamp(value, 0.0L, 1.0e7L));
    needsUpdate_.store(true);
    logger_.log(Logging::LogLevel::Debug, "Set emFieldStrength: value={}", std::source_location::current(), value);
}

void UniversalEquation::setRenormFactor(long double value) {
    renormFactor_.store(std::clamp(value, 0.1L, 10.0L));
    needsUpdate_.store(true);
    logger_.log(Logging::LogLevel::Debug, "Set renormFactor: value={}", std::source_location::current(), value);
}

void UniversalEquation::setVacuumEnergy(long double value) {
    vacuumEnergy_.store(std::clamp(value, 0.0L, 1.0L));
    needsUpdate_.store(true);
    logger_.log(Logging::LogLevel::Debug, "Set vacuumEnergy: value={}", std::source_location::current(), value);
}

void UniversalEquation::setDebug(bool value) {
    debug_.store(value);
    logger_.log(Logging::LogLevel::Debug, "Set debug: value={}", std::source_location::current(), value);
}

void UniversalEquation::setCurrentVertices(uint64_t value) {
    currentVertices_.store(std::min(value, maxVertices_));
    needsUpdate_.store(true);
    logger_.log(Logging::LogLevel::Debug, "Set currentVertices: value={}", std::source_location::current(), value);
}

void UniversalEquation::setNavigator(DimensionalNavigator* nav) {
    navigator_ = nav;
    needsUpdate_.store(true);
    logger_.log(Logging::LogLevel::Debug, "Set navigator: value={}", std::source_location::current(), static_cast<void*>(nav));
}

void UniversalEquation::setNCubeVertex(int vertexIndex, const std::vector<long double>& vertex) {
    validateVertexIndex(vertexIndex);
    nCubeVertices_[vertexIndex] = vertex;
    needsUpdate_.store(true);
    logger_.log(Logging::LogLevel::Debug, "Set nCubeVertex for index {}: vertex size={}",
                std::source_location::current(), vertexIndex, vertex.size());
}

void UniversalEquation::setVertexMomentum(int vertexIndex, const std::vector<long double>& momentum) {
    validateVertexIndex(vertexIndex);
    vertexMomenta_[vertexIndex] = momentum;
    needsUpdate_.store(true);
    logger_.log(Logging::LogLevel::Debug, "Set vertexMomentum for index {}: momentum size={}",
                std::source_location::current(), vertexIndex, momentum.size());
}

void UniversalEquation::setVertexSpin(int vertexIndex, long double spin) {
    validateVertexIndex(vertexIndex);
    vertexSpins_[vertexIndex] = spin;
    needsUpdate_.store(true);
    logger_.log(Logging::LogLevel::Debug, "Set vertexSpin for index {}: spin={}",
                std::source_location::current(), vertexIndex, spin);
}

void UniversalEquation::setVertexWaveAmplitude(int vertexIndex, long double amplitude) {
    validateVertexIndex(vertexIndex);
    vertexWaveAmplitudes_[vertexIndex] = amplitude;
    needsUpdate_.store(true);
    logger_.log(Logging::LogLevel::Debug, "Set vertexWaveAmplitude for index {}: amplitude={}",
                std::source_location::current(), vertexIndex, amplitude);
}

void UniversalEquation::setProjectedVertex(int vertexIndex, const glm::vec3& vertex) {
    validateVertexIndex(vertexIndex);
    projectedVerts_[vertexIndex] = vertex;
    needsUpdate_.store(true);
    logger_.log(Logging::LogLevel::Debug, "Set projectedVertex for index {}: vertex=({},{},{})",
                std::source_location::current(), vertexIndex, vertex.x, vertex.y, vertex.z);
}

void UniversalEquation::setNCubeVertices(const std::vector<std::vector<long double>>& vertices) {
    nCubeVertices_ = vertices;
    needsUpdate_.store(true);
    logger_.log(Logging::LogLevel::Debug, "Set nCubeVertices: size={}", std::source_location::current(), vertices.size());
}

void UniversalEquation::setVertexMomenta(const std::vector<std::vector<long double>>& momenta) {
    vertexMomenta_ = momenta;
    needsUpdate_.store(true);
    logger_.log(Logging::LogLevel::Debug, "Set vertexMomenta: size={}", std::source_location::current(), momenta.size());
}

void UniversalEquation::setVertexSpins(const std::vector<long double>& spins) {
    vertexSpins_ = spins;
    needsUpdate_.store(true);
    logger_.log(Logging::LogLevel::Debug, "Set vertexSpins: size={}", std::source_location::current(), spins.size());
}

void UniversalEquation::setVertexWaveAmplitudes(const std::vector<long double>& amplitudes) {
    vertexWaveAmplitudes_ = amplitudes;
    needsUpdate_.store(true);
    logger_.log(Logging::LogLevel::Debug, "Set vertexWaveAmplitudes: size={}", std::source_location::current(), amplitudes.size());
}

void UniversalEquation::setProjectedVertices(const std::vector<glm::vec3>& vertices) {
    projectedVerts_ = vertices;
    needsUpdate_.store(true);
    logger_.log(Logging::LogLevel::Debug, "Set projectedVertices: size={}", std::source_location::current(), vertices.size());
    validateProjectedVertices();
}

void UniversalEquation::setTotalCharge(long double value) {
    totalCharge_.store(value);
    logger_.log(Logging::LogLevel::Debug, "Set totalCharge: value={}", std::source_location::current(), value);
}

void UniversalEquation::setMaterialDensity(long double density) {
    materialDensity_.store(std::clamp(density, 0.0L, 1.0e6L)); // Reasonable range for density
    needsUpdate_.store(true);
    logger_.log(Logging::LogLevel::Debug, "Set materialDensity: value={}", std::source_location::current(), materialDensity_.load());
}

void UniversalEquation::evolveTimeStep(long double dt) {
    logger_.log(Logging::LogLevel::Info, "Evolving time step: dt={}", std::source_location::current(), dt);
    for (size_t i = 0; i < nCubeVertices_.size(); ++i) {
        validateVertexIndex(static_cast<int>(i));
        for (size_t j = 0; j < static_cast<size_t>(getCurrentDimension()); ++j) {
            nCubeVertices_[i][j] += vertexMomenta_[i][j] * dt;
        }
    }
    simulationTime_.fetch_add(static_cast<float>(dt));
    needsUpdate_.store(true);
    logger_.log(Logging::LogLevel::Debug, "Time step evolved: simulationTime={}", std::source_location::current(), simulationTime_.load());
}

void UniversalEquation::updateMomentum() {
    logger_.log(Logging::LogLevel::Info, "Updating momentum for {} vertices", std::source_location::current(), nCubeVertices_.size());
    for (size_t i = 0; i < vertexMomenta_.size(); ++i) {
        validateVertexIndex(static_cast<int>(i));
        auto acc = computeGravitationalAcceleration(static_cast<int>(i));
        for (size_t j = 0; j < static_cast<size_t>(getCurrentDimension()); ++j) {
            vertexMomenta_[i][j] += acc[j] * 0.01L;
        }
    }
    needsUpdate_.store(true);
    logger_.log(Logging::LogLevel::Debug, "Momentum updated", std::source_location::current());
}

void UniversalEquation::advanceCycle() {
    logger_.log(Logging::LogLevel::Info, "Advancing simulation cycle", std::source_location::current());
    updateMomentum();
    evolveTimeStep(0.01L);
    logger_.log(Logging::LogLevel::Debug, "Simulation cycle advanced", std::source_location::current());
}

std::vector<UniversalEquation::DimensionData> UniversalEquation::computeBatch(int startDim, int endDim) {
    logger_.log(Logging::LogLevel::Info, "Starting batch computation from dimension {} to {}", std::source_location::current(), startDim, endDim);
    std::vector<DimensionData> results;
    int originalDim = getCurrentDimension();
    for (int dim = startDim; dim <= endDim && dim <= maxDimensions_; ++dim) {
        setCurrentDimension(dim);
        initializeWithRetry();
        EnergyResult result = compute();
        DimensionData data;
        data.dimension = dim;
        data.observable = result.observable;
        data.potential = result.potential;
        data.nurbMatter = result.nurbMatter;
        data.nurbEnergy = result.nurbEnergy;
        data.spinEnergy = result.spinEnergy;
        data.momentumEnergy = result.momentumEnergy;
        data.fieldEnergy = result.fieldEnergy;
        data.GodWaveEnergy = result.GodWaveEnergy;
        results.push_back(data);
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Computed dimension {}: {}", std::source_location::current(), dim, data.toString());
        }
    }
    setCurrentDimension(originalDim);
    initializeWithRetry();
    logger_.log(Logging::LogLevel::Info, "Batch computation completed: results size={}", std::source_location::current(), results.size());
    return results;
}

void UniversalEquation::exportToCSV(const std::string& filename, const std::vector<DimensionData>& data) const {
    logger_.log(Logging::LogLevel::Info, "Exporting to CSV: filename={}", std::source_location::current(), filename);
    std::ofstream file(filename);
    if (!file.is_open()) {
        logger_.log(Logging::LogLevel::Error, "Failed to open file for CSV export: {}", std::source_location::current(), filename);
        throw std::runtime_error("Failed to open CSV file");
    }
    file << "Dimension,Observable,Potential,NURB_Matter,NURB_Energy,Spin_Energy,Momentum_Energy,Field_Energy,God_Wave_Energy\n";
    for (const auto& d : data) {
        file << std::fixed << std::setprecision(10)
             << d.dimension << "," << d.observable << "," << d.potential << ","
             << d.nurbMatter << "," << d.nurbEnergy << "," << d.spinEnergy << ","
             << d.momentumEnergy << "," << d.fieldEnergy << "," << d.GodWaveEnergy << "\n";
    }
    file.close();
    logger_.log(Logging::LogLevel::Debug, "CSV export completed", std::source_location::current());
}

UniversalEquation::DimensionData UniversalEquation::updateCache() {
    logger_.log(Logging::LogLevel::Info, "Updating cache", std::source_location::current());
    EnergyResult result = compute();
    dimensionData_.dimension = getCurrentDimension();
    dimensionData_.observable = result.observable;
    dimensionData_.potential = result.potential;
    dimensionData_.nurbMatter = result.nurbMatter;
    dimensionData_.nurbEnergy = result.nurbEnergy;
    dimensionData_.spinEnergy = result.spinEnergy;
    dimensionData_.momentumEnergy = result.momentumEnergy;
    dimensionData_.fieldEnergy = result.fieldEnergy;
    dimensionData_.GodWaveEnergy = result.GodWaveEnergy;
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Cache updated: {}", std::source_location::current(), dimensionData_.toString());
    }
    return dimensionData_;
}

long double UniversalEquation::computeGodWaveAmplitude(int vertexIndex, long double time) const {
    validateVertexIndex(vertexIndex);
    long double result = getGodWaveFreq() * vertexWaveAmplitudes_[vertexIndex] * std::cos(getGodWaveFreq() * time);
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Computed God wave amplitude for vertex {} at time {}: result={}",
                    std::source_location::current(), vertexIndex, time, result);
    }
    return result;
}

const std::vector<long double>& UniversalEquation::getNCubeVertex(int vertexIndex) const {
    validateVertexIndex(vertexIndex);
    return nCubeVertices_[vertexIndex];
}

const std::vector<long double>& UniversalEquation::getVertexMomentum(int vertexIndex) const {
    validateVertexIndex(vertexIndex);
    return vertexMomenta_[vertexIndex];
}

long double UniversalEquation::getVertexSpin(int vertexIndex) const {
    validateVertexIndex(vertexIndex);
    return vertexSpins_[vertexIndex];
}

long double UniversalEquation::getVertexWaveAmplitude(int vertexIndex) const {
    validateVertexIndex(vertexIndex);
    return vertexWaveAmplitudes_[vertexIndex];
}

const glm::vec3& UniversalEquation::getProjectedVertex(int vertexIndex) const {
    validateVertexIndex(vertexIndex);
    return projectedVerts_[vertexIndex];
}