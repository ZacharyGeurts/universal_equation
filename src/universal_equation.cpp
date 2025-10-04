// UniversalEquation.cpp: Quantum simulation for n-dimensional hypercube lattices with NURBS fields.
// Models quantum effects, deterministic collapse, and QED features with thread-safe, high-precision calculations.
// Features: OpenMP optimization, memory safety, CSV exports, and Vulkan integration.
// Usage: UniversalEquation eq(5, 3, 1.0); eq.evolveTimeStep(0.01); eq.exportToCSV("data.csv", eq.computeBatch(1, 5));
// Zachary Geurts 2025 (powered by Grok with Heisenberg swagger)

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

// Computes NURBS basis function
long double UniversalEquation::computeNURBSBasis(int i, int p, long double u, const std::vector<long double>& knots) const {
    if (i < 0 || i + p + 1 >= static_cast<int>(knots.size())) {
        return 0.0L;
    }
    if (p == 0) {
        return (knots[i] <= u && u < knots[i + 1]) ? 1.0L : 0.0L;
    }
    long double denom1 = knots[i + p] - knots[i];
    long double denom2 = knots[i + p + 1] - knots[i + 1];
    long double term1 = 0.0L, term2 = 0.0L;
    if (denom1 > 1e-15L) {
        term1 = ((u - knots[i]) / denom1) * computeNURBSBasis(i, p - 1, u, knots);
    }
    if (denom2 > 1e-15L) {
        term2 = ((knots[i + p + 1] - u) / denom2) * computeNURBSBasis(i + 1, p - 1, u, knots);
    }
    long double result = term1 + term2;
    if (std::isnan(result) || std::isinf(result)) {
        if (debug_) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cerr << "[ERROR] NURBS basis computation produced invalid result: i=" << i << ", p=" << p << ", u=" << u << "\n";
        }
        return 0.0L;
    }
    return result;
}

// Evaluates NURBS curve
long double UniversalEquation::evaluateNURBS(long double u, const std::vector<long double>& controlPoints,
                                            const std::vector<long double>& weights,
                                            const std::vector<long double>& knots, int degree) const {
    if (controlPoints.size() != weights.size() || controlPoints.empty()) {
        throw std::invalid_argument("Invalid NURBS control points or weights");
    }
    long double num = 0.0L, denom = 0.0L;
    for (size_t i = 0; i < controlPoints.size(); ++i) {
        long double basis = computeNURBSBasis(i, degree, u, knots);
        num += basis * weights[i] * controlPoints[i];
        denom += basis * weights[i];
    }
    long double result = (denom > 1e-15L) ? num / denom : 0.0L;
    if (std::isnan(result) || std::isinf(result)) {
        if (debug_) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cerr << "[ERROR] NURBS evaluation produced invalid result: u=" << u << "\n";
        }
        return 0.0L;
    }
    return result;
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

// Computes spin interaction
long double UniversalEquation::computeSpinInteraction(int vertexIndex1, int vertexIndex2) const {
    if (vertexIndex1 < 0 || vertexIndex2 < 0 || static_cast<size_t>(vertexIndex1) >= vertexSpins_.size() ||
        static_cast<size_t>(vertexIndex2) >= vertexSpins_.size()) {
        throw std::out_of_range("Invalid vertex index for spin");
    }
    long double spin1 = vertexSpins_[vertexIndex1];
    long double spin2 = vertexSpins_[vertexIndex2];
    long double result = spinInteraction_.load() * spin1 * spin2;
    if (std::isnan(result) || std::isinf(result)) {
        if (debug_) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cerr << "[ERROR] Invalid spin interaction: spin1=" << spin1 << ", spin2=" << spin2 << "\n";
        }
        return 0.0L;
    }
    return result;
}

// Computes vector potential
std::vector<long double> UniversalEquation::computeVectorPotential(int vertexIndex, long double distance) const {
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= nCubeVertices_.size()) {
        throw std::out_of_range("Invalid vertex index for vector potential");
    }
    const auto& vertex = nCubeVertices_[vertexIndex];
    long double charge = vertexSpins_[vertexIndex] * 2.0L;
    std::vector<long double> vecPot(std::min(3, currentDimension_.load()), 0.0L);
    for (int k = 0; k < std::min(3, currentDimension_.load()); ++k) {
        vecPot[k] = charge * vertex[k] / (1.0L + std::max(distance, 1e-15L));
    }
    return vecPot;
}

// Computes EM field
std::vector<long double> UniversalEquation::computeEMField(int vertexIndex) const {
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= nCubeVertices_.size()) {
        throw std::out_of_range("Invalid vertex index for EM field");
    }
    std::vector<long double> field(std::min(3, currentDimension_.load()), 0.0L);
    long double phase = omega_ * currentDimension_.load();
    for (int k = 0; k < std::min(3, currentDimension_.load()); ++k) {
        field[k] = emFieldStrength_.load() * std::cos(phase + k * M_PIl / 3.0L);
    }
    return field;
}

// Computes Lorentz factor
long double UniversalEquation::computeLorentzFactor(int vertexIndex) const {
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= vertexMomenta_.size()) {
        throw std::out_of_range("Invalid vertex index for Lorentz factor");
    }
    long double momentumMag = 0.0L;
    const auto& momentum = vertexMomenta_[vertexIndex];
    for (const auto& p : momentum) {
        momentumMag += p * p;
    }
    momentumMag = std::sqrt(std::max(momentumMag, 0.0L));
    long double v = std::min(momentumMag, 0.999L);
    long double result = 1.0L / std::sqrt(1.0L - v * v);
    if (std::isnan(result) || std::isinf(result)) {
        if (debug_) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cerr << "[ERROR] Invalid Lorentz factor: vertex=" << vertexIndex << ", momentumMag=" << momentumMag << "\n";
        }
        return 1.0L;
    }
    return result;
}

// Computes God wave amplitude
long double UniversalEquation::computeGodWaveAmplitude(int vertexIndex, long double distance) const {
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= vertexWaveAmplitudes_.size()) {
        throw std::out_of_range("Invalid vertex index for God wave");
    }
    long double phase = GodWaveFreq_.load() * distance + omega_ * vertexIndex;
    long double amplitude = vertexWaveAmplitudes_[vertexIndex] * std::cos(phase);
    if (currentDimension_.load() == 1) {
        amplitude *= oneDPermeation_.load();
    }
    if (std::isnan(amplitude) || std::isinf(amplitude)) {
        if (debug_) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cerr << "[ERROR] Invalid God wave amplitude: vertex=" << vertexIndex << ", distance=" << distance << "\n";
        }
        return 0.0L;
    }
    return amplitude;
}

// Computes NURBS matter field
long double UniversalEquation::computeNurbMatter(long double distance) const {
    long double u = std::clamp(distance / 10.0L, 0.0L, 1.0L);
    long double result = nurbMatterStrength_.load() * evaluateNURBS(u, nurbMatterControlPoints_, nurbWeights_, nurbKnots_, 3);
    if (std::isnan(result) || std::isinf(result)) {
        if (debug_) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cerr << "[ERROR] Invalid NURBS matter: distance=" << distance << ", u=" << u << "\n";
        }
        return 0.0L;
    }
    return result;
}

// Computes NURBS energy field
long double UniversalEquation::computeNurbEnergy(long double distance) const {
    long double u = std::clamp(distance / 10.0L, 0.0L, 1.0L);
    long double base = nurbEnergyStrength_.load() * evaluateNURBS(u, nurbEnergyControlPoints_, nurbWeights_, nurbKnots_, 3);
    long double vacuumTerm = vacuumEnergy_.load() * (1.0L + u * invMaxDim_);
    long double waveTerm = nurbEnergyStrength_.load() * std::cos(GodWaveFreq_.load() * distance);
    long double result = base + vacuumTerm + waveTerm;
    if (std::isnan(result) || std::isinf(result)) {
        if (debug_) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cerr << "[ERROR] Invalid NURBS energy: distance=" << distance << ", u=" << u << "\n";
        }
        return 0.0L;
    }
    return result;
}

// Computes interaction strength
long double UniversalEquation::computeInteraction(int vertexIndex, long double distance) const {
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= nCubeVertices_.size()) {
        throw std::out_of_range("Invalid vertex index for interaction");
    }
    long double denom = std::max(1e-15L, std::pow(static_cast<long double>(currentDimension_.load()), (vertexIndex % maxDimensions_ + 1)));
    long double modifier = (currentDimension_.load() > 3 && (vertexIndex % maxDimensions_ + 1) > 3) ? weak_.load() : 1.0L;
    if (currentDimension_.load() == 3 && ((vertexIndex % maxDimensions_ + 1) == 2 || (vertexIndex % maxDimensions_ + 1) == 4)) {
        modifier *= threeDInfluence_.load();
    }
    long double lorentzFactor = computeLorentzFactor(vertexIndex);
    long double adjustedDistance = distance / std::max(lorentzFactor, 1e-15L);
    auto vecPot = computeVectorPotential(vertexIndex, adjustedDistance);
    long double vecPotMag = 0.0L;
    for (const auto& v : vecPot) {
        vecPotMag += v * v;
    }
    vecPotMag = std::sqrt(std::max(vecPotMag, 0.0L));
    long double spinTerm = computeSpinInteraction(vertexIndex, 0);
    auto emField = computeEMField(vertexIndex);
    long double fieldMag = 0.0L;
    for (const auto& f : emField) {
        fieldMag += f * f;
    }
    fieldMag = std::sqrt(std::max(fieldMag, 0.0L));
    long double GodWaveAmp = computeGodWaveAmplitude(vertexIndex, adjustedDistance);
    long double overlapFactor = 1.0L + GodWaveAmp * GodWaveAmp;
    long double recoilTerm = 0.0L;
    for (const auto& p : vertexMomenta_[vertexIndex]) {
        recoilTerm += p * p;
    }
    recoilTerm = std::sqrt(std::max(recoilTerm, 0.0L)) * 0.1L;
    long double result = influence_.load() * (1.0L / (denom * (1.0L + adjustedDistance))) * modifier *
                        (1.0L + spinTerm) * (1.0L + fieldMag) * vecPotMag * overlapFactor * (1.0L + recoilTerm);
    if (std::isnan(result) || std::isinf(result)) {
        if (debug_) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cerr << "[ERROR] Invalid interaction: vertex=" << vertexIndex << ", distance=" << adjustedDistance << "\n";
        }
        return 0.0L;
    }
    return result;
}

// Computes energies
UniversalEquation::EnergyResult UniversalEquation::compute() const {
    if (nCubeVertices_.empty()) {
        throw std::runtime_error("nCubeVertices_ is empty in compute");
    }
    if (needsUpdate_) {
        updateInteractions();
    }
    long double observable = influence_.load();
    int currDim = currentDimension_.load();
    if (currDim >= 2) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (cachedCos_.empty()) {
            throw std::runtime_error("cachedCos_ is empty");
        }
        observable += twoD_.load() * cachedCos_[currDim % cachedCos_.size()];
    }
    if (currDim == 3) {
        observable += threeDInfluence_.load();
    }
    long double carrollMod = 1.0L - carrollFactor_.load() * (1.0L - invMaxDim_ * currDim);
    observable *= carrollMod;
    long double avgScale = 1.0L;
    {
        std::vector<DimensionInteraction> localInteractions;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            localInteractions = interactions_;
        }
        if (!localInteractions.empty()) {
            long double sumScale = 0.0L;
            #pragma omp parallel for schedule(dynamic) reduction(+:sumScale)
            for (size_t i = 0; i < localInteractions.size(); ++i) {
                sumScale += perspectiveFocal_.load() / (perspectiveFocal_.load() + localInteractions[i].distance);
            }
            avgScale = sumScale / localInteractions.size();
        }
        std::lock_guard<std::mutex> lock(projMutex_);
        avgProjScale_ = avgScale;
    }
    observable *= avgScale;
    long double totalNurbMatter = 0.0L, totalNurbEnergy = 0.0L, interactionSum = 0.0L,
                totalSpinEnergy = 0.0L, totalMomentumEnergy = 0.0L, totalFieldEnergy = 0.0L,
                totalGodWaveEnergy = 0.0L;
    {
        std::vector<DimensionInteraction> localInteractions;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            localInteractions = interactions_;
        }
        if (localInteractions.size() > 1000) {
            #pragma omp parallel for schedule(dynamic) reduction(+:interactionSum,totalNurbMatter,totalNurbEnergy,totalSpinEnergy,totalMomentumEnergy,totalFieldEnergy,totalGodWaveEnergy)
            for (size_t i = 0; i < localInteractions.size(); ++i) {
                const auto& interaction = localInteractions[i];
                long double influence = interaction.strength;
                long double permeation = computePermeation(interaction.vertexIndex);
                long double nurbMatter = computeNurbMatter(interaction.distance);
                long double nurbEnergy = computeNurbEnergy(interaction.distance);
                long double spinTerm = computeSpinInteraction(interaction.vertexIndex, 0);
                long double momentumMag = 0.0L;
                for (const auto& p : vertexMomenta_[interaction.vertexIndex]) {
                    momentumMag += p * p;
                }
                momentumMag = std::sqrt(std::max(momentumMag, 0.0L));
                auto emField = computeEMField(interaction.vertexIndex);
                long double fieldMag = 0.0L;
                for (const auto& f : emField) {
                    fieldMag += f * f;
                }
                fieldMag = std::sqrt(std::max(fieldMag, 0.0L));
                long double GodWaveAmp = computeGodWaveAmplitude(interaction.vertexIndex, interaction.distance);
                long double mfScale = 1.0L - meanFieldApprox_.load() * (1.0L / std::max(1.0L, static_cast<long double>(localInteractions.size())));
                long double renorm = 1.0L / (1.0L + renormFactor_.load() * interactionSum);
                interactionSum += influence * safeExp(-alpha_.load() * interaction.distance) * permeation * nurbMatter * mfScale * renorm;
                totalNurbMatter += nurbMatter * influence * permeation * mfScale * renorm;
                totalNurbEnergy += nurbEnergy * influence * permeation * mfScale * renorm;
                totalSpinEnergy += spinTerm * influence * permeation * mfScale * renorm;
                totalMomentumEnergy += momentumMag * influence * permeation * mfScale * renorm;
                totalFieldEnergy += fieldMag * influence * permeation * mfScale * renorm;
                totalGodWaveEnergy += GodWaveAmp * GodWaveAmp * influence * permeation * mfScale * renorm;
            }
        } else {
            for (const auto& interaction : localInteractions) {
                long double influence = interaction.strength;
                long double permeation = computePermeation(interaction.vertexIndex);
                long double nurbMatter = computeNurbMatter(interaction.distance);
                long double nurbEnergy = computeNurbEnergy(interaction.distance);
                long double spinTerm = computeSpinInteraction(interaction.vertexIndex, 0);
                long double momentumMag = 0.0L;
                for (const auto& p : vertexMomenta_[interaction.vertexIndex]) {
                    momentumMag += p * p;
                }
                momentumMag = std::sqrt(std::max(momentumMag, 0.0L));
                auto emField = computeEMField(interaction.vertexIndex);
                long double fieldMag = 0.0L;
                for (const auto& f : emField) {
                    fieldMag += f * f;
                }
                fieldMag = std::sqrt(std::max(fieldMag, 0.0L));
                long double GodWaveAmp = computeGodWaveAmplitude(interaction.vertexIndex, interaction.distance);
                long double mfScale = 1.0L - meanFieldApprox_.load() * (1.0L / std::max(1.0L, static_cast<long double>(localInteractions.size())));
                long double renorm = 1.0L / (1.0L + renormFactor_.load() * interactionSum);
                interactionSum += influence * safeExp(-alpha_.load() * interaction.distance) * permeation * nurbMatter * mfScale * renorm;
                totalNurbMatter += nurbMatter * influence * permeation * mfScale * renorm;
                totalNurbEnergy += nurbEnergy * influence * permeation * mfScale * renorm;
                totalSpinEnergy += spinTerm * influence * permeation * mfScale * renorm;
                totalMomentumEnergy += momentumMag * influence * permeation * mfScale * renorm;
                totalFieldEnergy += fieldMag * influence * permeation * mfScale * renorm;
                totalGodWaveEnergy += GodWaveAmp * GodWaveAmp * influence * permeation * mfScale * renorm;
            }
        }
        long double totalInfluence = interactionSum + totalNurbMatter + totalNurbEnergy + totalSpinEnergy +
                                     totalMomentumEnergy + totalFieldEnergy + totalGodWaveEnergy;
        long double normFactor = (totalInfluence > 0.0L) ? totalCharge_ / totalInfluence : 1.0L;
        interactionSum *= normFactor;
        totalNurbMatter *= normFactor;
        totalNurbEnergy *= normFactor;
        totalSpinEnergy *= normFactor;
        totalMomentumEnergy *= normFactor;
        totalFieldEnergy *= normFactor;
        totalGodWaveEnergy *= normFactor;
    }
    observable += interactionSum;
    long double collapse = computeCollapse();
    EnergyResult result = {
        observable + collapse,
        std::max(0.0L, observable - collapse),
        totalNurbMatter,
        totalNurbEnergy,
        totalSpinEnergy,
        totalMomentumEnergy,
        totalFieldEnergy,
        totalGodWaveEnergy
    };
    if (std::isnan(result.observable) || std::isinf(result.observable)) {
        throw std::runtime_error("Numerical instability in compute");
    }
    return result;
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

// Computes permeation factor
long double UniversalEquation::computePermeation(int vertexIndex) const {
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= nCubeVertices_.size()) {
        throw std::out_of_range("Invalid vertex index for permeation");
    }
    if (vertexIndex == 1 || currentDimension_.load() == 1) return oneDPermeation_.load();
    if (currentDimension_.load() == 2 && (vertexIndex % maxDimensions_ + 1) > 2) return twoD_.load();
    if (currentDimension_.load() == 3 && ((vertexIndex % maxDimensions_ + 1) == 2 || (vertexIndex % maxDimensions_ + 1) == 4)) {
        return threeDInfluence_.load();
    }
    const auto& vertex = nCubeVertices_[vertexIndex];
    long double vertexMagnitude = 0.0L;
    for (size_t i = 0; i < std::min(static_cast<size_t>(currentDimension_.load()), vertex.size()); ++i) {
        vertexMagnitude += vertex[i] * vertex[i];
    }
    vertexMagnitude = std::sqrt(std::max(vertexMagnitude, 0.0L));
    long double result = 1.0L + beta_.load() * vertexMagnitude / std::max(1, currentDimension_.load());
    if (std::isnan(result) || std::isinf(result)) {
        if (debug_) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cerr << "[ERROR] Invalid permeation: vertex=" << vertexIndex << ", magnitude=" << vertexMagnitude << "\n";
        }
        return 1.0L;
    }
    return result;
}

// Computes collapse term
long double UniversalEquation::computeCollapse() const {
    if (currentDimension_.load() == 1) return 0.0L;
    long double phase = static_cast<long double>(currentDimension_.load()) / (2 * maxDimensions_);
    std::lock_guard<std::mutex> lock(mutex_);
    if (cachedCos_.empty()) {
        throw std::runtime_error("cachedCos_ is empty");
    }
    long double osc = std::abs(cachedCos_[static_cast<size_t>(2.0L * M_PIl * phase * cachedCos_.size()) % cachedCos_.size()]);
    long double symCollapse = collapse_.load() * currentDimension_.load() * safeExp(-beta_.load() * (currentDimension_.load() - 1)) * (0.8L * osc + 0.2L);
    long double vertexFactor = static_cast<long double>(nCubeVertices_.size() % 10) / 10.0L;
    long double asymTerm = asymCollapse_.load() * std::sin(M_PIl * phase + osc + vertexFactor) * safeExp(-alpha_.load() * phase);
    long double result = std::max(0.0L, symCollapse + asymTerm);
    if (std::isnan(result) || std::isinf(result)) {
        if (debug_) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cerr << "[ERROR] Invalid collapse term: symCollapse=" << symCollapse << ", asymTerm=" << asymTerm << "\n";
        }
        return 0.0L;
    }
    return result;
}

// Updates interactions
void UniversalEquation::updateInteractions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    interactions_.clear();
    {
        std::lock_guard<std::mutex> lock(projMutex_);
        projectedVerts_.clear();
    }
    int d = currentDimension_.load();
    uint64_t numVertices = std::min(static_cast<uint64_t>(1ULL << d), maxVertices_);
    numVertices = std::min(numVertices, static_cast<uint64_t>(1ULL << 10)); // Cap vertices
    if (d > 6) {
        uint64_t lodFactor = 1ULL << (d / 2);
        if (lodFactor == 0) lodFactor = 1;
        numVertices = std::min(numVertices / lodFactor, static_cast<uint64_t>(1024));
    }
    interactions_.reserve(numVertices - 1);
    {
        std::lock_guard<std::mutex> lock(projMutex_);
        projectedVerts_.reserve(numVertices);
    }
    if (nCubeVertices_.empty()) {
        throw std::runtime_error("nCubeVertices_ is empty");
    }
    const auto& referenceVertex = nCubeVertices_[0];
    long double trans = perspectiveTrans_.load();
    long double focal = perspectiveFocal_.load();
    int depthIdx = d - 1;
    long double depthRef = referenceVertex[depthIdx] + trans;
    if (depthRef <= 0.0L) depthRef = 0.001L;
    long double scaleRef = focal / depthRef;
    long double projRef[3] = {0.0L, 0.0L, 0.0L};
    int projDim = std::min(3, d - 1);
    for (int k = 0; k < projDim; ++k) {
        projRef[k] = referenceVertex[k] * scaleRef;
    }
    glm::vec3 projRefVec(static_cast<float>(projRef[0]), static_cast<float>(projRef[1]), static_cast<float>(projRef[2]));
    {
        std::lock_guard<std::mutex> lock(projMutex_);
        projectedVerts_.push_back(projRefVec);
    }
    if (numVertices > 1000) {
        #pragma omp parallel
        {
            std::vector<DimensionInteraction> localInteractions;
            std::vector<glm::vec3> localProjected;
            localInteractions.reserve((numVertices - 1) / omp_get_num_threads() + 1);
            localProjected.reserve((numVertices - 1) / omp_get_num_threads() + 1);
            #pragma omp for schedule(dynamic)
            for (uint64_t i = 1; i < numVertices; ++i) {
                const auto& v = nCubeVertices_[i];
                long double depthI = v[depthIdx] + trans;
                if (depthI <= 0.0L) depthI = 0.001L;
                long double scaleI = focal / depthI;
                long double projI[3] = {0.0L, 0.0L, 0.0L};
                for (int k = 0; k < projDim; ++k) {
                    projI[k] = v[k] * scaleI;
                }
                long double distSq = 0.0L;
                for (int k = 0; k < 3; ++k) {
                    long double diff = projI[k] - projRef[k];
                    distSq += diff * diff;
                }
                long double distance = std::sqrt(std::max(distSq, 0.0L));
                if (d == 1) {
                    distance = std::fabs(v[0] - referenceVertex[0]);
                }
                long double strength = computeInteraction(static_cast<int>(i), distance);
                auto vecPot = computeVectorPotential(static_cast<int>(i), distance);
                long double GodWaveAmp = computeGodWaveAmplitude(static_cast<int>(i), distance);
                localInteractions.emplace_back(static_cast<int>(i), distance, strength, vecPot, GodWaveAmp);
                glm::vec3 projIVec(static_cast<float>(projI[0]), static_cast<float>(projI[1]), static_cast<float>(projI[2]));
                localProjected.push_back(projIVec);
            }
            #pragma omp critical
            {
                interactions_.insert(interactions_.end(), localInteractions.begin(), localInteractions.end());
                std::lock_guard<std::mutex> lock(projMutex_);
                projectedVerts_.insert(projectedVerts_.end(), localProjected.begin(), localProjected.end());
            }
        }
    } else {
        for (uint64_t i = 1; i < numVertices; ++i) {
            const auto& v = nCubeVertices_[i];
            long double depthI = v[depthIdx] + trans;
            if (depthI <= 0.0L) depthI = 0.001L;
            long double scaleI = focal / depthI;
            long double projI[3] = {0.0L, 0.0L, 0.0L};
            for (int k = 0; k < projDim; ++k) {
                projI[k] = v[k] * scaleI;
            }
            long double distSq = 0.0L;
            for (int k = 0; k < 3; ++k) {
                long double diff = projI[k] - projRef[k];
                distSq += diff * diff;
            }
            long double distance = std::sqrt(std::max(distSq, 0.0L));
            if (d == 1) {
                distance = std::fabs(v[0] - referenceVertex[0]);
            }
            long double strength = computeInteraction(static_cast<int>(i), distance);
            auto vecPot = computeVectorPotential(static_cast<int>(i), distance);
            long double GodWaveAmp = computeGodWaveAmplitude(static_cast<int>(i), distance);
            interactions_.emplace_back(static_cast<int>(i), distance, strength, vecPot, GodWaveAmp);
            glm::vec3 projIVec(static_cast<float>(projI[0]), static_cast<float>(projI[1]), static_cast<float>(projI[2]));
            {
                std::lock_guard<std::mutex> lock(projMutex_);
                projectedVerts_.push_back(projIVec);
            }
        }
    }
    if (d == 3) {
        for (int adj : {2, 4}) {
            size_t vertexIndex = static_cast<size_t>(adj - 1);
            if (vertexIndex < nCubeVertices_.size() &&
                std::none_of(interactions_.begin(), interactions_.end(),
                             [adj](const auto& i) { return i.vertexIndex == adj; })) {
                const auto& v = nCubeVertices_[vertexIndex];
                long double depthI = v[depthIdx] + trans;
                if (depthI <= 0.0L) depthI = 0.001L;
                long double scaleI = focal / depthI;
                long double projI[3] = {0.0L, 0.0L, 0.0L};
                for (int k = 0; k < projDim; ++k) {
                    projI[k] = v[k] * scaleI;
                }
                long double distSq = 0.0L;
                for (int k = 0; k < 3; ++k) {
                    long double diff = projI[k] - projRef[k];
                    distSq += diff * diff;
                }
                long double distance = std::sqrt(std::max(distSq, 0.0L));
                long double strength = computeInteraction(static_cast<int>(vertexIndex), distance);
                auto vecPot = computeVectorPotential(static_cast<int>(vertexIndex), distance);
                long double GodWaveAmp = computeGodWaveAmplitude(static_cast<int>(vertexIndex), distance);
                interactions_.emplace_back(static_cast<int>(vertexIndex), distance, strength, vecPot, GodWaveAmp);
                glm::vec3 projIVec(static_cast<float>(projI[0]), static_cast<float>(projI[1]), static_cast<float>(projI[2]));
                {
                    std::lock_guard<std::mutex> lock(projMutex_);
                    projectedVerts_.push_back(projIVec);
                }
            }
        }
    }
    needsUpdate_ = false;
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