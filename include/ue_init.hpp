#ifndef UE_INIT_HPP
#define UE_INIT_HPP

#include <vector>
#include <cmath>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <glm/glm.hpp>
#include <random>

// used by AMOURANTH RTX September 2025
// Optimized for visualization of 30,000 balls in dimension 8
// Zachary Geurts 2025

struct DimensionData {
    int dimension;
    double observable;
    double potential;
    double darkMatter;
    double darkEnergy;
};

struct EnergyResult {
    double observable = 0.0;
    double potential = 0.0;
    double darkMatter = 0.0;
    double darkEnergy = 0.0;
};

struct DimensionInteraction {
    int dimension;
    double strength;
    double phase;
    DimensionInteraction(int dim, double str, double ph)
        : dimension(dim), strength(str), phase(ph) {}
};

struct Ball {
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 acceleration;
    float mass;
    float radius;
    float startTime;
    Ball(const glm::vec3& pos, const glm::vec3& vel, float m, float r, float start)
        : position(pos), velocity(vel), acceleration(0.0f), mass(m), radius(r), startTime(start) {}
};

class AMOURANTH;

class Xorshift {
public:
    Xorshift(uint32_t seed) : state_(seed) {}
    float nextFloat(float min, float max) {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return min + (max - min) * (state_ & 0x7FFFFFFF) / static_cast<float>(0x7FFFFFFF);
    }
private:
    uint32_t state_;
};

class UniversalEquation {
public:
    UniversalEquation(
        int maxDimensions = 8,
        int mode = 8,
        double influence = 2.5,
        double alpha = 0.0072973525693,
        bool debug = false
    ) : maxDimensions_(maxDimensions),
        currentDimension_(std::clamp(mode, 1, maxDimensions)),
        mode_(std::clamp(mode, 1, maxDimensions)),
        influence_(std::clamp(influence, 0.0, 10.0)),
        alpha_(std::clamp(alpha, 0.1, 10.0)),
        debug_(debug),
        wavePhase_(0.0),
        simulationTime_(0.0),
        needsUpdate_(true) {
        initializeNCube();
    }

    void setCurrentDimension(int dimension) {
        currentDimension_ = std::clamp(dimension, 1, maxDimensions_);
        needsUpdate_ = true;
    }
    void setInfluence(double influence) {
        influence_ = std::clamp(influence, 0.0, 10.0);
        needsUpdate_ = true;
    }
    void setAlpha(double alpha) {
        alpha_ = std::clamp(alpha, 0.1, 10.0);
        needsUpdate_ = true;
    }
    void setDebug(bool debug) { debug_ = debug; }
    void setMode(int mode) {
        mode_ = std::clamp(mode, 1, maxDimensions_);
        needsUpdate_ = true;
    }

    int getCurrentDimension() const { return currentDimension_.load(); }
    double getInfluence() const { return influence_.load(); }
    double getAlpha() const { return alpha_.load(); }
    bool getDebug() const { return debug_.load(); }
    int getMode() const { return mode_.load(); }
    int getMaxDimensions() const { return maxDimensions_; }
    double getWavePhase() const { return wavePhase_; }
    float getSimulationTime() const { return simulationTime_; }
    const std::vector<DimensionInteraction>& getInteractions() const {
        if (needsUpdate_) {
            updateInteractions();
            needsUpdate_ = false;
        }
        return interactions_;
    }
    const std::vector<std::vector<double>>& getNCubeVertices() const {
        return nCubeVertices_;
    }
    const std::vector<glm::vec3>& getProjectedVertices() const {
        return projectedVerts_;
    }
    double getAvgProjScale() const { return avgProjScale_; }
    const std::vector<Ball>& getBalls() const { return balls_; }
    std::mutex& getPhysicsMutex() const { return physicsMutex_; }

    void updateProjectedVertices(const std::vector<glm::vec3>& newVerts) {
        projectedVerts_ = newVerts;
    }

    void advanceCycle() {
        wavePhase_ += 0.1;
        needsUpdate_ = true;
    }

    EnergyResult compute() const {
        EnergyResult result;
        result.observable = influence_ * std::cos(wavePhase_);
        result.potential = influence_ * std::sin(wavePhase_);
        result.darkMatter = influence_ * 0.27;
        result.darkEnergy = influence_ * 0.68;
        return result;
    }

    void initializeCalculator(AMOURANTH* amouranth) {
        if (!amouranth) return;
        navigator_ = amouranth;
        initializeWithRetry();
    }

    DimensionData updateCache() {
        DimensionData data;
        data.dimension = currentDimension_;
        auto result = compute();
        data.observable = result.observable;
        data.potential = result.potential;
        data.darkMatter = result.darkMatter;
        data.darkEnergy = result.darkEnergy;
        return data;
    }

    std::vector<DimensionData> computeBatch(int startDim = 1, int endDim = -1) {
        if (endDim == -1) endDim = maxDimensions_;
        startDim = std::clamp(startDim, 1, maxDimensions_);
        endDim = std::clamp(endDim, startDim, maxDimensions_);
        std::vector<DimensionData> results;
        results.reserve(endDim - startDim + 1);
        for (int dim = startDim; dim <= endDim; ++dim) {
            setCurrentDimension(dim);
            results.push_back(updateCache());
        }
        return results;
    }

    // Reintroduced methods for AMOURANTH compatibility
    double computeInteraction(int vertexIndex, double distance) const {
        return influence_ * std::cos(wavePhase_ + vertexIndex * 0.1) / (distance + 1e-6);
    }
    double computePermeation(int vertexIndex) const {
        return influence_ * std::sin(wavePhase_ + vertexIndex * 0.1);
    }
    double computeDarkEnergy(double distance) const {
        return influence_ * 0.68 / (distance + 1e-6);
    }

    void initializeBalls(float baseMass = 1.2f, float baseRadius = 0.12f, size_t numBalls = 30000) {
        balls_.clear();
        simulationTime_ = 0.0f;
        balls_.reserve(numBalls);
        auto result = compute();
        float massScale = static_cast<float>(result.darkMatter);
        Xorshift rng(12345);
        for (size_t i = 0; i < numBalls; ++i) {
            glm::vec3 pos(rng.nextFloat(-5.0f, 5.0f), rng.nextFloat(-5.0f, 5.0f), rng.nextFloat(-2.0f, 2.0f));
            glm::vec3 vel(rng.nextFloat(-1.0f, 1.0f), rng.nextFloat(-1.0f, 1.0f), rng.nextFloat(-1.0f, 1.0f));
            float startTime = i * 0.1f;
            balls_.emplace_back(pos, vel, baseMass * massScale, baseRadius, startTime);
        }
    }

    void updateBalls(float deltaTime) {
        simulationTime_ += deltaTime;
        auto& balls = balls_;
        auto interactions = getInteractions();
        auto result = compute();

        // Update accelerations in parallel
#pragma omp parallel for
        for (size_t i = 0; i < balls.size(); ++i) {
            if (simulationTime_ < balls[i].startTime) continue;
            glm::vec3 force(0.0f);
            double interactionStrength = (i < interactions.size()) ? interactions[i].strength : 0.0;
            force += glm::vec3(
                static_cast<float>(result.observable),
                static_cast<float>(result.potential),
                static_cast<float>(result.darkEnergy)
            ) * static_cast<float>(interactionStrength);
            balls[i].acceleration = force / balls[i].mass;
        }

        // Boundary collisions (moved from renderMode8)
        const glm::vec3 boundsMin(-5.0f, -5.0f, -2.0f);
        const glm::vec3 boundsMax(5.0f, 5.0f, 2.0f);
#pragma omp parallel for
        for (size_t i = 0; i < balls.size(); ++i) {
            if (simulationTime_ < balls[i].startTime) continue;
            auto& pos = balls[i].position;
            auto& vel = balls[i].velocity;
            if (pos.x < boundsMin.x) { pos.x = boundsMin.x; vel.x = -vel.x; }
            if (pos.x > boundsMax.x) { pos.x = boundsMax.x; vel.x = -vel.x; }
            if (pos.y < boundsMin.y) { pos.y = boundsMin.y; vel.y = -vel.y; }
            if (pos.y > boundsMax.y) { pos.y = boundsMax.y; vel.y = -vel.y; }
            if (pos.z < boundsMin.z) { pos.z = boundsMin.z; vel.z = -vel.z; }
            if (pos.z > boundsMax.z) { pos.z = boundsMax.z; vel.z = -vel.z; }
        }

        // Grid-based collision detection
        const int gridSize = 10;
        const float cellSize = 10.0f / gridSize;
        std::vector<std::vector<size_t>> grid(gridSize * gridSize * gridSize);
        for (size_t i = 0; i < balls.size(); ++i) {
            if (simulationTime_ < balls[i].startTime) continue;
            glm::vec3 pos = balls[i].position;
            int x = static_cast<int>((pos.x + 5.0f) / cellSize);
            int y = static_cast<int>((pos.y + 5.0f) / cellSize);
            int z = static_cast<int>((pos.z + 2.0f) / cellSize * 0.5f);
            x = std::clamp(x, 0, gridSize - 1);
            y = std::clamp(y, 0, gridSize - 1);
            z = std::clamp(z, 0, gridSize - 1);
            int cellIdx = z * gridSize * gridSize + y * gridSize + x;
            grid[cellIdx].push_back(i);
        }

        // Resolve collisions in neighboring cells
#pragma omp parallel for
        for (size_t i = 0; i < balls.size(); ++i) {
            if (simulationTime_ < balls[i].startTime) continue;
            glm::vec3 pos = balls[i].position;
            int x = static_cast<int>((pos.x + 5.0f) / cellSize);
            int y = static_cast<int>((pos.y + 5.0f) / cellSize);
            int z = static_cast<int>((pos.z + 2.0f) / cellSize * 0.5f);
            x = std::clamp(x, 0, gridSize - 1);
            y = std::clamp(y, 0, gridSize - 1);
            z = std::clamp(z, 0, gridSize - 1);

            for (int dz = -1; dz <= 1; ++dz) {
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        int nx = x + dx, ny = y + dy, nz = z + dz;
                        if (nx < 0 || nx >= gridSize || ny < 0 || ny >= gridSize || nz < 0 || nz >= gridSize) continue;
                        int cellIdx = nz * gridSize * gridSize + ny * gridSize + nx;
                        for (size_t j : grid[cellIdx]) {
                            if (j <= i || simulationTime_ < balls[j].startTime) continue;
                            glm::vec3 delta = balls[j].position - balls[i].position;
                            float distance = glm::length(delta);
                            float minDistance = balls[i].radius + balls[j].radius;
                            if (distance < minDistance && distance > 0.0f) {
                                glm::vec3 normal = delta / distance;
                                glm::vec3 relVelocity = balls[j].velocity - balls[i].velocity;
                                float impulse = -2.0f * glm::dot(relVelocity, normal) / (1.0f / balls[i].mass + 1.0f / balls[j].mass);
#pragma omp critical
                                {
                                    balls[i].velocity += (impulse / balls[i].mass) * normal;
                                    balls[j].velocity -= (impulse / balls[j].mass) * normal;
                                    float overlap = minDistance - distance;
                                    balls[i].position -= normal * (overlap * 0.5f);
                                    balls[j].position += normal * (overlap * 0.5f);
                                }
                            }
                        }
                    }
                }
            }
        }

        // Update velocities and positions in parallel
#pragma omp parallel for
        for (size_t i = 0; i < balls.size(); ++i) {
            if (simulationTime_ < balls[i].startTime) continue;
            balls[i].velocity += balls[i].acceleration * deltaTime;
            balls[i].position += balls[i].velocity * deltaTime;
        }
    }

private:
    int maxDimensions_;
    std::atomic<int> currentDimension_;
    std::atomic<int> mode_;
    std::atomic<double> influence_;
    std::atomic<double> alpha_;
    std::atomic<bool> debug_;
    double wavePhase_;
    float simulationTime_;
    mutable std::vector<DimensionInteraction> interactions_;
    std::vector<std::vector<double>> nCubeVertices_;
    mutable std::vector<glm::vec3> projectedVerts_;
    mutable double avgProjScale_;
    mutable std::mutex physicsMutex_;
    mutable std::vector<Ball> balls_;
    mutable std::atomic<bool> needsUpdate_;
    AMOURANTH* navigator_;

    void initializeNCube() {
        nCubeVertices_.clear();
        uint64_t maxVertices = 1ULL << maxDimensions_;
        nCubeVertices_.reserve(maxVertices);
        for (uint64_t i = 0; i < maxVertices; ++i) {
            std::vector<double> vertex(maxDimensions_);
            for (int d = 0; d < maxDimensions_; ++d) {
                vertex[d] = ((i >> d) & 1) ? 1.0 : -1.0;
            }
            nCubeVertices_.push_back(vertex);
        }
        projectedVerts_.resize(maxVertices, glm::vec3(0.0f));
        avgProjScale_ = 1.0;
    }

    void updateInteractions() const {
        interactions_.clear();
        interactions_.reserve(nCubeVertices_.size());
        for (size_t i = 0; i < nCubeVertices_.size(); ++i) {
            double distance = 0.0;
            for (const auto& v : nCubeVertices_[i]) {
                distance += v * v;
            }
            distance = std::sqrt(distance);
            double strength = influence_ * std::cos(wavePhase_ + i * 0.1) / (distance + 1e-6);
            interactions_.emplace_back(currentDimension_, strength, wavePhase_ + i * 0.1);
        }
    }

    void initializeWithRetry() {
        initializeNCube();
        updateInteractions();
    }
};

#endif // UE_INIT_HPP