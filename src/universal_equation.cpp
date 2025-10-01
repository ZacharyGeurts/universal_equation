// UniversalEquation.cpp implementation
// Zachary Geurts 2025 (enhanced by Grok for foundational cracks and perspective dimensions)
// Simulates quantum-like interactions in n-dimensional hypercube lattices, perfect for data scientists exploring physics.
// Models complex systems with:
// - Carroll-Schrödinger ultra-relativistic limit (see "Carroll–Schrödinger equation as the ultra-relativistic limit of the tachyon equation", Sci Rep 2025).
// - Deterministic asymmetric collapse for the quantum measurement problem (inspired by "A Solution to the Quantum Measurement Problem", MDPI 2025).
// - Mean-field approximation to simplify many-body interactions (arXiv:2508.00118, "Dynamical mean field theory with quantum computing").
// - Perspective projection for 3D visualization of n-D vertices (Noll 1967).
// Features:
// - Thread-safe with mutexes and atomics for reliable parallel computations.
// - Optimized with OpenMP for fast processing on multi-core CPUs.
// - Memory-safe with retry logic and LOD (Level of Detail) for high dimensions.
// - Data science-friendly: computeBatch() and exportToCSV() generate datasets for analysis in Python/R/Excel.
// Usage: Create UniversalEquation, compute energies, export results (e.g., equation.computeBatch(1, 5); equation.exportToCSV("data.csv", results);).
// Updated: Fixed missing darkEnergy initializer in updateCache, added copy constructor/assignment for computeBatch.
// Updated: Enhanced comments for data scientists, optimized OpenMP scheduling, improved mutex granularity.

#include "universal_equation.hpp"
#include <cmath>
#include <thread>
#include <fstream>
#include <memory>

// Constructor: Initializes simulation with clamped parameters for stability.
// Parameters control dimensionality, interaction strengths, and physical effects (e.g., dark matter, relativistic terms).
// Example: UniversalEquation eq(5, 3, 2.0) for a 5D simulation with stronger interactions.
// Throws std::exception on initialization failure (e.g., memory allocation errors).
UniversalEquation::UniversalEquation(
    int maxDimensions, int mode, double influence, double weak, double collapse,
    double twoD, double threeDInfluence, double oneDPermeation, double darkMatterStrength,
    double darkEnergyStrength, double alpha, double beta, double carrollFactor,
    double meanFieldApprox, double asymCollapse, double perspectiveTrans,
    double perspectiveFocal, bool debug)
    : maxDimensions_(std::max(1, std::min(maxDimensions == 0 ? 20 : maxDimensions, 20))),
      currentDimension_(std::clamp(mode, 1, maxDimensions_)),
      mode_(std::clamp(mode, 1, maxDimensions_)),
      maxVertices_(1ULL << std::min(maxDimensions_, 20)),
      influence_(std::clamp(influence, 0.0, 10.0)),
      weak_(std::clamp(weak, 0.0, 1.0)),
      collapse_(std::clamp(collapse, 0.0, 5.0)),
      twoD_(std::clamp(twoD, 0.0, 5.0)),
      threeDInfluence_(std::clamp(threeDInfluence, 0.0, 5.0)),
      oneDPermeation_(std::clamp(oneDPermeation, 0.0, 5.0)),
      darkMatterStrength_(std::clamp(darkMatterStrength, 0.0, 1.0)),
      darkEnergyStrength_(std::clamp(darkEnergyStrength, 0.0, 2.0)),
      alpha_(std::clamp(alpha, 0.1, 10.0)),
      beta_(std::clamp(beta, 0.0, 1.0)),
      carrollFactor_(std::clamp(carrollFactor, 0.0, 1.0)),
      meanFieldApprox_(std::clamp(meanFieldApprox, 0.0, 1.0)),
      asymCollapse_(std::clamp(asymCollapse, 0.0, 1.0)),
      perspectiveTrans_(std::clamp(perspectiveTrans, 0.0, 10.0)),
      perspectiveFocal_(std::clamp(perspectiveFocal, 1.0, 20.0)),
      debug_(debug),
      omega_(maxDimensions_ > 0 ? 2.0 * M_PI / (2 * maxDimensions_ - 1) : 1.0),
      invMaxDim_(maxDimensions_ > 0 ? 1.0 / maxDimensions_ : 1e-15),
      interactions_(),
      nCubeVertices_(),
      projectedVerts_(),
      avgProjScale_(1.0),
      projMutex_(),
      needsUpdate_(true),
      cachedCos_(),
      navigator_(nullptr),
      mutex_(),
      debugMutex_() {
    try {
        initializeWithRetry();
        if (debug_) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cout << "[DEBUG] Initialized: maxDimensions=" << maxDimensions_ << ", mode=" << mode_.load()
                      << ", carroll=" << carrollFactor_ << ", meanField=" << meanFieldApprox_
                      << ", asymCollapse=" << asymCollapse_ << ", perspTrans=" << perspectiveTrans_
                      << ", perspFocal=" << perspectiveFocal_ << "\n";
        }
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cerr << "[ERROR] Constructor failed: " << e.what() << "\n";
        throw;
    }
}

// Copy constructor: Creates a new instance with copied state for thread-local use in computeBatch.
// Handles non-copyable members (mutexes, atomics) by initializing new mutexes and copying atomic values.
// Example: Used in computeBatch to create independent instances for parallel processing.
UniversalEquation::UniversalEquation(const UniversalEquation& other)
    : maxDimensions_(other.maxDimensions_),
      currentDimension_(other.currentDimension_.load()),
      mode_(other.mode_.load()),
      maxVertices_(other.maxVertices_),
      influence_(other.influence_.load()),
      weak_(other.weak_.load()),
      collapse_(other.collapse_.load()),
      twoD_(other.twoD_.load()),
      threeDInfluence_(other.threeDInfluence_.load()),
      oneDPermeation_(other.oneDPermeation_.load()),
      darkMatterStrength_(other.darkMatterStrength_.load()),
      darkEnergyStrength_(other.darkEnergyStrength_.load()),
      alpha_(other.alpha_.load()),
      beta_(other.beta_.load()),
      carrollFactor_(other.carrollFactor_.load()),
      meanFieldApprox_(other.meanFieldApprox_.load()),
      asymCollapse_(other.asymCollapse_.load()),
      perspectiveTrans_(other.perspectiveTrans_.load()),
      perspectiveFocal_(other.perspectiveFocal_.load()),
      debug_(other.debug_.load()),
      omega_(other.omega_),
      invMaxDim_(other.invMaxDim_),
      interactions_(other.interactions_),
      nCubeVertices_(other.nCubeVertices_),
      projectedVerts_(other.projectedVerts_),
      avgProjScale_(other.avgProjScale_),
      projMutex_(),
      needsUpdate_(other.needsUpdate_.load()),
      cachedCos_(other.cachedCos_),
      navigator_(other.navigator_),
      mutex_(),
      debugMutex_() {
    try {
        initializeWithRetry();
        if (debug_) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cout << "[DEBUG] Copy constructor initialized: maxDimensions=" << maxDimensions_ << "\n";
        }
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cerr << "[ERROR] Copy constructor failed: " << e.what() << "\n";
        throw;
    }
}

// Copy assignment operator: Assigns state from another instance, handling non-copyable members.
// Initializes new mutexes and copies atomic values for thread safety.
UniversalEquation& UniversalEquation::operator=(const UniversalEquation& other) {
    if (this != &other) {
        maxDimensions_ = other.maxDimensions_;
        currentDimension_.store(other.currentDimension_.load());
        mode_.store(other.mode_.load());
        maxVertices_ = other.maxVertices_;
        influence_.store(other.influence_.load());
        weak_.store(other.weak_.load());
        collapse_.store(other.collapse_.load());
        twoD_.store(other.twoD_.load());
        threeDInfluence_.store(other.threeDInfluence_.load());
        oneDPermeation_.store(other.oneDPermeation_.load());
        darkMatterStrength_.store(other.darkMatterStrength_.load());
        darkEnergyStrength_.store(other.darkEnergyStrength_.load());
        alpha_.store(other.alpha_.load());
        beta_.store(other.beta_.load());
        carrollFactor_.store(other.carrollFactor_.load());
        meanFieldApprox_.store(other.meanFieldApprox_.load());
        asymCollapse_.store(other.asymCollapse_.load());
        perspectiveTrans_.store(other.perspectiveTrans_.load());
        perspectiveFocal_.store(other.perspectiveFocal_.load());
        debug_.store(other.debug_.load());
        omega_ = other.omega_;
        invMaxDim_ = other.invMaxDim_;
        interactions_ = other.interactions_;
        nCubeVertices_ = other.nCubeVertices_;
        projectedVerts_ = other.projectedVerts_;
        avgProjScale_ = other.avgProjScale_;
        needsUpdate_.store(other.needsUpdate_.load());
        cachedCos_ = other.cachedCos_;
        navigator_ = other.navigator_;
        try {
            initializeWithRetry();
            if (debug_) {
                std::lock_guard<std::mutex> lock(debugMutex_);
                std::cout << "[DEBUG] Copy assignment completed: maxDimensions=" << maxDimensions_ << "\n";
            }
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cerr << "[ERROR] Copy assignment failed: " << e.what() << "\n";
            throw;
        }
    }
    return *this;
}

// Advances simulation to the next dimension, cycling from maxDimensions_ to 1.
// Resets internal state for new calculations.
// Example: equation.advanceCycle(); to move to the next dimension and recompute energies.
void UniversalEquation::advanceCycle() {
    int newDimension = (currentDimension_.load() == maxDimensions_) ? 1 : currentDimension_.load() + 1;
    currentDimension_.store(newDimension);
    mode_.store(newDimension);
    needsUpdate_.store(true);
    initializeWithRetry();
    if (debug_) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Cycle advanced: dimension=" << currentDimension_.load() << ", mode=" << mode_.load() << "\n";
    }
}

// Computes energy components for the current dimension, returning an EnergyResult.
// Incorporates relativistic effects (Carroll limit), quantum collapse, and cosmological terms (dark matter/energy).
// Uses OpenMP for parallel processing of large vertex sets, thread-safe with mutexes.
// Example: auto result = equation.compute(); to get energies for analysis or visualization.
UniversalEquation::EnergyResult UniversalEquation::compute() const {
    if (debug_) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Starting compute for dimension: " << currentDimension_.load() << "\n";
    }
    if (needsUpdate_) {
        updateInteractions();
    }
    double observable = influence_.load();
    int currDim = currentDimension_.load();
    if (currDim >= 2) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (cachedCos_.empty()) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cerr << "[ERROR] cachedCos_ is empty in compute\n";
            throw std::runtime_error("cachedCos_ is empty");
        }
        observable += twoD_.load() * cachedCos_[currDim % cachedCos_.size()];
    }
    if (currDim == 3) {
        observable += threeDInfluence_.load();
    }

    // Apply Carroll limit to modulate time-like terms (simulates ultra-relativistic effects).
    double carrollMod = 1.0 - carrollFactor_.load() * (1.0 - invMaxDim_ * currDim);
    observable *= carrollMod;

    // Compute perspective modulation: Average scale from vertex interactions for 3D projection.
    double avgScale = 1.0;
    {
        std::vector<DimensionInteraction> localInteractions;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            localInteractions = interactions_;
        }
        if (!localInteractions.empty()) {
            double sumScale = 0.0;
            #pragma omp parallel for schedule(dynamic) reduction(+:sumScale)
            for (size_t i = 0; i < localInteractions.size(); ++i) {
                sumScale += perspectiveFocal_.load() / (perspectiveFocal_.load() + localInteractions[i].distance);
            }
            avgScale = sumScale / localInteractions.size();
        }
        std::lock_guard<std::mutex> lock(projMutex_);
        avgProjScale_ = avgScale;
    }
    observable *= avgScale;

    // Sum interaction contributions, including dark matter and dark energy effects.
    double totalDarkMatter = 0.0, totalDarkEnergy = 0.0, interactionSum = 0.0;
    {
        std::vector<DimensionInteraction> localInteractions;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            localInteractions = interactions_;
        }
        if (localInteractions.size() > 1000) {
            #pragma omp parallel for schedule(dynamic) reduction(+:interactionSum,totalDarkMatter,totalDarkEnergy)
            for (size_t i = 0; i < localInteractions.size(); ++i) {
                const auto& interaction = localInteractions[i];
                double influence = interaction.strength;
                double permeation = computePermeation(interaction.vertexIndex);
                double darkMatter = darkMatterStrength_.load();
                double darkEnergy = computeDarkEnergy(interaction.distance);
                double mfScale = 1.0 - meanFieldApprox_.load() * (1.0 / std::max(1.0, static_cast<double>(localInteractions.size())));
                interactionSum += influence * safeExp(-alpha_.load() * interaction.distance) * permeation * darkMatter * mfScale;
                totalDarkMatter += darkMatter * influence * permeation * mfScale;
                totalDarkEnergy += darkEnergy * influence * permeation * mfScale;
            }
        } else {
            for (const auto& interaction : localInteractions) {
                double influence = interaction.strength;
                double permeation = computePermeation(interaction.vertexIndex);
                double darkMatter = darkMatterStrength_.load();
                double darkEnergy = computeDarkEnergy(interaction.distance);
                double mfScale = 1.0 - meanFieldApprox_.load() * (1.0 / std::max(1.0, static_cast<double>(localInteractions.size())));
                interactionSum += influence * safeExp(-alpha_.load() * interaction.distance) * permeation * darkMatter * mfScale;
                totalDarkMatter += darkMatter * influence * permeation * mfScale;
                totalDarkEnergy += darkEnergy * influence * permeation * mfScale;
            }
        }
    }
    observable += interactionSum;

    // Apply deterministic collapse term to model quantum measurement effects.
    double collapse = computeCollapse();
    EnergyResult result = {
        observable + collapse,
        std::max(0.0, observable - collapse),
        totalDarkMatter,
        totalDarkEnergy
    };

    if (debug_) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Compute(D=" << currDim << "): " << result.toString()
                  << " (carrollMod=" << carrollMod << ", mfScale=" << (1.0 - meanFieldApprox_.load())
                  << ", avgPerspScale=" << avgScale << ")\n";
    }
    return result;
}

// Computes interaction strength for a vertex at a given distance, with dimension-specific modifiers.
// Used internally to calculate energy contributions; access results via getInteractions().
// Example: Stronger threeDInfluence_ boosts interactions in 3D simulations.
double UniversalEquation::computeInteraction(int vertexIndex, double distance) const {
    double denom = std::max(1e-15, std::pow(static_cast<double>(currentDimension_.load()), (vertexIndex % maxDimensions_ + 1)));
    double modifier = (currentDimension_.load() > 3 && (vertexIndex % maxDimensions_ + 1) > 3) ? weak_.load() : 1.0;
    if (currentDimension_.load() == 3 && ((vertexIndex % maxDimensions_ + 1) == 2 || (vertexIndex % maxDimensions_ + 1) == 4)) {
        modifier *= threeDInfluence_.load();
    }
    double result = influence_.load() * (1.0 / (denom * (1.0 + distance))) * modifier;
    if (debug_ && interactions_.size() <= 100) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Interaction(vertex=" << vertexIndex << ", dist=" << distance << "): " << result << "\n";
    }
    return result;
}

// Computes permeation factor for a vertex, adjusting interaction strength based on dimension and position.
// Used internally; influences energy calculations by scaling interactions.
// Throws std::out_of_range for invalid vertex indices.
double UniversalEquation::computePermeation(int vertexIndex) const {
    if (vertexIndex < 0 || nCubeVertices_.empty()) {
        if (debug_) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cerr << "[ERROR] Invalid vertex index " << vertexIndex << " or empty vertex list\n";
        }
        throw std::out_of_range("Invalid vertex index or empty vertex list");
    }
    if (vertexIndex == 1 || currentDimension_.load() == 1) return oneDPermeation_.load();
    if (currentDimension_.load() == 2 && (vertexIndex % maxDimensions_ + 1) > 2) return twoD_.load();
    if (currentDimension_.load() == 3 && ((vertexIndex % maxDimensions_ + 1) == 2 || (vertexIndex % maxDimensions_ + 1) == 4)) {
        return threeDInfluence_.load();
    }

    size_t safeIndex = static_cast<size_t>(vertexIndex) % nCubeVertices_.size();
    std::vector<double> vertex;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (safeIndex >= nCubeVertices_.size()) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cerr << "[ERROR] Invalid vertex index " << safeIndex << " in nCubeVertices_\n";
            throw std::out_of_range("Invalid vertex index");
        }
        vertex = nCubeVertices_[safeIndex];
    }
    double vertexMagnitude = 0.0;
    if (vertex.size() > 100) {
        #pragma omp parallel for schedule(dynamic) reduction(+:vertexMagnitude)
        for (size_t i = 0; i < std::min(static_cast<size_t>(currentDimension_.load()), vertex.size()); ++i) {
            vertexMagnitude += vertex[i] * vertex[i];
        }
    } else {
        for (size_t i = 0; i < std::min(static_cast<size_t>(currentDimension_.load()), vertex.size()); ++i) {
            vertexMagnitude += vertex[i] * vertex[i];
        }
    }
    vertexMagnitude = std::sqrt(vertexMagnitude);
    double result = 1.0 + beta_.load() * vertexMagnitude / std::max(1, currentDimension_.load());
    if (debug_ && interactions_.size() <= 100) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Permeation(vertex=" << vertexIndex << "): " << result << "\n";
    }
    return result;
}

// Computes dark energy contribution for a given distance, modeling expansive forces.
// Used internally; results reflected in EnergyResult::darkEnergy.
double UniversalEquation::computeDarkEnergy(double distance) const {
    double d = std::min(distance, 10.0);
    double result = darkEnergyStrength_.load() * safeExp(d * invMaxDim_);
    if (debug_ && interactions_.size() <= 100) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] DarkEnergy(dist=" << distance << "): " << result << "\n";
    }
    return result;
}

// Computes collapse factor with deterministic asymmetric term for quantum measurement effects.
// Used internally; results reflected in EnergyResult::observable and EnergyResult::potential.
// Returns 0 for 1D to avoid collapse in trivial cases.
double UniversalEquation::computeCollapse() const {
    if (currentDimension_.load() == 1) return 0.0;
    double phase = static_cast<double>(currentDimension_.load()) / (2 * maxDimensions_);
    std::lock_guard<std::mutex> lock(mutex_);
    if (cachedCos_.empty()) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cerr << "[ERROR] cachedCos_ is empty in computeCollapse\n";
        throw std::runtime_error("cachedCos_ is empty");
    }
    double osc = std::abs(cachedCos_[static_cast<size_t>(2.0 * M_PI * phase * cachedCos_.size()) % cachedCos_.size()]);
    double symCollapse = collapse_.load() * currentDimension_.load() * safeExp(-beta_.load() * (currentDimension_.load() - 1)) * (0.8 * osc + 0.2);
    double vertexFactor = static_cast<double>(nCubeVertices_.size() % 10) / 10.0;
    double asymTerm = asymCollapse_.load() * sin(M_PI * phase + osc + vertexFactor) * safeExp(-alpha_.load() * phase);
    double result = std::max(0.0, symCollapse + asymTerm);
    if (debug_ && interactions_.size() <= 100) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Collapse(D=" << currentDimension_.load() << ", asym=" << asymTerm << "): " << result << "\n";
    }
    return result;
}

// Initializes n-cube vertices with memory pooling for efficiency.
// Uses OpenMP for parallel vertex generation in high dimensions (>1000 vertices).
// Thread-safe with mutexes to protect nCubeVertices_.
void UniversalEquation::initializeNCube() {
    std::lock_guard<std::mutex> lock(mutex_);
    nCubeVertices_.clear();
    uint64_t numVertices = std::min(static_cast<uint64_t>(1ULL << currentDimension_.load()), maxVertices_);
    nCubeVertices_.reserve(numVertices);
    if (debug_) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Initializing nCube with " << numVertices << " vertices for dimension " << currentDimension_.load() << "\n";
    }
    if (numVertices > 1000) {
        #pragma omp parallel
        {
            std::vector<std::vector<double>> localVertices;
            localVertices.reserve(numVertices / omp_get_num_threads() + 1);
            #pragma omp for schedule(dynamic)
            for (uint64_t i = 0; i < numVertices; ++i) {
                std::vector<double> vertex(currentDimension_.load(), 0.0);
                for (int j = 0; j < currentDimension_.load(); ++j) {
                    vertex[j] = (i & (1ULL << j)) ? 1.0 : -1.0;
                }
                localVertices.push_back(std::move(vertex));
            }
            #pragma omp critical
            {
                nCubeVertices_.insert(nCubeVertices_.end(), localVertices.begin(), localVertices.end());
            }
        }
    } else {
        for (uint64_t i = 0; i < numVertices; ++i) {
            std::vector<double> vertex(currentDimension_.load(), 0.0);
            for (int j = 0; j < currentDimension_.load(); ++j) {
                vertex[j] = (i & (1ULL << j)) ? 1.0 : -1.0;
            }
            nCubeVertices_.push_back(std::move(vertex));
        }
    }
    if (debug_ && nCubeVertices_.size() <= 100) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Initialized nCube with " << nCubeVertices_.size() << " vertices\n";
    }
}

// Updates vertex interactions with perspective projection and LOD for high dimensions.
// Computes 3D projections for visualization and interaction strengths.
// Thread-safe with mutexes, optimized with OpenMP for large vertex sets.
void UniversalEquation::updateInteractions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    interactions_.clear();
    {
        std::lock_guard<std::mutex> lock(projMutex_);
        projectedVerts_.clear();
    }
    int d = currentDimension_.load();
    uint64_t numVertices = std::min(static_cast<uint64_t>(1ULL << d), maxVertices_);
    if (d > 6) {
        uint64_t lodFactor = 1ULL << (d / 2);
        if (lodFactor == 0) lodFactor = 1;
        numVertices = std::min(numVertices / lodFactor, static_cast<uint64_t>(1024));
        if (debug_) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cout << "[OPT] LOD: Reduced verts from " << (1ULL << d) << " to " << numVertices << " for d=" << d << "\n";
        }
    }
    interactions_.reserve(numVertices - 1);
    {
        std::lock_guard<std::mutex> lock(projMutex_);
        projectedVerts_.reserve(numVertices);
    }
    if (nCubeVertices_.empty()) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cerr << "[ERROR] nCubeVertices_ is empty\n";
        throw std::runtime_error("nCubeVertices_ is empty");
    }
    const auto& referenceVertex = nCubeVertices_[0];
    double trans = perspectiveTrans_.load();
    double focal = perspectiveFocal_.load();
    int depthIdx = d - 1;
    double depthRef = referenceVertex[depthIdx] + trans;
    if (depthRef <= 0.0) depthRef = 0.001;
    double scaleRef = focal / depthRef;
    double projRef[3] = {0.0, 0.0, 0.0};
    int projDim = std::min(3, d - 1);
    for (int k = 0; k < projDim; ++k) {
        projRef[k] = referenceVertex[k] * scaleRef;
    }
    glm::vec3 projRefVec(static_cast<float>(projRef[0]), static_cast<float>(projRef[1]), static_cast<float>(projRef[2]));
    {
        std::lock_guard<std::mutex> lock(projMutex_);
        projectedVerts_.push_back(projRefVec);
    }
    if (debug_) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Updating perspective interactions for " << numVertices - 1
                  << " vertices (d=" << d << ", trans=" << trans << ", focal=" << focal << ")\n";
    }
    if (numVertices > 1000) {
        #pragma omp parallel
        {
            std::vector<DimensionInteraction> localInteractions;
            std::vector<glm::vec3> localProjected;
            localInteractions.reserve((numVertices - 1) / omp_get_num_threads() + 1);
            localProjected.reserve((numVertices - 1) / omp_get_num_threads() + 1);
            #pragma omp for schedule(dynamic)
            for (uint64_t i = 1; i < numVertices; ++i) {
                const auto& v = nCubeVertices_[i];
                double depthI = v[depthIdx] + trans;
                if (depthI <= 0.0) depthI = 0.001;
                double scaleI = focal / depthI;
                double projI[3] = {0.0, 0.0, 0.0};
                for (int k = 0; k < projDim; ++k) {
                    projI[k] = v[k] * scaleI;
                }
                double distSq = 0.0;
                for (int k = 0; k < 3; ++k) {
                    double diff = projI[k] - projRef[k];
                    distSq += diff * diff;
                }
                double distance = std::sqrt(distSq);
                if (d == 1) {
                    distance = std::fabs(v[0] - referenceVertex[0]);
                }
                double strength = computeInteraction(static_cast<int>(i), distance);
                localInteractions.emplace_back(static_cast<int>(i), distance, strength);
                glm::vec3 projIVec(static_cast<float>(projI[0]), static_cast<float>(projI[1]), static_cast<float>(projI[2]));
                localProjected.push_back(projIVec);
            }
            #pragma omp critical
            {
                interactions_.insert(interactions_.end(), localInteractions.begin(), localInteractions.end());
                std::lock_guard<std::mutex> lock(projMutex_);
                projectedVerts_.insert(projectedVerts_.end(), localProjected.begin(), localProjected.end());
            }
        }
    } else {
        for (uint64_t i = 1; i < numVertices; ++i) {
            const auto& v = nCubeVertices_[i];
            double depthI = v[depthIdx] + trans;
            if (depthI <= 0.0) depthI = 0.001;
            double scaleI = focal / depthI;
            double projI[3] = {0.0, 0.0, 0.0};
            for (int k = 0; k < projDim; ++k) {
                projI[k] = v[k] * scaleI;
            }
            double distSq = 0.0;
            for (int k = 0; k < 3; ++k) {
                double diff = projI[k] - projRef[k];
                distSq += diff * diff;
            }
            double distance = std::sqrt(distSq);
            if (d == 1) {
                distance = std::fabs(v[0] - referenceVertex[0]);
            }
            double strength = computeInteraction(static_cast<int>(i), distance);
            interactions_.emplace_back(static_cast<int>(i), distance, strength);
            glm::vec3 projIVec(static_cast<float>(projI[0]), static_cast<float>(projI[1]), static_cast<float>(projI[2]));
            {
                std::lock_guard<std::mutex> lock(projMutex_);
                projectedVerts_.push_back(projIVec);
            }
        }
    }
    if (d == 3) {
        for (int adj : {2, 4}) {
            size_t vertexIndex = static_cast<size_t>(adj - 1);
            if (vertexIndex < nCubeVertices_.size() &&
                std::none_of(interactions_.begin(), interactions_.end(),
                             [adj](const auto& i) { return i.vertexIndex == adj; })) {
                const auto& v = nCubeVertices_[vertexIndex];
                double depthI = v[depthIdx] + trans;
                if (depthI <= 0.0) depthI = 0.001;
                double scaleI = focal / depthI;
                double projI[3] = {0.0, 0.0, 0.0};
                for (int k = 0; k < projDim; ++k) {
                projI[k] = v[k] * scaleI;
                }
                double distSq = 0.0;
                for (int k = 0; k < 3; ++k) {
                    double diff = projI[k] - projRef[k];
                    distSq += diff * diff;
                }
                double distance = std::sqrt(distSq);
                double strength = computeInteraction(static_cast<int>(vertexIndex), distance);
                interactions_.emplace_back(static_cast<int>(vertexIndex), distance, strength);
                glm::vec3 projIVec(static_cast<float>(projI[0]), static_cast<float>(projI[1]), static_cast<float>(projI[2]));
                {
                    std::lock_guard<std::mutex> lock(projMutex_);
                    projectedVerts_.push_back(projIVec);
                }
            }
        }
    }
    needsUpdate_ = false;
    if (debug_ && interactions_.size() <= 100) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Perspective Interactions(D=" << d << "): ";
        for (const auto& i : interactions_) {
            std::cout << "(vertex=" << i.vertexIndex << ", projDist=" << i.distance << ", strength=" << i.strength << ") ";
        }
        std::cout << "\n";
    }
}

// Initializes simulation with retry logic for memory allocation errors.
// Reduces dimension if allocation fails, ensuring robustness.
// Example: Automatically handles high-dimensional vertex sets with memory pooling.
void UniversalEquation::initializeWithRetry() {
    while (currentDimension_.load() >= 1) {
        try {
            initializeNCube();
            {
                std::lock_guard<std::mutex> lock(mutex_);
                cachedCos_.resize(maxDimensions_ + 1);
                if (maxDimensions_ > 10) {
                    #pragma omp parallel for schedule(dynamic)
                    for (int i = 0; i <= maxDimensions_; ++i) {
                        cachedCos_[i] = std::cos(omega_ * i);
                    }
                } else {
                    for (int i = 0; i <= maxDimensions_; ++i) {
                        cachedCos_[i] = std::cos(omega_ * i);
                    }
                }
            }
            updateInteractions();
            if (debug_) {
                std::lock_guard<std::mutex> lock(debugMutex_);
                std::cout << "[DEBUG] initializeWithRetry completed for dimension " << currentDimension_.load() << "\n";
            }
            return;
        } catch (const std::bad_alloc& e) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cerr << "[ERROR] Failed to allocate memory for " << (1ULL << currentDimension_.load())
                      << " vertices. Reducing dimension to " << (currentDimension_.load() - 1) << ".\n";
            if (currentDimension_.load() == 1) {
                throw std::runtime_error("Failed to allocate memory even at dimension 1");
            }
            currentDimension_.store(currentDimension_.load() - 1);
            mode_.store(currentDimension_.load());
            maxVertices_ = 1ULL << std::min(currentDimension_.load(), 20);
            needsUpdate_ = true;
        }
    }
}

// Safe exponential function to prevent numerical overflow/underflow.
// Used in energy and interaction calculations for stability.
double UniversalEquation::safeExp(double x) const {
    double result = std::exp(std::clamp(x, -709.0, 709.0));
    if (debug_ && (std::isnan(result) || std::isinf(result))) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cerr << "[ERROR] safeExp produced invalid result for x=" << x << "\n";
    }
    return result;
}

// Initializes with DimensionalNavigator for Vulkan rendering integration.
// Required for visualization; skip if only analyzing data.
// Throws std::invalid_argument if navigator is null.
void UniversalEquation::initializeCalculator(DimensionalNavigator* navigator) {
    std::lock_guard<std::mutex> lock(mutex_);
    navigator_ = navigator;
    if (!navigator_) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cerr << "[ERROR] Navigator pointer is null\n";
        throw std::invalid_argument("Navigator pointer cannot be null");
    }
    needsUpdate_ = true;
    initializeWithRetry();
}

// Updates cached simulation data, returning a DimensionData struct.
// Calls compute() to get energy components and stores them with the current dimension.
// Use for single-dimension analysis or to update Vulkan rendering buffers.
// Example: auto data = equation.updateCache(); to get current state for analysis or visualization.
// Thread-safe, logs debug info if enabled.
UniversalEquation::DimensionData UniversalEquation::updateCache() {
    if (debug_) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Starting updateCache for dimension: " << currentDimension_.load() << "\n";
    }
    auto result = compute();
    DimensionData data{currentDimension_.load(), result.observable, result.potential,
                      result.darkMatter, result.darkEnergy};
    if (debug_) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] updateCache completed: " << data.toString() << "\n";
    }
    return data;
}

// Computes batch of dimension data in parallel, from startDim to endDim (default: 1 to maxDimensions_).
// Uses thread-local copies of UniversalEquation for OpenMP parallelization, ensuring thread safety.
// Ideal for data scientists generating large datasets for analysis.
// Example: auto data = equation.computeBatch(1, 5); to compute energies for dimensions 1-5.
std::vector<UniversalEquation::DimensionData> UniversalEquation::computeBatch(int startDim, int endDim) {
    if (endDim == -1) endDim = maxDimensions_;
    startDim = std::clamp(startDim, 1, maxDimensions_);
    endDim = std::clamp(endDim, startDim, maxDimensions_);
    int savedDim = currentDimension_.load();
    int savedMode = mode_.load();
    std::vector<DimensionData> results(endDim - startDim + 1);
    #pragma omp parallel for schedule(dynamic)
    for (int d = startDim; d <= endDim; ++d) {
        UniversalEquation temp(*this); // Create thread-local copy
        temp.currentDimension_.store(d);
        temp.mode_.store(d);
        temp.needsUpdate_.store(true);
        temp.initializeWithRetry();
        results[d - startDim] = temp.updateCache();
    }
    currentDimension_.store(savedDim);
    mode_.store(savedMode);
    needsUpdate_.store(true);
    initializeWithRetry();
    if (debug_) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] computeBatch completed for dimensions " << startDim << " to " << endDim << "\n";
    }
    return results;
}

// Exports batch data to a CSV file for analysis in Python, R, or Excel.
// Columns: Dimension, Observable, Potential, DarkMatter, DarkEnergy.
// Example: equation.exportToCSV("results.csv", data); to save results for analysis.
void UniversalEquation::exportToCSV(const std::string& filename, const std::vector<DimensionData>& data) const {
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