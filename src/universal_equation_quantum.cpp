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
    if (b == 0.0L || std::isnan(b) || std::isinf(b)) {
        return 0.0L;
    }
    long double result = a / b;
    if (std::isnan(result) || std::isinf(result)) {
        return 0.0L;
    }
    return result;
}

// Computes NURBS basis function
long double UniversalEquation::computeNURBSBasis(int i, int p, long double u, const std::vector<long double>& knots) const {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Starting NURBS basis computation for i={}, p={}, u={}", 
                    std::source_location::current(), i, p, u);
    }
    if (i < 0 || i + p + 1 >= static_cast<int>(knots.size())) {
        logger_.log(Logging::LogLevel::Error, "Invalid NURBS basis indices: i={}, p={}, knots size={}", 
                    std::source_location::current(), i, p, knots.size());
        return 0.0L;
    }
    if (p == 0) {
        long double result = (knots[i] <= u && u < knots[i + 1]) ? 1.0L : 0.0L;
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Base case (p=0) for i={}, u={}: result={}", 
                        std::source_location::current(), i, u, result);
            logger_.log(Logging::LogLevel::Info, "NURBS basis computation completed for i={}, p={}", 
                        std::source_location::current(), i, p);
        }
        return result;
    }
    long double denom1 = knots[i + p] - knots[i];
    long double denom2 = knots[i + p + 1] - knots[i + 1];
    long double term1 = 0.0L, term2 = 0.0L;
    if (denom1 > 1e-15L) {
        term1 = safe_div((u - knots[i]), denom1) * computeNURBSBasis(i, p - 1, u, knots);
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Term1 for i={}, p={}, u={}: numerator={}, denominator={}, recursive result={}", 
                        std::source_location::current(), i, p, u, (u - knots[i]), denom1, term1);
        }
    } else if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Term1 skipped due to small denominator: i={}, p={}, denom1={}", 
                    std::source_location::current(), i, p, denom1);
    }
    if (denom2 > 1e-15L) {
        term2 = safe_div((knots[i + p + 1] - u), denom2) * computeNURBSBasis(i + 1, p - 1, u, knots);
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Term2 for i={}, p={}, u={}: numerator={}, denominator={}, recursive result={}", 
                        std::source_location::current(), i, p, u, (knots[i + p + 1] - u), denom2, term2);
        }
    } else if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Term2 skipped due to small denominator: i={}, p={}, denom2={}", 
                    std::source_location::current(), i, p, denom2);
    }
    long double result = term1 + term2;
    if (std::isnan(result) || std::isinf(result)) {
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Warning, "Invalid NURBS basis result: i={}, p={}, u={}, term1={}, term2={}, result={}", 
                        std::source_location::current(), i, p, u, term1, term2, result);
        }
        return 0.0L;
    }
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Computed NURBS basis for i={}, p={}, u={}: result={}", 
                    std::source_location::current(), i, p, u, result);
        logger_.log(Logging::LogLevel::Info, "NURBS basis computation completed for i={}, p={}", 
                    std::source_location::current(), i, p);
    }
    return result;
}

// Evaluates NURBS curve
long double UniversalEquation::evaluateNURBS(long double u, const std::vector<long double>& controlPoints,
                                            const std::vector<long double>& weights,
                                            const std::vector<long double>& knots, int degree) const {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Starting NURBS curve evaluation for u={}", 
                    std::source_location::current(), u);
        logger_.log(Logging::LogLevel::Debug, "NURBS input: controlPoints size={}, weights size={}, knots size={}, degree={}", 
                    std::source_location::current(), controlPoints.size(), weights.size(), knots.size(), degree);
    }
    if (controlPoints.size() != weights.size() || controlPoints.empty()) {
        logger_.log(Logging::LogLevel::Error, "Invalid NURBS control points or weights: controlPoints size={}, weights size={}",
                    std::source_location::current(), controlPoints.size(), weights.size());
        throw std::invalid_argument("Invalid NURBS control points or weights");
    }
    long double num = 0.0L, denom = 0.0L;
    for (size_t i = 0; i < controlPoints.size(); ++i) {
        long double basis = computeNURBSBasis(i, degree, u, knots);
        long double contribution = basis * weights[i] * controlPoints[i];
        num += contribution;
        denom += basis * weights[i];
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Control point {}: basis={}, weight={}, controlPoint={}, contribution={}", 
                        std::source_location::current(), i, basis, weights[i], controlPoints[i], contribution);
        }
    }
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "NURBS evaluation: numerator={}, denominator={}", 
                    std::source_location::current(), num, denom);
    }
    long double result = safe_div(num, denom);
    if (std::isnan(result) || std::isinf(result)) {
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Warning, "Invalid NURBS evaluation result: u={}, num={}, denom={}, result={}", 
                        std::source_location::current(), u, num, denom, result);
        }
        return 0.0L;
    }
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Computed NURBS evaluation for u={}: result={}", 
                    std::source_location::current(), u, result);
        logger_.log(Logging::LogLevel::Info, "NURBS curve evaluation completed for u={}", 
                    std::source_location::current(), u);
    }
    return result;
}

// Computes spin interaction
long double UniversalEquation::computeSpinInteraction(int vertexIndex1, int vertexIndex2) const {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Starting spin interaction computation between vertices {} and {}", 
                    std::source_location::current(), vertexIndex1, vertexIndex2);
    }
    if (vertexIndex1 < 0 || vertexIndex2 < 0 || static_cast<size_t>(vertexIndex1) >= vertexSpins_.size() ||
        static_cast<size_t>(vertexIndex2) >= vertexSpins_.size()) {
        logger_.log(Logging::LogLevel::Error, "Invalid vertex indices for spin interaction: vertex1={}, vertex2={}, spins size={}",
                    std::source_location::current(), vertexIndex1, vertexIndex2, vertexSpins_.size());
        throw std::out_of_range("Invalid vertex index for spin interaction");
    }
    long double spin1 = vertexSpins_[vertexIndex1];
    long double spin2 = vertexSpins_[vertexIndex2];
    long double spinInteraction = spinInteraction_.load();
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Spin interaction parameters: vertex1={} (spin={}), vertex2={} (spin={}), spinInteraction={}", 
                    std::source_location::current(), vertexIndex1, spin1, vertexIndex2, spin2, spinInteraction);
    }
    long double result = spinInteraction * spin1 * spin2;
    if (std::isnan(result) || std::isinf(result)) {
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Warning, "Invalid spin interaction result: spin1={}, spin2={}, spinInteraction={}, result={}", 
                        std::source_location::current(), spin1, spin2, spinInteraction, result);
        }
        return 0.0L;
    }
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Computed spin interaction for vertices {} and {}: result={}", 
                    std::source_location::current(), vertexIndex1, vertexIndex2, result);
        logger_.log(Logging::LogLevel::Info, "Spin interaction computation completed for vertices {} and {}", 
                    std::source_location::current(), vertexIndex1, vertexIndex2);
    }
    return result;
}

// Computes vector potential
std::vector<long double> UniversalEquation::computeVectorPotential(int vertexIndex, long double distance) const {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Starting vector potential computation for vertex {}, distance={}", 
                    std::source_location::current(), vertexIndex, distance);
    }
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= nCubeVertices_.size()) {
        logger_.log(Logging::LogLevel::Error, "Invalid vertex index for vector potential: vertex={}, vertices size={}",
                    std::source_location::current(), vertexIndex, nCubeVertices_.size());
        throw std::out_of_range("Invalid vertex index for vector potential");
    }
    const auto& vertex = nCubeVertices_[vertexIndex];
    long double charge = vertexSpins_[vertexIndex] * 2.0L;
    int dim = std::min(3, currentDimension_.load());
    std::vector<long double> vecPot(dim, 0.0L);
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Vector potential parameters: vertex={}, charge={}, dimension={}", 
                    std::source_location::current(), vertexIndex, charge, dim);
    }
    for (int k = 0; k < dim; ++k) {
        vecPot[k] = safe_div(charge * vertex[k], (1.0L + std::max(distance, 1e-15L)));
        if (std::isnan(vecPot[k]) || std::isinf(vecPot[k])) {
            if (debug_.load()) {
                logger_.log(Logging::LogLevel::Warning, "Invalid vector potential component k={}: charge={}, vertex[k]={}, distance={}, result={}",
                            std::source_location::current(), k, charge, vertex[k], distance, vecPot[k]);
            }
            vecPot[k] = 0.0L;
        }
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Vector potential component k={}: value={}", 
                        std::source_location::current(), k, vecPot[k]);
        }
    }
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Vector potential computation completed for vertex {}", 
                    std::source_location::current(), vertexIndex);
    }
    return vecPot;
}

// Computes EM field
std::vector<long double> UniversalEquation::computeEMField(int vertexIndex) const {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Starting EM field computation for vertex {}", 
                    std::source_location::current(), vertexIndex);
    }
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= nCubeVertices_.size()) {
        logger_.log(Logging::LogLevel::Error, "Invalid vertex index for EM field: vertex={}, vertices size={}",
                    std::source_location::current(), vertexIndex, nCubeVertices_.size());
        throw std::out_of_range("Invalid vertex index for EM field");
    }
    int dim = std::min(3, currentDimension_.load());
    std::vector<long double> field(dim, 0.0L);
    long double phase = omega_ * currentDimension_.load();
    long double emStrength = emFieldStrength_.load();
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "EM field parameters: vertex={}, phase={}, emFieldStrength={}", 
                    std::source_location::current(), vertexIndex, phase, emStrength);
    }
    for (int k = 0; k < dim; ++k) {
        field[k] = emStrength * std::cos(phase + k * std::numbers::pi_v<long double> / 3.0L);
        if (std::isnan(field[k]) || std::isinf(field[k])) {
            if (debug_.load()) {
                logger_.log(Logging::LogLevel::Warning, "Invalid EM field component k={}: phase={}, emStrength={}, result={}", 
                            std::source_location::current(), k, phase, emStrength, field[k]);
            }
            field[k] = 0.0L;
        }
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "EM field component k={}: value={}", 
                        std::source_location::current(), k, field[k]);
        }
    }
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "EM field computation completed for vertex {}", 
                    std::source_location::current(), vertexIndex);
    }
    return field;
}

// Computes Lorentz factor
long double UniversalEquation::computeLorentzFactor(int vertexIndex) const {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Starting Lorentz factor computation for vertex {}", 
                    std::source_location::current(), vertexIndex);
    }
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= vertexMomenta_.size()) {
        logger_.log(Logging::LogLevel::Error, "Invalid vertex index for Lorentz factor: vertex={}, momenta size={}",
                    std::source_location::current(), vertexIndex, vertexMomenta_.size());
        throw std::out_of_range("Invalid vertex index for Lorentz factor");
    }
    long double momentumMag = 0.0L;
    const auto& momentum = vertexMomenta_[vertexIndex];
    for (size_t i = 0; i < momentum.size(); ++i) {
        momentumMag += momentum[i] * momentum[i];
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Vertex {}, dimension {}: momentum component squared={}", 
                        std::source_location::current(), vertexIndex, i, momentum[i] * momentum[i]);
        }
    }
    momentumMag = std::sqrt(std::max(momentumMag, 0.0L));
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Vertex {}: momentum magnitude={}", 
                    std::source_location::current(), vertexIndex, momentumMag);
    }
    long double v = std::min(momentumMag, 0.999L);
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Vertex {}: velocity clamped to v={}", 
                    std::source_location::current(), vertexIndex, v);
    }
    long double result = safe_div(1.0L, std::sqrt(1.0L - v * v));
    if (std::isnan(result) || std::isinf(result)) {
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Warning, "Invalid Lorentz factor: vertex={}, momentumMag={}, v={}, result={}", 
                        std::source_location::current(), vertexIndex, momentumMag, v, result);
        }
        return 1.0L;
    }
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Computed Lorentz factor for vertex {}: result={}", 
                    std::source_location::current(), vertexIndex, result);
        logger_.log(Logging::LogLevel::Info, "Lorentz factor computation completed for vertex {}", 
                    std::source_location::current(), vertexIndex);
    }
    return result;
}

// Computes God wave amplitude
long double UniversalEquation::computeGodWaveAmplitude(int vertexIndex, long double distance) const {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Starting God wave amplitude computation for vertex {}, distance={}", 
                    std::source_location::current(), vertexIndex, distance);
    }
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= vertexWaveAmplitudes_.size()) {
        logger_.log(Logging::LogLevel::Error, "Invalid vertex index for God wave: vertex={}, amplitudes size={}",
                    std::source_location::current(), vertexIndex, vertexWaveAmplitudes_.size());
        throw std::out_of_range("Invalid vertex index for God wave");
    }
    long double waveFreq = GodWaveFreq_.load();
    long double omega = omega_;
    long double phase = waveFreq * distance + omega * vertexIndex;
    long double baseAmplitude = vertexWaveAmplitudes_[vertexIndex];
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "God wave parameters: vertex={}, GodWaveFreq={}, omega={}, phase={}, baseAmplitude={}", 
                    std::source_location::current(), vertexIndex, waveFreq, omega, phase, baseAmplitude);
    }
    long double amplitude = baseAmplitude * std::cos(phase);
    if (currentDimension_.load() == 1) {
        long double permeation = oneDPermeation_.load();
        amplitude *= permeation;
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Vertex {}: applied oneDPermeation={}", 
                        std::source_location::current(), vertexIndex, permeation);
        }
    }
    if (std::isnan(amplitude) || std::isinf(amplitude)) {
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Warning, "Invalid God wave amplitude: vertex={}, distance={}, phase={}, amplitude={}", 
                        std::source_location::current(), vertexIndex, distance, phase, amplitude);
        }
        return 0.0L;
    }
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Computed God wave amplitude for vertex {}: result={}", 
                    std::source_location::current(), vertexIndex, amplitude);
        logger_.log(Logging::LogLevel::Info, "God wave amplitude computation completed for vertex {}", 
                    std::source_location::current(), vertexIndex);
    }
    return amplitude;
}

// Computes NURBS matter field
long double UniversalEquation::computeNurbMatter(long double distance) const {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Starting NURBS matter field computation for distance={}", 
                    std::source_location::current(), distance);
    }
    long double u = std::clamp(distance / 10.0L, 0.0L, 1.0L);
    if (u != distance / 10.0L && debug_.load()) {
        logger_.log(Logging::LogLevel::Warning, "Distance clamped for NURBS matter: distance={}, u={}", 
                    std::source_location::current(), distance, u);
    }
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "NURBS matter computation: u={}", 
                    std::source_location::current(), u);
    }
    long double matterStrength = nurbMatterStrength_.load();
    long double result = matterStrength * evaluateNURBS(u, nurbMatterControlPoints_, nurbWeights_, nurbKnots_, 3);
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "NURBS matter: strength={}, NURBS evaluation result={}", 
                    std::source_location::current(), matterStrength, result / matterStrength);
    }
    if (std::isnan(result) || std::isinf(result)) {
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Warning, "Invalid NURBS matter result: distance={}, u={}, result={}", 
                        std::source_location::current(), distance, u, result);
        }
        return 0.0L;
    }
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Computed NURBS matter for distance={}: result={}", 
                    std::source_location::current(), distance, result);
        logger_.log(Logging::LogLevel::Info, "NURBS matter field computation completed for distance={}", 
                    std::source_location::current(), distance);
    }
    return result;
}

// Computes NURBS energy field
long double UniversalEquation::computeNurbEnergy(long double distance) const {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Starting NURBS energy field computation for distance={}", 
                    std::source_location::current(), distance);
    }
    long double u = std::clamp(distance / 10.0L, 0.0L, 1.0L);
    if (u != distance / 10.0L && debug_.load()) {
        logger_.log(Logging::LogLevel::Warning, "Distance clamped for NURBS energy: distance={}, u={}", 
                    std::source_location::current(), distance, u);
    }
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "NURBS energy computation: u={}", 
                    std::source_location::current(), u);
    }
    long double energyStrength = nurbEnergyStrength_.load();
    long double base = energyStrength * evaluateNURBS(u, nurbEnergyControlPoints_, nurbWeights_, nurbKnots_, 3);
    long double vacuumTerm = vacuumEnergy_.load() * (1.0L + u * invMaxDim_);
    long double waveFreq = GodWaveFreq_.load();
    long double waveTerm = energyStrength * std::cos(waveFreq * distance);
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "NURBS energy components: base={}, vacuumTerm={}, waveTerm={}, energyStrength={}, waveFreq={}", 
                    std::source_location::current(), base, vacuumTerm, waveTerm, energyStrength, waveFreq);
    }
    long double result = base + vacuumTerm + waveTerm;
    if (std::isnan(result) || std::isinf(result)) {
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Warning, "Invalid NURBS energy result: distance={}, u={}, base={}, vacuumTerm={}, waveTerm={}, result={}", 
                        std::source_location::current(), distance, u, base, vacuumTerm, waveTerm, result);
        }
        return 0.0L;
    }
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Computed NURBS energy for distance={}: result={}", 
                    std::source_location::current(), distance, result);
        logger_.log(Logging::LogLevel::Info, "NURBS energy field computation completed for distance={}", 
                    std::source_location::current(), distance);
    }
    return result;
}

// Computes interaction strength
long double UniversalEquation::computeInteraction(int vertexIndex, long double distance) const {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Starting interaction computation for vertex {}, distance={}", 
                    std::source_location::current(), vertexIndex, distance);
    }
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= nCubeVertices_.size()) {
        logger_.log(Logging::LogLevel::Error, "Invalid vertex index for interaction: vertex={}, vertices size={}",
                    std::source_location::current(), vertexIndex, nCubeVertices_.size());
        throw std::out_of_range("Invalid vertex index for interaction");
    }
    int currDim = currentDimension_.load();
    long double denom = std::max(1e-15L, std::pow(static_cast<long double>(currDim), (vertexIndex % maxDimensions_ + 1)));
    long double modifier = (currDim > 3 && (vertexIndex % maxDimensions_ + 1) > 3) ? weak_.load() : 1.0L;
    if (currDim == 3 && ((vertexIndex % maxDimensions_ + 1) == 2 || (vertexIndex % maxDimensions_ + 1) == 4)) {
        modifier *= threeDInfluence_.load();
    }
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Interaction parameters: vertex={}, currentDimension={}, denom={}, modifier={}", 
                    std::source_location::current(), vertexIndex, currDim, denom, modifier);
    }
    long double lorentzFactor = computeLorentzFactor(vertexIndex);
    long double adjustedDistance = safe_div(distance, std::max(lorentzFactor, 1e-15L));
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Vertex {}: lorentzFactor={}, adjustedDistance={}", 
                    std::source_location::current(), vertexIndex, lorentzFactor, adjustedDistance);
    }
    auto vecPot = computeVectorPotential(vertexIndex, adjustedDistance);
    long double vecPotMag = 0.0L;
    for (size_t i = 0; i < vecPot.size(); ++i) {
        vecPotMag += vecPot[i] * vecPot[i];
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Vector potential component {}: value={}", 
                        std::source_location::current(), i, vecPot[i]);
        }
    }
    vecPotMag = std::sqrt(std::max(vecPotMag, 0.0L));
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Vertex {}: vector potential magnitude={}", 
                    std::source_location::current(), vertexIndex, vecPotMag);
    }
    long double spinTerm = computeSpinInteraction(vertexIndex, 0);
    auto emField = computeEMField(vertexIndex);
    long double fieldMag = 0.0L;
    for (size_t i = 0; i < emField.size(); ++i) {
        fieldMag += emField[i] * emField[i];
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "EM field component {}: value={}", 
                        std::source_location::current(), i, emField[i]);
        }
    }
    fieldMag = std::sqrt(std::max(fieldMag, 0.0L));
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Vertex {}: spinTerm={}, fieldMag={}", 
                    std::source_location::current(), vertexIndex, spinTerm, fieldMag);
    }
    long double GodWaveAmp = computeGodWaveAmplitude(vertexIndex, adjustedDistance);
    long double overlapFactor = 1.0L + GodWaveAmp * GodWaveAmp;
    long double recoilTerm = 0.0L;
    for (size_t i = 0; i < vertexMomenta_[vertexIndex].size(); ++i) {
        recoilTerm += vertexMomenta_[vertexIndex][i] * vertexMomenta_[vertexIndex][i];
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Vertex {}, momentum component {}: value={}", 
                        std::source_location::current(), vertexIndex, i, vertexMomenta_[vertexIndex][i]);
        }
    }
    recoilTerm = std::sqrt(std::max(recoilTerm, 0.0L)) * 0.1L;
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Vertex {}: GodWaveAmp={}, overlapFactor={}, recoilTerm={}", 
                    std::source_location::current(), vertexIndex, GodWaveAmp, overlapFactor, recoilTerm);
    }
    long double influence = influence_.load();
    long double result = influence * safe_div(1.0L, (denom * (1.0L + adjustedDistance))) * modifier *
                         (1.0L + spinTerm) * (1.0L + fieldMag) * vecPotMag * overlapFactor * (1.0L + recoilTerm);
    if (std::isnan(result) || std::isinf(result)) {
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Warning, "Invalid interaction result: vertex={}, distance={}, influence={}, denom={}, modifier={}, spinTerm={}, fieldMag={}, vecPotMag={}, overlapFactor={}, recoilTerm={}, result={}", 
                        std::source_location::current(), vertexIndex, adjustedDistance, influence, denom, modifier, spinTerm, fieldMag, vecPotMag, overlapFactor, recoilTerm, result);
        }
        return 0.0L;
    }
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Computed interaction for vertex {}: influence={}, denom={}, modifier={}, spinTerm={}, fieldMag={}, vecPotMag={}, overlapFactor={}, recoilTerm={}, result={}", 
                    std::source_location::current(), vertexIndex, influence, denom, modifier, spinTerm, fieldMag, vecPotMag, overlapFactor, recoilTerm, result);
        logger_.log(Logging::LogLevel::Info, "Interaction computation completed for vertex {}", 
                    std::source_location::current(), vertexIndex);
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
        logger_.log(Logging::LogLevel::Error, "nCubeVertices_ is empty in compute", 
                    std::source_location::current());
        throw std::runtime_error("nCubeVertices_ is empty in compute");
    }
    if (needsUpdate_.load()) {
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Needs update: triggering updateInteractions", 
                        std::source_location::current());
        }
        updateInteractions();
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Interactions updated in compute", 
                        std::source_location::current());
        }
    }
    long double observable = influence_.load();
    int currDim = currentDimension_.load();
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Initial observable: influence={}", 
                    std::source_location::current(), observable);
    }
    if (currDim >= 2) {
        if (cachedCos_.empty()) {
            logger_.log(Logging::LogLevel::Error, "cachedCos_ is empty", 
                        std::source_location::current());
            throw std::runtime_error("cachedCos_ is empty");
        }
        long double twoDTerm = twoD_.load() * cachedCos_[currDim % cachedCos_.size()];
        observable += twoDTerm;
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Dimension {}: added twoD term={}, new observable={}", 
                        std::source_location::current(), currDim, twoDTerm, observable);
        }
    }
    if (currDim == 3) {
        long double threeDTerm = threeDInfluence_.load();
        observable += threeDTerm;
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Dimension 3: added threeDInfluence={}, new observable={}", 
                        std::source_location::current(), threeDTerm, observable);
        }
    }
    long double carrollMod = 1.0L - carrollFactor_.load() * (1.0L - invMaxDim_ * currDim);
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Carroll modifier: carrollFactor={}, invMaxDim={}, currDim={}, carrollMod={}", 
                    std::source_location::current(), carrollFactor_.load(), invMaxDim_, currDim, carrollMod);
    }
    observable *= carrollMod;
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Observable after carrollMod: observable={}", 
                    std::source_location::current(), observable);
    }
    long double avgScale = 1.0L;
    {
        std::vector<DimensionInteraction> localInteractions = interactions_;
        if (!localInteractions.empty()) {
            long double sumScale = 0.0L;
            if (debug_.load()) {
                logger_.log(Logging::LogLevel::Debug, "Computing average scale for {} interactions", 
                            std::source_location::current(), localInteractions.size());
            }
            #pragma omp parallel for schedule(dynamic) reduction(+:sumScale)
            for (size_t i = 0; i < localInteractions.size(); ++i) {
                long double scale = safe_div(perspectiveFocal_.load(), (perspectiveFocal_.load() + localInteractions[i].distance));
                sumScale += scale;
                if (debug_.load()) {
                    #pragma omp critical
                    logger_.log(Logging::LogLevel::Debug, "Interaction {}: perspectiveFocal={}, distance={}, scale={}", 
                                std::source_location::current(), i, perspectiveFocal_.load(), localInteractions[i].distance, scale);
                }
            }
            avgScale = safe_div(sumScale, static_cast<long double>(localInteractions.size()));
            if (debug_.load()) {
                logger_.log(Logging::LogLevel::Debug, "Computed average scale: sumScale={}, interactions={}, avgScale={}", 
                            std::source_location::current(), sumScale, localInteractions.size(), avgScale);
            }
        }
        avgProjScale_.store(avgScale);
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Stored avgProjScale: value={}", 
                        std::source_location::current(), avgScale);
        }
    }
    observable *= avgScale;
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Observable after avgScale: observable={}", 
                    std::source_location::current(), observable);
    }
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
                long double vertexIndex = interaction.vertexIndex;
                long double distance = interaction.distance;
                long double permeation = computePermeation(vertexIndex);
                long double nurbMatter = computeNurbMatter(distance);
                long double nurbEnergy = computeNurbEnergy(distance);
                long double spinTerm = computeSpinInteraction(vertexIndex, 0);
                long double momentumMag = 0.0L;
                for (size_t j = 0; j < vertexMomenta_[vertexIndex].size(); ++j) {
                    momentumMag += vertexMomenta_[vertexIndex][j] * vertexMomenta_[vertexIndex][j];
                    if (debug_.load()) {
                        #pragma omp critical
                        logger_.log(Logging::LogLevel::Debug, "Interaction {}, vertex {}, momentum component {}: value={}", 
                                    std::source_location::current(), i, vertexIndex, j, vertexMomenta_[vertexIndex][j]);
                    }
                }
                momentumMag = std::sqrt(std::max(momentumMag, 0.0L));
                auto emField = computeEMField(vertexIndex);
                long double fieldMag = 0.0L;
                for (size_t j = 0; j < emField.size(); ++j) {
                    fieldMag += emField[j] * emField[j];
                    if (debug_.load()) {
                        #pragma omp critical
                        logger_.log(Logging::LogLevel::Debug, "Interaction {}, vertex {}, EM field component {}: value={}", 
                                    std::source_location::current(), i, vertexIndex, j, emField[j]);
                    }
                }
                fieldMag = std::sqrt(std::max(fieldMag, 0.0L));
                long double GodWaveAmp = computeGodWaveAmplitude(vertexIndex, distance);
                long double mfScale = 1.0L - meanFieldApprox_.load() * safe_div(1.0L, std::max(1.0L, static_cast<long double>(localInteractions.size())));
                long double renorm = safe_div(1.0L, (1.0L + renormFactor_.load() * interactionSum));
                long double interactionTerm = influence * safeExp(-alpha_.load() * distance) * permeation * nurbMatter * mfScale * renorm;
                interactionSum += interactionTerm;
                totalNurbMatter += nurbMatter * influence * permeation * mfScale * renorm;
                totalNurbEnergy += nurbEnergy * influence * permeation * mfScale * renorm;
                totalSpinEnergy += spinTerm * influence * permeation * mfScale * renorm;
                totalMomentumEnergy += momentumMag * influence * permeation * mfScale * renorm;
                totalFieldEnergy += fieldMag * influence * permeation * mfScale * renorm;
                totalGodWaveEnergy += GodWaveAmp * GodWaveAmp * influence * permeation * mfScale * renorm;
                if (debug_.load()) {
                    #pragma omp critical
                    logger_.log(Logging::LogLevel::Debug, "Interaction {}: vertex={}, distance={}, influence={}, permeation={}, nurbMatter={}, nurbEnergy={}, spinTerm={}, momentumMag={}, fieldMag={}, GodWaveAmp={}, mfScale={}, renorm={}, interactionTerm={}",
                                std::source_location::current(), i, vertexIndex, distance, influence, permeation, nurbMatter, nurbEnergy, spinTerm, momentumMag, fieldMag, GodWaveAmp, mfScale, renorm, interactionTerm);
                }
            }
        } else {
            for (size_t i = 0; i < localInteractions.size(); ++i) {
                const auto& interaction = localInteractions[i];
                long double influence = interaction.strength;
                long double vertexIndex = interaction.vertexIndex;
                long double distance = interaction.distance;
                long double permeation = computePermeation(vertexIndex);
                long double nurbMatter = computeNurbMatter(distance);
                long double nurbEnergy = computeNurbEnergy(distance);
                long double spinTerm = computeSpinInteraction(vertexIndex, 0);
                long double momentumMag = 0.0L;
                for (size_t j = 0; j < vertexMomenta_[vertexIndex].size(); ++j) {
                    momentumMag += vertexMomenta_[vertexIndex][j] * vertexMomenta_[vertexIndex][j];
                    if (debug_.load()) {
                        logger_.log(Logging::LogLevel::Debug, "Interaction {}, vertex {}, momentum component {}: value={}", 
                                    std::source_location::current(), i, vertexIndex, j, vertexMomenta_[vertexIndex][j]);
                    }
                }
                momentumMag = std::sqrt(std::max(momentumMag, 0.0L));
                auto emField = computeEMField(vertexIndex);
                long double fieldMag = 0.0L;
                for (size_t j = 0; j < emField.size(); ++j) {
                    fieldMag += emField[j] * emField[j];
                    if (debug_.load()) {
                        logger_.log(Logging::LogLevel::Debug, "Interaction {}, vertex {}, EM field component {}: value={}", 
                                    std::source_location::current(), i, vertexIndex, j, emField[j]);
                    }
                }
                fieldMag = std::sqrt(std::max(fieldMag, 0.0L));
                long double GodWaveAmp = computeGodWaveAmplitude(vertexIndex, distance);
                long double mfScale = 1.0L - meanFieldApprox_.load() * safe_div(1.0L, std::max(1.0L, static_cast<long double>(localInteractions.size())));
                long double renorm = safe_div(1.0L, (1.0L + renormFactor_.load() * interactionSum));
                long double interactionTerm = influence * safeExp(-alpha_.load() * distance) * permeation * nurbMatter * mfScale * renorm;
                interactionSum += interactionTerm;
                totalNurbMatter += nurbMatter * influence * permeation * mfScale * renorm;
                totalNurbEnergy += nurbEnergy * influence * permeation * mfScale * renorm;
                totalSpinEnergy += spinTerm * influence * permeation * mfScale * renorm;
                totalMomentumEnergy += momentumMag * influence * permeation * mfScale * renorm;
                totalFieldEnergy += fieldMag * influence * permeation * mfScale * renorm;
                totalGodWaveEnergy += GodWaveAmp * GodWaveAmp * influence * permeation * mfScale * renorm;
                if (debug_.load()) {
                    logger_.log(Logging::LogLevel::Debug, "Interaction {}: vertex={}, distance={}, influence={}, permeation={}, nurbMatter={}, nurbEnergy={}, spinTerm={}, momentumMag={}, fieldMag={}, GodWaveAmp={}, mfScale={}, renorm={}, interactionTerm={}",
                                std::source_location::current(), i, vertexIndex, distance, influence, permeation, nurbMatter, nurbEnergy, spinTerm, momentumMag, fieldMag, GodWaveAmp, mfScale, renorm, interactionTerm);
                }
            }
        }
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Pre-normalization sums: interactionSum={}, totalNurbMatter={}, totalNurbEnergy={}, totalSpinEnergy={}, totalMomentumEnergy={}, totalFieldEnergy={}, totalGodWaveEnergy={}",
                        std::source_location::current(), interactionSum, totalNurbMatter, totalNurbEnergy, totalSpinEnergy, totalMomentumEnergy, totalFieldEnergy, totalGodWaveEnergy);
        }
        long double totalInfluence = interactionSum + totalNurbMatter + totalNurbEnergy + totalSpinEnergy +
                                    totalMomentumEnergy + totalFieldEnergy + totalGodWaveEnergy;
        long double normFactor = (totalInfluence > 0.0L) ? safe_div(totalCharge_.load(), totalInfluence) : 1.0L;
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Normalization: totalInfluence={}, totalCharge={}, normFactor={}", 
                        std::source_location::current(), totalInfluence, totalCharge_.load(), normFactor);
        }
        interactionSum *= normFactor;
        totalNurbMatter *= normFactor;
        totalNurbEnergy *= normFactor;
        totalSpinEnergy *= normFactor;
        totalMomentumEnergy *= normFactor;
        totalFieldEnergy *= normFactor;
        totalGodWaveEnergy *= normFactor;
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Post-normalization sums: interactionSum={}, totalNurbMatter={}, totalNurbEnergy={}, totalSpinEnergy={}, totalMomentumEnergy={}, totalFieldEnergy={}, totalGodWaveEnergy={}",
                        std::source_location::current(), interactionSum, totalNurbMatter, totalNurbEnergy, totalSpinEnergy, totalMomentumEnergy, totalFieldEnergy, totalGodWaveEnergy);
        }
    }
    observable += interactionSum;
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Observable after interactionSum: observable={}", 
                    std::source_location::current(), observable);
    }
    long double collapse = computeCollapse();
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Collapse term: collapse={}", 
                    std::source_location::current(), collapse);
    }
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
        logger_.log(Logging::LogLevel::Error, "Numerical instability in compute: observable={}, potential={}, nurbMatter={}, nurbEnergy={}, spinEnergy={}, momentumEnergy={}, fieldEnergy={}, GodWaveEnergy={}", 
                    std::source_location::current(), result.observable, result.potential, result.nurbMatter, result.nurbEnergy, 
                    result.spinEnergy, result.momentumEnergy, result.fieldEnergy, result.GodWaveEnergy);
        throw std::runtime_error("Numerical instability in compute");
    }
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Energy computation result: observable={}, potential={}, nurbMatter={}, nurbEnergy={}, spinEnergy={}, momentumEnergy={}, fieldEnergy={}, GodWaveEnergy={}",
                    std::source_location::current(), result.observable, result.potential, result.nurbMatter, result.nurbEnergy, 
                    result.spinEnergy, result.momentumEnergy, result.fieldEnergy, result.GodWaveEnergy);
        logger_.log(Logging::LogLevel::Info, "Energy computation completed for {} vertices", 
                    std::source_location::current(), nCubeVertices_.size());
    }
    return result;
}

// Computes permeation factor
long double UniversalEquation::computePermeation(int vertexIndex) const {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Starting permeation factor computation for vertex {}", 
                    std::source_location::current(), vertexIndex);
    }
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= nCubeVertices_.size()) {
        logger_.log(Logging::LogLevel::Error, "Invalid vertex index for permeation: vertex={}, vertices size={}",
                    std::source_location::current(), vertexIndex, nCubeVertices_.size());
        throw std::out_of_range("Invalid vertex index for permeation");
    }
    if (vertexIndex == 1 || currentDimension_.load() == 1) {
        long double result = oneDPermeation_.load();
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Vertex {}: using oneDPermeation={}", 
                        std::source_location::current(), vertexIndex, result);
            logger_.log(Logging::LogLevel::Info, "Permeation factor computation completed for vertex {}", 
                        std::source_location::current(), vertexIndex);
        }
        return result;
    }
    if (currentDimension_.load() == 2 && (vertexIndex % maxDimensions_ + 1) > 2) {
        long double result = twoD_.load();
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Vertex {}: using twoD={}", 
                        std::source_location::current(), vertexIndex, result);
            logger_.log(Logging::LogLevel::Info, "Permeation factor computation completed for vertex {}", 
                        std::source_location::current(), vertexIndex);
        }
        return result;
    }
    if (currentDimension_.load() == 3 && ((vertexIndex % maxDimensions_ + 1) == 2 || (vertexIndex % maxDimensions_ + 1) == 4)) {
        long double result = threeDInfluence_.load();
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Vertex {}: using threeDInfluence={}", 
                        std::source_location::current(), vertexIndex, result);
            logger_.log(Logging::LogLevel::Info, "Permeation factor computation completed for vertex {}", 
                        std::source_location::current(), vertexIndex);
        }
        return result;
    }
    const auto& vertex = nCubeVertices_[vertexIndex];
    long double vertexMagnitude = 0.0L;
    for (size_t i = 0; i < std::min(static_cast<size_t>(currentDimension_.load()), vertex.size()); ++i) {
        vertexMagnitude += vertex[i] * vertex[i];
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Vertex {}, dimension {}: component squared={}", 
                        std::source_location::current(), vertexIndex, i, vertex[i] * vertex[i]);
        }
    }
    vertexMagnitude = std::sqrt(std::max(vertexMagnitude, 0.0L));
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Vertex {}: vertex magnitude={}", 
                    std::source_location::current(), vertexIndex, vertexMagnitude);
    }
    long double result = 1.0L + beta_.load() * safe_div(vertexMagnitude, std::max(1, currentDimension_.load()));
    if (std::isnan(result) || std::isinf(result)) {
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Warning, "Invalid permeation result: vertex={}, vertexMagnitude={}, beta={}, currentDimension={}, result={}", 
                        std::source_location::current(), vertexIndex, vertexMagnitude, beta_.load(), currentDimension_.load(), result);
        }
        return 1.0L;
    }
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Computed permeation for vertex {}: beta={}, vertexMagnitude={}, currentDimension={}, result={}", 
                    std::source_location::current(), vertexIndex, beta_.load(), vertexMagnitude, currentDimension_.load(), result);
        logger_.log(Logging::LogLevel::Info, "Permeation factor computation completed for vertex {}", 
                    std::source_location::current(), vertexIndex);
    }
    return result;
}

// Computes collapse term
long double UniversalEquation::computeCollapse() const {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Starting collapse term computation for dimension {}", 
                    std::source_location::current(), currentDimension_.load());
    }
    if (currentDimension_.load() == 1) {
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Dimension 1: returning collapse=0.0", 
                        std::source_location::current());
            logger_.log(Logging::LogLevel::Info, "Collapse term computation completed for dimension {}", 
                        std::source_location::current(), currentDimension_.load());
        }
        return 0.0L;
    }
    long double phase = safe_div(static_cast<long double>(currentDimension_.load()), (2 * maxDimensions_));
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Collapse parameters: currentDimension={}, maxDimensions={}, phase={}", 
                    std::source_location::current(), currentDimension_.load(), maxDimensions_, phase);
    }
    if (cachedCos_.empty()) {
        logger_.log(Logging::LogLevel::Error, "cachedCos_ is empty", 
                    std::source_location::current());
        throw std::runtime_error("cachedCos_ is empty");
    }
    long double osc = std::abs(cachedCos_[static_cast<size_t>(2.0L * std::numbers::pi_v<long double> * phase * cachedCos_.size()) % cachedCos_.size()]);
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Computed oscillation term: phase={}, osc={}", 
                    std::source_location::current(), phase, osc);
    }
    long double symCollapse = collapse_.load() * currentDimension_.load() * safeExp(-beta_.load() * (currentDimension_.load() - 1)) * (0.8L * osc + 0.2L);
    long double vertexFactor = safe_div(static_cast<long double>(nCubeVertices_.size() % 10), 10.0L);
    long double asymTerm = asymCollapse_.load() * std::sin(std::numbers::pi_v<long double> * phase + osc + vertexFactor) * safeExp(-alpha_.load() * phase);
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Collapse terms: symCollapse={}, vertexFactor={}, asymTerm={}, collapse={}, beta={}, alpha={}", 
                    std::source_location::current(), symCollapse, vertexFactor, asymTerm, collapse_.load(), beta_.load(), alpha_.load());
    }
    long double result = std::max(0.0L, symCollapse + asymTerm);
    if (std::isnan(result) || std::isinf(result)) {
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Warning, "Invalid collapse term result: symCollapse={}, asymTerm={}, result={}", 
                        std::source_location::current(), symCollapse, asymTerm, result);
        }
        return 0.0L;
    }
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Computed collapse term for dimension {}: result={}", 
                    std::source_location::current(), currentDimension_.load(), result);
        logger_.log(Logging::LogLevel::Info, "Collapse term computation completed for dimension {}", 
                    std::source_location::current(), currentDimension_.load());
    }
    return result;
}

// Updates interactions
void UniversalEquation::updateInteractions() const {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Starting interaction update for {} vertices", 
                    std::source_location::current(), nCubeVertices_.size());
    }
    interactions_.clear();
    projectedVerts_.clear();
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Cleared interactions and projectedVerts", 
                    std::source_location::current());
    }
    size_t d = static_cast<size_t>(currentDimension_.load());
    uint64_t numVertices = std::min(static_cast<uint64_t>(nCubeVertices_.size()), maxVertices_);
    numVertices = std::min(numVertices, static_cast<uint64_t>(1024)); // Cap vertices for performance
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Vertex count: initial numVertices={}, maxVertices={}, capped at 1024", 
                    std::source_location::current(), nCubeVertices_.size(), maxVertices_);
    }
    if (d > 6) {
        uint64_t lodFactor = 1ULL << (d / 2);
        if (lodFactor == 0) lodFactor = 1;
        numVertices = std::min(numVertices / lodFactor, static_cast<uint64_t>(1024));
        if (debug_.load() && numVertices < nCubeVertices_.size()) {
            logger_.log(Logging::LogLevel::Warning, "Reduced vertex count to {} due to high dimension ({}), lodFactor={}", 
                        std::source_location::current(), numVertices, d, lodFactor);
        }
    }
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Final vertex count for processing: numVertices={}", 
                    std::source_location::current(), numVertices);
    }

    // Per-thread local storage for interactions and projections
    std::vector<std::vector<DimensionInteraction>> localInteractions(omp_get_max_threads());
    std::vector<std::vector<glm::vec3>> localProjected(omp_get_max_threads());
    for (int t = 0; t < omp_get_max_threads(); ++t) {
        localInteractions[t].reserve((numVertices - 1) / omp_get_max_threads() + 1);
        localProjected[t].reserve((numVertices - 1) / omp_get_max_threads() + 1);
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Thread {}: reserved space for {} interactions and projections", 
                        std::source_location::current(), t, (numVertices - 1) / omp_get_max_threads() + 1);
        }
    }

    // Compute reference vertex projection
    std::vector<long double> referenceVertex(d, 0.0L);
    for (size_t i = 0; i < numVertices && i < nCubeVertices_.size(); ++i) {
        for (size_t j = 0; j < d; ++j) {
            referenceVertex[j] += nCubeVertices_[i][j];
            if (debug_.load()) {
                logger_.log(Logging::LogLevel::Debug, "Vertex {}, dimension {}: adding coordinate={}", 
                            std::source_location::current(), i, j, nCubeVertices_[i][j]);
            }
        }
    }
    for (size_t j = 0; j < d; ++j) {
        referenceVertex[j] = safe_div(referenceVertex[j], static_cast<long double>(numVertices));
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Reference vertex dimension {}: value={}", 
                        std::source_location::current(), j, referenceVertex[j]);
        }
    }
    long double trans = perspectiveTrans_.load();
    long double focal = perspectiveFocal_.load();
    if (std::isnan(trans) || std::isinf(trans)) {
        trans = 2.0L;
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Warning, "Clamped invalid perspectiveTrans to 2.0: original={}", 
                        std::source_location::current(), perspectiveTrans_.load());
        }
    }
    if (std::isnan(focal) || std::isinf(focal)) {
        focal = 4.0L;
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Warning, "Clamped invalid perspectiveFocal to 4.0: original={}", 
                        std::source_location::current(), perspectiveFocal_.load());
        }
    }
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Projection parameters: perspectiveTrans={}, perspectiveFocal={}", 
                    std::source_location::current(), trans, focal);
    }
    size_t depthIdx = d > 0 ? d - 1 : 0;
    long double depthRef = referenceVertex[depthIdx] + trans;
    if (depthRef <= 0.0L) {
        depthRef = 0.001L;
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Warning, "Clamped depthRef to 0.001: original depthRef={}", 
                        std::source_location::current(), referenceVertex[depthIdx] + trans);
        }
    }
    long double scaleRef = safe_div(focal, depthRef);
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Reference projection: depthIdx={}, depthRef={}, scaleRef={}", 
                    std::source_location::current(), depthIdx, depthRef, scaleRef);
    }
    glm::vec3 projRefVec(0.0f);
    size_t projDim = std::min<size_t>(3, d);
    for (size_t k = 0; k < projDim; ++k) {
        projRefVec[k] = static_cast<float>(referenceVertex[k] * scaleRef);
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Reference projection component {}: value={}", 
                        std::source_location::current(), k, projRefVec[k]);
        }
    }
    projectedVerts_.push_back(projRefVec);
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Added reference projection to projectedVerts: size={}", 
                    std::source_location::current(), projectedVerts_.size());
    }

    // Parallel compute interactions and projections
    std::latch latch(omp_get_max_threads());
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Starting parallel computation for interactions with {} threads", 
                    std::source_location::current(), omp_get_max_threads());
    }
    #pragma omp parallel
    {
        int thread_id = omp_get_thread_num();
        if (debug_.load()) {
            #pragma omp critical
            logger_.log(Logging::LogLevel::Debug, "Thread {}: starting interaction computation for vertices 1 to {}", 
                        std::source_location::current(), thread_id, numVertices - 1);
        }
        #pragma omp for schedule(dynamic)
        for (uint64_t i = 1; i < numVertices; ++i) {
            if (i >= nCubeVertices_.size()) {
                if (debug_.load()) {
                    #pragma omp critical
                    logger_.log(Logging::LogLevel::Warning, "Thread {}: skipping vertex {} (exceeds nCubeVertices size={})", 
                                std::source_location::current(), thread_id, i, nCubeVertices_.size());
                }
                continue;
            }
            const auto& v = nCubeVertices_[i];
            long double depthI = v[depthIdx] + trans;
            if (depthI <= 0.0L) {
                depthI = 0.001L;
                if (debug_.load()) {
                    #pragma omp critical
                    logger_.log(Logging::LogLevel::Warning, "Thread {}: clamped depthI to 0.001 for vertex {}, original depthI={}", 
                                std::source_location::current(), thread_id, i, v[depthIdx] + trans);
                }
            }
            long double scaleI = safe_div(focal, depthI);
            if (debug_.load()) {
                #pragma omp critical
                logger_.log(Logging::LogLevel::Debug, "Thread {}: vertex {}: depthI={}, scaleI={}", 
                            std::source_location::current(), thread_id, i, depthI, scaleI);
            }
            long double projI[3] = {0.0L, 0.0L, 0.0L};
            for (size_t k = 0; k < projDim; ++k) {
                projI[k] = v[k] * scaleI;
                if (debug_.load()) {
                    #pragma omp critical
                    logger_.log(Logging::LogLevel::Debug, "Thread {}: vertex {}, projection component {}: value={}", 
                                std::source_location::current(), thread_id, i, k, projI[k]);
                }
            }
            long double distSq = 0.0L;
            for (int k = 0; k < 3; ++k) {
                long double diff = projI[k] - projRefVec[k];
                distSq += diff * diff;
                if (debug_.load()) {
                    #pragma omp critical
                    logger_.log(Logging::LogLevel::Debug, "Thread {}: vertex {}, distance component {}: diff={}", 
                                std::source_location::current(), thread_id, i, k, diff);
                }
            }
            long double distance = std::sqrt(std::max(distSq, 0.0L));
            if (d == 1) {
                distance = std::fabs(v[0] - referenceVertex[0]);
                if (debug_.load()) {
                    #pragma omp critical
                    logger_.log(Logging::LogLevel::Debug, "Thread {}: vertex {}, dimension 1: adjusted distance={}", 
                                std::source_location::current(), thread_id, i, distance);
                }
            }
            if (debug_.load()) {
                #pragma omp critical
                logger_.log(Logging::LogLevel::Debug, "Thread {}: vertex {}: computed distance={}", 
                            std::source_location::current(), thread_id, i, distance);
            }
            long double strength = computeInteraction(static_cast<int>(i), distance);
            auto vecPot = computeVectorPotential(static_cast<int>(i), distance);
            long double GodWaveAmp = computeGodWaveAmplitude(static_cast<int>(i), distance);
            if (debug_.load()) {
                #pragma omp critical
                logger_.log(Logging::LogLevel::Debug, "Thread {}: vertex {}: strength={}, GodWaveAmp={}", 
                            std::source_location::current(), thread_id, i, strength, GodWaveAmp);
            }
            localInteractions[thread_id].emplace_back(static_cast<int>(i), distance, strength, vecPot, GodWaveAmp);
            glm::vec3 projIVec(static_cast<float>(projI[0]), static_cast<float>(projI[1]), static_cast<float>(projI[2]));
            localProjected[thread_id].push_back(projIVec);
            if (debug_.load()) {
                #pragma omp critical
                logger_.log(Logging::LogLevel::Debug, "Thread {}: vertex {}: added interaction and projection", 
                            std::source_location::current(), thread_id, i);
            }
        }
        #pragma omp critical
        {
            interactions_.insert(interactions_.end(), localInteractions[thread_id].begin(), localInteractions[thread_id].end());
            projectedVerts_.insert(projectedVerts_.end(), localProjected[thread_id].begin(), localProjected[thread_id].end());
            if (debug_.load()) {
                logger_.log(Logging::LogLevel::Debug, "Thread {}: merged {} interactions and {} projections", 
                            std::source_location::current(), thread_id, localInteractions[thread_id].size(), localProjected[thread_id].size());
            }
        }
        latch.count_down();
        if (debug_.load()) {
            #pragma omp critical
            logger_.log(Logging::LogLevel::Debug, "Thread {}: signaled latch", 
                        std::source_location::current(), thread_id);
        }
    }
    latch.wait();
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "All threads completed, latch released", 
                    std::source_location::current());
    }

    // Special case for dimension 3 (vertices 2 and 4)
    if (d == 3) {
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Info, "Processing special case for dimension 3, vertices 2 and 4", 
                        std::source_location::current());
        }
        for (int adj : {2, 4}) {
            size_t vertexIndex = static_cast<size_t>(adj - 1);
            if (vertexIndex < nCubeVertices_.size() &&
                std::none_of(interactions_.begin(), interactions_.end(),
                             [adj](const auto& i) { return i.vertexIndex == adj; })) {
                if (debug_.load()) {
                    logger_.log(Logging::LogLevel::Debug, "Processing vertex {} (adj={})", 
                                std::source_location::current(), vertexIndex, adj);
                }
                const auto& v = nCubeVertices_[vertexIndex];
                long double depthI = v[depthIdx] + trans;
                if (depthI <= 0.0L) {
                    depthI = 0.001L;
                    if (debug_.load()) {
                        logger_.log(Logging::LogLevel::Warning, "Clamped depthI to 0.001 for vertex {}, original depthI={}", 
                                    std::source_location::current(), vertexIndex, v[depthIdx] + trans);
                    }
                }
                long double scaleI = safe_div(focal, depthI);
                if (debug_.load()) {
                    logger_.log(Logging::LogLevel::Debug, "Vertex {}: depthI={}, scaleI={}", 
                                std::source_location::current(), vertexIndex, depthI, scaleI);
                }
                long double projI[3] = {0.0L, 0.0L, 0.0L};
                for (size_t k = 0; k < projDim; ++k) {
                    projI[k] = v[k] * scaleI;
                    if (debug_.load()) {
                        logger_.log(Logging::LogLevel::Debug, "Vertex {}, projection component {}: value={}", 
                                    std::source_location::current(), vertexIndex, k, projI[k]);
                    }
                }
                long double distSq = 0.0L;
                for (int k = 0; k < 3; ++k) {
                    long double diff = projI[k] - projRefVec[k];
                    distSq += diff * diff;
                    if (debug_.load()) {
                        logger_.log(Logging::LogLevel::Debug, "Vertex {}, distance component {}: diff={}", 
                                    std::source_location::current(), vertexIndex, k, diff);
                    }
                }
                long double distance = std::sqrt(std::max(distSq, 0.0L));
                if (debug_.load()) {
                    logger_.log(Logging::LogLevel::Debug, "Vertex {}: computed distance={}", 
                                std::source_location::current(), vertexIndex, distance);
                }
                long double strength = computeInteraction(static_cast<int>(vertexIndex), distance);
                auto vecPot = computeVectorPotential(static_cast<int>(vertexIndex), distance);
                long double GodWaveAmp = computeGodWaveAmplitude(static_cast<int>(vertexIndex), distance);
                if (debug_.load()) {
                    logger_.log(Logging::LogLevel::Debug, "Vertex {}: strength={}, GodWaveAmp={}", 
                                std::source_location::current(), vertexIndex, strength, GodWaveAmp);
                }
                interactions_.emplace_back(static_cast<int>(vertexIndex), distance, strength, vecPot, GodWaveAmp);
                glm::vec3 projIVec(static_cast<float>(projI[0]), static_cast<float>(projI[1]), static_cast<float>(projI[2]));
                projectedVerts_.push_back(projIVec);
                if (debug_.load()) {
                    logger_.log(Logging::LogLevel::Debug, "Vertex {}: added interaction and projection", 
                                std::source_location::current(), vertexIndex);
                }
            }
        }
    }

    needsUpdate_.store(false);
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Set needsUpdate to false", 
                    std::source_location::current());
        logger_.log(Logging::LogLevel::Debug, "Interaction update completed: {} interactions, {} projected vertices, avgProjScale={}",
                    std::source_location::current(), interactions_.size(), projectedVerts_.size(), avgProjScale_.load());
        logger_.log(Logging::LogLevel::Info, "Interaction update completed for {} vertices", 
                    std::source_location::current(), nCubeVertices_.size());
    }
}