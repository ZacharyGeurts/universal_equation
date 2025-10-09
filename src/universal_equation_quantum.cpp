// universal_equation_quantum.cpp
// Quantum physics calculations for the AMOURANTH RTX Engine.
// Implements gravitational potential and related quantum computations.
// Integrates with UniversalEquation core logic.
// Copyright Zachary Geurts 2025 (powered by Grok with Science B*! precision)

#include "ue_init.hpp"
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <source_location>
#include <atomic>
#include <vector>
#include <algorithm>
#include <thread>

long double UniversalEquation::computeGravitationalPotential(int vertexIndex1, int vertexIndex2) const {
    // Guard against excessive logging in hot loops
    static thread_local int logCount = 0;
    bool shouldLog = debug_.load() && (++logCount % 10000 == 0); // Log every 10000th call per thread

    if (shouldLog) {
        LOG_INFO_CAT("Quantum", "Starting gravitational potential computation between vertices {} and {}",
                     std::source_location::current(), vertexIndex1, vertexIndex2);
    }

    // Check for empty vertex array with atomic size for thread safety
    size_t size = nCubeVertices_.size();
    if (size == 0) {
        if (shouldLog) {
            LOG_WARNING_CAT("Quantum", "Empty nCubeVertices_, returning 0 potential",
                            std::source_location::current());
        }
        return 0.0L;
    }

    // Validate and clamp vertexIndex1
    if (vertexIndex1 < 0 || static_cast<size_t>(vertexIndex1) >= size) {
        if (shouldLog) {
            LOG_WARNING_CAT("Quantum", "Invalid vertexIndex1 {} (size={}), clamping to 0",
                            std::source_location::current(), vertexIndex1, size);
        }
        vertexIndex1 = 0;
    }

    // Validate and clamp vertexIndex2
    if (vertexIndex2 < 0 || static_cast<size_t>(vertexIndex2) >= size) {
        vertexIndex2 = static_cast<int>(size - 1); // Use last valid index
        if (shouldLog) {
            LOG_WARNING_CAT("Quantum", "Clamped vertexIndex2 to {} (original={}, size={})",
                            std::source_location::current(), vertexIndex2, vertexIndex2, size);
        }
    }

    // Skip if vertices are the same to avoid self-interaction
    if (vertexIndex1 == vertexIndex2) {
        if (shouldLog) {
            LOG_DEBUG_CAT("Quantum", "Skipping self-interaction for vertex {}",
                          std::source_location::current(), vertexIndex1);
        }
        return 0.0L;
    }

    // Safe access to vertices with bounds check
    const auto& v1 = nCubeVertices_[static_cast<size_t>(vertexIndex1)];
    const auto& v2 = nCubeVertices_[static_cast<size_t>(vertexIndex2)];

    // Validate vector sizes
    size_t dim = static_cast<size_t>(getCurrentDimension());
    if (v1.size() != dim || v2.size() != dim) {
        if (shouldLog) {
            LOG_ERROR_CAT("Quantum", "Dimension mismatch: v1.size()={}, v2.size()={}, expected={}",
                          std::source_location::current(), v1.size(), v2.size(), dim);
        }
        return 0.0L; // Return safe value instead of throwing in hot path
    }

    // Compute distance between vertices with safeguards
    long double distance = 0.0L;
    bool validDistance = true;
    for (size_t i = 0; i < dim; ++i) {
        long double diff = v1[i] - v2[i];
        if (std::isnan(diff) || std::isinf(diff)) {
            if (shouldLog) {
                LOG_WARNING_CAT("Quantum", "NaN/Inf diff in dimension {} for vertices {} and {}",
                                std::source_location::current(), i, vertexIndex1, vertexIndex2);
            }
            diff = 0.0L;
        }
        distance += diff * diff;
        if (std::isnan(distance) || std::isinf(distance)) {
            validDistance = false;
            break;
        }
    }
    if (!validDistance) {
        if (shouldLog) {
            LOG_WARNING_CAT("Quantum", "Invalid distance computation for vertices {} and {}, using fallback",
                            std::source_location::current(), vertexIndex1, vertexIndex2);
        }
        distance = 1.0L;
    } else {
        distance = std::sqrt(distance);
        if (std::isnan(distance) || std::isinf(distance) || distance < 0.0L) {
            if (shouldLog) {
                LOG_WARNING_CAT("Quantum", "Invalid sqrt distance for vertices {} and {}: distance={}. Using minDistance",
                                std::source_location::current(), vertexIndex1, vertexIndex2, distance);
            }
            distance = 1e-10L;
        }
    }

    // Avoid division by zero with minimum distance
    constexpr long double minDistance = 1e-10L;
    if (distance < minDistance) {
        if (shouldLog) {
            LOG_DEBUG_CAT("Quantum", "Distance too small for vertices {} and {}: distance={}. Setting to minimum",
                          std::source_location::current(), vertexIndex1, vertexIndex2, distance);
        }
        distance = minDistance;
    }

    // Compute gravitational potential: V = -G * m1 * m2 / r
    constexpr long double G = 6.67430e-11L; // Gravitational constant (m^3 kg^-1 s^-2)
    constexpr long double volume = 1e-3L; // Volume scaling (0.001 m^3, assuming ~0.1m per vertex)
    long double density = std::clamp(materialDensity_.load(), 0.0L, 1.0e6L);
    long double mass = density * volume; // Mass (kg) = density (kg/m^3) * volume (m^3)
    long double potential = -safe_div(G * mass * mass, distance);

    // Apply influence factor with clamp
    long double influence = getInfluence();
    influence = std::clamp(influence, 0.0L, 1.0e6L); // Prevent extreme values
    potential *= influence;

    if (std::isnan(potential) || std::isinf(potential)) {
        if (shouldLog) {
            LOG_WARNING_CAT("Quantum", "Invalid potential for vertices {} and {}: potential={}. Returning 0",
                            std::source_location::current(), vertexIndex1, vertexIndex2, potential);
        }
        return 0.0L;
    }

    if (shouldLog) {
        LOG_DEBUG_CAT("Quantum", "Computed gravitational potential for vertices {} and {}: distance={}, potential={}",
                      std::source_location::current(), vertexIndex1, vertexIndex2, distance, potential);
    }

    return potential;
}

std::vector<long double> UniversalEquation::computeGravitationalAcceleration(int vertexIndex) const {
    static thread_local int logCount = 0;
    bool shouldLog = debug_.load() && (++logCount % 1000 == 0); // Log every 1000th call per thread

    if (shouldLog) {
        LOG_INFO_CAT("Quantum", "Computing gravitational acceleration for vertex {}",
                     std::source_location::current(), vertexIndex);
    }

    // Validate vertexIndex
    size_t size = nCubeVertices_.size();
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= size) {
        if (shouldLog) {
            LOG_WARNING_CAT("Quantum", "Invalid vertexIndex {} (size={}), returning zero acceleration",
                            std::source_location::current(), vertexIndex, size);
        }
        return std::vector<long double>(getCurrentDimension(), 0.0L);
    }

    // Check for empty vertex array
    if (size == 0) {
        if (shouldLog) {
            LOG_WARNING_CAT("Quantum", "Empty nCubeVertices_, returning zero acceleration",
                            std::source_location::current());
        }
        return std::vector<long double>(getCurrentDimension(), 0.0L);
    }

    size_t dim = static_cast<size_t>(getCurrentDimension());
    std::vector<long double> acceleration(dim, 0.0L);
    constexpr long double G = 6.67430e-11L; // Gravitational constant (m^3 kg^-1 s^-2)
    constexpr long double volume = 1e-3L; // Volume scaling (0.001 m^3)
    long double density = std::clamp(materialDensity_.load(), 0.0L, 1.0e6L);
    long double mass = density * volume; // Mass (kg)
    uint64_t numVertices = std::min(static_cast<uint64_t>(size), getMaxVertices());
    uint64_t sampleStep = std::max<uint64_t>(1, numVertices / 100); // Sample ~100 interactions per vertex

    const auto& v1 = nCubeVertices_[static_cast<size_t>(vertexIndex)];

    // Ensure dimension consistency for v1
    if (v1.size() != dim) {
        if (shouldLog) {
            LOG_ERROR_CAT("Quantum", "Dimension mismatch for v1: size={}, expected={}",
                          std::source_location::current(), v1.size(), dim);
        }
        return acceleration; // Return zeros
    }

    for (uint64_t j = 0; j < numVertices; j += sampleStep) {
        size_t j_idx = static_cast<size_t>(j % size); // Wrap around safely
        if (j_idx == static_cast<size_t>(vertexIndex)) continue; // Skip self-interaction

        const auto& v2 = nCubeVertices_[j_idx];

        // Ensure dimension consistency for v2
        if (v2.size() != dim) {
            if (shouldLog) {
                LOG_ERROR_CAT("Quantum", "Dimension mismatch for v2 at index {}: size={}, expected={}",
                              std::source_location::current(), j_idx, v2.size(), dim);
            }
            continue; // Skip invalid vertices
        }

        // Compute distance with safeguards
        long double distance = 0.0L;
        bool validDistance = true;
        for (size_t i = 0; i < dim; ++i) {
            long double diff = v1[i] - v2[i];
            if (std::isnan(diff) || std::isinf(diff)) {
                if (shouldLog) {
                    LOG_WARNING_CAT("Quantum", "NaN/Inf diff in dimension {} for vertices {} and {}",
                                    std::source_location::current(), i, vertexIndex, j_idx);
                }
                diff = 0.0L;
            }
            distance += diff * diff;
            if (std::isnan(distance) || std::isinf(distance)) {
                validDistance = false;
                break;
            }
        }
        if (!validDistance) {
            if (shouldLog) {
                LOG_WARNING_CAT("Quantum", "Invalid distance computation for vertex {} and {}, skipping",
                                std::source_location::current(), vertexIndex, j_idx);
            }
            continue;
        }
        distance = std::sqrt(distance);
        if (std::isnan(distance) || std::isinf(distance) || distance <= 0.0L) {
            if (shouldLog) {
                LOG_WARNING_CAT("Quantum", "Invalid distance for vertex {} and {}: distance={}. Skipping",
                                std::source_location::current(), vertexIndex, j_idx, distance);
            }
            continue;
        }

        constexpr long double minDistance = 1e-10L;
        if (distance < minDistance) {
            if (shouldLog) {
                LOG_DEBUG_CAT("Quantum", "Skipping vertex {}: distance too small ({})",
                              std::source_location::current(), j_idx, distance);
            }
            continue;
        }

        // Compute gravitational acceleration: a = G * m / r^2 * (unit vector)
        long double forceMagnitude = safe_div(G * mass * mass, distance * distance);
        for (size_t i = 0; i < dim; ++i) {
            long double diff = v2[i] - v1[i];
            long double unitVector = safe_div(diff, distance);
            if (std::isnan(unitVector) || std::isinf(unitVector)) {
                if (shouldLog) {
                    LOG_WARNING_CAT("Quantum", "Invalid unit vector in dimension {} for vertex {} and {}, skipping dim",
                                    std::source_location::current(), i, vertexIndex, j_idx);
                }
                continue;
            }
            acceleration[i] += forceMagnitude * unitVector;
            if (std::isnan(acceleration[i]) || std::isinf(acceleration[i])) {
                acceleration[i] = 0.0L; // Reset invalid value
            }
        }
    }

    // Scale up the sampled acceleration with clamp
    long double scale = static_cast<long double>(sampleStep);
    scale = std::clamp(scale, 1.0L, static_cast<long double>(numVertices)); // Prevent overflow
    for (size_t i = 0; i < dim; ++i) {
        acceleration[i] *= scale;
        if (std::isnan(acceleration[i]) || std::isinf(acceleration[i])) {
            if (shouldLog) {
                LOG_WARNING_CAT("Quantum", "Invalid scaled acceleration in dimension {} for vertex {}: value={}. Resetting to 0",
                                std::source_location::current(), i, vertexIndex, acceleration[i]);
            }
            acceleration[i] = 0.0L;
        }
    }

    // Apply influence factor with clamp
    long double influence = getInfluence();
    influence = std::clamp(influence, 0.0L, 1.0e6L); // Prevent extreme values
    for (size_t i = 0; i < dim; ++i) {
        acceleration[i] *= influence;
        if (std::isnan(acceleration[i]) || std::isinf(acceleration[i])) {
            acceleration[i] = 0.0L;
        }
    }

    if (shouldLog) {
        LOG_DEBUG_CAT("Quantum", "Computed gravitational acceleration for vertex {}: components={}",
                      std::source_location::current(), vertexIndex, acceleration.size());
    }
    return acceleration;
}