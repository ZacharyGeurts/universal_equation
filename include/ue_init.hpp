#ifndef UE_INIT_HPP
#define UE_INIT_HPP

// AMOURANTH RTX Physics, October 2025
// Computes multidimensional physics for visualization of 30,000 balls in dimension 8.
// Thread-safe with std::atomic and OpenMP; uses C++20 features for parallelism.
// Dependencies: GLM, OpenMP, C++20 standard library.
// Usage: Instantiate with max dimensions, mode, influence, alpha, and debug flag.
// Zachary Geurts 2025

#include <vector>
#include <cmath>
#include <atomic>
#include <algorithm>
#include <glm/glm.hpp>
#include <random>
#include <format>
#include <iostream>
#include <syncstream>
#include <omp.h>

class AMOURANTH; // Forward declaration, assuming defined in core.hpp

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

// Custom formatters for std::atomic types
template <>
struct std::formatter<std::atomic<int>, char> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const std::atomic<int>& value, FormatContext& ctx) const {
        return std::format_to(ctx.out(), "{}", value.load());
    }
};

template <>
struct std::formatter<std::atomic<double>, char> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const std::atomic<double>& value, FormatContext& ctx) const {
        return std::format_to(ctx.out(), "{:.10f}", value.load());
    }
};

template <>
struct std::formatter<std::atomic<bool>, char> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const std::atomic<bool>& value, FormatContext& ctx) const {
        return std::format_to(ctx.out(), "{}", value.load());
    }
};

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
        needsUpdate_(true),
        navigator_(nullptr) {
        if (debug_) {
            std::osyncstream(std::cout) << std::format("[DEBUG] Constructing UniversalEquation: maxDimensions={}, mode={}, influence={}, alpha={}, debug={}\n",
                                                       maxDimensions_, mode_, influence_, alpha_, debug_);
        }
        initializeNCube();
        if (debug_) {
            std::osyncstream(std::cout) << std::format("[DEBUG] UniversalEquation constructed successfully\n");
        }
    }

    void setCurrentDimension(int dimension) {
        currentDimension_.store(std::clamp(dimension, 1, maxDimensions_));
        needsUpdate_.store(true);
    }
    void setInfluence(double influence) {
        influence_.store(std::clamp(influence, 0.0, 10.0));
        needsUpdate_.store(true);
    }
    void setAlpha(double alpha) {
        alpha_.store(std::clamp(alpha, 0.1, 10.0));
        needsUpdate_.store(true);
    }
    void setDebug(bool debug) { debug_.store(debug); }
    void setMode(int mode) {
        mode_.store(std::clamp(mode, 1, maxDimensions_));
        needsUpdate_.store(true);
    }

    int getCurrentDimension() const { return currentDimension_.load(); }
    double getInfluence() const { return influence_.load(); }
    double getAlpha() const { return alpha_.load(); }
    bool getDebug() const { return debug_.load(); }
    int getMode() const { return mode_.load(); }
    int getMaxDimensions() const { return maxDimensions_; }
    double getWavePhase() const { return wavePhase_.load(); }
    float getSimulationTime() const { return simulationTime_.load(); }
    const std::vector<DimensionInteraction>& getInteractions() const {
        if (needsUpdate_.load()) {
            updateInteractions();
            needsUpdate_.store(false);
        }
        return interactions_;
    }
    const std::vector<std::vector<double>>& getNCubeVertices() const { return nCubeVertices_; }
    const std::vector<glm::vec3>& getProjectedVertices() const { return projectedVerts_; }
    double getAvgProjScale() const { return avgProjScale_.load(); }
    const std::vector<Ball>& getBalls() const { return balls_; }

    void updateProjectedVertices(const std::vector<glm::vec3>& newVerts) {
        projectedVerts_ = newVerts;
    }

    void advanceCycle() {
        wavePhase_.fetch_add(0.1);
        needsUpdate_.store(true);
    }

    EnergyResult compute() const {
        EnergyResult result;
        result.observable = influence_.load() * std::cos(wavePhase_.load());
        result.potential = influence_.load() * std::sin(wavePhase_.load());
        result.darkMatter = influence_.load() * 0.27;
        result.darkEnergy = influence_.load() * 0.68;
        return result;
    }

    void initializeCalculator(AMOURANTH* amouranth) {
        if (!amouranth) return;
        navigator_ = amouranth;
        initializeWithRetry();
    }

    DimensionData updateCache() {
        DimensionData data;
        data.dimension = currentDimension_.load();
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

    double computeInteraction(int vertexIndex, double distance) const {
        return influence_.load() * std::cos(wavePhase_.load() + vertexIndex * 0.1) / (distance + 1e-6);
    }
    double computePermeation(int vertexIndex) const {
        return influence_.load() * std::sin(wavePhase_.load() + vertexIndex * 0.1);
    }
    double computeDarkEnergy(double distance) const {
        return influence_.load() * 0.68 / (distance + 1e-6);
    }

    void initializeBalls(float baseMass = 1.2f, float baseRadius = 0.12f, size_t numBalls = 30000) {
        balls_.clear();
        simulationTime_.store(0.0f);
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
        simulationTime_.fetch_add(deltaTime);
        auto interactions = getInteractions();
        auto result = compute();

        // Compute forces in parallel
        std::vector<glm::vec3> forces(balls_.size(), glm::vec3(0.0f));
#pragma omp parallel for
        for (size_t i = 0; i < balls_.size(); ++i) {
            if (simulationTime_.load() < balls_[i].startTime) continue;
            double interactionStrength = (i < interactions.size()) ? interactions[i].strength : 0.0;
            forces[i] = glm::vec3(
                static_cast<float>(result.observable),
                static_cast<float>(result.potential),
                static_cast<float>(result.darkEnergy)
            ) * static_cast<float>(interactionStrength);
            balls_[i].acceleration = forces[i] / balls_[i].mass;
        }

        // Apply boundary conditions in parallel
        const glm::vec3 boundsMin(-5.0f, -5.0f, -2.0f);
        const glm::vec3 boundsMax(5.0f, 5.0f, 2.0f);
#pragma omp parallel for
        for (size_t i = 0; i < balls_.size(); ++i) {
            if (simulationTime_.load() < balls_[i].startTime) continue;
            auto& pos = balls_[i].position;
            auto& vel = balls_[i].velocity;
            if (pos.x < boundsMin.x) { pos.x = boundsMin.x; vel.x = -vel.x; }
            if (pos.x > boundsMax.x) { pos.x = boundsMax.x; vel.x = -vel.x; }
            if (pos.y < boundsMin.y) { pos.y = boundsMin.y; vel.y = -vel.y; }
            if (pos.y > boundsMax.y) { pos.y = boundsMax.y; vel.y = -vel.y; }
            if (pos.z < boundsMin.z) { pos.z = boundsMin.z; vel.z = -vel.z; }
            if (pos.z > boundsMax.z) { pos.z = boundsMax.z; vel.z = -vel.z; }
        }

        // Spatial grid for collision detection
        const int gridSize = 10;
        const float cellSize = 10.0f / gridSize;
        std::vector<std::vector<size_t>> grid(gridSize * gridSize * gridSize);
        for (size_t i = 0; i < balls_.size(); ++i) {
            if (simulationTime_.load() < balls_[i].startTime) continue;
            glm::vec3 pos = balls_[i].position;
            int x = static_cast<int>((pos.x + 5.0f) / cellSize);
            int y = static_cast<int>((pos.y + 5.0f) / cellSize);
            int z = static_cast<int>((pos.z + 2.0f) / cellSize * 0.5f);
            x = std::clamp(x, 0, gridSize - 1);
            y = std::clamp(y, 0, gridSize - 1);
            z = std::clamp(z, 0, gridSize - 1);
            int cellIdx = z * gridSize * gridSize + y * gridSize + x;
            grid[cellIdx].push_back(i);
        }

        // Handle collisions in parallel with per-thread storage
        std::vector<std::vector<std::pair<size_t, size_t>>> threadCollisions(omp_get_max_threads());
#pragma omp parallel for
        for (size_t i = 0; i < balls_.size(); ++i) {
            if (simulationTime_.load() < balls_[i].startTime) continue;
            glm::vec3 pos = balls_[i].position;
            int x = static_cast<int>((pos.x + 5.0f) / cellSize);
            int y = static_cast<int>((pos.y + 5.0f) / cellSize);
            int z = static_cast<int>((pos.z + 2.0f) / cellSize * 0.5f);
            x = std::clamp(x, 0, gridSize - 1);
            y = std::clamp(y, 0, gridSize - 1);
            z = std::clamp(z, 0, gridSize - 1);

            int threadId = omp_get_thread_num();
            for (int dz = -1; dz <= 1; ++dz) {
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        int nx = x + dx, ny = y + dy, nz = z + dz;
                        if (nx < 0 || nx >= gridSize || ny < 0 || ny >= gridSize || nz < 0 || nz >= gridSize) continue;
                        int cellIdx = nz * gridSize * gridSize + ny * gridSize + nx;
                        for (size_t j : grid[cellIdx]) {
                            if (j <= i || simulationTime_.load() < balls_[j].startTime) continue;
                            glm::vec3 delta = balls_[j].position - balls_[i].position;
                            float distance = glm::length(delta);
                            float minDistance = balls_[i].radius + balls_[j].radius;
                            if (distance < minDistance && distance > 0.0f) {
                                threadCollisions[threadId].emplace_back(i, j);
                            }
                        }
                    }
                }
            }
        }

        // Process collisions sequentially to avoid race conditions
        for (const auto& collisions : threadCollisions) {
            for (const auto& [i, j] : collisions) {
                glm::vec3 delta = balls_[j].position - balls_[i].position;
                float distance = glm::length(delta);
                float minDistance = balls_[i].radius + balls_[j].radius;
                if (distance < minDistance && distance > 0.0f) {
                    glm::vec3 normal = delta / distance;
                    glm::vec3 relVelocity = balls_[j].velocity - balls_[i].velocity;
                    float impulse = -2.0f * glm::dot(relVelocity, normal) / (1.0f / balls_[i].mass + 1.0f / balls_[j].mass);
                    balls_[i].velocity += (impulse / balls_[i].mass) * normal;
                    balls_[j].velocity -= (impulse / balls_[j].mass) * normal;
                    float overlap = minDistance - distance;
                    balls_[i].position -= normal * (overlap * 0.5f);
                    balls_[j].position += normal * (overlap * 0.5f);
                }
            }
        }

        // Update positions in parallel
#pragma omp parallel for
        for (size_t i = 0; i < balls_.size(); ++i) {
            if (simulationTime_.load() < balls_[i].startTime) continue;
            balls_[i].velocity += balls_[i].acceleration * deltaTime;
            balls_[i].position += balls_[i].velocity * deltaTime;
        }
    }

private:
    int maxDimensions_;
    std::atomic<int> currentDimension_;
    std::atomic<int> mode_;
    std::atomic<double> influence_;
    std::atomic<double> alpha_;
    std::atomic<bool> debug_;
    std::atomic<double> wavePhase_;
    std::atomic<float> simulationTime_;
    mutable std::vector<DimensionInteraction> interactions_;
    std::vector<std::vector<double>> nCubeVertices_;
    mutable std::vector<glm::vec3> projectedVerts_;
    mutable std::atomic<double> avgProjScale_;
    mutable std::vector<Ball> balls_;
    std::atomic<bool> needsUpdate_; // Added missing member
    AMOURANTH* navigator_;

    void initializeNCube() {
        if (debug_.load()) {
            std::osyncstream(std::cout) << std::format("[DEBUG] Before nCubeVertices_.clear(): size={}, capacity={}\n",
                                                       nCubeVertices_.size(), nCubeVertices_.capacity());
        }
        nCubeVertices_.clear();
        if (debug_.load()) {
            std::osyncstream(std::cout) << std::format("[DEBUG] After nCubeVertices_.clear(): size={}, capacity={}\n",
                                                       nCubeVertices_.size(), nCubeVertices_.capacity());
        }
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
        avgProjScale_.store(1.0);
        if (debug_.load()) {
            std::osyncstream(std::cout) << std::format("[DEBUG] nCubeVertices_ initialized: size={}, capacity={}\n",
                                                       nCubeVertices_.size(), nCubeVertices_.capacity());
        }
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
            double strength = influence_.load() * std::cos(wavePhase_.load() + i * 0.1) / (distance + 1e-6);
            interactions_.emplace_back(currentDimension_.load(), strength, wavePhase_.load() + i * 0.1);
        }
    }

    void initializeWithRetry() {
        initializeNCube();
        updateInteractions();
    }
};

#endif // UE_INIT_HPP