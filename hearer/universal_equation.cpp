// ue engine
#include "universal_equation.hpp"

UniversalEquation::UniversalEquation(int maxDimensions, int mode, double influence,
                                   double weak, double collapse, double twoD,
                                   double threeDInfluence, double oneDPermeation,
                                   double darkMatterStrength, double darkEnergyStrength,
                                   double alpha, double beta, bool debug)
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
      debug_(debug),
      omega_(maxDimensions_ > 0 ? 2.0 * M_PI / (2 * maxDimensions_ - 1) : 1.0),
      invMaxDim_(maxDimensions_ > 0 ? 1.0 / maxDimensions_ : 1e-15),
      interactions_(),
      nCubeVertices_(),
      needsUpdate_(true),
      navigator_(nullptr) {
    try {
        initializeWithRetry();
        if (debug_.load()) {
            std::lock_guard<std::mutex> lock(debugMutex_);
            std::cout << "[DEBUG] Initialized: maxDimensions=" << maxDimensions_ << ", mode=" << mode_.load()
                      << ", currentDimension=" << currentDimension_.load() << ", maxVertices=" << maxVertices_ << "\n";
        }
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cerr << "[ERROR] Constructor failed: " << e.what() << "\n";
        throw;
    }
}

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
            std::cout << "[DEBUG] Dimension set to: " << currentDimension_.load() << ", mode: " << mode_.load() << "\n";
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

    double totalDarkMatter = 0.0, totalDarkEnergy = 0.0, interactionSum = 0.0;
    {
        std::vector<DimensionInteraction> localInteractions;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            localInteractions = interactions_;
        }
        // Only parallelize for large vertex counts to reduce overhead
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
                    interactionSum += influence * safeExp(-alpha_.load() * interaction.distance) * permeation * darkMatter;
                    totalDarkMatter += darkMatter * influence * permeation;
                    totalDarkEnergy += darkEnergy * influence * permeation;
                }
            }
        } else {
            for (const auto& interaction : localInteractions) {
                double influence = interaction.strength;
                double permeation = computePermeation(interaction.vertexIndex);
                double darkMatter = darkMatterStrength_.load();
                double darkEnergy = computeDarkEnergy(interaction.distance);
                interactionSum += influence * safeExp(-alpha_.load() * interaction.distance) * permeation * darkMatter;
                totalDarkMatter += darkMatter * influence * permeation;
                totalDarkEnergy += darkEnergy * influence * permeation;
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

    if (debug_.load()) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Compute(D=" << currDim << "): " << result.toString() << "\n";
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
    // Only parallelize for large dimensions
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
    double result = std::max(0.0, collapse_.load() * currentDimension_.load() * safeExp(-beta_.load() * (currentDimension_.load() - 1)) * (0.8 * osc + 0.2));
    if (debug_.load() && interactions_.size() <= 100) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Collapse(D=" << currentDimension_.load() << "): " << result << "\n";
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
    // Only parallelize for large vertex counts
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

void UniversalEquation::updateInteractions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    interactions_.clear();
    uint64_t numVertices = std::min(static_cast<uint64_t>(1ULL << currentDimension_.load()), maxVertices_);
    interactions_.reserve(numVertices - 1);
    if (nCubeVertices_.empty()) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cerr << "[ERROR] nCubeVertices_ is empty in updateInteractions\n";
        throw std::runtime_error("nCubeVertices_ is empty");
    }
    const auto& referenceVertex = nCubeVertices_[0];
    if (debug_.load()) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Updating interactions for " << numVertices - 1 << " vertices\n";
    }
    // Only parallelize for large vertex counts
    if (numVertices > 1000) {
        #pragma omp parallel
        {
            std::vector<DimensionInteraction> localInteractions;
            localInteractions.reserve((numVertices - 1) / omp_get_num_threads() + 1);
            #pragma omp for
            for (uint64_t i = 1; i < numVertices; ++i) {
                double distance = 0.0;
                for (int j = 0; j < currentDimension_.load(); ++j) {
                    double diff = nCubeVertices_[i][j] - referenceVertex[j];
                    distance += diff * diff;
                }
                distance = std::sqrt(distance);
                double strength = computeInteraction(static_cast<int>(i), distance);
                localInteractions.push_back(DimensionInteraction(static_cast<int>(i), distance, strength));
            }
            #pragma omp critical
            {
                interactions_.insert(interactions_.end(), localInteractions.begin(), localInteractions.end());
            }
        }
    } else {
        for (uint64_t i = 1; i < numVertices; ++i) {
            double distance = 0.0;
            for (int j = 0; j < currentDimension_.load(); ++j) {
                double diff = nCubeVertices_[i][j] - referenceVertex[j];
                distance += diff * diff;
            }
            distance = std::sqrt(distance);
            double strength = computeInteraction(static_cast<int>(i), distance);
            interactions_.push_back(DimensionInteraction(static_cast<int>(i), distance, strength));
        }
    }
    if (currentDimension_.load() == 3) {
        for (int adj : {2, 4}) {
            size_t vertexIndex = static_cast<size_t>(adj - 1);
            if (vertexIndex < nCubeVertices_.size() &&
                std::none_of(interactions_.begin(), interactions_.end(),
                             [adj](const auto& i) { return i.vertexIndex == adj; })) {
                double distance = 0.0;
                for (int j = 0; j < currentDimension_.load(); ++j) {
                    double diff = nCubeVertices_[vertexIndex][j] - referenceVertex[j];
                    distance += diff * diff;
                }
                distance = std::sqrt(distance);
                double strength = computeInteraction(static_cast<int>(vertexIndex), distance);
                interactions_.push_back(DimensionInteraction(static_cast<int>(vertexIndex), distance, strength));
            }
        }
    }
    needsUpdate_.store(false);
    if (debug_.load() && interactions_.size() <= 100) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[DEBUG] Interactions(D=" << currentDimension_.load() << "): ";
        for (const auto& i : interactions_) {
            std::cout << "(vertex=" << i.vertexIndex << ", dist=" << i.distance << ", strength=" << i.strength << ") ";
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
                // Only parallelize for large maxDimensions_
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