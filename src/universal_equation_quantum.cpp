// universal_equation_quantum.cpp: Quantum-specific computations for the UniversalEquation class.
// Implements NURBS-based field calculations, spin interactions, electromagnetic fields, Lorentz factors,
// and wave function dynamics for quantum simulation on n-dimensional hypercube lattices.

// How this ties into the quantum world:
// The UniversalEquation class simulates quantum phenomena by modeling particles and fields on an n-dimensional hypercube lattice,
// integrating concepts from quantum mechanics and quantum electrodynamics (QED). It represents quantum states through vertex-based
// wave amplitudes and spins, evolving them over time with interactions governed by electromagnetic fields, Lorentz factors, and
// NURBS-based matter and energy fields. The "God wave" introduces a phase-coherent oscillation to mimic quantum coherence and
// entanglement effects. Deterministic wave function collapse is modeled via the computeCollapse method, balancing symmetric and
// asymmetric terms influenced by lattice dimensionality. The system incorporates relativistic effects through the Lorentz factor and
// supports high-dimensional simulations with thread-safe, high-precision calculations optimized via OpenMP.

// Copyright Zachary Geurts 2025 (powered by Grok with Heisenberg swagger) - "Science B*!"

#include "universal_equation.hpp"
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <mutex>
#include <omp.h>

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