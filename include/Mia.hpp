#ifndef MIA_HPP
#define MIA_HPP

#include <atomic>
#include <thread>
#include <fstream>
#include <stdexcept>
#include <chrono>
#include <format>
#include <source_location>
#include <omp.h>
#include <vector>
#include <vulkan/vulkan.h>
#include <immintrin.h> // SIMD intrinsics
#include "engine/logging.hpp"
#include "core.hpp"

class Mia {
public:
    Mia(AMOURANTH* amouranth, const Logging::Logger& logger, VkDevice device, VkPhysicalDevice physicalDevice, VkQueue computeQueue)
        : amouranth_(amouranth), logger_(logger), running_(true), lastTime_(std::chrono::high_resolution_clock::now()),
          deltaTime_(0.0f), randomBuffer_(131'072), bufferIndex_(0), bufferSize_(0), // 1MB / 8 bytes
          device_(device), physicalDevice_(physicalDevice), computeQueue_(computeQueue),
          randomBufferVk_(VK_NULL_HANDLE), randomBufferMemory_(VK_NULL_HANDLE),
          timestampQueryPool_(VK_NULL_HANDLE), computePipeline_(VK_NULL_HANDLE),
          computePipelineLayout_(VK_NULL_HANDLE), computeDescriptorSetLayout_(VK_NULL_HANDLE),
          computeDescriptorSet_(VK_NULL_HANDLE) {
        // Initialize random buffer using /dev/urandom
        fillRandomBuffer();

        // Initialize Vulkan resources
        initializeVulkanResources();

        // Cache physics parameters
        updatePhysicsParams();

        // Start update thread
        updateThread_ = std::thread([this] { updateLoop(); });
        logger_.log(Logging::LogLevel::Debug, "Mia initialized for 13000+ FPS with 1MB random buffer", std::source_location::current());
    }

    ~Mia() {
        running_.store(false, std::memory_order_release);
        if (updateThread_.joinable()) {
            updateThread_.join();
        }
        // Clean up Vulkan resources
        if (randomBufferVk_ != VK_NULL_HANDLE) {
            vkDestroyBuffer(device_, randomBufferVk_, nullptr);
        }
        if (randomBufferMemory_ != VK_NULL_HANDLE) {
            vkFreeMemory(device_, randomBufferMemory_, nullptr);
        }
        if (timestampQueryPool_ != VK_NULL_HANDLE) {
            vkDestroyQueryPool(device_, timestampQueryPool_, nullptr);
        }
        if (computePipeline_ != VK_NULL_HANDLE) {
            vkDestroyPipeline(device_, computePipeline_, nullptr);
        }
        if (computePipelineLayout_ != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device_, computePipelineLayout_, nullptr);
        }
        if (computeDescriptorSetLayout_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device_, computeDescriptorSetLayout_, nullptr);
        }
        logger_.log(Logging::LogLevel::Debug, "Mia destroyed", std::source_location::current());
    }

    // Get elapsed time since last update (in seconds)
    float getDeltaTime() const {
        return deltaTime_.load(std::memory_order_relaxed);
    }

    // Generate a physics-driven random long double
    long double getRandom() {
        if (bufferIndex_ >= bufferSize_) {
            logger_.log(Logging::LogLevel::Error, "Random buffer empty, /dev/urandom failed",
                        std::source_location::current());
            throw std::runtime_error("Random buffer empty, /dev/urandom failed");
        }

        long double baseRandom;
        #pragma omp critical
        {
            uint64_t raw = randomBuffer_[bufferIndex_++];
            // SIMD-optimized normalization
            __m256d vec = _mm256_set1_pd(static_cast<double>(raw));
            __m256d max = _mm256_set1_pd(static_cast<double>(std::numeric_limits<uint64_t>::max()));
            vec = _mm256_div_pd(vec, max);
            baseRandom = static_cast<long double>(vec[0]);
        }

        // Polynomial mixing for richer entropy
        long double physicsFactor = godWaveFreq_ * (nurbEnergy_ + godWaveEnergy_ + spinEnergy_ * spinEnergy_ + momentumEnergy_ * fieldEnergy_);
        long double randomValue = baseRandom * physicsFactor;
        randomValue = std::fmod(randomValue, 1.0L); // Normalize to [0, 1)

        if (std::isnan(randomValue) || std::isinf(randomValue)) {
            logger_.log(Logging::LogLevel::Error, "Invalid random value: value={}",
                        std::source_location::current(), randomValue);
            throw std::runtime_error("Invalid random value");
        }

        return randomValue;
    }

    // Populate Vulkan storage buffer with random numbers for shaders
    void updateRandomBufferVk(VkCommandBuffer commandBuffer, uint32_t count) {
        // Use compute shader for bulk generation
        struct PushConstants {
            uint32_t count;
            uint32_t seed;
            long double nurbEnergy;
            long double godWaveEnergy;
            long double spinEnergy;
            long double momentumEnergy;
            long double fieldEnergy;
            long double godWaveFreq;
        } pushConstants;
        pushConstants.count = count;
        // Generate seed from /dev/urandom
        std::ifstream urandom("/dev/urandom", std::ios::binary);
        if (!urandom.is_open()) {
            logger_.log(Logging::LogLevel::Error, "Failed to open /dev/urandom for compute shader seed",
                        std::source_location::current());
            throw std::runtime_error("Failed to open /dev/urandom for compute shader seed");
        }
        urandom.read(reinterpret_cast<char*>(&pushConstants.seed), sizeof(uint32_t));
        urandom.close();
        pushConstants.nurbEnergy = nurbEnergy_;
        pushConstants.godWaveEnergy = godWaveEnergy_;
        pushConstants.spinEnergy = spinEnergy_;
        pushConstants.momentumEnergy = momentumEnergy_;
        pushConstants.fieldEnergy = fieldEnergy_;
        pushConstants.godWaveFreq = godWaveFreq_;

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline_);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout_, 0, 1, &computeDescriptorSet_, 0, nullptr);
        vkCmdPushConstants(commandBuffer, computePipelineLayout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants), &pushConstants);
        vkCmdDispatch(commandBuffer, (count + 255) / 256, 1, 1);

        // Barrier to ensure buffer is ready for shader use
        VkMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 1, &barrier, 0, nullptr, 0, nullptr);
    }

    // Write timestamp to Vulkan query pool
    void writeTimestamp(VkCommandBuffer commandBuffer, uint32_t queryIndex) {
        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, timestampQueryPool_, queryIndex);
    }

    // Get timestamp from query pool (in seconds)
    float getGpuTimestamp(uint32_t queryIndex) const {
        uint64_t timestamp;
        VkResult result = vkGetQueryPoolResults(device_, timestampQueryPool_, queryIndex, 1, sizeof(uint64_t), &timestamp, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
        if (result != VK_SUCCESS) {
            logger_.log(Logging::LogLevel::Warning, "Failed to get timestamp query: result={}",
                        std::source_location::current(), result);
            return deltaTime_.load(std::memory_order_relaxed);
        }
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(physicalDevice_, &properties);
        float nanosecondsPerTick = properties.limits.timestampPeriod;
        return static_cast<float>(timestamp) * nanosecondsPerTick / 1e9f;
    }

    // Get Vulkan buffer for random numbers
    VkBuffer getRandomBufferVk() const { return randomBufferVk_; }

private:
    void fillRandomBuffer() {
        std::ifstream urandom("/dev/urandom", std::ios::binary);
        if (!urandom.is_open()) {
            logger_.log(Logging::LogLevel::Error, "Failed to open /dev/urandom for random buffer",
                        std::source_location::current());
            throw std::runtime_error("Failed to open /dev/urandom for random buffer");
        }
        #pragma omp critical
        {
            urandom.read(reinterpret_cast<char*>(randomBuffer_.data()), randomBuffer_.size() * sizeof(uint64_t));
            bufferIndex_ = 0;
            bufferSize_ = randomBuffer_.size();
        }
        urandom.close();
    }

    void updatePhysicsParams() {
        const auto& cache = amouranth_->getCache();
        nurbEnergy_ = cache.empty() ? 1.0L : cache[0].nurbEnergy;
        godWaveEnergy_ = cache.empty() ? 1.0L : cache[0].GodWaveEnergy;
        spinEnergy_ = cache.empty() ? 0.032774L : cache[0].spinEnergy;
        momentumEnergy_ = cache.empty() ? 1.0L : cache[0].momentumEnergy;
        fieldEnergy_ = cache.empty() ? 1.0L : cache[0].fieldEnergy;
        physicsParamsValid_.store(true, std::memory_order_release);
    }

    void initializeVulkanResources() {
        // Create random number buffer
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = 131'072 * sizeof(long double); // 1MB for shader access
        bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device_, &bufferInfo, nullptr, &randomBufferVk_) != VK_SUCCESS) {
            logger_.log(Logging::LogLevel::Error, "Failed to create random buffer",
                        std::source_location::current());
            throw std::runtime_error("Failed to create random buffer");
        }

        // Allocate memory
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device_, randomBufferVk_, &memRequirements);
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (vkAllocateMemory(device_, &allocInfo, nullptr, &randomBufferMemory_) != VK_SUCCESS) {
            logger_.log(Logging::LogLevel::Error, "Failed to allocate random buffer memory",
                        std::source_location::current());
            throw std::runtime_error("Failed to allocate random buffer memory");
        }
        vkBindBufferMemory(device_, randomBufferVk_, randomBufferMemory_, 0);

        // Create timestamp query pool
        VkQueryPoolCreateInfo queryPoolInfo{};
        queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        queryPoolInfo.queryCount = 128; // Large pool for high-frequency queries
        if (vkCreateQueryPool(device_, &queryPoolInfo, nullptr, &timestampQueryPool_) != VK_SUCCESS) {
            logger_.log(Logging::LogLevel::Error, "Failed to create timestamp query pool",
                        std::source_location::current());
            throw std::runtime_error("Failed to create timestamp query pool");
        }

        // Create compute pipeline for GPU RNG
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 0;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &binding;
        if (vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &computeDescriptorSetLayout_) != VK_SUCCESS) {
            logger_.log(Logging::LogLevel::Error, "Failed to create compute descriptor set layout",
                        std::source_location::current());
            throw std::runtime_error("Failed to create compute descriptor set layout");
        }

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &computeDescriptorSetLayout_;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(uint32_t) * 2 + sizeof(long double) * 5; // count, seed, physics params
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
        if (vkCreatePipelineLayout(device_, &pipelineLayoutInfo, nullptr, &computePipelineLayout_) != VK_SUCCESS) {
            logger_.log(Logging::LogLevel::Error, "Failed to create compute pipeline layout",
                        std::source_location::current());
            throw std::runtime_error("Failed to create compute pipeline layout");
        }

        // Assume compute shader (xorshift.comp) is precompiled to SPIR-V
        VkShaderModule shaderModule = createShaderModule("xorshift.comp");
        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipelineInfo.stage.module = shaderModule;
        pipelineInfo.stage.pName = "main";
        pipelineInfo.layout = computePipelineLayout_;
        if (vkCreateComputePipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline_) != VK_SUCCESS) {
            logger_.log(Logging::LogLevel::Error, "Failed to create compute pipeline",
                        std::source_location::current());
            throw std::runtime_error("Failed to create compute pipeline");
        }
        vkDestroyShaderModule(device_, shaderModule, nullptr);
    }

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProperties);
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
        logger_.log(Logging::LogLevel::Error, "Failed to find suitable memory type",
                    std::source_location::current());
        throw std::runtime_error("Failed to find suitable memory type");
    }

    VkShaderModule createShaderModule(const std::string& filename) {
        // Placeholder: Load SPIR-V from file
        logger_.log(Logging::LogLevel::Debug, "Loading shader: {}", std::source_location::current(), filename);
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        // Assume file loading logic here
        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device_, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            logger_.log(Logging::LogLevel::Error, "Failed to create shader module: {}", std::source_location::current(), filename);
            throw std::runtime_error("Failed to create shader module");
        }
        return shaderModule;
    }

    void updateLoop() {
        #pragma omp parallel
        {
            #pragma omp single
            {
                while (running_.load(std::memory_order_relaxed)) {
                    #pragma omp task
                    {
                        // Update deltaTime
                        auto currentTime = std::chrono::high_resolution_clock::now();
                        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(currentTime - lastTime_.load(std::memory_order_relaxed));
                        deltaTime_.store(static_cast<float>(duration.count()) / 1e9f, std::memory_order_relaxed);
                        lastTime_.store(currentTime, std::memory_order_relaxed);
                    }
                    #pragma omp task
                    {
                        // Update random buffer and physics parameters
                        if (bufferIndex_ >= bufferSize_) {
                            fillRandomBuffer();
                        }
                        if (!physicsParamsValid_.load(std::memory_order_relaxed)) {
                            updatePhysicsParams();
                        }
                    }
                    #pragma omp taskwait
                    std::this_thread::sleep_for(std::chrono::microseconds(1)); // Match 13000+ FPS
                }
            }
        }
    }

    AMOURANTH* amouranth_;
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
    VkDevice device_;
    VkPhysicalDevice physicalDevice_;
    VkQueue computeQueue_;
    VkBuffer randomBufferVk_;
    VkDeviceMemory randomBufferMemory_;
    VkQueryPool timestampQueryPool_;
    VkPipeline computePipeline_;
    VkPipelineLayout computePipelineLayout_;
    VkDescriptorSetLayout computeDescriptorSetLayout_;
    VkDescriptorSet computeDescriptorSet_;
};

#endif // MIA_HPP