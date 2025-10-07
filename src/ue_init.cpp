// ue_init.cpp: Implementation of UniversalEquation for AMOURANTH RTX Physics.
// Computes multidimensional physics for 30,000 balls in 8 dimensions.
// Thread-safe with std::atomic and OpenMP; uses C++20 features.
// Dependencies: GLM, OpenMP, C++20 standard library.
// Zachary Geurts 2025

#include "ue_init.hpp"
#include "engine/core.hpp"
#include <glm/glm.hpp>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <syncstream>

void UniversalEquation::initializeNCube() {
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

void UniversalEquation::updateInteractions() const {
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

void UniversalEquation::initializeWithRetry() {
    initializeNCube();
    updateInteractions();
}