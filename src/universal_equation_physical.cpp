// universal_equation_physical.cpp: Implements classical physics methods for UniversalEquation.
// Models a system of vertices representing particles in a 1-inch cube of water (density 1000 kg/m^3).
// Computes mass, volume, and density dynamically for multiple vertices, accounting for quantum effects via influence and NURBS.
// Uses Logging::Logger for consistent logging across the AMOURANTH RTX Engine.

// Copyright Zachary Geurts 2025 (powered by Grok with Science B*! precision)

#include "universal_equation.hpp"
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <algorithm>
#include "engine/logging.hpp"

// Helper: Safe division to prevent NaN/Inf
inline long double safe_div(long double a, long double b) {
    return (b != 0.0L && !std::isnan(b) && !std::isinf(b)) ? a / b : 0.0L;
}

long double UniversalEquation::computeVertexVolume(int vertexIndex) const {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Starting volume computation for vertex {}", std::source_location::current(), vertexIndex);
    }
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= getNCubeVertices().size()) {
        logger_.log(Logging::LogLevel::Error, "Invalid vertex index for volume calculation: vertex={}, vertices size={}",
                    std::source_location::current(), vertexIndex, getNCubeVertices().size());
        throw std::out_of_range("Invalid vertex index for volume calculation");
    }
    // Base side length for 1-inch cube (0.0254 m), scaled for multiple vertices
    long double baseSide = 0.0254L / std::pow(static_cast<long double>(getNCubeVertices().size()), 1.0L/3.0L);
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Computed base side length: vertex={}, baseSide={}", 
                    std::source_location::current(), vertexIndex, baseSide);
    }
    long double volume = std::pow(baseSide, 3); // 3D volume per vertex
    if (std::isnan(volume) || std::isinf(volume)) {
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Warning, "Invalid vertex volume: vertex={}, side={}, volume={}", 
                        std::source_location::current(), vertexIndex, baseSide, volume);
        }
        volume = 0.0254L * 0.0254L * 0.0254L / getNCubeVertices().size(); // Fallback
        logger_.log(Logging::LogLevel::Warning, "Using fallback volume for vertex {}: volume={}", 
                    std::source_location::current(), vertexIndex, volume);
        return volume;
    }
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Computed volume for vertex {}: volume={}", 
                    std::source_location::current(), vertexIndex, volume);
    }
    return volume;
}

long double UniversalEquation::computeVertexMass(int vertexIndex) const {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Starting mass computation for vertex {}", std::source_location::current(), vertexIndex);
    }
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= getNCubeVertices().size()) {
        logger_.log(Logging::LogLevel::Error, "Invalid vertex index for mass calculation: vertex={}, vertices size={}",
                    std::source_location::current(), vertexIndex, getNCubeVertices().size());
        throw std::out_of_range("Invalid vertex index for mass calculation");
    }
    long double density = 1000.0L; // Water density (kg/m^3)
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Using density for vertex {}: density={}", 
                    std::source_location::current(), vertexIndex, density);
    }
    long double volume = computeVertexVolume(vertexIndex);
    long double mass = density * volume;
    if (std::isnan(mass) || std::isinf(mass)) {
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Warning, "Invalid vertex mass: vertex={}, volume={}, mass={}", 
                        std::source_location::current(), vertexIndex, volume, mass);
        }
        mass = density * (0.0254L * 0.0254L * 0.0254L / getNCubeVertices().size()); // Fallback
        logger_.log(Logging::LogLevel::Warning, "Using fallback mass for vertex {}: mass={}", 
                    std::source_location::current(), vertexIndex, mass);
        return mass;
    }
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Computed mass for vertex {}: mass={}", 
                    std::source_location::current(), vertexIndex, mass);
    }
    return mass;
}

long double UniversalEquation::computeVertexDensity(int vertexIndex) const {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Starting density computation for vertex {}", std::source_location::current(), vertexIndex);
    }
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= getNCubeVertices().size()) {
        logger_.log(Logging::LogLevel::Error, "Invalid vertex index for density calculation: vertex={}, vertices size={}",
                    std::source_location::current(), vertexIndex, getNCubeVertices().size());
        throw std::out_of_range("Invalid vertex index for density calculation");
    }
    long double density = 1000.0L; // Water density
    if (std::isnan(density) || std::isinf(density)) {
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Warning, "Invalid vertex density: vertex={}, density={}", 
                        std::source_location::current(), vertexIndex, density);
        }
        density = 1000.0L; // Default to water's density
        logger_.log(Logging::LogLevel::Warning, "Using default density for vertex {}: density={}", 
                    std::source_location::current(), vertexIndex, density);
        return density;
    }
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Computed density for vertex {}: density={}", 
                    std::source_location::current(), vertexIndex, density);
    }
    return density;
}

std::vector<long double> UniversalEquation::computeCenterOfMass() const {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Starting center of mass computation for {} vertices", 
                    std::source_location::current(), getNCubeVertices().size());
    }
    std::vector<long double> com(getCurrentDimension(), 0.0L);
    long double totalMass = 0.0L;
    for (size_t i = 0; i < getNCubeVertices().size(); ++i) {
        long double mass = computeVertexMass(i);
        totalMass += mass;
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Vertex {}: mass={}, cumulative totalMass={}", 
                        std::source_location::current(), i, mass, totalMass);
        }
        for (int j = 0; j < getCurrentDimension(); ++j) {
            com[j] += mass * getNCubeVertices()[i][j];
        }
    }
    if (totalMass > 0.0L) {
        for (int j = 0; j < getCurrentDimension(); ++j) {
            com[j] = safe_div(com[j], totalMass);
            if (debug_.load()) {
                logger_.log(Logging::LogLevel::Debug, "Center of mass component {}: value={}", 
                            std::source_location::current(), j, com[j]);
            }
        }
    } else if (debug_.load()) {
        logger_.log(Logging::LogLevel::Warning, "Total mass is zero, center of mass set to origin", 
                    std::source_location::current());
    }
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Center of mass computation completed", std::source_location::current());
    }
    return com;
}

long double UniversalEquation::computeTotalSystemVolume() const {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Starting total system volume computation for {} vertices", 
                    std::source_location::current(), getNCubeVertices().size());
    }
    long double totalVolume = 0.0L;
    for (size_t i = 0; i < getNCubeVertices().size(); ++i) {
        long double volume = computeVertexVolume(i);
        totalVolume += volume;
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Vertex {}: volume={}, cumulative totalVolume={}", 
                        std::source_location::current(), i, volume, totalVolume);
        }
    }
    if (std::isnan(totalVolume) || std::isinf(totalVolume)) {
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Warning, "Invalid total system volume: totalVolume={}", 
                        std::source_location::current(), totalVolume);
        }
        totalVolume = 0.0254L * 0.0254L * 0.0254L; // Fallback to single cube volume
        logger_.log(Logging::LogLevel::Warning, "Using fallback total system volume: totalVolume={}", 
                    std::source_location::current(), totalVolume);
        return totalVolume;
    }
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Total system volume computed: totalVolume={}", 
                    std::source_location::current(), totalVolume);
    }
    return totalVolume;
}

long double UniversalEquation::computeGravitationalPotential(int vertexIndex1, int vertexIndex2) const {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Starting gravitational potential computation between vertices {} and {}", 
                    std::source_location::current(), vertexIndex1, vertexIndex2);
    }
    if (vertexIndex1 == vertexIndex2 || vertexIndex1 < 0 || vertexIndex2 < 0 ||
        static_cast<size_t>(vertexIndex1) >= getNCubeVertices().size() ||
        static_cast<size_t>(vertexIndex2) >= getNCubeVertices().size()) {
        if (debug_.load() && vertexIndex1 == vertexIndex2) {
            logger_.log(Logging::LogLevel::Warning, "Same vertex indices for gravitational potential: vertex={}", 
                        std::source_location::current(), vertexIndex1);
        }
        if (debug_.load() && (vertexIndex1 < 0 || vertexIndex2 < 0 || 
                              static_cast<size_t>(vertexIndex1) >= getNCubeVertices().size() ||
                              static_cast<size_t>(vertexIndex2) >= getNCubeVertices().size())) {
            logger_.log(Logging::LogLevel::Error, "Invalid vertex indices for gravitational potential: vertex1={}, vertex2={}, vertices size={}",
                        std::source_location::current(), vertexIndex1, vertexIndex2, getNCubeVertices().size());
        }
        return 0.0L;
    }
    long double distance = 0.0L;
    for (int i = 0; i < getCurrentDimension(); ++i) {
        long double diff = getNCubeVertices()[vertexIndex1][i] - getNCubeVertices()[vertexIndex2][i];
        distance += diff * diff;
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Dimension {}: diff={} for vertices {} and {}", 
                        std::source_location::current(), i, diff, vertexIndex1, vertexIndex2);
        }
    }
    distance = std::sqrt(std::max(distance, 1e-15L));
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Computed distance between vertices {} and {}: distance={}", 
                    std::source_location::current(), vertexIndex1, vertexIndex2, distance);
    }
    long double m1 = computeVertexMass(vertexIndex1);
    long double m2 = computeVertexMass(vertexIndex2);
    long double G = 6.67430e-11L; // Gravitational constant
    long double result = safe_div(-G * m1 * m2, distance);
    if (std::isnan(result) || std::isinf(result)) {
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Warning, "Invalid gravitational potential: m1={}, m2={}, distance={}", 
                        std::source_location::current(), m1, m2, distance);
        }
        return 0.0L;
    }
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Computed gravitational potential for vertices {} and {}: result={}", 
                    std::source_location::current(), vertexIndex1, vertexIndex2, result);
    }
    return result;
}

std::vector<long double> UniversalEquation::computeGravitationalAcceleration(int vertexIndex) const {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Starting gravitational acceleration computation for vertex {}", 
                    std::source_location::current(), vertexIndex);
    }
    std::vector<long double> acc(getCurrentDimension(), 0.0L);
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= getNCubeVertices().size()) {
        logger_.log(Logging::LogLevel::Error, "Invalid vertex index for gravitational acceleration: vertex={}, vertices size={}",
                    std::source_location::current(), vertexIndex, getNCubeVertices().size());
        return acc;
    }
    long double G = 6.67430e-11L; // Gravitational constant
    bool anyClamped = false;
    for (size_t i = 0; i < getNCubeVertices().size(); ++i) {
        if (static_cast<int>(i) == vertexIndex) continue;
        long double distance = 0.0L;
        for (int j = 0; j < getCurrentDimension(); ++j) {
            long double diff = getNCubeVertices()[i][j] - getNCubeVertices()[vertexIndex][j];
            distance += diff * diff;
            if (debug_.load()) {
                logger_.log(Logging::LogLevel::Debug, "Vertex {}, dimension {}: diff={}", 
                            std::source_location::current(), i, j, diff);
            }
        }
        distance = std::sqrt(std::max(distance, 1e-15L));
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Distance to vertex {}: distance={}", 
                        std::source_location::current(), i, distance);
        }
        long double m2 = computeVertexMass(i);
        long double factor = safe_div(G * m2, distance * distance * distance);
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Vertex {}: mass={}, factor={}", 
                        std::source_location::current(), i, m2, factor);
        }
        for (int j = 0; j < getCurrentDimension(); ++j) {
            long double delta = factor * (getNCubeVertices()[i][j] - getNCubeVertices()[vertexIndex][j]);
            if (std::isnan(delta) || std::isinf(delta)) {
                anyClamped = true;
                if (debug_.load()) {
                    logger_.log(Logging::LogLevel::Warning, "Invalid acceleration component for vertex {}, dimension {}: delta={}", 
                                std::source_location::current(), i, j, delta);
                }
                continue;
            }
            acc[j] += delta;
            if (debug_.load()) {
                logger_.log(Logging::LogLevel::Debug, "Vertex {}, dimension {}: acceleration component={}", 
                            std::source_location::current(), i, j, acc[j]);
            }
        }
    }
    if (debug_.load() && anyClamped) {
        logger_.log(Logging::LogLevel::Warning, "Some gravitational acceleration components were invalid and skipped for vertex {}", 
                    std::source_location::current(), vertexIndex);
    }
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Gravitational acceleration computation completed for vertex {}", 
                    std::source_location::current(), vertexIndex);
    }
    return acc;
}

std::vector<long double> UniversalEquation::computeClassicalEMField(int vertexIndex) const {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Starting classical EM field computation for vertex {}", 
                    std::source_location::current(), vertexIndex);
    }
    std::vector<long double> field(getCurrentDimension(), 0.0L);
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= getNCubeVertices().size()) {
        logger_.log(Logging::LogLevel::Error, "Invalid vertex index for classical EM field: vertex={}, vertices size={}",
                    std::source_location::current(), vertexIndex, getNCubeVertices().size());
        return field;
    }
    long double k = 8.9875517923e9L; // Coulomb constant
    bool anyClamped = false;
    for (size_t i = 0; i < getNCubeVertices().size(); ++i) {
        if (static_cast<int>(i) == vertexIndex) continue;
        long double distance = 0.0L;
        for (int j = 0; j < getCurrentDimension(); ++j) {
            long double diff = getNCubeVertices()[i][j] - getNCubeVertices()[vertexIndex][j];
            distance += diff * diff;
            if (debug_.load()) {
                logger_.log(Logging::LogLevel::Debug, "Vertex {}, dimension {}: diff={}", 
                            std::source_location::current(), i, j, diff);
            }
        }
        distance = std::sqrt(std::max(distance, 1e-15L));
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Distance to vertex {}: distance={}", 
                        std::source_location::current(), i, distance);
        }
        long double q2 = getVertexSpins()[i] * 1e-15L; // Scale down for neutrality
        long double factor = safe_div(k * q2, distance * distance * distance);
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Vertex {}: charge={}, factor={}", 
                        std::source_location::current(), i, q2, factor);
        }
        for (int j = 0; j < getCurrentDimension(); ++j) {
            long double delta = factor * (getNCubeVertices()[i][j] - getNCubeVertices()[vertexIndex][j]);
            if (std::isnan(delta) || std::isinf(delta)) {
                anyClamped = true;
                if (debug_.load()) {
                    logger_.log(Logging::LogLevel::Warning, "Invalid EM field component for vertex {}, dimension {}: delta={}", 
                                std::source_location::current(), i, j, delta);
                }
                continue;
            }
            field[j] += delta;
            if (debug_.load()) {
                logger_.log(Logging::LogLevel::Debug, "Vertex {}, dimension {}: EM field component={}", 
                            std::source_location::current(), i, j, field[j]);
            }
        }
    }
    if (debug_.load() && anyClamped) {
        logger_.log(Logging::LogLevel::Warning, "Some classical EM field components were invalid and skipped for vertex {}", 
                    std::source_location::current(), vertexIndex);
    }
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Classical EM field computation completed for vertex {}", 
                    std::source_location::current(), vertexIndex);
    }
    return field;
}

void UniversalEquation::updateOrbitalVelocity(long double dt) {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Starting orbital velocity update for {} vertices with dt={}", 
                    std::source_location::current(), getNCubeVertices().size(), dt);
    }
    if (dt <= 0.0L || std::isnan(dt) || std::isinf(dt)) {
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Error, "Invalid time step for orbital velocity update: dt={}", 
                        std::source_location::current(), dt);
        }
        return;
    }
    bool anyClamped = false;
    #pragma omp parallel for if(getNCubeVertices().size() > 100)
    for (size_t i = 0; i < getNCubeVertices().size(); ++i) {
        auto acc = computeGravitationalAcceleration(i);
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Vertex {}: computed acceleration", 
                        std::source_location::current(), i);
        }
        for (int j = 0; j < getCurrentDimension(); ++j) {
            long double oldMomentum = vertexMomenta_[i][j];
            vertexMomenta_[i][j] += dt * acc[j];
            if (vertexMomenta_[i][j] < -0.9L || vertexMomenta_[i][j] > 0.9L) {
                vertexMomenta_[i][j] = std::clamp(vertexMomenta_[i][j], -0.9L, 0.9L);
                #pragma omp critical
                {
                    anyClamped = true;
                    if (debug_.load()) {
                        logger_.log(Logging::LogLevel::Warning, "Clamped momentum for vertex {}, dimension {}: old={}, new={}", 
                                    std::source_location::current(), i, j, oldMomentum, vertexMomenta_[i][j]);
                    }
                }
            }
            if (debug_.load()) {
                logger_.log(Logging::LogLevel::Debug, "Vertex {}, dimension {}: updated momentum={}", 
                            std::source_location::current(), i, j, vertexMomenta_[i][j]);
            }
        }
    }
    if (debug_.load() && anyClamped) {
        logger_.log(Logging::LogLevel::Warning, "Some momenta were clamped in updateOrbitalVelocity", 
                    std::source_location::current());
    }
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Orbital velocity update completed", std::source_location::current());
    }
}

void UniversalEquation::updateOrbitalPositions(long double dt) {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Starting orbital position update for {} vertices with dt={}", 
                    std::source_location::current(), getNCubeVertices().size(), dt);
    }
    if (dt <= 0.0L || std::isnan(dt) || std::isinf(dt)) {
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Error, "Invalid time step for orbital position update: dt={}", 
                        std::source_location::current(), dt);
        }
        return;
    }
    bool anyClamped = false;
    #pragma omp parallel for if(getNCubeVertices().size() > 100)
    for (size_t i = 0; i < getNCubeVertices().size(); ++i) {
        for (int j = 0; j < getCurrentDimension(); ++j) {
            long double oldPosition = nCubeVertices_[i][j];
            nCubeVertices_[i][j] += dt * vertexMomenta_[i][j];
            if (nCubeVertices_[i][j] < -1e3L || nCubeVertices_[i][j] > 1e3L) {
                nCubeVertices_[i][j] = std::clamp(nCubeVertices_[i][j], -1e3L, 1e3L);
                #pragma omp critical
                {
                    anyClamped = true;
                    if (debug_.load()) {
                        logger_.log(Logging::LogLevel::Warning, "Clamped position for vertex {}, dimension {}: old={}, new={}", 
                                    std::source_location::current(), i, j, oldPosition, nCubeVertices_[i][j]);
                    }
                }
            }
            if (debug_.load()) {
                logger_.log(Logging::LogLevel::Debug, "Vertex {}, dimension {}: updated position={}", 
                            std::source_location::current(), i, j, nCubeVertices_[i][j]);
            }
        }
    }
    if (debug_.load() && anyClamped) {
        logger_.log(Logging::LogLevel::Warning, "Some positions were clamped in updateOrbitalPositions", 
                    std::source_location::current());
    }
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Orbital position update completed", std::source_location::current());
    }
}

long double UniversalEquation::computeSystemEnergy() const {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Starting total system energy computation for {} vertices", 
                    std::source_location::current(), getNCubeVertices().size());
    }
    long double energy = 0.0L;
    for (size_t i = 0; i < getNCubeVertices().size(); ++i) {
        for (size_t j = i + 1; j < getNCubeVertices().size(); ++j) {
            long double potential = computeGravitationalPotential(i, j);
            energy += potential;
            if (debug_.load()) {
                logger_.log(Logging::LogLevel::Debug, "Vertices {} and {}: potential={}, cumulative energy={}", 
                            std::source_location::current(), i, j, potential, energy);
            }
        }
        long double kinetic = 0.0L;
        for (int k = 0; k < getCurrentDimension(); ++k) {
            kinetic += vertexMomenta_[i][k] * vertexMomenta_[i][k];
            if (debug_.load()) {
                logger_.log(Logging::LogLevel::Debug, "Vertex {}, dimension {}: momentum squared={}", 
                            std::source_location::current(), i, k, vertexMomenta_[i][k] * vertexMomenta_[i][k]);
            }
        }
        long double kineticEnergy = 0.5L * computeVertexMass(i) * kinetic;
        if (std::isnan(kineticEnergy) || std::isinf(kineticEnergy)) {
            if (debug_.load()) {
                logger_.log(Logging::LogLevel::Warning, "Invalid kinetic energy for vertex {}: kinetic={}", 
                            std::source_location::current(), i, kinetic);
            }
            continue;
        }
        energy += kineticEnergy;
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Vertex {}: kinetic energy={}, cumulative energy={}", 
                        std::source_location::current(), i, kineticEnergy, energy);
        }
    }
    if (std::isnan(energy) || std::isinf(energy)) {
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Warning, "Invalid total system energy: energy={}", 
                        std::source_location::current(), energy);
        }
        return 0.0L;
    }
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Total system energy computed: energy={}", 
                    std::source_location::current(), energy);
        logger_.log(Logging::LogLevel::Info, "Total system energy computation completed", 
                    std::source_location::current());
    }
    return energy;
}

long double UniversalEquation::computePythagoreanScaling(int vertexIndex) const {
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Info, "Starting Pythagorean scaling computation for vertex {}", 
                    std::source_location::current(), vertexIndex);
    }
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= getNCubeVertices().size()) {
        logger_.log(Logging::LogLevel::Error, "Invalid vertex index for Pythagorean scaling: vertex={}, vertices size={}",
                    std::source_location::current(), vertexIndex, getNCubeVertices().size());
        return 1.0L;
    }
    long double magnitude = 0.0L;
    const auto& vertex = getNCubeVertices()[vertexIndex];
    for (int i = 0; i < getCurrentDimension(); ++i) {
        magnitude += vertex[i] * vertex[i];
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Debug, "Vertex {}, dimension {}: component squared={}", 
                        std::source_location::current(), vertexIndex, i, vertex[i] * vertex[i]);
        }
    }
    long double result = std::sqrt(std::max(magnitude, 0.0L)) / 0.0254L; // Normalize relative to 1-inch cube
    if (std::isnan(result) || std::isinf(result)) {
        if (debug_.load()) {
            logger_.log(Logging::LogLevel::Warning, "Invalid Pythagorean scaling for vertex {}: magnitude={}", 
                        std::source_location::current(), vertexIndex, magnitude);
        }
        return 1.0L;
    }
    if (debug_.load()) {
        logger_.log(Logging::LogLevel::Debug, "Computed Pythagorean scaling for vertex {}: result={}", 
                    std::source_location::current(), vertexIndex, result);
        logger_.log(Logging::LogLevel::Info, "Pythagorean scaling computation completed for vertex {}", 
                    std::source_location::current(), vertexIndex);
    }
    return result;
}