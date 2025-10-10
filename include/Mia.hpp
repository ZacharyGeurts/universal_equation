#pragma once
#ifndef MIA_HPP
#define MIA_HPP

#include <atomic>
#include <thread>
#include <random>
#include <fstream>
#include <stdexcept>
#include <chrono>
#include <format>
#include <source_location>
#include <omp.h>
#include <vector>
#include <vulkan/vulkan.h>
#include "engine/logging.hpp"
#include "ue_init.hpp"

class Mia {
public:
    Mia(const AMOURANTH* amouranth, const Logging::Logger& logger)
        : amouranth_(amouranth), logger_(logger), running_(true), lastTime_(std::chrono::high_resolution_clock::now()),
          deltaTime_(0.0f), randomBuffer_(131'072), bufferIndex_(0), bufferSize_(0) {
        std::ifstream urandom("/dev/urandom", std::ios::binary);
        if (urandom.is_open()) {
            uint32_t seed;
            urandom.read(reinterpret_cast<char*>(&seed), sizeof(seed));
            urandom.close();
            rng_.seed(seed);
        } else {
            logger_.log(Logging::LogLevel::Warning, "Mia", "Failed to open /dev/urandom, falling back to std::random_device",
                        std::source_location::current());
            std::random_device rd;
            rng_.seed(rd());
        }

        fillRandomBuffer();
        updatePhysicsParams();
        updateThread_ = std::thread([this] { updateLoop(); });
        logger_.log(Logging::LogLevel::Debug, "Mia", "Mia initialized for 13000+ FPS with 1MB random buffer",
                    std::source_location::current());
    }

    ~Mia() {
        running_.store(false, std::memory_order_release);
        if (updateThread_.joinable()) {
            updateThread_.join();
        }
        logger_.log(Logging::LogLevel::Debug, "Mia", "Mia destroyed", std::source_location::current());
    }

    float getDeltaTime() const {
        return deltaTime_.load(std::memory_order_relaxed);
    }

    long double getRandom() {
        long double baseRandom;
        if (bufferIndex_ < bufferSize_) {
            #pragma omp critical
            {
                uint64_t raw = randomBuffer_[bufferIndex_++];
                baseRandom = static_cast<long double>(raw) / static_cast<long double>(std::numeric_limits<uint64_t>::max());
            }
        } else {
            std::uniform_real_distribution<long double> dist(0.0L, 1.0L);
            baseRandom = dist(rng_);
        }

        long double physicsFactor = godWaveFreq_ * (nurbEnergy_ + godWaveEnergy_ + spinEnergy_ + momentumEnergy_ + fieldEnergy_);
        long double randomValue = baseRandom * physicsFactor;
        randomValue = std::fmod(randomValue, 1.0L);

        if (std::isnan(randomValue) || std::isinf(randomValue)) {
            logger_.log(Logging::LogLevel::Warning, "Mia", "Invalid random value, returning fallback: value={}",
                        std::source_location::current(), randomValue);
            std::uniform_real_distribution<long double> dist(0.0L, 1.0L);
            return dist(rng_);
        }

        return randomValue;
    }

private:
    void fillRandomBuffer() {
        std::ifstream urandom("/dev/urandom", std::ios::binary);
        if (urandom.is_open()) {
            #pragma omp critical
            {
                urandom.read(reinterpret_cast<char*>(randomBuffer_.data()), randomBuffer_.size() * sizeof(uint64_t));
                bufferIndex_ = 0;
                bufferSize_ = randomBuffer_.size();
            }
            urandom.close();
        } else {
            logger_.log(Logging::LogLevel::Warning, "Mia", "Failed to fill random buffer, using std::mt19937_64",
                        std::source_location::current());
            bufferSize_ = 0;
        }
    }

    void updatePhysicsParams() {
        const auto& cache = amouranth_->getCache();
        nurbEnergy_ = cache.empty() ? 1.0L : cache[0].nurbEnergy;
        godWaveEnergy_ = cache.empty() ? 1.0L : cache[0].nurbEnergy;
        spinEnergy_ = cache.empty() ? 0.032774L : cache[0].nurbMatter;
        momentumEnergy_ = cache.empty() ? 1.0L : cache[0].potential;
        fieldEnergy_ = cache.empty() ? 1.0L : cache[0].observable;
        physicsParamsValid_.store(true, std::memory_order_release);
    }

    void updateLoop() {
        #pragma omp parallel
        {
            #pragma omp single
            {
                while (running_.load(std::memory_order_relaxed)) {
                    #pragma omp task
                    {
                        auto currentTime = std::chrono::high_resolution_clock::now();
                        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(currentTime - lastTime_.load(std::memory_order_relaxed));
                        deltaTime_.store(static_cast<float>(duration.count()) / 1e9f, std::memory_order_relaxed);
                        lastTime_.store(currentTime, std::memory_order_relaxed);
                    }
                    #pragma omp task
                    {
                        if (bufferIndex_ >= bufferSize_) {
                            fillRandomBuffer();
                        }
                        if (!physicsParamsValid_.load(std::memory_order_relaxed)) {
                            updatePhysicsParams();
                        }
                    }
                    #pragma omp taskwait
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                }
            }
        }
    }

    const AMOURANTH* amouranth_;
    const Logging::Logger& logger_;
    std::atomic<bool> running_;
    std::atomic<std::chrono::high_resolution_clock::time_point> lastTime_;
    std::atomic<float> deltaTime_;
    std::vector<uint64_t> randomBuffer_;
    std::atomic<size_t> bufferIndex_;
    std::atomic<size_t> bufferSize_;
    std::atomic<bool> physicsParamsValid_{false};
    long double nurbEnergy_{1.0L};
    long double godWaveEnergy_{1.0L};
    long double spinEnergy_{0.032774L};
    long double momentumEnergy_{1.0L};
    long double fieldEnergy_{1.0L};
    const long double godWaveFreq_{1.0L};
    std::thread updateThread_;
    std::mt19937_64 rng_;
};

#endif // MIA_HPP