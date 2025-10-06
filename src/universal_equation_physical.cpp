// universal_equation_physical.cpp: Implements classical physics methods for UniversalEquation.
// Models a system of vertices representing particles in a 1-inch cube of water (density 1000 kg/m^3).
// Computes mass, volume, and density dynamically for multiple vertices, accounting for quantum effects via influence and NURBS.

// Copyright Zachary Geurts 2025 (powered by Grok with Science B*! precision)

#include "universal_equation.hpp"
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <algorithm>
#include <syncstream>
#include <iostream>

// ANSI color codes
#define RESET "\033[0m"
#define MAGENTA "\033[1;35m" // Bold magenta for errors
#define CYAN "\033[1;36m"    // Bold cyan for debug
#define YELLOW "\033[1;33m"  // Bold yellow for warnings
#define GREEN "\033[1;32m"   // Bold green for info

// Helper: Safe division to prevent NaN/Inf
inline long double safe_div(long double a, long double b) {
    return (b != 0.0L && !std::isnan(b) && !std::isinf(b)) ? a / b : 0.0L;
}

long double UniversalEquation::computeVertexVolume(int vertexIndex) const {
    if (debug_.load()) {
        std::osyncstream(std::cout) << GREEN << "[INFO] Computing volume for vertex " << vertexIndex << RESET << std::endl;
    }
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= getNCubeVertices().size()) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Invalid vertex index for volume calculation: vertex=" << vertexIndex
                                    << ", vertices size=" << getNCubeVertices().size() << RESET << std::endl;
        throw std::out_of_range("Invalid vertex index for volume calculation");
    }
    // Base side length for 1-inch cube (0.0254 m), scaled for multiple vertices
    long double baseSide = 0.0254L / std::pow(static_cast<long double>(getNCubeVertices().size()), 1.0L/3.0L);
    long double volume = std::pow(baseSide, 3); // 3D volume per vertex
    if (std::isnan(volume) || std::isinf(volume)) {
        if (debug_.load()) {
            std::osyncstream(std::cerr) << YELLOW << "[WARNING] Invalid vertex volume: vertex=" << vertexIndex << ", side=" << baseSide << RESET << std::endl;
        }
        return 0.0254L * 0.0254L * 0.0254L / getNCubeVertices().size(); // Fallback
    }
    return volume;
}

long double UniversalEquation::computeVertexMass(int vertexIndex) const {
    if (debug_.load()) {
        std::osyncstream(std::cout) << GREEN << "[INFO] Computing mass for vertex " << vertexIndex << RESET << std::endl;
    }
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= getNCubeVertices().size()) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Invalid vertex index for mass calculation: vertex=" << vertexIndex
                                    << ", vertices size=" << getNCubeVertices().size() << RESET << std::endl;
        throw std::out_of_range("Invalid vertex index for mass calculation");
    }
    long double density = 1000.0L; // Water density (kg/m^3)
    long double volume = computeVertexVolume(vertexIndex);
    long double mass = density * volume;
    if (std::isnan(mass) || std::isinf(mass)) {
        if (debug_.load()) {
            std::osyncstream(std::cerr) << YELLOW << "[WARNING] Invalid vertex mass: vertex=" << vertexIndex << ", volume=" << volume << RESET << std::endl;
        }
        return density * (0.0254L * 0.0254L * 0.0254L / getNCubeVertices().size()); // Fallback
    }
    return mass;
}

long double UniversalEquation::computeVertexDensity(int vertexIndex) const {
    if (debug_.load()) {
        std::osyncstream(std::cout) << GREEN << "[INFO] Computing density for vertex " << vertexIndex << RESET << std::endl;
    }
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= getNCubeVertices().size()) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Invalid vertex index for density calculation: vertex=" << vertexIndex
                                    << ", vertices size=" << getNCubeVertices().size() << RESET << std::endl;
        throw std::out_of_range("Invalid vertex index for density calculation");
    }
    long double density = 1000.0L; // Water density
    if (std::isnan(density) || std::isinf(density)) {
        if (debug_.load()) {
            std::osyncstream(std::cerr) << YELLOW << "[WARNING] Invalid vertex density: vertex=" << vertexIndex << RESET << std::endl;
        }
        return 1000.0L; // Default to water's density
    }
    return density;
}

std::vector<long double> UniversalEquation::computeCenterOfMass() const {
    if (debug_.load()) {
        std::osyncstream(std::cout) << GREEN << "[INFO] Computing center of mass for " << getNCubeVertices().size() << " vertices" RESET << std::endl;
    }
    std::vector<long double> com(getCurrentDimension(), 0.0L);
    long double totalMass = 0.0L;
    for (size_t i = 0; i < getNCubeVertices().size(); ++i) {
        long double mass = computeVertexMass(i);
        totalMass += mass;
        for (int j = 0; j < getCurrentDimension(); ++j) {
            com[j] += mass * getNCubeVertices()[i][j];
        }
    }
    if (totalMass > 0.0L) {
        for (int j = 0; j < getCurrentDimension(); ++j) {
            com[j] = safe_div(com[j], totalMass);
        }
    } else if (debug_.load()) {
        std::osyncstream(std::cout) << YELLOW << "[WARNING] Total mass is zero, center of mass set to origin" RESET << std::endl;
    }
    return com;
}

long double UniversalEquation::computeTotalSystemVolume() const {
    if (debug_.load()) {
        std::osyncstream(std::cout) << GREEN << "[INFO] Computing total system volume for " << getNCubeVertices().size() << " vertices" RESET << std::endl;
    }
    long double totalVolume = 0.0L;
    for (size_t i = 0; i < getNCubeVertices().size(); ++i) {
        totalVolume += computeVertexVolume(i);
    }
    if (std::isnan(totalVolume) || std::isinf(totalVolume)) {
        if (debug_.load()) {
            std::osyncstream(std::cerr) << YELLOW << "[WARNING] Invalid total system volume: " << totalVolume << RESET << std::endl;
        }
        return 0.0254L * 0.0254L * 0.0254L; // Fallback to single cube volume
    }
    return totalVolume;
}

long double UniversalEquation::computeGravitationalPotential(int vertexIndex1, int vertexIndex2) const {
    if (debug_.load()) {
        std::osyncstream(std::cout) << GREEN << "[INFO] Computing gravitational potential between vertices " << vertexIndex1 << " and " << vertexIndex2 << RESET << std::endl;
    }
    if (vertexIndex1 == vertexIndex2 || vertexIndex1 < 0 || vertexIndex2 < 0 ||
        static_cast<size_t>(vertexIndex1) >= getNCubeVertices().size() ||
        static_cast<size_t>(vertexIndex2) >= getNCubeVertices().size()) {
        if (debug_.load() && vertexIndex1 == vertexIndex2) {
            std::osyncstream(std::cout) << YELLOW << "[WARNING] Same vertex indices for gravitational potential: " << vertexIndex1 << RESET << std::endl;
        }
        if (debug_.load() && (vertexIndex1 < 0 || vertexIndex2 < 0 || static_cast<size_t>(vertexIndex1) >= getNCubeVertices().size() ||
                              static_cast<size_t>(vertexIndex2) >= getNCubeVertices().size())) {
            std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Invalid vertex indices for gravitational potential: vertex1=" << vertexIndex1
                                        << ", vertex2=" << vertexIndex2 << ", vertices size=" << getNCubeVertices().size() << RESET << std::endl;
        }
        return 0.0L;
    }
    long double distance = 0.0L;
    for (int i = 0; i < getCurrentDimension(); ++i) {
        long double diff = getNCubeVertices()[vertexIndex1][i] - getNCubeVertices()[vertexIndex2][i];
        distance += diff * diff;
    }
    distance = std::sqrt(std::max(distance, 1e-15L));
    long double m1 = computeVertexMass(vertexIndex1);
    long double m2 = computeVertexMass(vertexIndex2);
    long double G = 6.67430e-11L; // Gravitational constant
    long double result = safe_div(-G * m1 * m2, distance);
    if (std::isnan(result) || std::isinf(result)) {
        if (debug_.load()) {
            std::osyncstream(std::cerr) << YELLOW << "[WARNING] Invalid gravitational potential: m1=" << m1 << ", m2=" << m2 << ", distance=" << distance << RESET << std::endl;
        }
        return 0.0L;
    }
    return result;
}

std::vector<long double> UniversalEquation::computeGravitationalAcceleration(int vertexIndex) const {
    if (debug_.load()) {
        std::osyncstream(std::cout) << GREEN << "[INFO] Computing gravitational acceleration for vertex " << vertexIndex << RESET << std::endl;
    }
    std::vector<long double> acc(getCurrentDimension(), 0.0L);
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= getNCubeVertices().size()) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Invalid vertex index for gravitational acceleration: vertex=" << vertexIndex
                                    << ", vertices size=" << getNCubeVertices().size() << RESET << std::endl;
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
        }
        distance = std::sqrt(std::max(distance, 1e-15L));
        long double m2 = computeVertexMass(i);
        long double factor = safe_div(G * m2, distance * distance * distance);
        for (int j = 0; j < getCurrentDimension(); ++j) {
            long double delta = factor * (getNCubeVertices()[i][j] - getNCubeVertices()[vertexIndex][j]);
            if (std::isnan(delta) || std::isinf(delta)) {
                anyClamped = true;
                continue;
            }
            acc[j] += delta;
        }
    }
    if (debug_.load() && anyClamped) {
        std::osyncstream(std::cout) << YELLOW << "[WARNING] Some gravitational acceleration components were invalid and skipped" RESET << std::endl;
    }
    return acc;
}

std::vector<long double> UniversalEquation::computeClassicalEMField(int vertexIndex) const {
    if (debug_.load()) {
        std::osyncstream(std::cout) << GREEN << "[INFO] Computing classical EM field for vertex " << vertexIndex << RESET << std::endl;
    }
    std::vector<long double> field(getCurrentDimension(), 0.0L);
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= getNCubeVertices().size()) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Invalid vertex index for classical EM field: vertex=" << vertexIndex
                                    << ", vertices size=" << getNCubeVertices().size() << RESET << std::endl;
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
        }
        distance = std::sqrt(std::max(distance, 1e-15L));
        long double q2 = getVertexSpins()[i] * 1e-15L; // Scale down for neutrality
        long double factor = safe_div(k * q2, distance * distance * distance);
        for (int j = 0; j < getCurrentDimension(); ++j) {
            long double delta = factor * (getNCubeVertices()[i][j] - getNCubeVertices()[vertexIndex][j]);
            if (std::isnan(delta) || std::isinf(delta)) {
                anyClamped = true;
                continue;
            }
            field[j] += delta;
        }
    }
    if (debug_.load() && anyClamped) {
        std::osyncstream(std::cout) << YELLOW << "[WARNING] Some classical EM field components were invalid and skipped" RESET << std::endl;
    }
    return field;
}

void UniversalEquation::updateOrbitalVelocity(long double dt) {
    if (dt <= 0.0L || std::isnan(dt) || std::isinf(dt)) {
        if (debug_.load()) {
            std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Invalid time step for orbital velocity update: dt=" << dt << RESET << std::endl;
        }
        return;
    }
    if (debug_.load()) {
        std::osyncstream(std::cout) << GREEN << "[INFO] Updating orbital velocities for " << getNCubeVertices().size() << " vertices with dt=" << dt << RESET << std::endl;
    }
    bool anyClamped = false;
    #pragma omp parallel for if(getNCubeVertices().size() > 100)
    for (size_t i = 0; i < getNCubeVertices().size(); ++i) {
        auto acc = computeGravitationalAcceleration(i);
        for (int j = 0; j < getCurrentDimension(); ++j) {
            vertexMomenta_[i][j] += dt * acc[j];
            if (vertexMomenta_[i][j] < -0.9L || vertexMomenta_[i][j] > 0.9L) {
                vertexMomenta_[i][j] = std::clamp(vertexMomenta_[i][j], -0.9L, 0.9L);
                #pragma omp critical
                anyClamped = true;
            }
        }
    }
    if (debug_.load() && anyClamped) {
        std::osyncstream(std::cout) << YELLOW << "[WARNING] Some momenta were clamped in updateOrbitalVelocity" RESET << std::endl;
    }
    if (debug_.load()) {
        std::osyncstream(std::cout) << GREEN << "[INFO] Orbital velocity update completed" RESET << std::endl;
    }
}

void UniversalEquation::updateOrbitalPositions(long double dt) {
    if (dt <= 0.0L || std::isnan(dt) || std::isinf(dt)) {
        if (debug_.load()) {
            std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Invalid time step for orbital position update: dt=" << dt << RESET << std::endl;
        }
        return;
    }
    if (debug_.load()) {
        std::osyncstream(std::cout) << GREEN << "[INFO] Updating orbital positions for " << getNCubeVertices().size() << " vertices with dt=" << dt << RESET << std::endl;
    }
    bool anyClamped = false;
    #pragma omp parallel for if(getNCubeVertices().size() > 100)
    for (size_t i = 0; i < getNCubeVertices().size(); ++i) {
        for (int j = 0; j < getCurrentDimension(); ++j) {
            nCubeVertices_[i][j] += dt * vertexMomenta_[i][j];
            if (nCubeVertices_[i][j] < -1e3L || nCubeVertices_[i][j] > 1e3L) {
                nCubeVertices_[i][j] = std::clamp(nCubeVertices_[i][j], -1e3L, 1e3L);
                #pragma omp critical
                anyClamped = true;
            }
        }
    }
    if (debug_.load() && anyClamped) {
        std::osyncstream(std::cout) << YELLOW << "[WARNING] Some positions were clamped in updateOrbitalPositions" RESET << std::endl;
    }
    if (debug_.load()) {
        std::osyncstream(std::cout) << GREEN << "[INFO] Orbital position update completed" RESET << std::endl;
    }
}

long double UniversalEquation::computeSystemEnergy() const {
    if (debug_.load()) {
        std::osyncstream(std::cout) << GREEN << "[INFO] Computing total system energy for " << getNCubeVertices().size() << " vertices" RESET << std::endl;
    }
    long double energy = 0.0L;
    for (size_t i = 0; i < getNCubeVertices().size(); ++i) {
        for (size_t j = i + 1; j < getNCubeVertices().size(); ++j) {
            energy += computeGravitationalPotential(i, j);
        }
        long double kinetic = 0.0L;
        for (int k = 0; k < getCurrentDimension(); ++k) {
            kinetic += vertexMomenta_[i][k] * vertexMomenta_[i][k];
        }
        long double kineticEnergy = 0.5L * computeVertexMass(i) * kinetic;
        if (std::isnan(kineticEnergy) || std::isinf(kineticEnergy)) {
            if (debug_.load()) {
                std::osyncstream(std::cerr) << YELLOW << "[WARNING] Invalid kinetic energy for vertex " << i << ": kinetic=" << kinetic << RESET << std::endl;
            }
            continue;
        }
        energy += kineticEnergy;
    }
    if (std::isnan(energy) || std::isinf(energy)) {
        if (debug_.load()) {
            std::osyncstream(std::cerr) << YELLOW << "[WARNING] Invalid total system energy: " << energy << RESET << std::endl;
        }
        return 0.0L;
    }
    if (debug_.load()) {
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Total system energy: " << energy << RESET << std::endl;
    }
    return energy;
}

long double UniversalEquation::computePythagoreanScaling(int vertexIndex) const {
    if (debug_.load()) {
        std::osyncstream(std::cout) << GREEN << "[INFO] Computing Pythagorean scaling for vertex " << vertexIndex << RESET << std::endl;
    }
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= getNCubeVertices().size()) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Invalid vertex index for Pythagorean scaling: vertex=" << vertexIndex
                                    << ", vertices size=" << getNCubeVertices().size() << RESET << std::endl;
        return 1.0L;
    }
    long double magnitude = 0.0L;
    const auto& vertex = getNCubeVertices()[vertexIndex];
    for (int i = 0; i < getCurrentDimension(); ++i) {
        magnitude += vertex[i] * vertex[i];
    }
    long double result = std::sqrt(std::max(magnitude, 0.0L)) / 0.0254L; // Normalize relative to 1-inch cube
    if (std::isnan(result) || std::isinf(result)) {
        if (debug_.load()) {
            std::osyncstream(std::cerr) << YELLOW << "[WARNING] Invalid Pythagorean scaling for vertex " << vertexIndex << ": magnitude=" << magnitude << RESET << std::endl;
        }
        return 1.0L;
    }
    return result;
}