#ifndef UE_INIT_HPP
#define UE_INIT_HPP

#include <vector>
#include <string>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <fstream>
#include <random>
#include <iostream>
#include <glm/glm.hpp>

// used by AMOURANTH RTX September 2025
// this is not part of universal_equation and is for visualization
// Zachary Geurts 2025

// Structure to hold data for each rendered dimension
struct DimensionData {
    int dimension;
    double observable;
    double potential;
    double darkMatter;
    double darkEnergy;
    std::string toString() const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6)
            << "Dimension: " << dimension
            << ", Observable: " << observable
            << ", Potential: " << potential
            << ", Dark Matter: " << darkMatter
            << ", Dark Energy: " << darkEnergy;
        return oss.str();
    }
};

// Structure to hold energy computation results
struct EnergyResult {
    double observable = 0.0;
    double potential = 0.0;
    double darkMatter = 0.0;
    double darkEnergy = 0.0;
    std::string toString() const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6)
            << "Observable: " << observable
            << ", Potential: " << potential
            << ", Dark Matter: " << darkMatter
            << ", Dark Energy: " << darkEnergy;
        return oss.str();
    }
};

// Structure to hold dimension interaction data
struct DimensionInteraction {
    int dimension;
    double strength;
    double phase;
    DimensionInteraction(int dim, double str, double ph)
        : dimension(dim), strength(str), phase(ph) {}
};

// Structure to hold ball physics properties
struct Ball {
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 acceleration;
    float mass;
    float radius;
    float startTime; // Start time in seconds for staggered initialization
    Ball(const glm::vec3& pos, const glm::vec3& vel, float m, float r, float start)
        : position(pos), velocity(vel), acceleration(0.0f), mass(m), radius(r), startTime(start) {}
};

// Forward declaration for Vulkan rendering
class AMOURANTH;

// Class for computing multidimensional physics simulations for visualization
class UniversalEquation {
public:
    // Constructor with simulation parameters
    UniversalEquation(
        int maxDimensions = 11,
        int mode = 3,
        double influence = 1.0,
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

    // Parameter setters (thread-safe, clamped)
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
    void setDebug(bool debug) {
        debug_ = debug;
    }
    void setMode(int mode) {
        mode_ = std::clamp(mode, 1, maxDimensions_);
        needsUpdate_ = true;
    }

    // Parameter getters (thread-safe)
    int getCurrentDimension() const { return currentDimension_.load(); }
    double getInfluence() const { return influence_.load(); }
    double getAlpha() const { return alpha_.load(); }
    bool getDebug() const { return debug_.load(); }
    int getMode() const { return mode_.load(); }
    int getMaxDimensions() const { return maxDimensions_; }
    double getWavePhase() const { return wavePhase_; }
    float getSimulationTime() const { return simulationTime_; }
    const std::vector<DimensionInteraction>& getInteractions() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (debug_) {
            std::cout << "[DEBUG] Entering getInteractions, needsUpdate=" << needsUpdate_ << "\n";
        }
        if (needsUpdate_) {
            updateInteractions();
            needsUpdate_ = false;
        }
        return interactions_;
    }
    const std::vector<std::vector<double>>& getNCubeVertices() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return nCubeVertices_;
    }
    const std::vector<glm::vec3>& getProjectedVertices() const {
        std::lock_guard<std::mutex> lock(projMutex_);
        return projectedVerts_;
    }
    double getAvgProjScale() const {
        std::lock_guard<std::mutex> lock(projMutex_);
        return avgProjScale_;
    }
    const std::vector<Ball>& getBalls() const {
        std::lock_guard<std::mutex> lock(physicsMutex_);
        return balls_;
    }
    std::mutex& getPhysicsMutex() const { return physicsMutex_; }

    // Updates projected vertices (thread-safe)
    void updateProjectedVertices(const std::vector<glm::vec3>& newVerts) {
        std::lock_guard<std::mutex> lock(projMutex_);
        projectedVerts_ = newVerts;
    }

    // Advances simulation to next dimension
    void advanceCycle() {
        wavePhase_ += 0.1;
        needsUpdate_ = true;
    }

    // Computes energy components
    EnergyResult compute() const {
        EnergyResult result;
        result.observable = influence_ * std::cos(wavePhase_);
        result.potential = influence_ * std::sin(wavePhase_);
        result.darkMatter = influence_ * 0.27;
        result.darkEnergy = influence_ * 0.68;
        return result;
    }

    // Initializes with AMOURANTH for Vulkan rendering
    void initializeCalculator(AMOURANTH* amouranth) {
        if (!amouranth) {
            throw std::invalid_argument("AMOURANTH cannot be null");
        }
        navigator_ = amouranth;
        initializeWithRetry();
        if (debug_) {
            std::cout << "[DEBUG] Initialized calculator with AMOURANTH\n";
        }
    }

    // Updates and returns cached data
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

    // Computes batch of dimension data
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

    // Exports data to CSV
    void exportToCSV(const std::string& filename, const std::vector<DimensionData>& data) const {
        std::ofstream ofs(filename);
        if (!ofs) {
            throw std::runtime_error("Cannot open CSV file for writing: " + filename);
        }
        ofs << "Dimension,Observable,Potential,DarkMatter,DarkEnergy\n";
        for (const auto& d : data) {
            ofs << d.dimension << ","
                << std::fixed << std::setprecision(6) << d.observable << ","
                << d.potential << ","
                << d.darkMatter << ","
                << d.darkEnergy << "\n";
        }
        if (debug_) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cout << "[DEBUG] Exported data to " << filename << "\n";
        }
    }

    // Computes interaction strength
    double computeInteraction(int vertexIndex, double distance) const {
        return influence_ * std::cos(wavePhase_ + vertexIndex * 0.1) / (distance + 1e-6);
    }

    // Computes permeation factor
    double computePermeation(int vertexIndex) const {
        return influence_ * std::sin(wavePhase_ + vertexIndex * 0.1);
    }

    // Computes dark energy contribution
    double computeDarkEnergy(double distance) const {
        return influence_ * 0.68 / (distance + 1e-6);
    }

    // Initializes balls with physics properties, staggered start times, and initial velocities
    void initializeBalls(float baseMass = 1.0f, float baseRadius = 0.1f, size_t numBalls = 200) {
        std::lock_guard<std::mutex> lock(physicsMutex_);
        balls_.clear();
        simulationTime_ = 0.0f; // Reset simulation time
        auto verts = getProjectedVertices();
        balls_.reserve(numBalls);
        auto result = compute();
        float massScale = static_cast<float>(result.darkMatter);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> distX(-5.0f, 5.0f);
        std::uniform_real_distribution<float> distY(-5.0f, 5.0f);
        std::uniform_real_distribution<float> distZ(-2.0f, 2.0f);
        std::uniform_real_distribution<float> velDist(-1.0f, 1.0f); // Initial velocity range
        for (size_t i = 0; i < numBalls; ++i) {
            glm::vec3 pos = (i < verts.size()) ? verts[i] : glm::vec3(distX(gen), distY(gen), distZ(gen));
            glm::vec3 vel(velDist(gen), velDist(gen), velDist(gen)); // Random initial velocity
            float startTime = i * 0.001f; // 1ms delay per ball
            balls_.emplace_back(pos, vel, baseMass * massScale, baseRadius, startTime);
            if (debug_) {
                std::cout << "[DEBUG] Initialized ball " << i << " with startTime=" << startTime
                          << "s, velocity=(" << vel.x << ", " << vel.y << ", " << vel.z << ")\n";
            }
        }
        if (debug_) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cout << "[DEBUG] Initialized " << balls_.size() << " balls with mass=" << baseMass * massScale
                      << ", radius=" << baseRadius << ", requested numBalls=" << numBalls << "\n";
        }
    }

    // Updates ball physics, respecting start times (with sphere-sphere collisions)
    void updateBalls(float deltaTime) {
        std::lock_guard<std::mutex> lock(physicsMutex_);
        simulationTime_ += deltaTime;
        if (debug_) {
            std::cout << "[DEBUG] Starting updateBalls with " << balls_.size() << " balls, simulationTime=" << simulationTime_ << "s\n";
        }

        auto interactions = getInteractions();
        auto result = compute();
        if (debug_) {
            std::cout << "[DEBUG] Got interactions and computed result\n";
        }

        // Update accelerations for active balls
        for (size_t i = 0; i < balls_.size(); ++i) {
            if (simulationTime_ < balls_[i].startTime) continue;
            glm::vec3 force(0.0f);
            double interactionStrength = (i < interactions.size()) ? interactions[i].strength : 0.0;
            force += glm::vec3(
                static_cast<float>(result.observable),
                static_cast<float>(result.potential),
                static_cast<float>(result.darkEnergy)
            ) * static_cast<float>(interactionStrength);
            balls_[i].acceleration = force / balls_[i].mass;
            if (debug_) {
                std::cout << "[DEBUG] Updated acceleration for ball " << i << "\n";
            }
        }

        // Detect and resolve sphere-sphere collisions
        bool allStarted = true;
        for (const auto& ball : balls_) {
            if (simulationTime_ < ball.startTime) {
                allStarted = false;
                break;
            }
        }
        if (balls_.size() > 1 && allStarted) {
            for (size_t i = 0; i < balls_.size(); ++i) {
                for (size_t j = i + 1; j < balls_.size(); ++j) { // Corrected loop: ++j
                    if (simulationTime_ < balls_[i].startTime || simulationTime_ < balls_[j].startTime) continue;
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
                        if (debug_) {
                            std::cout << "[DEBUG] Resolved collision between balls " << i << " and " << j << "\n";
                        }
                    }
                }
            }
        }

        // Update velocities and positions for active balls
        for (size_t i = 0; i < balls_.size(); ++i) {
            if (simulationTime_ < balls_[i].startTime) continue;
            balls_[i].velocity += balls_[i].acceleration * deltaTime;
            balls_[i].position += balls_[i].velocity * deltaTime;
            if (debug_) {
                std::cout << "[DEBUG] Updated velocity and position for ball " << i << "\n";
            }
        }

        // Update projectedVerts_ for rendering
        {
            std::lock_guard<std::mutex> projLock(projMutex_);
            projectedVerts_ = std::vector<glm::vec3>(balls_.size());
            for (size_t i = 0; i < balls_.size(); ++i) {
                projectedVerts_[i] = (simulationTime_ >= balls_[i].startTime) ? balls_[i].position : glm::vec3(0.0f);
            }
            if (debug_) {
                std::cout << "[DEBUG] Updated projectedVerts_\n";
            }
        }

        if (debug_) {
            std::cout << "[DEBUG] Completed updateBalls for deltaTime=" << deltaTime << "\n";
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
    float simulationTime_; // Tracks total simulation time
    mutable std::vector<DimensionInteraction> interactions_;
    std::vector<std::vector<double>> nCubeVertices_;
    mutable std::vector<glm::vec3> projectedVerts_;
    mutable double avgProjScale_;
    mutable std::mutex mutex_;
    mutable std::mutex projMutex_;
    mutable std::mutex debugMutex_;
    mutable std::mutex physicsMutex_;
    mutable std::vector<Ball> balls_;
    mutable std::atomic<bool> needsUpdate_;
    AMOURANTH* navigator_;

    // Initializes n-cube vertices
    void initializeNCube() {
        nCubeVertices_.clear();
        uint64_t maxVertices = 1ULL << maxDimensions_;
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

    // Updates interaction data (no lock, called from getInteractions)
    void updateInteractions() const {
        interactions_.clear();
        for (int i = 0; i < static_cast<int>(nCubeVertices_.size()); ++i) {
            double distance = 0.0;
            for (const auto& v : nCubeVertices_[i]) {
                distance += v * v;
            }
            distance = std::sqrt(distance);
            double strength = computeInteraction(i, distance);
            interactions_.emplace_back(currentDimension_, strength, wavePhase_ + i * 0.1);
        }
        if (debug_) {
            std::cout << "[DEBUG] Updated interactions for " << interactions_.size() << " vertices\n";
        }
    }

    // Initializes with retry logic
    void initializeWithRetry() {
        initializeNCube();
        updateInteractions();
    }
};

#endif // UE_INIT_HPP