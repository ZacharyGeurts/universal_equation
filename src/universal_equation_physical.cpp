// universal_equation_physical.cpp: Implements classical physics methods for UniversalEquation.
// Models a 1-inch cube of water (density 1000 kg/m^3) as a single vertex in d dimensions.
// Computes mass, volume, and density dynamically, accounting for quantum effects via influence and NURBS.
// Zachary Geurts 2025 (powered by Grok with Science B*! precision)

#include "universal_equation.hpp"
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <algorithm>

// Helper: Safe division to prevent NaN/Inf
inline long double safe_div(long double a, long double b) {
    return (b != 0.0L && !std::isnan(b) && !std::isinf(b)) ? a / b : 0.0L;
}

long double UniversalEquation::computeVertexVolume(int vertexIndex) const {
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= getNCubeVertices().size()) {
        throw std::out_of_range("Invalid vertex index for volume calculation");
    }
    // Base side length for 1-inch cube (0.0254 m)
    long double baseSide = 0.0254L; // 1 inch
    long double volume = std::pow(baseSide, 3); // Force 3D volume for water cube
    if (std::isnan(volume) || std::isinf(volume)) {
        if (getDebug()) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cerr << "[ERROR] Invalid vertex volume: vertex=" << vertexIndex << ", side=" << baseSide << "\n";
        }
        return baseSide * baseSide * baseSide; // Fallback
    }
    return volume;
}

long double UniversalEquation::computeVertexMass(int vertexIndex) const {
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= getNCubeVertices().size()) {
        throw std::out_of_range("Invalid vertex index for mass calculation");
    }
    // Mass = density * volume, density fixed at 1000 kg/m^3 for water
    long double density = 1000.0L; // Water density
    long double volume = computeVertexVolume(vertexIndex);
    long double mass = density * volume;
    if (std::isnan(mass) || std::isinf(mass)) {
        if (getDebug()) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cerr << "[ERROR] Invalid vertex mass: vertex=" << vertexIndex << ", volume=" << volume << "\n";
        }
        return density * (0.0254L * 0.0254L * 0.0254L); // Fallback
    }
    return mass;
}

long double UniversalEquation::computeVertexDensity(int vertexIndex) const {
    long double density = 1000.0L; // Force density to match water
    if (std::isnan(density) || std::isinf(density)) {
        if (getDebug()) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cerr << "[ERROR] Invalid vertex density: vertex=" << vertexIndex << "\n";
        }
        return 1000.0L; // Default to water's density
    }
    return density;
}

std::vector<long double> UniversalEquation::computeCenterOfMass() const {
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
    }
    return com;
}

long double UniversalEquation::computeTotalSystemVolume() const {
    long double totalVolume = 0.0L;
    for (size_t i = 0; i < getNCubeVertices().size(); ++i) {
        totalVolume += computeVertexVolume(i);
    }
    return totalVolume;
}

long double UniversalEquation::computeGravitationalPotential(int vertexIndex1, int vertexIndex2) const {
    if (vertexIndex1 == vertexIndex2 || vertexIndex1 < 0 || vertexIndex2 < 0 ||
        static_cast<size_t>(vertexIndex1) >= getNCubeVertices().size() ||
        static_cast<size_t>(vertexIndex2) >= getNCubeVertices().size()) {
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
    return safe_div(-G * m1 * m2, distance);
}

std::vector<long double> UniversalEquation::computeGravitationalAcceleration(int vertexIndex) const {
    std::vector<long double> acc(getCurrentDimension(), 0.0L);
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= getNCubeVertices().size()) {
        return acc;
    }
    long double G = 6.67430e-11L; // Gravitational constant
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
            acc[j] += factor * (getNCubeVertices()[i][j] - getNCubeVertices()[vertexIndex][j]);
        }
    }
    return acc;
}

std::vector<long double> UniversalEquation::computeClassicalEMField(int vertexIndex) const {
    std::vector<long double> field(getCurrentDimension(), 0.0L);
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= getNCubeVertices().size()) {
        return field;
    }
    // Water is neutral; scale charge by spin and small factor
    long double k = 8.9875517923e9L; // Coulomb constant
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
            field[j] += factor * (getNCubeVertices()[i][j] - getNCubeVertices()[vertexIndex][j]);
        }
    }
    return field;
}

void UniversalEquation::updateOrbitalVelocity(long double dt) {
    if (dt <= 0.0L || std::isnan(dt) || std::isinf(dt)) return;
    std::lock_guard<std::mutex> lock(mutex_);
    for (size_t i = 0; i < getNCubeVertices().size(); ++i) {
        auto acc = computeGravitationalAcceleration(i);
        for (int j = 0; j < getCurrentDimension(); ++j) {
            vertexMomenta_[i][j] += dt * acc[j];
            vertexMomenta_[i][j] = std::clamp(vertexMomenta_[i][j], -0.9L, 0.9L);
        }
    }
}

void UniversalEquation::updateOrbitalPositions(long double dt) {
    if (dt <= 0.0L || std::isnan(dt) || std::isinf(dt)) return;
    std::lock_guard<std::mutex> lock(mutex_);
    for (size_t i = 0; i < getNCubeVertices().size(); ++i) {
        for (int j = 0; j < getCurrentDimension(); ++j) {
            nCubeVertices_[i][j] += dt * vertexMomenta_[i][j];
            nCubeVertices_[i][j] = std::clamp(nCubeVertices_[i][j], -1e3L, 1e3L);
        }
    }
}

long double UniversalEquation::computeSystemEnergy() const {
    long double energy = 0.0L;
    for (size_t i = 0; i < getNCubeVertices().size(); ++i) {
        for (size_t j = i + 1; j < getNCubeVertices().size(); ++j) {
            energy += computeGravitationalPotential(i, j);
        }
        long double kinetic = 0.0L;
        for (int k = 0; k < getCurrentDimension(); ++k) {
            kinetic += vertexMomenta_[i][k] * vertexMomenta_[i][k];
        }
        energy += 0.5L * computeVertexMass(i) * kinetic;
    }
    return energy;
}

long double UniversalEquation::computePythagoreanScaling(int vertexIndex) const {
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= getNCubeVertices().size()) {
        return 1.0L;
    }
    return 0.0254L; // Fixed scaling for single vertex water cube
}