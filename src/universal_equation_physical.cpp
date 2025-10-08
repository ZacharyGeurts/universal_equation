// universal_equation_physical.cpp: Implements classical physics methods for UniversalEquation.
// Models a system of vertices representing particles in a 1-inch cube of water (density 1000 kg/m^3).
// Computes mass, volume, and density dynamically for multiple vertices, accounting for quantum effects via influence and NURBS.
// Uses Logging::Logger for consistent, thread-safe logging across the AMOURANTH RTX Engine.
// Thread-safety note: Uses std::latch for parallel updates and std::atomic for scalar members to avoid mutexes.
// Copyright Zachary Geurts 2025 (powered by Grok with Science B*! precision)

#include "ue_init.hpp"
#include "engine/core.hpp"
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <algorithm>
#include <latch>
#include "engine/logging.hpp"

// Note: The following functions have been removed to avoid multiple definition errors:
// - computeVertexVolume
// - computeVertexMass
// - computeVertexDensity
// - computeCenterOfMass
// - computeTotalSystemVolume
// - computeGravitationalPotential
// - computeGravitationalAcceleration
// - computeClassicalEMField
// - updateOrbitalVelocity
// - updateOrbitalPositions
// - computeSystemEnergy
// - computePythagoreanScaling
// These are assumed to be defined in universal_equation_quantum.cpp or elsewhere.

// Remaining non-conflicting functions:

long double UniversalEquation::computeCircleArea(long double radius) const {
    logger_.log(Logging::LogLevel::Info, "Starting circle area computation: radius={}",
                std::source_location::current(), radius);
    if (radius < 0.0L || std::isnan(radius) || std::isinf(radius)) {
        logger_.log(Logging::LogLevel::Warning, "Invalid radius for circle area: radius={}",
                    std::source_location::current(), radius);
        return 0.0L;
    }
    long double area = std::numbers::pi_v<long double> * radius * radius;
    logger_.log(Logging::LogLevel::Debug, "Computed circle area: radius={}, area={}",
                std::source_location::current(), radius, area);
    return area;
}

long double UniversalEquation::computeSphereVolume(long double radius) const {
    logger_.log(Logging::LogLevel::Info, "Starting sphere volume computation: radius={}",
                std::source_location::current(), radius);
    if (radius < 0.0L || std::isnan(radius) || std::isinf(radius)) {
        logger_.log(Logging::LogLevel::Warning, "Invalid radius for sphere volume: radius={}",
                    std::source_location::current(), radius);
        return 0.0L;
    }
    long double volume = (4.0L / 3.0L) * std::numbers::pi_v<long double> * radius * radius * radius;
    logger_.log(Logging::LogLevel::Debug, "Computed sphere volume: radius={}, volume={}",
                std::source_location::current(), radius, volume);
    return volume;
}

long double UniversalEquation::computeKineticEnergy(int vertexIndex) const {
    logger_.log(Logging::LogLevel::Info, "Starting kinetic energy computation for vertex {}",
                std::source_location::current(), vertexIndex);
    validateVertexIndex(vertexIndex);
    long double kinetic = 0.0L;
    for (int k = 0; k < currentDimension_.load(); ++k) {
        kinetic += vertexMomenta_[vertexIndex][k] * vertexMomenta_[vertexIndex][k];
    }
    long double mass = computeVertexMass(vertexIndex);
    long double kineticEnergy = 0.5L * mass * kinetic;
    if (std::isnan(kineticEnergy) || std::isinf(kineticEnergy)) {
        logger_.log(Logging::LogLevel::Warning, "Invalid kinetic energy for vertex {}: kinetic={}, mass={}",
                    std::source_location::current(), vertexIndex, kinetic, mass);
        return 0.0L;
    }
    logger_.log(Logging::LogLevel::Debug, "Computed kinetic energy for vertex {}: energy={}",
                std::source_location::current(), vertexIndex, kineticEnergy);
    return kineticEnergy;
}

long double UniversalEquation::computeGravitationalForceMagnitude(int vertexIndex1, int vertexIndex2) const {
    logger_.log(Logging::LogLevel::Info, "Starting gravitational force magnitude computation between vertices {} and {}",
                std::source_location::current(), vertexIndex1, vertexIndex2);
    if (vertexIndex1 == vertexIndex2) {
        logger_.log(Logging::LogLevel::Warning, "Same vertex indices for gravitational force: vertex={}",
                    std::source_location::current(), vertexIndex1);
        return 0.0L;
    }
    validateVertexIndex(vertexIndex1);
    validateVertexIndex(vertexIndex2);
    long double distance = 0.0L;
    for (int i = 0; i < currentDimension_.load(); ++i) {
        long double diff = nCubeVertices_[vertexIndex1][i] - nCubeVertices_[vertexIndex2][i];
        distance += diff * diff;
    }
    distance = std::sqrt(std::max(distance, 1e-15L));
    long double m1 = computeVertexMass(vertexIndex1);
    long double m2 = computeVertexMass(vertexIndex2);
    long double G = 6.67430e-11L; // Gravitational constant
    long double force = safe_div(G * m1 * m2, distance * distance);
    if (std::isnan(force) || std::isinf(force)) {
        logger_.log(Logging::LogLevel::Warning, "Invalid gravitational force magnitude: m1={}, m2={}, distance={}",
                    std::source_location::current(), m1, m2, distance);
        return 0.0L;
    }
    logger_.log(Logging::LogLevel::Debug, "Computed gravitational force magnitude for vertices {} and {}: force={}",
                std::source_location::current(), vertexIndex1, vertexIndex2, force);
    return force;
}

long double UniversalEquation::computeCoulombForceMagnitude(int vertexIndex1, int vertexIndex2) const {
    logger_.log(Logging::LogLevel::Info, "Starting Coulomb force magnitude computation between vertices {} and {}",
                std::source_location::current(), vertexIndex1, vertexIndex2);
    if (vertexIndex1 == vertexIndex2) {
        logger_.log(Logging::LogLevel::Warning, "Same vertex indices for Coulomb force: vertex={}",
                    std::source_location::current(), vertexIndex1);
        return 0.0L;
    }
    validateVertexIndex(vertexIndex1);
    validateVertexIndex(vertexIndex2);
    long double distance = 0.0L;
    for (int i = 0; i < currentDimension_.load(); ++i) {
        long double diff = nCubeVertices_[vertexIndex1][i] - nCubeVertices_[vertexIndex2][i];
        distance += diff * diff;
    }
    distance = std::sqrt(std::max(distance, 1e-15L));
    long double q1 = vertexSpins_[vertexIndex1] * 1e-15L; // Scale down for neutrality
    long double q2 = vertexSpins_[vertexIndex2] * 1e-15L;
    long double k = 8.9875517923e9L; // Coulomb constant
    long double force = safe_div(k * q1 * q2, distance * distance);
    if (std::isnan(force) || std::isinf(force)) {
        logger_.log(Logging::LogLevel::Warning, "Invalid Coulomb force magnitude: q1={}, q2={}, distance={}",
                    std::source_location::current(), q1, q2, distance);
        return 0.0L;
    }
    logger_.log(Logging::LogLevel::Debug, "Computed Coulomb force magnitude for vertices {} and {}: force={}",
                std::source_location::current(), vertexIndex1, vertexIndex2, force);
    return force;
}

long double UniversalEquation::computePressure(int vertexIndex) const {
    logger_.log(Logging::LogLevel::Info, "Starting pressure computation for vertex {}",
                std::source_location::current(), vertexIndex);
    validateVertexIndex(vertexIndex);
    long double volume = computeVertexVolume(vertexIndex);
    long double forceSum = 0.0L;
    for (size_t i = 0; i < nCubeVertices_.size(); ++i) {
        if (static_cast<int>(i) == vertexIndex) continue;
        forceSum += computeGravitationalForceMagnitude(vertexIndex, static_cast<int>(i));
    }
    long double pressure = safe_div(forceSum, computeCircleArea(std::pow(volume, 1.0L/3.0L)));
    if (std::isnan(pressure) || std::isinf(pressure)) {
        logger_.log(Logging::LogLevel::Warning, "Invalid pressure for vertex {}: forceSum={}, volume={}",
                    std::source_location::current(), vertexIndex, forceSum, volume);
        return 0.0L;
    }
    logger_.log(Logging::LogLevel::Debug, "Computed pressure for vertex {}: pressure={}",
                std::source_location::current(), vertexIndex, pressure);
    return pressure;
}

long double UniversalEquation::computeBuoyantForce(int vertexIndex) const {
    logger_.log(Logging::LogLevel::Info, "Starting buoyant force computation for vertex {}",
                std::source_location::current(), vertexIndex);
    validateVertexIndex(vertexIndex);
    long double density = materialDensity_.load();
    long double volume = computeVertexVolume(vertexIndex);
    long double g = 9.81L; // Gravitational acceleration on Earth
    long double buoyantForce = density * volume * g;
    if (std::isnan(buoyantForce) || std::isinf(buoyantForce)) {
        logger_.log(Logging::LogLevel::Warning, "Invalid buoyant force for vertex {}: density={}, volume={}",
                    std::source_location::current(), vertexIndex, density, volume);
        return 0.0L;
    }
    logger_.log(Logging::LogLevel::Debug, "Computed buoyant force for vertex {}: force={}",
                std::source_location::current(), vertexIndex, buoyantForce);
    return buoyantForce;
}

long double UniversalEquation::computeCentripetalAcceleration(int vertexIndex, long double radius) const {
    logger_.log(Logging::LogLevel::Info, "Starting centripetal acceleration computation for vertex {} with radius={}",
                std::source_location::current(), vertexIndex, radius);
    validateVertexIndex(vertexIndex);
    if (radius <= 0.0L || std::isnan(radius) || std::isinf(radius)) {
        logger_.log(Logging::LogLevel::Warning, "Invalid radius for centripetal acceleration: vertex={}, radius={}",
                    std::source_location::current(), vertexIndex, radius);
        return 0.0L;
    }
    long double speed = 0.0L;
    for (int i = 0; i < currentDimension_.load(); ++i) {
        speed += vertexMomenta_[vertexIndex][i] * vertexMomenta_[vertexIndex][i];
    }
    speed = std::sqrt(std::max(speed, 0.0L));
    long double acceleration = safe_div(speed * speed, radius);
    if (std::isnan(acceleration) || std::isinf(acceleration)) {
        logger_.log(Logging::LogLevel::Warning, "Invalid centripetal acceleration for vertex {}: speed={}, radius={}",
                    std::source_location::current(), vertexIndex, speed, radius);
        return 0.0L;
    }
    logger_.log(Logging::LogLevel::Debug, "Computed centripetal acceleration for vertex {}: acceleration={}",
                std::source_location::current(), vertexIndex, acceleration);
    return acceleration;
}

bool UniversalEquation::computeSphereCollision(int vertexIndex1, int vertexIndex2) const {
    logger_.log(Logging::LogLevel::Info, "Starting sphere collision computation between vertices {} and {}",
                std::source_location::current(), vertexIndex1, vertexIndex2);
    if (vertexIndex1 == vertexIndex2) {
        logger_.log(Logging::LogLevel::Warning, "Same vertex indices for collision: vertex={}",
                    std::source_location::current(), vertexIndex1);
        return false;
    }
    validateVertexIndex(vertexIndex1);
    validateVertexIndex(vertexIndex2);
    long double radius1 = std::pow(computeVertexVolume(vertexIndex1), 1.0L/3.0L);
    long double radius2 = std::pow(computeVertexVolume(vertexIndex2), 1.0L/3.0L);
    long double distance = 0.0L;
    for (int i = 0; i < currentDimension_.load(); ++i) {
        long double diff = nCubeVertices_[vertexIndex1][i] - nCubeVertices_[vertexIndex2][i];
        distance += diff * diff;
    }
    distance = std::sqrt(std::max(distance, 1e-15L));
    bool collision = distance < (radius1 + radius2);
    logger_.log(Logging::LogLevel::Debug, "Computed sphere collision for vertices {} and {}: distance={}, radius1={}, radius2={}, collision={}",
                std::source_location::current(), vertexIndex1, vertexIndex2, distance, radius1, radius2, collision);
    return collision;
}

void UniversalEquation::resolveSphereCollision(int vertexIndex1, int vertexIndex2) {
    logger_.log(Logging::LogLevel::Info, "Starting sphere collision resolution between vertices {} and {}",
                std::source_location::current(), vertexIndex1, vertexIndex2);
    if (vertexIndex1 == vertexIndex2) {
        logger_.log(Logging::LogLevel::Warning, "Same vertex indices for collision resolution: vertex={}",
                    std::source_location::current(), vertexIndex1);
        return;
    }
    validateVertexIndex(vertexIndex1);
    validateVertexIndex(vertexIndex2);
    long double m1 = computeVertexMass(vertexIndex1);
    long double m2 = computeVertexMass(vertexIndex2);
    std::vector<long double> v1 = vertexMomenta_[vertexIndex1];
    std::vector<long double> v2 = vertexMomenta_[vertexIndex2];
    long double distance = 0.0L;
    for (int i = 0; i < currentDimension_.load(); ++i) {
        long double diff = nCubeVertices_[vertexIndex1][i] - nCubeVertices_[vertexIndex2][i];
        distance += diff * diff;
    }
    distance = std::sqrt(std::max(distance, 1e-15L));
    std::vector<long double> normal(currentDimension_.load(), 0.0L);
    for (int i = 0; i < currentDimension_.load(); ++i) {
        normal[i] = safe_div(nCubeVertices_[vertexIndex2][i] - nCubeVertices_[vertexIndex1][i], distance);
    }
    long double relativeVelocity = 0.0L;
    for (int i = 0; i < currentDimension_.load(); ++i) {
        relativeVelocity += (v1[i] - v2[i]) * normal[i];
    }
    long double impulse = safe_div(2.0L * relativeVelocity, m1 + m2);
    for (int i = 0; i < currentDimension_.load(); ++i) {
        vertexMomenta_[vertexIndex1][i] -= impulse * m2 * normal[i];
        vertexMomenta_[vertexIndex2][i] += impulse * m1 * normal[i];
        if (std::isnan(vertexMomenta_[vertexIndex1][i]) || std::isinf(vertexMomenta_[vertexIndex1][i]) ||
            std::isnan(vertexMomenta_[vertexIndex2][i]) || std::isinf(vertexMomenta_[vertexIndex2][i])) {
            logger_.log(Logging::LogLevel::Warning, "Invalid momentum after collision resolution: vertex1={}, vertex2={}, dimension={}",
                        std::source_location::current(), vertexIndex1, vertexIndex2, i);
            vertexMomenta_[vertexIndex1][i] = 0.0L;
            vertexMomenta_[vertexIndex2][i] = 0.0L;
        }
    }
    logger_.log(Logging::LogLevel::Debug, "Resolved sphere collision for vertices {} and {}",
                std::source_location::current(), vertexIndex1, vertexIndex2);
}

bool UniversalEquation::computeAABBCollision2D(int vertexIndex1, int vertexIndex2) const {
    logger_.log(Logging::LogLevel::Info, "Starting AABB collision computation between vertices {} and {}",
                std::source_location::current(), vertexIndex1, vertexIndex2);
    if (vertexIndex1 == vertexIndex2) {
        logger_.log(Logging::LogLevel::Warning, "Same vertex indices for AABB collision: vertex={}",
                    std::source_location::current(), vertexIndex1);
        return false;
    }
    validateVertexIndex(vertexIndex1);
    validateVertexIndex(vertexIndex2);
    long double side1 = std::pow(computeVertexVolume(vertexIndex1), 1.0L/3.0L) * 0.5L;
    long double side2 = std::pow(computeVertexVolume(vertexIndex2), 1.0L/3.0L) * 0.5L;
    bool collision = true;
    for (int i = 0; i < std::min(2, currentDimension_.load()); ++i) {
        long double min1 = nCubeVertices_[vertexIndex1][i] - side1;
        long double max1 = nCubeVertices_[vertexIndex1][i] + side1;
        long double min2 = nCubeVertices_[vertexIndex2][i] - side2;
        long double max2 = nCubeVertices_[vertexIndex2][i] + side2;
        if (max1 < min2 || max2 < min1) {
            collision = false;
            break;
        }
    }
    logger_.log(Logging::LogLevel::Debug, "Computed AABB collision for vertices {} and {}: collision={}",
                std::source_location::current(), vertexIndex1, vertexIndex2, collision);
    return collision;
}

long double UniversalEquation::computeEffectiveMass(int vertexIndex1, int vertexIndex2) const {
    logger_.log(Logging::LogLevel::Info, "Starting effective mass computation for vertices {} and {}",
                std::source_location::current(), vertexIndex1, vertexIndex2);
    if (vertexIndex1 == vertexIndex2) {
        logger_.log(Logging::LogLevel::Warning, "Same vertex indices for effective mass: vertex={}",
                    std::source_location::current(), vertexIndex1);
        return 0.0L;
    }
    validateVertexIndex(vertexIndex1);
    validateVertexIndex(vertexIndex2);
    long double m1 = computeVertexMass(vertexIndex1);
    long double m2 = computeVertexMass(vertexIndex2);
    long double effectiveMass = safe_div(m1 * m2, m1 + m2);
    if (std::isnan(effectiveMass) || std::isinf(effectiveMass)) {
        logger_.log(Logging::LogLevel::Warning, "Invalid effective mass: m1={}, m2={}",
                    std::source_location::current(), m1, m2);
        return 0.0L;
    }
    logger_.log(Logging::LogLevel::Debug, "Computed effective mass for vertices {} and {}: effectiveMass={}",
                std::source_location::current(), vertexIndex1, vertexIndex2, effectiveMass);
    return effectiveMass;
}

std::vector<long double> UniversalEquation::computeTorque(int vertexIndex, const std::vector<long double>& pivot) const {
    logger_.log(Logging::LogLevel::Info, "Starting torque computation for vertex {} with pivot",
                std::source_location::current(), vertexIndex);
    validateVertexIndex(vertexIndex);
    if (static_cast<int>(pivot.size()) != currentDimension_.load()) {
        logger_.log(Logging::LogLevel::Error, "Invalid pivot size: pivot.size={}, currentDimension={}",
                    std::source_location::current(), pivot.size(), currentDimension_.load());
        throw std::invalid_argument("Invalid pivot size");
    }
    std::vector<long double> torque(currentDimension_.load(), 0.0L);
    long double mass = computeVertexMass(vertexIndex);
    std::vector<long double> r(currentDimension_.load());
    for (int i = 0; i < currentDimension_.load(); ++i) {
        r[i] = nCubeVertices_[vertexIndex][i] - pivot[i];
    }
    auto force = computeGravitationalAcceleration(vertexIndex);
    for (int i = 0; i < currentDimension_.load(); ++i) {
        torque[i] = mass * r[i] * force[i];
        if (std::isnan(torque[i]) || std::isinf(torque[i])) {
            logger_.log(Logging::LogLevel::Warning, "Invalid torque component for vertex {}, dimension {}: torque={}",
                        std::source_location::current(), vertexIndex, i, torque[i]);
            torque[i] = 0.0L;
        }
    }
    logger_.log(Logging::LogLevel::Debug, "Computed torque for vertex {}: torque size={}",
                std::source_location::current(), vertexIndex, torque.size());
    return torque;
}

std::vector<long double> UniversalEquation::computeDragForce(int vertexIndex) const {
    logger_.log(Logging::LogLevel::Info, "Starting drag force computation for vertex {}",
                std::source_location::current(), vertexIndex);
    validateVertexIndex(vertexIndex);
    long double density = materialDensity_.load();
    long double velocityMagnitude = 0.0L;
    for (int i = 0; i < currentDimension_.load(); ++i) {
        velocityMagnitude += vertexMomenta_[vertexIndex][i] * vertexMomenta_[vertexIndex][i];
    }
    velocityMagnitude = std::sqrt(std::max(velocityMagnitude, 0.0L));
    long double area = computeCircleArea(std::pow(computeVertexVolume(vertexIndex), 1.0L/3.0L));
    long double Cd = 0.47L; // Drag coefficient for sphere
    std::vector<long double> dragForce(currentDimension_.load(), 0.0L);
    long double factor = 0.5L * density * velocityMagnitude * velocityMagnitude * Cd * area;
    for (int i = 0; i < currentDimension_.load(); ++i) {
        dragForce[i] = -safe_div(factor * vertexMomenta_[vertexIndex][i], velocityMagnitude + 1e-15L);
        if (std::isnan(dragForce[i]) || std::isinf(dragForce[i])) {
            logger_.log(Logging::LogLevel::Warning, "Invalid drag force component for vertex {}, dimension {}: dragForce={}",
                        std::source_location::current(), vertexIndex, i, dragForce[i]);
            dragForce[i] = 0.0L;
        }
    }
    logger_.log(Logging::LogLevel::Debug, "Computed drag force for vertex {}: dragForce size={}",
                std::source_location::current(), vertexIndex, dragForce.size());
    return dragForce;
}

std::vector<long double> UniversalEquation::computeSpringForce(int vertexIndex1, int vertexIndex2, long double springConstant, long double restLength) const {
    logger_.log(Logging::LogLevel::Info, "Starting spring force computation between vertices {} and {} with k={}, L0={}",
                std::source_location::current(), vertexIndex1, vertexIndex2, springConstant, restLength);
    if (vertexIndex1 == vertexIndex2) {
        logger_.log(Logging::LogLevel::Warning, "Same vertex indices for spring force: vertex={}",
                    std::source_location::current(), vertexIndex1);
        return std::vector<long double>(currentDimension_.load(), 0.0L);
    }
    validateVertexIndex(vertexIndex1);
    validateVertexIndex(vertexIndex2);
    if (springConstant < 0.0L || restLength < 0.0L || std::isnan(springConstant) || std::isinf(springConstant) ||
        std::isnan(restLength) || std::isinf(restLength)) {
        logger_.log(Logging::LogLevel::Warning, "Invalid spring parameters: k={}, L0={}",
                    std::source_location::current(), springConstant, restLength);
        return std::vector<long double>(currentDimension_.load(), 0.0L);
    }
    long double distance = 0.0L;
    for (int i = 0; i < currentDimension_.load(); ++i) {
        long double diff = nCubeVertices_[vertexIndex1][i] - nCubeVertices_[vertexIndex2][i];
        distance += diff * diff;
    }
    distance = std::sqrt(std::max(distance, 1e-15L));
    long double displacement = distance - restLength;
    long double forceMagnitude = -springConstant * displacement;
    std::vector<long double> springForce(currentDimension_.load(), 0.0L);
    for (int i = 0; i < currentDimension_.load(); ++i) {
        springForce[i] = safe_div(forceMagnitude * (nCubeVertices_[vertexIndex2][i] - nCubeVertices_[vertexIndex1][i]), distance);
        if (std::isnan(springForce[i]) || std::isinf(springForce[i])) {
            logger_.log(Logging::LogLevel::Warning, "Invalid spring force component for vertex {}, dimension {}: springForce={}",
                        std::source_location::current(), vertexIndex1, i, springForce[i]);
            springForce[i] = 0.0L;
        }
    }
    logger_.log(Logging::LogLevel::Debug, "Computed spring force for vertices {} and {}: springForce size={}",
                std::source_location::current(), vertexIndex1, vertexIndex2, springForce.size());
    return springForce;
}