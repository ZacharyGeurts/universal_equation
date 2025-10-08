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

long double UniversalEquation::computeGravitationalPotential(int vertexIndex1, int vertexIndex2) const {
    // Guard against excessive logging in hot loops
    static thread_local int logCount = 0;
    bool shouldLog = debug_.load() && (++logCount % 10000 == 0); // Log every 10000th call per thread

    if (shouldLog) {
        logger_.log(Logging::LogLevel::Info, "Starting gravitational potential computation between vertices {} and {}",
                    std::source_location::current(), vertexIndex1, vertexIndex2);
    }

    // Check for empty vertex array
    size_t size = nCubeVertices_.size();
    if (size == 0) {
        if (shouldLog) {
            logger_.log(Logging::LogLevel::Warning, "Empty nCubeVertices_, returning 0 potential",
                        std::source_location::current());
        }
        return 0.0L;
    }

    // Validate vertexIndex1
    validateVertexIndex(vertexIndex1, std::source_location::current());

    // Clamp vertexIndex2 to valid range if invalid
    if (vertexIndex2 < 0 || static_cast<size_t>(vertexIndex2) >= size) {
        vertexIndex2 = static_cast<int>(size - 1); // Use last valid index
        if (shouldLog) {
            logger_.log(Logging::LogLevel::Warning, "Clamped vertexIndex2 to {} (original={}, size={})",
                        std::source_location::current(), vertexIndex2, vertexIndex2, size);
        }
    }

    // Validate vertexIndex2 after clamping
    validateVertexIndex(vertexIndex2, std::source_location::current());

    // Skip if vertices are the same to avoid self-interaction
    if (vertexIndex1 == vertexIndex2) {
        if (shouldLog) {
            logger_.log(Logging::LogLevel::Debug, "Skipping self-interaction for vertex {}",
                        std::source_location::current(), vertexIndex1);
        }
        return 0.0L;
    }

    // Validate vector sizes
    const auto& v1 = nCubeVertices_[vertexIndex1];
    const auto& v2 = nCubeVertices_[vertexIndex2];
    size_t dim = static_cast<size_t>(getCurrentDimension());
    if (v1.size() != dim || v2.size() != dim) {
        if (shouldLog) {
            logger_.log(Logging::LogLevel::Error, "Dimension mismatch: v1.size()={}, v2.size()={}, expected={}",
                        std::source_location::current(), v1.size(), v2.size(), dim);
        }
        throw std::runtime_error("Dimension mismatch in vertex vectors");
    }

    // Compute distance between vertices
    long double distance = 0.0L;
    for (size_t i = 0; i < dim; ++i) {
        long double diff = v1[i] - v2[i];
        if (std::isnan(diff) || std::isinf(diff)) {
            if (shouldLog) {
                logger_.log(Logging::LogLevel::Warning, "NaN/Inf diff in dimension {} for vertices {} and {}",
                            std::source_location::current(), i, vertexIndex1, vertexIndex2);
            }
            diff = 0.0L;
        }
        distance += diff * diff;
        if (std::isnan(distance) || std::isinf(distance)) {
            if (shouldLog) {
                logger_.log(Logging::LogLevel::Warning, "NaN/Inf in distance computation for vertices {} and {}, resetting distance",
                            std::source_location::current(), vertexIndex1, vertexIndex2);
            }
            distance = 1.0L; // Reset to safe value
            break;
        }
    }
    distance = std::sqrt(distance);
    if (std::isnan(distance) || std::isinf(distance) || distance < 0.0L) {
        if (shouldLog) {
            logger_.log(Logging::LogLevel::Warning, "Invalid distance for vertices {} and {}: distance={}. Using minDistance",
                        std::source_location::current(), vertexIndex1, vertexIndex2, distance);
        }
        distance = 1e-10L;
    }

    // Avoid division by zero
    constexpr long double minDistance = 1e-10L;
    if (distance < minDistance) {
        if (shouldLog) {
            logger_.log(Logging::LogLevel::Debug, "Distance too small for vertices {} and {}: distance={}. Setting to minimum",
                        std::source_location::current(), vertexIndex1, vertexIndex2, distance);
        }
        distance = minDistance;
    }

    // Compute gravitational potential: V = -G * m1 * m2 / r
    constexpr long double G = 6.67430e-11L; // Gravitational constant (m^3 kg^-1 s^-2)
    constexpr long double volume = 1e-3L; // Volume scaling (0.001 m^3, assuming ~0.1m per vertex)
    long double mass = std::clamp(materialDensity_.load(), 0.0L, 1.0e6L) * volume; // Mass (kg) = density (kg/m^3) * volume (m^3)
    long double potential = -safe_div(G * mass * mass, distance);

    // Apply influence factor
    potential *= getInfluence();

    if (std::isnan(potential) || std::isinf(potential)) {
        if (shouldLog) {
            logger_.log(Logging::LogLevel::Warning, "Invalid potential for vertices {} and {}: potential={}. Returning 0",
                        std::source_location::current(), vertexIndex1, vertexIndex2, potential);
        }
        return 0.0L;
    }

    if (shouldLog) {
        logger_.log(Logging::LogLevel::Debug, "Computed gravitational potential for vertices {} and {}: distance={}, potential={}",
                    std::source_location::current(), vertexIndex1, vertexIndex2, distance, potential);
    }

    return potential;
}

std::vector<long double> UniversalEquation::computeGravitationalAcceleration(int vertexIndex) const {
    static thread_local int logCount = 0;
    bool shouldLog = debug_.load() && (++logCount % 1000 == 0); // Log every 1000th call per thread

    if (shouldLog) {
        logger_.log(Logging::LogLevel::Info, "Computing gravitational acceleration for vertex {}",
                    std::source_location::current(), vertexIndex);
    }

    validateVertexIndex(vertexIndex, std::source_location::current());

    // Check for empty vertex array
    size_t size = nCubeVertices_.size();
    if (size == 0) {
        if (shouldLog) {
            logger_.log(Logging::LogLevel::Warning, "Empty nCubeVertices_, returning zero acceleration",
                        std::source_location::current());
        }
        return std::vector<long double>(getCurrentDimension(), 0.0L);
    }

    std::vector<long double> acceleration(getCurrentDimension(), 0.0L);
    constexpr long double G = 6.67430e-11L; // Gravitational constant (m^3 kg^-1 s^-2)
    constexpr long double volume = 1e-3L; // Volume scaling (0.001 m^3)
    long double mass = std::clamp(materialDensity_.load(), 0.0L, 1.0e6L) * volume; // Mass (kg)
    uint64_t numVertices = std::min(static_cast<uint64_t>(nCubeVertices_.size()), getMaxVertices());
    uint64_t sampleStep = std::max<uint64_t>(1, numVertices / 100); // Sample ~100 interactions per vertex

    for (uint64_t j = 0; j < numVertices && j < nCubeVertices_.size(); j += sampleStep) {
        if (static_cast<int>(j) == vertexIndex) continue; // Skip self-interaction

        long double distance = 0.0L;
        const auto& v1 = nCubeVertices_[vertexIndex];
        const auto& v2 = nCubeVertices_[j];
        size_t dim = static_cast<size_t>(getCurrentDimension());
        if (v1.size() != dim || v2.size() != dim) {
            if (shouldLog) {
                logger_.log(Logging::LogLevel::Error, "Dimension mismatch: v1.size()={}, v2.size()={}, expected={}",
                            std::source_location::current(), v1.size(), v2.size(), dim);
            }
            continue; // Skip invalid vertices
        }
        for (size_t i = 0; i < dim; ++i) {
            long double diff = v1[i] - v2[i];
            if (std::isnan(diff) || std::isinf(diff)) {
                if (shouldLog) {
                    logger_.log(Logging::LogLevel::Warning, "NaN/Inf diff in dimension {} for vertices {} and {}",
                                std::source_location::current(), i, vertexIndex, j);
                }
                diff = 0.0L;
            }
            distance += diff * diff;
            if (std::isnan(distance) || std::isinf(distance)) {
                if (shouldLog) {
                    logger_.log(Logging::LogLevel::Warning, "NaN/Inf in distance for vertex {} and {}, skipping",
                                std::source_location::current(), vertexIndex, j);
                }
                distance = 1.0L;
                break;
            }
        }
        distance = std::sqrt(distance);
        if (std::isnan(distance) || std::isinf(distance) || distance <= 0.0L) {
            if (shouldLog) {
                logger_.log(Logging::LogLevel::Warning, "Invalid distance for vertex {} and {}: distance={}. Skipping",
                            std::source_location::current(), vertexIndex, j, distance);
            }
            continue;
        }

        constexpr long double minDistance = 1e-10L;
        if (distance < minDistance) {
            if (shouldLog) {
                logger_.log(Logging::LogLevel::Debug, "Skipping vertex {}: distance too small ({})",
                            std::source_location::current(), j, distance);
            }
            continue;
        }

        // Compute gravitational acceleration: a = G * m / r^2 * (unit vector)
        long double forceMagnitude = safe_div(G * mass * mass, distance * distance);
        for (size_t i = 0; i < dim; ++i) {
            long double unitVector = safe_div(v2[i] - v1[i], distance);
            if (std::isnan(unitVector) || std::isinf(unitVector)) {
                if (shouldLog) {
                    logger_.log(Logging::LogLevel::Warning, "Invalid unit vector in dimension {} for vertex {} and {}, skipping",
                                std::source_location::current(), i, vertexIndex, j);
                }
                continue;
            }
            acceleration[i] += forceMagnitude * unitVector;
        }
    }

    // Scale up the sampled acceleration
    long double scale = static_cast<long double>(sampleStep);
    for (size_t i = 0; i < static_cast<size_t>(getCurrentDimension()); ++i) {
        acceleration[i] *= scale;
        if (std::isnan(acceleration[i]) || std::isinf(acceleration[i])) {
            if (shouldLog) {
                logger_.log(Logging::LogLevel::Warning, "Invalid acceleration in dimension {} for vertex {}: value={}. Resetting to 0",
                            std::source_location::current(), i, vertexIndex, acceleration[i]);
            }
            acceleration[i] = 0.0L;
        }
    }

    // Apply influence factor
    long double influence = getInfluence();
    for (size_t i = 0; i < static_cast<size_t>(getCurrentDimension()); ++i) {
        acceleration[i] *= influence;
    }

    if (shouldLog) {
        logger_.log(Logging::LogLevel::Debug, "Computed gravitational acceleration for vertex {}: components={}",
                    std::source_location::current(), vertexIndex, acceleration.size());
    }
    return acceleration;
}