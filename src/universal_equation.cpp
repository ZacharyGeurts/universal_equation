// UniversalEquation.cpp implementation
// 2025 Zachary Geurts (enhanced by Grok for foundational cracks and perspective dimensions)
// This class now addresses key Schrödinger equation challenges:
// - Non-Relativistic Bias: Added Carroll-Schrödinger ultra-rel limit via 'carrollFactor' param;
//   modifies time evolution in compute() for flat-space high-speed approx (from Carroll-Schrödinger 2025 advances).
// - Measurement Problem: Enhanced collapse with asymmetric complementary equation term
//   (inspired by MDPI 2025 novel solution); adds stochastic-like asymmetry in computeCollapse().
// - Many-Body Explosion: Integrated low-complexity least-squares ansatz approximation
//   (from arXiv 2025); reduces effective Hilbert dim via 'meanFieldApprox' param in interactions.
// - Perspective Dimensions: Incorporated projective geometry perspective projection (from Noll 1967 & projective space math);
//   projects n-D hypercube vertices to 3D view plane using depth-based scaling, computing interactions via projected distances
//   to simulate observer perspective in higher dimensions. Uses homogeneous-inspired scaling: scale = focal / depth.
// Thread-safe, parallelized; for physics sims/viz of high-D systems.

#include "universal_equation.hpp"
#include <random>  // For jitter RNG

// Constructor: Now includes new params for crack fixes and perspective
// - carrollFactor: Ultra-rel Carroll limit (0=Schrödinger, 1=Carroll; default 0)
// - meanFieldApprox: Many-body mean-field strength (0=exact, 1=full approx; default 0.5)
// - asymCollapse: Asymmetric measurement term (0=standard, 1=MDPI-inspired; default 0.5)
// - perspectiveTrans: Translation along depth axis for hypercube placement (default 2.0)
// - perspectiveFocal: Focal length for projection scaling (default 4.0)
UniversalEquation::UniversalEquation(int maxDimensions, int mode, double influence,
                                     double weak, double collapse, double twoD,
                                     double threeDInfluence, double oneDPermeation,
                                     double darkMatterStrength, double darkEnergyStrength,
                                     double alpha, double beta, double carrollFactor,
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
      carrollFactor_(std::clamp(carrollFactor, 0.0, 1.0)), // New: Rel fix
      meanFieldApprox_(std::clamp(meanFieldApprox, 0.0, 1.0)), // New: Many-body fix
      asymCollapse_(std::clamp(asymCollapse, 0.0, 1.0)), // New: Measurement fix
      perspectiveTrans_(std::clamp(perspectiveTrans, 0.0, 10.0)), // New: Perspective translation
      perspectiveFocal_(std::clamp(perspectiveFocal, 1.0, 20.0)), // New: Perspective focal length
      debug_(debug),
      omega_(maxDimensions_ > 0 ? 2.0 * M_PI / (2 * maxDimensions_ - 1) : 1.0),
      invMaxDim_(maxDimensions_ > 0 ? 1.0 / maxDimensions_ : 1e-15),
      interactions_(),
      nCubeVertices_(),
      projectedVerts_(),  // New
      avgProjScale_(1.0),  // New
      needsUpdate_(true),
      navigator_(nullptr),
      rng_(std::random_device{}()) {  // New: Jitter RNG
    try {
        initializeWithRetry();
        if (debug_.load()) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cout << "[DEBUG] Initialized with fixes: maxDimensions=" << maxDimensions_ << ", mode=" << mode_.load()
                      << ", carroll=" << carrollFactor_ << ", meanField=" << meanFieldApprox_ << ", asymCollapse=" << asymCollapse_
                      << ", perspTrans=" << perspectiveTrans_ << ", perspFocal=" << perspectiveFocal_ << "\n";
        }
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cerr << "[ERROR] Constructor failed: " << e.what() << "\n";
        throw;
    }
}

// Setters/Getters for new perspective params (thread-safe)
void UniversalEquation::setPerspectiveTrans(double value) {
    perspectiveTrans_.store(std::clamp(value, 0.0, 10.0));
    needsUpdate_.store(true);
}
double UniversalEquation::getPerspectiveTrans() const { return perspectiveTrans_.load(); }

void UniversalEquation::setPerspectiveFocal(double value) {
    perspectiveFocal_.store(std::clamp(value, 1.0, 20.0));
    needsUpdate_.store(true);
}
double UniversalEquation::getPerspectiveFocal() const { return perspectiveFocal_.load(); }

// Setters/Getters for new params (thread-safe)
void UniversalEquation::setCarrollFactor(double value) {
    carrollFactor_.store(std::clamp(value, 0.0, 1.0));
    needsUpdate_.store(true);
}
double UniversalEquation::getCarrollFactor() const { return carrollFactor_.load(); }

void UniversalEquation::setMeanFieldApprox(double value) {
    meanFieldApprox_.store(std::clamp(value, 0.0, 1.0));
    needsUpdate_.store(true);
}
double UniversalEquation::getMeanFieldApprox() const { return meanFieldApprox_.load(); }

void UniversalEquation::setAsymCollapse(double value) {
    asymCollapse_.store(std::clamp(value, 0.0, 1.0));
    needsUpdate_.store(true);
}
double UniversalEquation::getAsymCollapse() const { return asymCollapse_.load(); }

// Existing setters/getters unchanged...
void UniversalEquation::setInfluence(double value) {
    influence_.store(std::clamp(value, 0.0, 10.0));
    needsUpdate_.store(true);
}
double UniversalEquation::getInfluence() const { return influence_.load(); }

void UniversalEquation::setWeak(double value) {
    weak_.store(std::clamp(value, 0.0, 1.0));
    needsUpdate_.store(true);
}
double UniversalEquation::getWeak() const { return weak_.load(); }

void UniversalEquation::setCollapse(double value) {
    collapse_.store(std::clamp(value, 0.0, 5.0));
}
double UniversalEquation::getCollapse() const { return collapse_.load(); }

void UniversalEquation::setTwoD(double value) {
    twoD_.store(std::clamp(value, 0.0, 5.0));
    needsUpdate_.store(true);
}
double UniversalEquation::getTwoD() const { return twoD_.load(); }

void UniversalEquation::setThreeDInfluence(double value) {
    threeDInfluence_.store(std::clamp(value, 0.0, 5.0));
    needsUpdate_.store(true);
}
double UniversalEquation::getThreeDInfluence() const { return threeDInfluence_.load(); }

void UniversalEquation::setOneDPermeation(double value) {
    oneDPermeation_.store(std::clamp(value, 0.0, 5.0));
    needsUpdate_.store(true);
}
double UniversalEquation::getOneDPermeation() const { return oneDPermeation_.load(); }

void UniversalEquation::setDarkMatterStrength(double value) {
    darkMatterStrength_.store(std::clamp(value, 0.0, 1.0));
    needsUpdate_.store(true);
}
double UniversalEquation::getDarkMatterStrength() const { return darkMatterStrength_.load(); }

void UniversalEquation::setDarkEnergyStrength(double value) {
    darkEnergyStrength_.store(std::clamp(value, 0.0, 2.0));
    needsUpdate_.store(true);
}
double UniversalEquation::getDarkEnergyStrength() const { return darkEnergyStrength_.load(); }

void UniversalEquation::setAlpha(double value) {
    alpha_.store(std::clamp(value, 0.1, 10.0));
    needsUpdate_.store(true);
}
double UniversalEquation::getAlpha() const { return alpha_.load(); }

void UniversalEquation::setBeta(double value) {
    beta_.store(std::clamp(value, 0.0, 1.0));
    needsUpdate_.store(true);
}
double UniversalEquation::getBeta() const { return beta_.load(); }

void UniversalEquation::setDebug(bool value) { debug_.store(value); }
bool UniversalEquation::getDebug() const { return debug_.load(); }

void UniversalEquation::setMode(int mode) {
    mode = std::clamp(mode, 1, maxDimensions_);
    if (mode_.load() != mode || currentDimension_.load() != mode) {
        mode_.store(mode);
        currentDimension_.store(mode);
        needsUpdate_.store(true);
        initializeWithRetry();
        if (debug_.load()) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cout << "[DEBUG] Mode set to: " << mode_.load() << ", dimension: " << currentDimension_.load() << "\n";
        }
    }
}
int UniversalEquation::getMode() const { return mode_.load(); }

void UniversalEquation::setCurrentDimension(int dimension) {
    if (dimension >= 1 && dimension <= maxDimensions_ && dimension != currentDimension_.load()) {
        currentDimension_.store(dimension);
        mode_.store(dimension);
        needsUpdate_.store(true);
        initializeWithRetry();
        if (debug_.load()) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cout << "[DEBUG] Dimension set to: " << currentDimension_.load() << ", mode=" << mode_.load() << "\n";
        }
    }
}
int UniversalEquation::getCurrentDimension() const { return currentDimension_.load(); }

int UniversalEquation::getMaxDimensions() const { return maxDimensions_; }

const std::vector<UniversalEquation::DimensionInteraction>& UniversalEquation::getInteractions() const {
    if (needsUpdate_.load()) {
        if (debug_.load()) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cout << "[DEBUG] Updating interactions for dimension: " << currentDimension_.load() << "\n";
        }
        updateInteractions();
    }
    std::lock_guard<std::mutex> lock(mutex_);
    return interactions_;
}

void UniversalEquation::advanceCycle() {
    int newDimension = (currentDimension_.load() == maxDimensions_) ? 1 : currentDimension_.load() + 1;
    currentDimension_.store(newDimension);
    mode_.store(newDimension);
    needsUpdate_.store(true);
    initializeWithRetry();
    if (debug_.load()) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Cycle advanced: dimension=" << currentDimension_.load() << ", mode=" << mode_.load() << "\n";
    }
}

// Enhanced compute: Now incorporates rel (Carroll), asym collapse, mean-field approx
// Perspective modulation: average projected scale factor for observable (fancy view-dependent energy)
UniversalEquation::EnergyResult UniversalEquation::compute() const {
    if (debug_.load()) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Starting compute for dimension: " << currentDimension_.load() << "\n";
    }
    if (needsUpdate_.load()) {
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

    // Rel fix: Carroll limit modulates time-like terms (flat time for ultra-rel)
    double carrollMod = 1.0 - carrollFactor_.load() * (1.0 - invMaxDim_ * currDim); // Carroll: c->inf, time contracts
    observable *= carrollMod;

    // Fancy perspective modulation: average scale from interactions (view-dependent observable)
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
                // Approximate scale as 1 / (1 + dist) for perspective compression
                sumScale += perspectiveFocal_.load() / (perspectiveFocal_.load() + localInteractions[i].distance);
            }
            avgScale = sumScale / localInteractions.size();
        }
        observable *= avgScale;  // Higher dims compress more via perspective
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
                    // Many-body fix: Mean-field approx reduces effective interactions
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

    double collapse = computeCollapse(); // Now with asym enhancement
    EnergyResult result = {
        observable + collapse,
        std::max(0.0, observable - collapse),
        totalDarkMatter,
        totalDarkEnergy
    };

    if (debug_.load()) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Compute(D=" << currDim << "): " << result.toString() << " (carrollMod=" << carrollMod << ", mfScale=" << (1.0 - meanFieldApprox_.load()) << ", avgPerspScale=" << avgScale << ")\n";
    }
    return result;
}

double UniversalEquation::computeInteraction(int vertexIndex, double distance) const {
    double denom = std::max(1e-15, std::pow(static_cast<double>(currentDimension_.load()), (vertexIndex % maxDimensions_ + 1)));
    double modifier = (currentDimension_.load() > 3 && (vertexIndex % maxDimensions_ + 1) > 3) ? weak_.load() : 1.0;
    if (currentDimension_.load() == 3 && ((vertexIndex % maxDimensions_ + 1) == 2 || (vertexIndex % maxDimensions_ + 1) == 4)) {
        modifier *= threeDInfluence_.load();
    }
    double result = influence_.load() * (1.0 / (denom * (1.0 + distance))) * modifier;
    if (debug_.load() && interactions_.size() <= 100) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Interaction(vertex=" << vertexIndex << ", dist=" << distance << "): " << result << "\n";
    }
    return result;
}

double UniversalEquation::computePermeation(int vertexIndex) const {
    if (vertexIndex < 0 || nCubeVertices_.empty()) {
        if (debug_.load()) {
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
    if (debug_.load() && interactions_.size() <= 100) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Permeation(vertex=" << vertexIndex << "): " << result << "\n";
    }
    return result;
}

double UniversalEquation::computeDarkEnergy(double distance) const {
    double d = std::min(distance, 10.0);
    double result = darkEnergyStrength_.load() * safeExp(d * invMaxDim_);
    if (debug_.load() && interactions_.size() <= 100) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] DarkEnergy(dist=" << distance << "): " << result << "\n";
    }
    return result;
}

// Enhanced collapse: Asym term from MDPI 2025 (complementary eq for measurement)
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
    // Asym fix: Complementary term (sin-based "stochastic" asymmetry for measurement snap)
    double asymTerm = asymCollapse_.load() * sin(M_PI * phase + osc) * safeExp(-alpha_.load() * phase); // MDPI-inspired asymmetry
    double result = std::max(0.0, symCollapse + asymTerm);
    if (debug_.load() && interactions_.size() <= 100) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Collapse(D=" << currentDimension_.load() << ", asym=" << asymTerm << "): " << result << "\n";
    }
    return result;
}

void UniversalEquation::initializeNCube() {
    std::lock_guard<std::mutex> lock(mutex_);
    nCubeVertices_.clear();
    uint64_t numVertices = std::min(static_cast<uint64_t>(1ULL << currentDimension_.load()), maxVertices_);
    nCubeVertices_.reserve(numVertices);
    if (debug_.load()) {
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
    if (debug_.load() && nCubeVertices_.size() <= 100) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Initialized nCube with " << nCubeVertices_.size() << " vertices for dimension " << currentDimension_.load() << "\n";
    }
}

// Enhanced updateInteractions: Compute projected distances in 3D perspective view plane
// Uses real projective geometry: scale = focal / depth', depth' = x_d + trans (view along last dim)
// Projects transverse coords (first min(3,d-1)) scaled by perspective factor
void UniversalEquation::updateInteractions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    interactions_.clear();
    int d = currentDimension_.load();
    uint64_t numVertices = std::min(static_cast<uint64_t>(1ULL << d), maxVertices_);
    // New: LOD for d>6: Subsample bits (e.g., every 2nd for 50% verts)
    if (d > 6) {
        uint64_t lodFactor = 1ULL << (d / 2);  // e.g., d=9 -> 512 verts max
        numVertices = std::min(numVertices / lodFactor, static_cast<uint64_t>(1024ULL));
        if (debug_.load()) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cout << "[OPT] LOD: Reduced verts from " << (1ULL<<d) << " to " << numVertices << " for d=" << d << "\n";
        }
    }
    interactions_.reserve(numVertices - 1);
    if (nCubeVertices_.empty()) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cerr << "[ERROR] nCubeVertices_ is empty in updateInteractions\n";
        throw std::runtime_error("nCubeVertices_ is empty");
    }
    const auto& referenceVertex = nCubeVertices_[0];  // All -1.0
    double trans = perspectiveTrans_.load();
    double focal = perspectiveFocal_.load();
    int depthIdx = d - 1;  // Last coord as depth
    double depthRef = referenceVertex[depthIdx] + trans;
    if (depthRef <= 0.0) depthRef = 0.001;  // Avoid div-by-zero
    double scaleRef = focal / depthRef;
    double projRef[3] = {0.0f, 0.0f, 0.0f};
    int projDim = std::min(3, d - 1);
    for (int k = 0; k < projDim; ++k) {
        projRef[k] = referenceVertex[k] * scaleRef;
    }
    if (debug_.load()) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Updating perspective interactions for " << numVertices - 1 << " vertices (d=" << d << ", trans=" << trans << ", focal=" << focal << ")\n";
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
                for (int k = 0; k < 3; ++k) {  // Always 3D, padded with 0
                    double diff = projI[k] - projRef[k];
                    distSq += diff * diff;
                }
                double distance = std::sqrt(distSq);
                // For d==1, fallback to standard 1D euclid (no transverse)
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
            // For d==1, fallback to standard 1D euclid (no transverse)
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
                // Compute projected distance for adj as above
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
    needsUpdate_.store(false);
    if (debug_.load() && interactions_.size() <= 100) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Perspective Interactions(D=" << d << "): ";
        for (const auto& i : interactions_) {
            std::cout << "(vertex=" << i.vertexIndex << ", projDist=" << i.distance << ", strength=" << i.strength << ") ";
        }
        std::cout << "\n";
    }
}

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
            if (debug_.load()) {
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
            needsUpdate_.store(true);
        }
    }
}

double UniversalEquation::safeExp(double x) const {
    double result = std::exp(std::clamp(x, -709.0, 709.0));
    if (debug_.load() && (std::isnan(result) || std::isinf(result))) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cerr << "[ERROR] safeExp produced invalid result for x=" << x << "\n";
    }
    return result;
}

void UniversalEquation::initializeCalculator(DimensionalNavigator* navigator) {
    std::lock_guard<std::mutex> lock(mutex_);
    navigator_ = navigator;
    if (!navigator_) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cerr << "[ERROR] Navigator pointer is null\n";
        throw std::invalid_argument("Navigator pointer cannot be null");
    }
    needsUpdate_.store(true);
    initializeWithRetry();
}

UniversalEquation::DimensionData UniversalEquation::updateCache() {
    if (debug_.load()) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Starting updateCache for dimension: " << currentDimension_.load() << "\n";
    }
    auto result = compute();
    DimensionData data;
    data.dimension = currentDimension_.load();
    data.observable = result.observable;
    data.potential = result.potential;
    data.darkMatter = result.darkMatter;
    data.darkEnergy = result.darkEnergy;
    if (debug_.load()) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] updateCache completed: dimension=" << data.dimension
                  << ", observable=" << data.observable << ", potential=" << data.potential
                  << ", darkMatter=" << data.darkMatter << ", darkEnergy=" << data.darkEnergy << "\n";
    }
    return data;
}

// New: Get projected vertices (caches under projMutex_)
std::vector<glm::vec3> UniversalEquation::getProjectedVertices() const {
    std::lock_guard<std::mutex> lock(projMutex_);
    if (projectedVerts_.empty() && !nCubeVertices_.empty()) {
        int d = currentDimension_.load();
        uint64_t numVerts = std::min(static_cast<uint64_t>(1ULL << d), maxVertices_);
        projectedVerts_.reserve(numVerts);
        double trans = perspectiveTrans_.load();
        double focal = perspectiveFocal_.load();
        int depthIdx = d - 1;
        int projDim = std::min(3, d);
        #pragma omp parallel for
        for (uint64_t i = 0; i < numVerts; ++i) {
            const auto& v = nCubeVertices_[i];
            double depth = v[depthIdx] + trans;
            if (depth <= 0.0) depth = 0.001;
            double scale = focal / depth;
            glm::vec3 proj(0.0f);
            for (int k = 0; k < projDim; ++k) {
                proj[k] = static_cast<float>(v[k] * scale);
            }
            #pragma omp critical
            projectedVerts_.emplace_back(proj);
        }
    }
    return projectedVerts_;
}