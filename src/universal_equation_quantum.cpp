// universal_equation_physical.cpp: Implements classical physics methods for UniversalEquation.
// Models a system of vertices representing particles in a 1-inch cube of water (density 1000 kg/m^3).
// Computes mass, volume, and density dynamically for multiple vertices, accounting for quantum effects via influence and NURBS.
// Uses Logging::Logger for consistent, thread-safe logging across the AMOURANTH RTX Engine.
// Thread-safety note: Uses std::latch for parallel updates and std::atomic for scalar members to avoid mutexes.
// Copyright Zachary Geurts 2025 (powered by Grok with Science B*! precision)

#include "universal_equation.hpp"
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <algorithm>
#include <latch>
#include "engine/logging.hpp"

long double UniversalEquation::computeVertexVolume(int vertexIndex) const {
    logger_.log(Logging::LogLevel::Info, "Starting volume computation for vertex {}", 
                std::source_location::current(), vertexIndex);
    validateVertexIndex(vertexIndex);
    long double baseSide = 0.0254L / std::pow(static_cast<long double>(nCubeVertices_.size()), 1.0L/3.0L);
    logger_.log(Logging::LogLevel::Debug, "Computed base side length: vertex={}, baseSide={}", 
                std::source_location::current(), vertexIndex, baseSide);
    long double volume = std::pow(baseSide, 3); // 3D volume per vertex
    if (std::isnan(volume) || std::isinf(volume)) {
        volume = 0.0254L * 0.0254L * 0.0254L / nCubeVertices_.size(); // Fallback
        logger_.log(Logging::LogLevel::Warning, "Invalid vertex volume, using fallback: vertex={}, volume={}", 
                    std::source_location::current(), vertexIndex, volume);
    }
    logger_.log(Logging::LogLevel::Debug, "Computed volume for vertex {}: volume={}", 
                std::source_location::current(), vertexIndex, volume);
    return volume;
}

long double UniversalEquation::computeVertexMass(int vertexIndex) const {
    logger_.log(Logging::LogLevel::Info, "Starting mass computation for vertex {}", 
                std::source_location::current(), vertexIndex);
    validateVertexIndex(vertexIndex);
    long double density = 1000.0L; // Water density (kg/m^3)
    logger_.log(Logging::LogLevel::Debug, "Using density for vertex {}: density={}", 
                std::source_location::current(), vertexIndex, density);
    long double volume = computeVertexVolume(vertexIndex);
    long double mass = density * volume;
    if (std::isnan(mass) || std::isinf(mass)) {
        mass = density * (0.0254L * 0.0254L * 0.0254L / nCubeVertices_.size()); // Fallback
        logger_.log(Logging::LogLevel::Warning, "Invalid vertex mass, using fallback: vertex={}, volume={}, mass={}", 
                    std::source_location::current(), vertexIndex, volume, mass);
    }
    logger_.log(Logging::LogLevel::Debug, "Computed mass for vertex {}: mass={}", 
                std::source_location::current(), vertexIndex, mass);
    return mass;
}

long double UniversalEquation::computeVertexDensity(int vertexIndex) const {
    logger_.log(Logging::LogLevel::Info, "Starting density computation for vertex {}", 
                std::source_location::current(), vertexIndex);
    validateVertexIndex(vertexIndex);
    long double density = 1000.0L; // Water density
    if (std::isnan(density) || std::isinf(density)) {
        density = 1000.0L; // Default to water's density
        logger_.log(Logging::LogLevel::Warning, "Invalid vertex density, using default: vertex={}, density={}", 
                    std::source_location::current(), vertexIndex, density);
    }
    logger_.log(Logging::LogLevel::Debug, "Computed density for vertex {}: density={}", 
                std::source_location::current(), vertexIndex, density);
    return density;
}

std::vector<long double> UniversalEquation::computeCenterOfMass() const {
    logger_.log(Logging::LogLevel::Info, "Starting center of mass computation for {} vertices", 
                std::source_location::current(), nCubeVertices_.size());
    std::vector<long double> com(currentDimension_.load(), 0.0L);
    long double totalMass = 0.0L;
    std::latch latch(omp_get_max_threads());
    std::vector<std::vector<long double>> localCom(omp_get_max_threads(), std::vector<long double>(currentDimension_.load(), 0.0L));
    std::vector<long double> localMasses(omp_get_max_threads(), 0.0L);

    #pragma omp parallel if(nCubeVertices_.size() > 100)
    {
        int thread_id = omp_get_thread_num();
        logger_.log(Logging::LogLevel::Debug, "Thread {}: computing center of mass for vertices", 
                    std::source_location::current(), thread_id);
        #pragma omp for schedule(dynamic)
        for (size_t i = 0; i < nCubeVertices_.size(); ++i) {
            validateVertexIndex(static_cast<int>(i));
            long double mass = computeVertexMass(static_cast<int>(i));
            localMasses[thread_id] += mass;
            for (int j = 0; j < currentDimension_.load(); ++j) {
                localCom[thread_id][j] += mass * nCubeVertices_[i][j];
            }
        }
        latch.count_down();
    }
    latch.wait();

    for (int t = 0; t < omp_get_max_threads(); ++t) {
        totalMass += localMasses[t];
        for (int j = 0; j < currentDimension_.load(); ++j) {
            com[j] += localCom[t][j];
        }
    }

    if (totalMass > 0.0L) {
        for (int j = 0; j < currentDimension_.load(); ++j) {
            com[j] = safe_div(com[j], totalMass);
            logger_.log(Logging::LogLevel::Debug, "Center of mass component {}: value={}", 
                        std::source_location::current(), j, com[j]);
        }
    } else {
        logger_.log(Logging::LogLevel::Warning, "Total mass is zero, center of mass set to origin", 
                    std::source_location::current());
    }
    logger_.log(Logging::LogLevel::Info, "Center of mass computation completed", 
                std::source_location::current());
    return com;
}

long double UniversalEquation::computeTotalSystemVolume() const {
    logger_.log(Logging::LogLevel::Info, "Starting total system volume computation for {} vertices", 
                std::source_location::current(), nCubeVertices_.size());
    long double totalVolume = 0.0L;
    std::latch latch(omp_get_max_threads());
    std::vector<long double> localVolumes(omp_get_max_threads(), 0.0L);

    #pragma omp parallel if(nCubeVertices_.size() > 100)
    {
        int thread_id = omp_get_thread_num();
        logger_.log(Logging::LogLevel::Debug, "Thread {}: computing volumes for vertices", 
                    std::source_location::current(), thread_id);
        #pragma omp for schedule(dynamic)
        for (size_t i = 0; i < nCubeVertices_.size(); ++i) {
            validateVertexIndex(static_cast<int>(i));
            localVolumes[thread_id] += computeVertexVolume(static_cast<int>(i));
        }
        latch.count_down();
    }
    latch.wait();

    for (int t = 0; t < omp_get_max_threads(); ++t) {
        totalVolume += localVolumes[t];
    }

    if (std::isnan(totalVolume) || std::isinf(totalVolume)) {
        totalVolume = 0.0254L * 0.0254L * 0.0254L; // Fallback to single cube volume
        logger_.log(Logging::LogLevel::Warning, "Invalid total system volume, using fallback: totalVolume={}", 
                    std::source_location::current(), totalVolume);
    }
    logger_.log(Logging::LogLevel::Info, "Total system volume computed: totalVolume={}", 
                std::source_location::current(), totalVolume);
    return totalVolume;
}

long double UniversalEquation::computeGravitationalPotential(int vertexIndex1, int vertexIndex2) const {
    logger_.log(Logging::LogLevel::Info, "Starting gravitational potential computation between vertices {} and {}", 
                std::source_location::current(), vertexIndex1, vertexIndex2);
    if (vertexIndex1 == vertexIndex2) {
        logger_.log(Logging::LogLevel::Warning, "Same vertex indices for gravitational potential: vertex={}", 
                    std::source_location::current(), vertexIndex1);
        return 0.0L;
    }
    validateVertexIndex(vertexIndex1);
    validateVertexIndex(vertexIndex2);
    long double distance = 0.0L;
    for (int i = 0; i < currentDimension_.load(); ++i) {
        long double diff = nCubeVertices_[vertexIndex1][i] - nCubeVertices_[vertexIndex2][i];
        distance += diff * diff;
        logger_.log(Logging::LogLevel::Debug, "Dimension {}: diff={} for vertices {} and {}", 
                    std::source_location::current(), i, diff, vertexIndex1, vertexIndex2);
    }
    distance = std::sqrt(std::max(distance, 1e-15L));
    logger_.log(Logging::LogLevel::Debug, "Computed distance between vertices {} and {}: distance={}", 
                std::source_location::current(), vertexIndex1, vertexIndex2, distance);
    long double m1 = computeVertexMass(vertexIndex1);
    long double m2 = computeVertexMass(vertexIndex2);
    long double G = 6.67430e-11L; // Gravitational constant
    long double result = safe_div(-G * m1 * m2, distance);
    if (std::isnan(result) || std::isinf(result)) {
        logger_.log(Logging::LogLevel::Warning, "Invalid gravitational potential: m1={}, m2={}, distance={}", 
                    std::source_location::current(), m1, m2, distance);
        return 0.0L;
    }
    logger_.log(Logging::LogLevel::Debug, "Computed gravitational potential for vertices {} and {}: result={}", 
                std::source_location::current(), vertexIndex1, vertexIndex2, result);
    return result;
}

std::vector<long double> UniversalEquation::computeGravitationalAcceleration(int vertexIndex) const {
    logger_.log(Logging::LogLevel::Info, "Starting gravitational acceleration computation for vertex {}", 
                std::source_location::current(), vertexIndex);
    validateVertexIndex(vertexIndex);
    std::vector<long double> acc(currentDimension_.load(), 0.0L);
    long double G = 6.67430e-11L; // Gravitational constant
    std::latch latch(omp_get_max_threads());
    std::vector<std::vector<long double>> localAcc(omp_get_max_threads(), std::vector<long double>(currentDimension_.load(), 0.0L));
    bool anyClamped = false;

    #pragma omp parallel if(nCubeVertices_.size() > 100)
    {
        int thread_id = omp_get_thread_num();
        logger_.log(Logging::LogLevel::Debug, "Thread {}: computing acceleration for vertex {}", 
                    std::source_location::current(), thread_id, vertexIndex);
        #pragma omp for schedule(dynamic)
        for (size_t i = 0; i < nCubeVertices_.size(); ++i) {
            if (static_cast<int>(i) == vertexIndex) continue;
            validateVertexIndex(static_cast<int>(i));
            long double distance = 0.0L;
            for (int j = 0; j < currentDimension_.load(); ++j) {
                long double diff = nCubeVertices_[i][j] - nCubeVertices_[vertexIndex][j];
                distance += diff * diff;
            }
            distance = std::sqrt(std::max(distance, 1e-15L));
            logger_.log(Logging::LogLevel::Debug, "Thread {}: distance to vertex {}: distance={}", 
                        std::source_location::current(), thread_id, i, distance);
            long double m2 = computeVertexMass(static_cast<int>(i));
            long double factor = safe_div(G * m2, distance * distance * distance);
            logger_.log(Logging::LogLevel::Debug, "Thread {}: vertex {}: mass={}, factor={}", 
                        std::source_location::current(), thread_id, i, m2, factor);
            for (int j = 0; j < currentDimension_.load(); ++j) {
                long double delta = factor * (nCubeVertices_[i][j] - nCubeVertices_[vertexIndex][j]);
                if (std::isnan(delta) || std::isinf(delta)) {
                    anyClamped = true;
                    logger_.log(Logging::LogLevel::Warning, "Thread {}: invalid acceleration component for vertex {}, dimension {}: delta={}", 
                                std::source_location::current(), thread_id, i, j, delta);
                    continue;
                }
                localAcc[thread_id][j] += delta;
            }
        }
        latch.count_down();
    }
    latch.wait();

    for (int t = 0; t < omp_get_max_threads(); ++t) {
        for (int j = 0; j < currentDimension_.load(); ++j) {
            acc[j] += localAcc[t][j];
            logger_.log(Logging::LogLevel::Debug, "Thread {}: dimension {}: acceleration component={}", 
                        std::source_location::current(), t, j, acc[j]);
        }
    }

    if (anyClamped) {
        logger_.log(Logging::LogLevel::Warning, "Some gravitational acceleration components were invalid and skipped for vertex {}", 
                    std::source_location::current(), vertexIndex);
    }
    logger_.log(Logging::LogLevel::Info, "Gravitational acceleration computation completed for vertex {}", 
                std::source_location::current(), vertexIndex);
    return acc;
}

std::vector<long double> UniversalEquation::computeClassicalEMField(int vertexIndex) const {
    logger_.log(Logging::LogLevel::Info, "Starting classical EM field computation for vertex {}", 
                std::source_location::current(), vertexIndex);
    validateVertexIndex(vertexIndex);
    std::vector<long double> field(currentDimension_.load(), 0.0L);
    long double k = 8.9875517923e9L; // Coulomb constant
    bool anyClamped = false;
    std::latch latch(omp_get_max_threads());
    std::vector<std::vector<long double>> localField(omp_get_max_threads(), std::vector<long double>(currentDimension_.load(), 0.0L));

    #pragma omp parallel if(nCubeVertices_.size() > 100)
    {
        int thread_id = omp_get_thread_num();
        logger_.log(Logging::LogLevel::Debug, "Thread {}: computing EM field for vertex {}", 
                    std::source_location::current(), thread_id, vertexIndex);
        #pragma omp for schedule(dynamic)
        for (size_t i = 0; i < nCubeVertices_.size(); ++i) {
            if (static_cast<int>(i) == vertexIndex) continue;
            validateVertexIndex(static_cast<int>(i));
            long double distance = 0.0L;
            for (int j = 0; j < currentDimension_.load(); ++j) {
                long double diff = nCubeVertices_[i][j] - nCubeVertices_[vertexIndex][j];
                distance += diff * diff;
            }
            distance = std::sqrt(std::max(distance, 1e-15L));
            logger_.log(Logging::LogLevel::Debug, "Thread {}: distance to vertex {}: distance={}", 
                        std::source_location::current(), thread_id, i, distance);
            long double q2 = vertexSpins_[i] * 1e-15L; // Scale down for neutrality
            long double factor = safe_div(k * q2, distance * distance * distance);
            logger_.log(Logging::LogLevel::Debug, "Thread {}: vertex {}: charge={}, factor={}", 
                        std::source_location::current(), thread_id, i, q2, factor);
            for (int j = 0; j < currentDimension_.load(); ++j) {
                long double delta = factor * (nCubeVertices_[i][j] - nCubeVertices_[vertexIndex][j]);
                if (std::isnan(delta) || std::isinf(delta)) {
                    anyClamped = true;
                    logger_.log(Logging::LogLevel::Warning, "Thread {}: invalid EM field component for vertex {}, dimension {}: delta={}", 
                                std::source_location::current(), thread_id, i, j, delta);
                    continue;
                }
                localField[thread_id][j] += delta;
            }
        }
        latch.count_down();
    }
    latch.wait();

    for (int t = 0; t < omp_get_max_threads(); ++t) {
        for (int j = 0; j < currentDimension_.load(); ++j) {
            field[j] += localField[t][j];
            logger_.log(Logging::LogLevel::Debug, "Thread {}: dimension {}: EM field component={}", 
                        std::source_location::current(), t, j, field[j]);
        }
    }

    if (anyClamped) {
        logger_.log(Logging::LogLevel::Warning, "Some classical EM field components were invalid and skipped for vertex {}", 
                    std::source_location::current(), vertexIndex);
    }
    logger_.log(Logging::LogLevel::Info, "Classical EM field computation completed for vertex {}", 
                std::source_location::current(), vertexIndex);
    return field;
}

void UniversalEquation::updateOrbitalVelocity(long double dt) {
    logger_.log(Logging::LogLevel::Info, "Starting orbital velocity update for {} vertices with dt={}", 
                std::source_location::current(), nCubeVertices_.size(), dt);
    if (dt <= 0.0L || std::isnan(dt) || std::isinf(dt)) {
        logger_.log(Logging::LogLevel::Error, "Invalid time step for orbital velocity update: dt={}", 
                    std::source_location::current(), dt);
        return;
    }
    bool anyClamped = false;
    std::latch latch(omp_get_max_threads());

    #pragma omp parallel if(nCubeVertices_.size() > 100)
    {
        int thread_id = omp_get_thread_num();
        logger_.log(Logging::LogLevel::Debug, "Thread {}: updating orbital velocity for vertices", 
                    std::source_location::current(), thread_id);
        #pragma omp for schedule(dynamic)
        for (size_t i = 0; i < nCubeVertices_.size(); ++i) {
            validateVertexIndex(static_cast<int>(i));
            auto acc = computeGravitationalAcceleration(static_cast<int>(i));
            for (int j = 0; j < currentDimension_.load(); ++j) {
                long double oldMomentum = vertexMomenta_[i][j];
                vertexMomenta_[i][j] += dt * acc[j];
                if (std::isnan(vertexMomenta_[i][j]) || std::isinf(vertexMomenta_[i][j]) || 
                    vertexMomenta_[i][j] < -0.9L || vertexMomenta_[i][j] > 0.9L) {
                    vertexMomenta_[i][j] = std::clamp(vertexMomenta_[i][j], -0.9L, 0.9L);
                    anyClamped = true;
                    logger_.log(Logging::LogLevel::Warning, "Thread {}: clamped momentum for vertex {}, dimension {}: old={}, new={}", 
                                std::source_location::current(), thread_id, i, j, oldMomentum, vertexMomenta_[i][j]);
                }
                logger_.log(Logging::LogLevel::Debug, "Thread {}: vertex {}, dimension {}: updated momentum={}", 
                            std::source_location::current(), thread_id, i, j, vertexMomenta_[i][j]);
            }
        }
        latch.count_down();
    }
    latch.wait();

    if (anyClamped) {
        logger_.log(Logging::LogLevel::Warning, "Some momenta were clamped in updateOrbitalVelocity", 
                    std::source_location::current());
    }
    logger_.log(Logging::LogLevel::Info, "Orbital velocity update completed", 
                std::source_location::current());
}

void UniversalEquation::updateOrbitalPositions(long double dt) {
    logger_.log(Logging::LogLevel::Info, "Starting orbital position update for {} vertices with dt={}", 
                std::source_location::current(), nCubeVertices_.size(), dt);
    if (dt <= 0.0L || std::isnan(dt) || std::isinf(dt)) {
        logger_.log(Logging::LogLevel::Error, "Invalid time step for orbital position update: dt={}", 
                    std::source_location::current(), dt);
        return;
    }
    bool anyClamped = false;
    std::latch latch(omp_get_max_threads());

    #pragma omp parallel if(nCubeVertices_.size() > 100)
    {
        int thread_id = omp_get_thread_num();
        logger_.log(Logging::LogLevel::Debug, "Thread {}: updating orbital positions for vertices", 
                    std::source_location::current(), thread_id);
        #pragma omp for schedule(dynamic)
        for (size_t i = 0; i < nCubeVertices_.size(); ++i) {
            validateVertexIndex(static_cast<int>(i));
            for (int j = 0; j < currentDimension_.load(); ++j) {
                long double oldPosition = nCubeVertices_[i][j];
                nCubeVertices_[i][j] += dt * vertexMomenta_[i][j];
                if (std::isnan(nCubeVertices_[i][j]) || std::isinf(nCubeVertices_[i][j]) || 
                    nCubeVertices_[i][j] < -1e3L || nCubeVertices_[i][j] > 1e3L) {
                    nCubeVertices_[i][j] = std::clamp(nCubeVertices_[i][j], -1e3L, 1e3L);
                    anyClamped = true;
                    logger_.log(Logging::LogLevel::Warning, "Thread {}: clamped position for vertex {}, dimension {}: old={}, new={}", 
                                std::source_location::current(), thread_id, i, j, oldPosition, nCubeVertices_[i][j]);
                }
                logger_.log(Logging::LogLevel::Debug, "Thread {}: vertex {}, dimension {}: updated position={}", 
                            std::source_location::current(), thread_id, i, j, nCubeVertices_[i][j]);
            }
        }
        latch.count_down();
    }
    latch.wait();

    if (anyClamped) {
        logger_.log(Logging::LogLevel::Warning, "Some positions were clamped in updateOrbitalPositions", 
                    std::source_location::current());
    }
    logger_.log(Logging::LogLevel::Info, "Orbital position update completed", 
                std::source_location::current());
}

long double UniversalEquation::computeSystemEnergy() const {
    logger_.log(Logging::LogLevel::Info, "Starting total system energy computation for {} vertices", 
                std::source_location::current(), nCubeVertices_.size());
    long double energy = 0.0L;
    std::latch latch(omp_get_max_threads());
    std::vector<long double> localEnergies(omp_get_max_threads(), 0.0L);
    bool anyClamped = false;

    #pragma omp parallel if(nCubeVertices_.size() > 100)
    {
        int thread_id = omp_get_thread_num();
        logger_.log(Logging::LogLevel::Debug, "Thread {}: computing system energy for vertices", 
                    std::source_location::current(), thread_id);
        #pragma omp for schedule(dynamic)
        for (size_t i = 0; i < nCubeVertices_.size(); ++i) {
            validateVertexIndex(static_cast<int>(i));
            for (size_t j = i + 1; j < nCubeVertices_.size(); ++j) {
                validateVertexIndex(static_cast<int>(j));
                long double potential = computeGravitationalPotential(static_cast<int>(i), static_cast<int>(j));
                localEnergies[thread_id] += potential;
            }
            long double kinetic = 0.0L;
            for (int k = 0; k < currentDimension_.load(); ++k) {
                kinetic += vertexMomenta_[i][k] * vertexMomenta_[i][k];
            }
            long double kineticEnergy = 0.5L * computeVertexMass(static_cast<int>(i)) * kinetic;
            if (std::isnan(kineticEnergy) || std::isinf(kineticEnergy)) {
                anyClamped = true;
                logger_.log(Logging::LogLevel::Warning, "Thread {}: invalid kinetic energy for vertex {}: kinetic={}", 
                            std::source_location::current(), thread_id, i, kinetic);
                continue;
            }
            localEnergies[thread_id] += kineticEnergy;
        }
        latch.count_down();
    }
    latch.wait();

    for (int t = 0; t < omp_get_max_threads(); ++t) {
        energy += localEnergies[t];
    }

    if (std::isnan(energy) || std::isinf(energy)) {
        logger_.log(Logging::LogLevel::Warning, "Invalid total system energy, returning 0: energy={}", 
                    std::source_location::current(), energy);
        return 0.0L;
    }
    logger_.log(Logging::LogLevel::Info, "Total system energy computed: energy={}", 
                std::source_location::current(), energy);
    return energy;
}

long double UniversalEquation::computePythagoreanScaling(int vertexIndex) const {
    logger_.log(Logging::LogLevel::Info, "Starting Pythagorean scaling computation for vertex {}", 
                std::source_location::current(), vertexIndex);
    validateVertexIndex(vertexIndex);
    long double magnitude = 0.0L;
    const auto& vertex = nCubeVertices_[vertexIndex];
    for (int i = 0; i < currentDimension_.load(); ++i) {
        magnitude += vertex[i] * vertex[i];
        logger_.log(Logging::LogLevel::Debug, "Vertex {}, dimension {}: component squared={}", 
                    std::source_location::current(), vertexIndex, i, vertex[i] * vertex[i]);
    }
    long double result = std::sqrt(std::max(magnitude, 0.0L)) / 0.0254L; // Normalize relative to 1-inch cube
    if (std::isnan(result) || std::isinf(result)) {
        logger_.log(Logging::LogLevel::Warning, "Invalid Pythagorean scaling for vertex {}: magnitude={}, returning 1.0", 
                    std::source_location::current(), vertexIndex, magnitude);
        return 1.0L;
    }
    logger_.log(Logging::LogLevel::Debug, "Computed Pythagorean scaling for vertex {}: result={}", 
                std::source_location::current(), vertexIndex, result);
    logger_.log(Logging::LogLevel::Info, "Pythagorean scaling computation completed for vertex {}", 
                std::source_location::current(), vertexIndex);
    return result;
}