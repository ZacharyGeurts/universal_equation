// UniversalEquation.cpp implementation
// Zachary Geurts 2025 (enhanced by Grok for foundational cracks and perspective dimensions)
// Simulates quantum-like interactions in n-dimensional hypercube lattices.
// Addresses Schrödinger challenges:
// - Carroll-Schrödinger ultra-relativistic limit via 'carrollFactor' (flat-space approximation).
// - Asymmetric collapse term (MDPI 2025-inspired) for measurement problem, now deterministic.
// - Mean-field approximation (arXiv 2025) to reduce many-body complexity.
// - Perspective projection (Noll 1967) for 3D visualization of n-D vertices.
// Thread-safe, parallelized with OpenMP for physics simulations and visualization.
// Vulkan: Use with DimensionalNavigator for rendering energy distributions/vertices.

#include "universal_equation.hpp"
#include <cmath>
#include <thread>

// Constructor: Initializes simulation with clamped parameters
// Includes fixes for relativity, measurement, many-body, and perspective projection
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
      needsUpdate_(true),
      navigator_(nullptr) {
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

// Advances simulation to next dimension
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

// Computes energy components with Carroll limit, asymmetric collapse, and perspective modulation
// Vulkan: Pass result to shaders as uniforms
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

    // Carroll limit: Modulates time-like terms
    double carrollMod = 1.0 - carrollFactor_.load() * (1.0 - invMaxDim_ * currDim);
    observable *= carrollMod;

    // Perspective modulation: Average scale from interactions
    double avgScale = 1.0;
    {
        std::vector<DimensionInteraction> localInteractions;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            localInteractions = interactions_;
        }
        if (!localInteractions.empty()) {
            double sumScale = 0.0;
            #pragma omp parallel for reduction(+:sumScale)
            for (size_t i = 0; i < localInteractions.size(); ++i) {
                sumScale += perspectiveFocal_.load() / (perspectiveFocal_.load() + localInteractions[i].distance);
            }
            avgScale = sumScale / localInteractions.size();
        }
        observable *= avgScale;
    }

    double totalDarkMatter = 0.0, totalDarkEnergy = 0.0, interactionSum = 0.0;
    {
        std::vector<DimensionInteraction> localInteractions;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            localInteractions = interactions_;
        }
        if (localInteractions.size() > 1000) {
            #pragma omp parallel reduction(+:interactionSum,totalDarkMatter,totalDarkEnergy)
            {
                #pragma omp for
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

// Computes interaction strength with dimension-specific modifiers
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

// Computes permeation factor for a vertex
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
        #pragma omp parallel for reduction(+:vertexMagnitude)
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

// Computes dark energy contribution
double UniversalEquation::computeDarkEnergy(double distance) const {
    double d = std::min(distance, 10.0);
    double result = darkEnergyStrength_.load() * safeExp(d * invMaxDim_);
    if (debug_ && interactions_.size() <= 100) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] DarkEnergy(dist=" << distance << "): " << result << "\n";
    }
    return result;
}

// Computes collapse factor with deterministic asymmetric term (MDPI 2025)
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
    // Deterministic asymmetric term: Use vertex count for variation
    double vertexFactor = static_cast<double>(nCubeVertices_.size() % 10) / 10.0; // Normalize to [0,1]
    double asymTerm = asymCollapse_.load() * sin(M_PI * phase + osc + vertexFactor) * safeExp(-alpha_.load() * phase);
    double result = std::max(0.0, symCollapse + asymTerm);
    if (debug_ && interactions_.size() <= 100) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Collapse(D=" << currentDimension_.load() << ", asym=" << asymTerm << "): " << result << "\n";
    }
    return result;
}

// Initializes n-cube vertices with perspective projection
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
            #pragma omp for
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

// Updates interactions with perspective projection and LOD for high dimensions
void UniversalEquation::updateInteractions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    interactions_.clear();
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
    double projRef[3] = {0.0f, 0.0f, 0.0f};
    int projDim = std::min(3, d - 1);
    for (int k = 0; k < projDim; ++k) {
        projRef[k] = referenceVertex[k] * scaleRef;
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
            localInteractions.reserve((numVertices - 1) / omp_get_num_threads() + 1);
            #pragma omp for
            for (uint64_t i = 1; i < numVertices; ++i) {
                const auto& v = nCubeVertices_[i];
                double depthI = v[depthIdx] + trans;
                if (depthI <= 0.0) depthI = 0.001;
                double scaleI = focal / depthI;
                double projI[3] = {0.0f, 0.0f, 0.0f};
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
            }
            #pragma omp critical
            {
                interactions_.insert(interactions_.end(), localInteractions.begin(), localInteractions.end());
            }
        }
    } else {
        for (uint64_t i = 1; i < numVertices; ++i) {
            const auto& v = nCubeVertices_[i];
            double depthI = v[depthIdx] + trans;
            if (depthI <= 0.0) depthI = 0.001;
            double scaleI = focal / depthI;
            double projI[3] = {0.0f, 0.0f, 0.0f};
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
                double projI[3] = {0.0f, 0.0f, 0.0f};
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

// Initializes with retry logic for memory errors
void UniversalEquation::initializeWithRetry() {
    while (currentDimension_.load() >= 1) {
        try {
            initializeNCube();
            {
                std::lock_guard<std::mutex> lock(mutex_);
                cachedCos_.resize(maxDimensions_ + 1);
                if (maxDimensions_ > 10) {
                    #pragma omp parallel for
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

// Safe exponential function
double UniversalEquation::safeExp(double x) const {
    double result = std::exp(std::clamp(x, -709.0, 709.0));
    if (debug_ && (std::isnan(result) || std::isinf(result))) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cerr << "[ERROR] safeExp produced invalid result for x=" << x << "\n";
    }
    return result;
}

// Initializes with DimensionalNavigator for Vulkan rendering
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

// Updates cached simulation data
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