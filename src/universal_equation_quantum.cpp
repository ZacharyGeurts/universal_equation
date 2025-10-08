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
    // Log the start of computation
    logger_.log(Logging::LogLevel::Info, "Starting gravitational potential computation between vertices {} and {}",
                std::source_location::current(), vertexIndex1, vertexIndex2);

    // Validate vertexIndex1
    validateVertexIndex(vertexIndex1, std::source_location::current());

    // Clamp vertexIndex2 to valid range if invalid
    size_t size = nCubeVertices_.size();
    if (vertexIndex2 < 0 || static_cast<size_t>(vertexIndex2) >= size) {
        logger_.log(Logging::LogLevel::Warning, "Invalid vertexIndex2: vertexIndex2={}, size={}. Clamping to 0",
                    std::source_location::current(), vertexIndex2, size);
        vertexIndex2 = 0;
    }

    // Validate vertexIndex2 after clamping
    validateVertexIndex(vertexIndex2, std::source_location::current());

    // Skip if vertices are the same to avoid self-interaction
    if (vertexIndex1 == vertexIndex2) {
        logger_.log(Logging::LogLevel::Debug, "Skipping self-interaction for vertex {}",
                    std::source_location::current(), vertexIndex1);
        return 0.0L;
    }

    // Compute distance between vertices
    long double distance = 0.0L;
    const auto& v1 = nCubeVertices_[vertexIndex1];
    const auto& v2 = nCubeVertices_[vertexIndex2];
    for (size_t i = 0; i < static_cast<size_t>(getCurrentDimension()); ++i) {
        long double diff = v1[i] - v2[i];
        distance += diff * diff;
    }
    distance = std::sqrt(distance);

    // Avoid division by zero
    constexpr long double minDistance = 1e-10L;
    if (distance < minDistance) {
        logger_.log(Logging::LogLevel::Debug, "Distance too small for vertices {} and {}: distance={}. Setting to minimum",
                    std::source_location::current(), vertexIndex1, vertexIndex2, distance);
        distance = minDistance;
    }

    // Compute gravitational potential: V = -G * m1 * m2 / r
    constexpr long double G = 6.67430e-11L; // Gravitational constant (m^3 kg^-1 s^-2)
    constexpr long double volume = 1e-3L; // Volume scaling (0.001 m^3, assuming ~0.1m per vertex)
    long double mass = materialDensity_ * volume; // Mass (kg) = density (kg/m^3) * volume (m^3)
    long double potential = -safe_div(G * mass * mass, distance);

    // Apply influence factor
    potential *= getInfluence();

    logger_.log(Logging::LogLevel::Debug, "Computed gravitational potential for vertices {} and {}: distance={}, potential={}",
                std::source_location::current(), vertexIndex1, vertexIndex2, distance, potential);

    return potential;
}

std::vector<long double> UniversalEquation::computeGravitationalAcceleration(int vertexIndex) const {
    logger_.log(Logging::LogLevel::Info, "Computing gravitational acceleration for vertex {}",
                std::source_location::current(), vertexIndex);

    validateVertexIndex(vertexIndex, std::source_location::current());

    std::vector<long double> acceleration(getCurrentDimension(), 0.0L);
    constexpr long double G = 6.67430e-11L; // Gravitational constant (m^3 kg^-1 s^-2)
    constexpr long double volume = 1e-3L; // Volume scaling (0.001 m^3)
    long double mass = materialDensity_ * volume; // Mass (kg)

    for (size_t j = 0; j < nCubeVertices_.size(); ++j) {
        if (static_cast<int>(j) == vertexIndex) continue; // Skip self-interaction

        long double distance = 0.0L;
        const auto& v1 = nCubeVertices_[vertexIndex];
        const auto& v2 = nCubeVertices_[j];
        for (size_t i = 0; i < static_cast<size_t>(getCurrentDimension()); ++i) {
            long double diff = v1[i] - v2[i];
            distance += diff * diff;
        }
        distance = std::sqrt(distance);

        constexpr long double minDistance = 1e-10L;
        if (distance < minDistance) {
            logger_.log(Logging::LogLevel::Debug, "Skipping vertex {}: distance too small ({})",
                        std::source_location::current(), j, distance);
            continue;
        }

        // Compute gravitational acceleration: a = G * m / r^2 * (unit vector)
        long double forceMagnitude = safe_div(G * mass * mass, distance * distance);
        for (size_t i = 0; i < static_cast<size_t>(getCurrentDimension()); ++i) {
            long double unitVector = safe_div(v2[i] - v1[i], distance);
            acceleration[i] += forceMagnitude * unitVector;
        }
    }

    // Apply influence factor
    long double influence = getInfluence();
    for (size_t i = 0; i < static_cast<size_t>(getCurrentDimension()); ++i) {
        acceleration[i] *= influence;
    }

    logger_.log(Logging::LogLevel::Debug, "Computed gravitational acceleration for vertex {}: components={}",
                std::source_location::current(), vertexIndex, acceleration.size());
    return acceleration;
}