// universal_equation_quantum.cpp: Quantum-specific computations for the UniversalEquation class.
// Implements NURBS-based field calculations, spin interactions, electromagnetic fields, Lorentz factors,
// and wave function dynamics for quantum simulation on n-dimensional hypercube lattices.
// Uses Logging::Logger for consistent logging across the AMOURANTH RTX Engine.

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
#include <numbers>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <latch>
#include <omp.h>
#include <glm/glm.hpp>
#include "engine/logging.hpp"

// Helper: Safe division to prevent NaN/Inf
inline long double safe_div(long double a, long double b) {
    return (b != 0.0L && !std::isnan(b) && !std::isinf(b)) ? a / b : 0.0L;
}

// Computes NURBS basis function
long double UniversalEquation::computeNURBSBasis(int i, int p, long double u, const std::vector<long double>& knots) const {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Computing NURBS basis for i={}, p={}, u={}", 
                    std::source_location::current(), i, p, u);
    }
    if (i < 0 || i + p + 1 >= static_cast<int>(knots.size())) {
        logger_.log(Logging::LogLevel::Error, "Invalid NURBS basis indices: i={}, p={}, knots size={}", 
                    std::source_location::current(), i, p, knots.size());
        return 0.0L;
    }
    if (p == 0) {
        return (knots[i] <= u && u < knots[i + 1]) ? 1.0L : 0.0L;
    }
    long double denom1 = knots[i + p] - knots[i];
    long double denom2 = knots[i + p + 1] - knots[i + 1];
    long double term1 = 0.0L, term2 = 0.0L;
    if (denom1 > 1e-15L) {
        term1 = safe_div((u - knots[i]), denom1) * computeNURBSBasis(i, p - 1, u, knots);
    }
    if (denom2 > 1e-15L) {
        term2 = safe_div((knots[i + p + 1] - u), denom2) * computeNURBSBasis(i + 1, p - 1, u, knots);
    }
    long double result = term1 + term2;
    if (std::isnan(result) || std::isinf(result)) {
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Warning, "NURBS basis computation produced invalid result: i={}, p={}, u={}", 
                        std::source_location::current(), i, p, u);
        }
        return 0.0L;
    }
    return result;
}

// Evaluates NURBS curve
long double UniversalEquation::evaluateNURBS(long double u, const std::vector<long double>& controlPoints,
                                            const std::vector<long double>& weights,
                                            const std::vector<long double>& knots, int degree) const {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Evaluating NURBS curve for u={}", std::source_location::current(), u);
    }
    if (controlPoints.size() != weights.size() || controlPoints.empty()) {
        logger_.log(Logging::LogLevel::Error, "Invalid NURBS control points or weights: controlPoints size={}, weights size={}",
                    std::source_location::current(), controlPoints.size(), weights.size());
        throw std::invalid_argument("Invalid NURBS control points or weights");
    }
    long double num = 0.0L, denom = 0.0L;
    for (size_t i = 0; i < controlPoints.size(); ++i) {
        long double basis = computeNURBSBasis(i, degree, u, knots);
        num += basis * weights[i] * controlPoints[i];
        denom += basis * weights[i];
    }
    long double result = safe_div(num, denom);
    if (std::isnan(result) || std::isinf(result)) {
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Warning, "NURBS evaluation produced invalid result: u={}", 
                        std::source_location::current(), u);
        }
        return 0.0L;
    }
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "NURBS evaluation result: {}", std::source_location::current(), result);
    }
    return result;
}

// Computes spin interaction
long double UniversalEquation::computeSpinInteraction(int vertexIndex1, int vertexIndex2) const {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Computing spin interaction between vertices {} and {}", 
                    std::source_location::current(), vertexIndex1, vertexIndex2);
    }
    if (vertexIndex1 < 0 || vertexIndex2 < 0 || static_cast<size_t>(vertexIndex1) >= vertexSpins_.size() ||
        static_cast<size_t>(vertexIndex2) >= vertexSpins_.size()) {
        logger_.log(Logging::LogLevel::Error, "Invalid vertex index for spin: vertex1={}, vertex2={}, spins size={}",
                    std::source_location::current(), vertexIndex1, vertexIndex2, vertexSpins_.size());
        throw std::out_of_range("Invalid vertex index for spin");
    }
    long double spin1 = vertexSpins_[vertexIndex1];
    long double spin2 = vertexSpins_[vertexIndex2];
    long double result = spinInteraction_.load() * spin1 * spin2;
    if (std::isnan(result) || std::isinf(result)) {
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Warning, "Invalid spin interaction: spin1={}, spin2={}", 
                        std::source_location::current(), spin1, spin2);
        }
        return 0.0L;
    }
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Spin interaction result: {}", std::source_location::current(), result);
    }
    return result;
}

// Computes vector potential
std::vector<long double> UniversalEquation::computeVectorPotential(int vertexIndex, long double distance) const {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Computing vector potential for vertex {}, distance={}", 
                    std::source_location::current(), vertexIndex, distance);
    }
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= nCubeVertices_.size()) {
        logger_.log(Logging::LogLevel::Error, "Invalid vertex index for vector potential: vertex={}, vertices size={}",
                    std::source_location::current(), vertexIndex, nCubeVertices_.size());
        throw std::out_of_range("Invalid vertex index for vector potential");
    }
    const auto& vertex = nCubeVertices_[vertexIndex];
    long double charge = vertexSpins_[vertexIndex] * 2.0L;
    std::vector<long double> vecPot(std::min(3, currentDimension_.load()), 0.0L);
    for (int k = 0; k < std::min(3, currentDimension_.load()); ++k) {
        vecPot[k] = safe_div(charge * vertex[k], (1.0L + std::max(distance, 1e-15L)));
        if (std::isnan(vecPot[k]) || std::isinf(vecPot[k])) {
            if (debug_.load()) {
                logger_.log(Logging::LogLevel::Warning, "Invalid vector potential component k={}: charge={}, vertex[k]={}, distance={}",
                            std::source_location::current(), k, charge, vertex[k], distance);
            }
            vecPot[k] = 0.0L;
        }
    }
    return vecPot;
}

// Computes EM field
std::vector<long double> UniversalEquation::computeEMField(int vertexIndex) const {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Computing EM field for vertex {}", std::source_location::current(), vertexIndex);
    }
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= nCubeVertices_.size()) {
        logger_.log(Logging::LogLevel::Error, "Invalid vertex index for EM field: vertex={}, vertices size={}",
                    std::source_location::current(), vertexIndex, nCubeVertices_.size());
        throw std::out_of_range("Invalid vertex index for EM field");
    }
    std::vector<long double> field(std::min(3, currentDimension_.load()), 0.0L);
    long double phase = omega_ * currentDimension_.load();
    for (int k = 0; k < std::min(3, currentDimension_.load()); ++k) {
        field[k] = emFieldStrength_.load() * std::cos(phase + k * std::numbers::pi_v<long double> / 3.0L);
        if (std::isnan(field[k]) || std::isinf(field[k])) {
            if (debug_.load()) {
                logger_.log(Logging::LogLevel::Warning, "Invalid EM field component k={}: phase={}", 
                            std::source_location::current(), k, phase);
            }
            field[k] = 0.0L;
        }
    }
    return field;
}

// Computes Lorentz factor
long double UniversalEquation::computeLorentzFactor(int vertexIndex) const {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Computing Lorentz factor for vertex {}", 
                    std::source_location::current(), vertexIndex);
    }
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= vertexMomenta_.size()) {
        logger_.log(Logging::LogLevel::Error, "Invalid vertex index for Lorentz factor: vertex={}, momenta size={}",
                    std::source_location::current(), vertexIndex, vertexMomenta_.size());
        throw std::out_of_range("Invalid vertex index for Lorentz factor");
    }
    long double momentumMag = 0.0L;
    const auto& momentum = vertexMomenta_[vertexIndex];
    for (const auto& p : momentum) {
        momentumMag += p * p;
    }
    momentumMag = std::sqrt(std::max(momentumMag, 0.0L));
    long double v = std::min(momentumMag, 0.999L);
    long double result = safe_div(1.0L, std::sqrt(1.0L - v * v));
    if (std::isnan(result) || std::isinf(result)) {
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Warning, "Invalid Lorentz factor: vertex={}, momentumMag={}", 
                        std::source_location::current(), vertexIndex, momentumMag);
        }
        return 1.0L;
    }
    return result;
}

// Computes God wave amplitude
long double UniversalEquation::computeGodWaveAmplitude(int vertexIndex, long double distance) const {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Computing God wave amplitude for vertex {}, distance={}", 
                    std::source_location::current(), vertexIndex, distance);
    }
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= vertexWaveAmplitudes_.size()) {
        logger_.log(Logging::LogLevel::Error, "Invalid vertex index for God wave: vertex={}, amplitudes size={}",
                    std::source_location::current(), vertexIndex, vertexWaveAmplitudes_.size());
        throw std::out_of_range("Invalid vertex index for God wave");
    }
    long double phase = GodWaveFreq_.load() * distance + omega_ * vertexIndex;
    long double amplitude = vertexWaveAmplitudes_[vertexIndex] * std::cos(phase);
    if (currentDimension_.load() == 1) {
        amplitude *= oneDPermeation_.load();
    }
    if (std::isnan(amplitude) || std::isinf(amplitude)) {
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Warning, "Invalid God wave amplitude: vertex={}, distance={}", 
                        std::source_location::current(), vertexIndex, distance);
        }
        return 0.0L;
    }
    return amplitude;
}

// Computes NURBS matter field
long double UniversalEquation::computeNurbMatter(long double distance) const {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Computing NURBS matter field for distance={}", 
                    std::source_location::current(), distance);
    }
    long double u = std::clamp(distance / 10.0L, 0.0L, 1.0L);
    if (u != distance / 10.0L && debug_.load()) {
        logger_.log(Logging::LogLevel::Warning, "Distance clamped for NURBS matter: distance={}, u={}", 
                    std::source_location::current(), distance, u);
    }
    long double result = nurbMatterStrength_.load() * evaluateNURBS(u, nurbMatterControlPoints_, nurbWeights_, nurbKnots_, 3);
    if (std::isnan(result) || std::isinf(result)) {
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Warning, "Invalid NURBS matter: distance={}, u={}", 
                        std::source_location::current(), distance, u);
        }
        return 0.0L;
    }
    return result;
}

// Computes NURBS energy field
long double UniversalEquation::computeNurbEnergy(long double distance) const {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Computing NURBS energy field for distance={}", 
                    std::source_location::current(), distance);
    }
    long double u = std::clamp(distance / 10.0L, 0.0L, 1.0L);
    if (u != distance / 10.0L && debug_.load()) {
        logger_.log(Logging::LogLevel::Warning, "Distance clamped for NURBS energy: distance={}, u={}", 
                    std::source_location::current(), distance, u);
    }
    long double base = nurbEnergyStrength_.load() * evaluateNURBS(u, nurbEnergyControlPoints_, nurbWeights_, nurbKnots_, 3);
    long double vacuumTerm = vacuumEnergy_.load() * (1.0L + u * invMaxDim_);
    long double waveTerm = nurbEnergyStrength_.load() * std::cos(GodWaveFreq_.load() * distance);
    long double result = base + vacuumTerm + waveTerm;
    if (std::isnan(result) || std::isinf(result)) {
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Warning, "Invalid NURBS energy: distance={}, u={}", 
                        std::source_location::current(), distance, u);
        }
        return 0.0L;
    }
    return result;
}

// Computes interaction strength
long double UniversalEquation::computeInteraction(int vertexIndex, long double distance) const {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Computing interaction for vertex {}, distance={}", 
                    std::source_location::current(), vertexIndex, distance);
    }
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= nCubeVertices_.size()) {
        logger_.log(Logging::LogLevel::Error, "Invalid vertex index for interaction: vertex={}, vertices size={}",
                    std::source_location::current(), vertexIndex, nCubeVertices_.size());
        throw std::out_of_range("Invalid vertex index for interaction");
    }
    long double denom = std::max(1e-15L, std::pow(static_cast<long double>(currentDimension_.load()), (vertexIndex % maxDimensions_ + 1)));
    long double modifier = (currentDimension_.load() > 3 && (vertexIndex % maxDimensions_ + 1) > 3) ? weak_.load() : 1.0L;
    if (currentDimension_.load() == 3 && ((vertexIndex % maxDimensions_ + 1) == 2 || (vertexIndex % maxDimensions_ + 1) == 4)) {
        modifier *= threeDInfluence_.load();
    }
    long double lorentzFactor = computeLorentzFactor(vertexIndex);
    long double adjustedDistance = safe_div(distance, std::max(lorentzFactor, 1e-15L));
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
    long double result = influence_.load() * safe_div(1.0L, (denom * (1.0L + adjustedDistance))) * modifier *
                        (1.0L + spinTerm) * (1.0L + fieldMag) * vecPotMag * overlapFactor * (1.0L + recoilTerm);
    if (std::isnan(result) || std::isinf(result)) {
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Warning, "Invalid interaction: vertex={}, distance={}", 
                        std::source_location::current(), vertexIndex, adjustedDistance);
        }
        return 0.0L;
    }
    return result;
}

// Computes energies
UniversalEquation::EnergyResult UniversalEquation::compute() const {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Starting energy computation for {} vertices", 
                    std::source_location::current(), nCubeVertices_.size());
    }
    if (nCubeVertices_.empty()) {
        logger_.log(Logging::LogLevel::Error, "nCubeVertices_ is empty in compute", std::source_location::current());
        throw std::runtime_error("nCubeVertices_ is empty in compute");
    }
    if (needsUpdate_.load()) {
        updateInteractions();
    }
    long double observable = influence_.load();
    int currDim = currentDimension_.load();
    if (currDim >= 2) {
        if (cachedCos_.empty()) {
            logger_.log(Logging::LogLevel::Error, "cachedCos_ is empty", std::source_location::current());
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
        std::vector<DimensionInteraction> localInteractions = interactions_;
        if (!localInteractions.empty()) {
            long double sumScale = 0.0L;
            #pragma omp parallel for schedule(dynamic) reduction(+:sumScale)
            for (size_t i = 0; i < localInteractions.size(); ++i) {
                sumScale += safe_div(perspectiveFocal_.load(), (perspectiveFocal_.load() + localInteractions[i].distance));
            }
            avgScale = safe_div(sumScale, static_cast<long double>(localInteractions.size()));
        }
        avgProjScale_.store(avgScale);
    }
    observable *= avgScale;
    long double totalNurbMatter = 0.0L, totalNurbEnergy = 0.0L, interactionSum = 0.0L,
                totalSpinEnergy = 0.0L, totalMomentumEnergy = 0.0L, totalFieldEnergy = 0.0L,
                totalGodWaveEnergy = 0.0L;
    {
        std::vector<DimensionInteraction> localInteractions = interactions_;
        if (localInteractions.size() > 1000) {
            if (debug_.load()) {
                logger_.log(Logging::LogLevel::Info, "Using parallel processing for {} interactions", 
                            std::source_location::current(), localInteractions.size());
            }
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
                long double mfScale = 1.0L - meanFieldApprox_.load() * safe_div(1.0L, std::max(1.0L, static_cast<long double>(localInteractions.size())));
                long double renorm = safe_div(1.0L, (1.0L + renormFactor_.load() * interactionSum));
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
                long double mfScale = 1.0L - meanFieldApprox_.load() * safe_div(1.0L, std::max(1.0L, static_cast<long double>(localInteractions.size())));
                long double renorm = safe_div(1.0L, (1.0L + renormFactor_.load() * interactionSum));
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
        long double normFactor = (totalInfluence > 0.0L) ? safe_div(totalCharge_.load(), totalInfluence) : 1.0L;
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
        logger_.log(Logging::LogLevel::Error, "Numerical instability in compute: observable={}", 
                    std::source_location::current(), result.observable);
        throw std::runtime_error("Numerical instability in compute");
    }
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Energy computation result: observable={}, potential={}, nurbMatter={}, nurbEnergy={}",
                    std::source_location::current(), result.observable, result.potential, result.nurbMatter, result.nurbEnergy);
    }
    return result;
}

// Computes permeation factor
long double UniversalEquation::computePermeation(int vertexIndex) const {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Computing permeation factor for vertex {}", 
                    std::source_location::current(), vertexIndex);
    }
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= nCubeVertices_.size()) {
        logger_.log(Logging::LogLevel::Error, "Invalid vertex index for permeation: vertex={}, vertices size={}",
                    std::source_location::current(), vertexIndex, nCubeVertices_.size());
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
    long double result = 1.0L + beta_.load() * safe_div(vertexMagnitude, std::max(1, currentDimension_.load()));
    if (std::isnan(result) || std::isinf(result)) {
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Warning, "Invalid permeation: vertex={}, magnitude={}", 
                        std::source_location::current(), vertexIndex, vertexMagnitude);
        }
        return 1.0L;
    }
    return result;
}

// Computes collapse term
long double UniversalEquation::computeCollapse() const {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Computing collapse term for dimension {}", 
                    std::source_location::current(), currentDimension_.load());
    }
    if (currentDimension_.load() == 1) return 0.0L;
    long double phase = safe_div(static_cast<long double>(currentDimension_.load()), (2 * maxDimensions_));
    if (cachedCos_.empty()) {
        logger_.log(Logging::LogLevel::Error, "cachedCos_ is empty", std::source_location::current());
        throw std::runtime_error("cachedCos_ is empty");
    }
    long double osc = std::abs(cachedCos_[static_cast<size_t>(2.0L * std::numbers::pi_v<long double> * phase * cachedCos_.size()) % cachedCos_.size()]);
    long double symCollapse = collapse_.load() * currentDimension_.load() * safeExp(-beta_.load() * (currentDimension_.load() - 1)) * (0.8L * osc + 0.2L);
    long double vertexFactor = safe_div(static_cast<long double>(nCubeVertices_.size() % 10), 10.0L);
    long double asymTerm = asymCollapse_.load() * std::sin(std::numbers::pi_v<long double> * phase + osc + vertexFactor) * safeExp(-alpha_.load() * phase);
    long double result = std::max(0.0L, symCollapse + asymTerm);
    if (std::isnan(result) || std::isinf(result)) {
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Warning, "Invalid collapse term: symCollapse={}, asymTerm={}", 
                        std::source_location::current(), symCollapse, asymTerm);
        }
        return 0.0L;
    }
    return result;
}

// Updates interactions
void UniversalEquation::updateInteractions() const {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Updating interactions for {} vertices", 
                    std::source_location::current(), nCubeVertices_.size());
    }
    interactions_.clear();
    projectedVerts_.clear();
    size_t d = static_cast<size_t>(currentDimension_.load());
    uint64_t numVertices = std::min(static_cast<uint64_t>(nCubeVertices_.size()), maxVertices_);
    numVertices = std::min(numVertices, static_cast<uint64_t>(1024)); // Cap vertices for performance
    if (d > 6) {
        uint64_t lodFactor = 1ULL << (d / 2);
        if (lodFactor == 0) lodFactor = 1;
        numVertices = std::min(numVertices / lodFactor, static_cast<uint64_t>(1024));
        if (debug_.load() && numVertices < nCubeVertices_.size()) {
            logger_.log(Logging::LogLevel::Warning, "Reduced vertex count to {} due to high dimension ({})", 
                        std::source_location::current(), numVertices, d);
        }
    }

    // Per-thread local storage for interactions and projections
    std::vector<std::vector<DimensionInteraction>> localInteractions(omp_get_max_threads());
    std::vector<std::vector<glm::vec3>> localProjected(omp_get_max_threads());
    for (int t = 0; t < omp_get_max_threads(); ++t) {
        localInteractions[t].reserve((numVertices - 1) / omp_get_max_threads() + 1);
        localProjected[t].reserve((numVertices - 1) / omp_get_max_threads() + 1);
    }

    // Compute reference vertex projection
    std::vector<long double> referenceVertex(d, 0.0L);
    for (size_t i = 0; i < numVertices && i < nCubeVertices_.size(); ++i) {
        for (size_t j = 0; j < d; ++j) {
            referenceVertex[j] += nCubeVertices_[i][j];
        }
    }
    for (size_t j = 0; j < d; ++j) {
        referenceVertex[j] = safe_div(referenceVertex[j], static_cast<long double>(numVertices));
    }
    long double trans = perspectiveTrans_.load();
    long double focal = perspectiveFocal_.load();
    if (std::isnan(trans) || std::isinf(trans)) {
        trans = 2.0L;
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Warning, "Clamped invalid perspectiveTrans to 2.0", std::source_location::current());
        }
    }
    if (std::isnan(focal) || std::isinf(focal)) {
        focal = 4.0L;
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Warning, "Clamped invalid perspectiveFocal to 4.0", std::source_location::current());
        }
    }
    size_t depthIdx = d > 0 ? d - 1 : 0;
    long double depthRef = referenceVertex[depthIdx] + trans;
    if (depthRef <= 0.0L) {
        depthRef = 0.001L;
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Warning, "Clamped depthRef to 0.001", std::source_location::current());
        }
    }
    long double scaleRef = safe_div(focal, depthRef);
    glm::vec3 projRefVec(0.0f);
    size_t projDim = std::min<size_t>(3, d);
    for (size_t k = 0; k < projDim; ++k) {
        projRefVec[k] = static_cast<float>(referenceVertex[k] * scaleRef);
    }
    projectedVerts_.push_back(projRefVec);

    // Parallel compute interactions and projections
    std::latch latch(omp_get_max_threads());
    #pragma omp parallel
    {
        int thread_id = omp_get_thread_num();
        #pragma omp for schedule(dynamic)
        for (uint64_t i = 1; i < numVertices; ++i) {
            if (i >= nCubeVertices_.size()) continue;
            const auto& v = nCubeVertices_[i];
            long double depthI = v[depthIdx] + trans;
            if (depthI <= 0.0L) {
                depthI = 0.001L;
                if (debug_.load()) {
                    #pragma omp critical
                    logger_.log(Logging::LogLevel::Warning, "Clamped depthI to 0.001 for vertex {}", 
                                std::source_location::current(), i);
                }
            }
            long double scaleI = safe_div(focal, depthI);
            long double projI[3] = {0.0L, 0.0L, 0.0L};
            for (size_t k = 0; k < projDim; ++k) {
                projI[k] = v[k] * scaleI;
            }
            long double distSq = 0.0L;
            for (int k = 0; k < 3; ++k) {
                long double diff = projI[k] - projRefVec[k];
                distSq += diff * diff;
            }
            long double distance = std::sqrt(std::max(distSq, 0.0L));
            if (d == 1) {
                distance = std::fabs(v[0] - referenceVertex[0]);
            }
            long double strength = computeInteraction(static_cast<int>(i), distance);
            auto vecPot = computeVectorPotential(static_cast<int>(i), distance);
            long double GodWaveAmp = computeGodWaveAmplitude(static_cast<int>(i), distance);
            localInteractions[thread_id].emplace_back(static_cast<int>(i), distance, strength, vecPot, GodWaveAmp);
            glm::vec3 projIVec(static_cast<float>(projI[0]), static_cast<float>(projI[1]), static_cast<float>(projI[2]));
            localProjected[thread_id].push_back(projIVec);
        }
        #pragma omp critical
        {
            interactions_.insert(interactions_.end(), localInteractions[thread_id].begin(), localInteractions[thread_id].end());
            projectedVerts_.insert(projectedVerts_.end(), localProjected[thread_id].begin(), localProjected[thread_id].end());
        }
        latch.count_down();
    }
    latch.wait();

    // Special case for dimension 3 (vertices 2 and 4)
    if (d == 3) {
        for (int adj : {2, 4}) {
            size_t vertexIndex = static_cast<size_t>(adj - 1);
            if (vertexIndex < nCubeVertices_.size() &&
                std::none_of(interactions_.begin(), interactions_.end(),
                             [adj](const auto& i) { return i.vertexIndex == adj; })) {
                const auto& v = nCubeVertices_[vertexIndex];
                long double depthI = v[depthIdx] + trans;
                if (depthI <= 0.0L) {
                    depthI = 0.001L;
                    if (debug_.load()) {
                        logger_.log(Logging::LogLevel::Warning, "Clamped depthI to 0.001 for vertex {}", 
                                    std::source_location::current(), vertexIndex);
                    }
                }
                long double scaleI = safe_div(focal, depthI);
                long double projI[3] = {0.0L, 0.0L, 0.0L};
                for (size_t k = 0; k < projDim; ++k) {
                    projI[k] = v[k] * scaleI;
                }
                long double distSq = 0.0L;
                for (int k = 0; k < 3; ++k) {
                    long double diff = projI[k] - projRefVec[k];
                    distSq += diff * diff;
                }
                long double distance = std::sqrt(std::max(distSq, 0.0L));
                long double strength = computeInteraction(static_cast<int>(vertexIndex), distance);
                auto vecPot = computeVectorPotential(static_cast<int>(vertexIndex), distance);
                long double GodWaveAmp = computeGodWaveAmplitude(static_cast<int>(vertexIndex), distance);
                interactions_.emplace_back(static_cast<int>(vertexIndex), distance, strength, vecPot, GodWaveAmp);
                glm::vec3 projIVec(static_cast<float>(projI[0]), static_cast<float>(projI[1]), static_cast<float>(projI[2]));
                projectedVerts_.push_back(projIVec);
            }
        }
    }

    needsUpdate_.store(false);

    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Updated {} interactions, projected {} vertices, avgProjScale={}",
                    std::source_location::current(), interactions_.size(), projectedVerts_.size(), avgProjScale_.load());
        logger_.log(Logging::LogLevel::Info, "Interaction update completed", std::source_location::current());
    }
}