// Fancy-Ass RenderMode5: Vulkan/GLM Hyperdrive Edition (C++17, D=5)
// - Renders 5D n-Cube as spheres, modulated by observable/darkEnergy
// - Constexpr constants, inlined sin/cos, GLM swizzles for cache-friendly math
// - Dry humor: "Math’s having an existential crisis" warnings, "n-Cube’s 5D meltdown"
// - Vulkan: Per-draw push constants, zoom-clamped proj, early-exit guards
// - C++17: std::copy_if + std::for_each, explicit lambda params
// Compile: g++ -O3 -std=c++17 -Wall -Wextra -g -fopenmp -Iinclude

#include "core.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <vulkan/vulkan.h>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <vector>

static constexpr int kMaxRenderedDimensions = 9;
static constexpr float kMinZoom = 0.01f;
static constexpr float kMaxValueClamp = 1.3f;
static constexpr float kOscAmp = 0.2f;
static constexpr float kScaleBias = 0.3f;
static constexpr float kRadiusBase = 3.0f;
static constexpr float kSphereScale = 0.26f;  // Smaller for 5D
static constexpr float kInteractScale = 0.16f;
static constexpr float kZOffset = 14.0f;  // Further for 5D
static constexpr float kCamNear = 0.1f;
static constexpr float kCamFar = 1000.0f;
static constexpr float kFovRad = glm::radians(45.0f);
static constexpr float kExpDecay = -1.0f;
static constexpr float kPermeateMin = 0.01f;
static constexpr glm::vec3 kDefaultColor{0.8f, 0.9f, 0.95f};
static constexpr glm::vec3 kCamUp{0.0f, 1.0f, 0.0f};
static constexpr glm::vec3 kCamTarget{0.0f, 0.0f, 0.0f};

namespace {
    // Osc/value
    auto makeOscValue(const DimensionData& cacheEntry, float wavePhase) {
        return [wavePhase, &cacheEntry](float baseOsc = 1.0f) -> float {
            const float deMod = static_cast<float>(cacheEntry.darkEnergy) * 0.65f;
            const float sinProd = sinf(wavePhase * (1.0f + deMod));
            const float osc = baseOsc + kOscAmp * sinProd;
            const float rawValue = static_cast<float>(cacheEntry.observable * osc);
            return std::clamp(rawValue, kMinZoom, kMaxValueClamp);
        };
    }

    // Precomp cycle
    auto precompCycle(float wavePhase) {
        return std::fmod(wavePhase / (2.0f * kMaxRenderedDimensions), 1.0f);
    }

    // Model builder
    auto buildModel(float angle, float cycleProgress, float wavePhase, float scaleFactor, const glm::vec3& posOffset = {}) {
        return [angle, cycleProgress, wavePhase, scaleFactor, posOffset](glm::mat4 base = glm::mat4(1.0f)) -> glm::mat4 {
            const float rotAngle = wavePhase * 0.7f;  // Faster for 5D
            const glm::vec3 rotAxis{sinf(angle * 0.6f), cosf(angle * 0.6f), 0.2f};
            return glm::rotate(glm::scale(glm::translate(base, posOffset), glm::vec3(kSphereScale * scaleFactor)),
                               rotAngle, rotAxis);
        };
    }

    // Color gen
    auto genBaseColor(float wavePhase, size_t i, float cycleProgress) {
        return glm::vec3(
            0.6f + 0.4f * cosf(wavePhase + static_cast<float>(i) * 1.3f + cycleProgress),
            0.5f + 0.2f * sinf(wavePhase + static_cast<float>(i) * 1.0f),
            0.8f - 0.2f * cosf(wavePhase * 0.8f + static_cast<float>(i))
        );
    }

    // Interaction strength
    auto computeStrength(const AMOURANTH& amour, const UniversalEquation::DimensionInteraction& pair, float alpha) {
        return glm::max(kPermeateMin, glm::min(kMaxValueClamp,
            static_cast<float>(amour.computeInteraction(pair.vertexIndex, pair.distance) *
                               std::exp(kExpDecay * glm::abs(alpha * pair.distance)) *
                               amour.computePermeation(pair.vertexIndex) *
                               glm::max(0.0f, static_cast<float>(pair.strength)))));
    }

    // Pos offset: 5D-adjusted
    auto genOffsetPos(float dist, float strength, float angle, float cycleProgress) {
        const float offsetMult = dist * 0.5f * (1.0f + static_cast<float>(strength) * 0.6f);  // Tighter for 5D
        return glm::vec3(
            offsetMult * cosf(angle + cycleProgress),
            offsetMult * sinf(angle + cycleProgress),
            offsetMult * 0.3f * sinf(angle * 0.75f)
        );
    }
}

void renderMode5(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer, VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase, const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout) {
    (void)imageIndex;

    // Bind buffers
    VkBuffer vertexBuffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    // Early-exit
    if (amouranth->getSphereIndices().empty()) {
        std::cerr << "Warning: Sphere indices empty. Math’s having an existential crisis.\n";
        return;
    }

    // Proj
    const float zoomFactor = std::max(zoomLevel, kMinZoom);
    const float aspect = static_cast<float>(width) / std::max(1.0f, static_cast<float>(height));
    const glm::mat4 proj = glm::perspective(kFovRad, aspect, kCamNear, kCamFar);

    // Cam
    const glm::vec3 camPos = amouranth->isUserCamActive() ? amouranth->getUserCamPos() : glm::vec3(0.0f, 0.0f, kZOffset * zoomFactor);
    const glm::mat4 view = glm::lookAt(camPos, kCamTarget, kCamUp);

    // Cycle
    const float cycleProgress = precompCycle(wavePhase);

    // Cache guard
    if (cache.size() < kMaxRenderedDimensions) {
        std::cerr << "Warning: Cache size " << cache.size() << " < " << kMaxRenderedDimensions << ". Dimensions slacking.\n";
        return;
    }

    // Dim 5: Index 4
    constexpr size_t i = 4;
    if (cache[i].dimension != 5) {
        std::cerr << "Warning: Invalid cache for dimension 5. Math’s 5D meltdown.\n";
        return;
    }

    // Osc/value
    const auto oscValue = makeOscValue(cache[i], wavePhase);
    const float value = oscValue(1.0f);

    // Angle/scale/radius
    const float angle = wavePhase + 5 * 2.0f * glm::pi<float>() / kMaxRenderedDimensions;
    const float scaleFactor = 1.0f + static_cast<float>(cache[i].observable) * kScaleBias;
    const float radius = kRadiusBase * scaleFactor;
    const glm::vec3 pos{
        radius * cosf(angle + cycleProgress),
        radius * sinf(angle + cycleProgress),
        radius * sinf(wavePhase + static_cast<float>(i) * 0.6f) * 0.3f
    };

    // Model
    const auto modelBuilder = buildModel(angle, cycleProgress, wavePhase, scaleFactor, pos);
    const glm::mat4 model = modelBuilder();

    // Color
    const glm::vec3 baseColor = genBaseColor(wavePhase, i, cycleProgress);

    // Push main sphere
    const PushConstants pushConstants = {
        model, view, proj, baseColor, value, 5.0f, wavePhase, cycleProgress,
        static_cast<float>(cache[i].darkMatter), static_cast<float>(cache[i].darkEnergy)
    };
    vkCmdPushConstants(commandBuffer, pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(PushConstants), &pushConstants);
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getSphereIndices().size()), 1, 0, 0, 0);

    // Set dim, fetch pairs
    amouranth->setCurrentDimension(5);
    const auto pairs = amouranth->getInteractions();
    if (pairs.empty()) {
        const glm::mat4 fallbackModel = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f)), glm::vec3(kSphereScale * zoomFactor));
        const PushConstants fallbackPush = {fallbackModel, view, proj, kDefaultColor, 0.4f, 5.0f, wavePhase, cycleProgress, 0.4f, 0.4f};
        vkCmdPushConstants(commandBuffer, pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(PushConstants), &fallbackPush);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getSphereIndices().size()), 1, 0, 0, 0);
        std::cerr << "Warning: No interactions for dimension 5. Math’s lonely.\n";
        return;
    }

    // Mode guard
    const auto modeGuard = [&amouranth]() { return amouranth->getMode() == 5; };

    // Interactions loop
    std::vector<UniversalEquation::DimensionInteraction> filteredPairs;
    filteredPairs.reserve(pairs.size());
    std::copy_if(pairs.begin(), pairs.end(), std::back_inserter(filteredPairs),
                 [&modeGuard](const UniversalEquation::DimensionInteraction& p) { (void)p; return modeGuard(); });
    std::for_each(filteredPairs.begin(), filteredPairs.end(), [&](const UniversalEquation::DimensionInteraction& pair) {
        const float alpha = amouranth->getAlpha();
        const float interactionStrength = computeStrength(*amouranth, pair, alpha);
        const float iAngle = wavePhase + static_cast<float>(pair.vertexIndex) * 1.9f + static_cast<float>(pair.distance) * 0.6f;
        const glm::vec3 offsetPos = genOffsetPos(static_cast<float>(pair.distance), static_cast<float>(pair.strength), iAngle, cycleProgress);

        const glm::mat4 iModel = glm::scale(glm::translate(glm::mat4(1.0f), offsetPos), glm::vec3(kInteractScale * zoomFactor));

        const glm::vec3 iColor{
            0.8f - 0.2f * sinf(iAngle),
            0.7f - 0.15f * cosf(iAngle * 1.6f),
            1.0f - 0.1f * sinf(iAngle * 1.2f)
        };

        const float strengthMod = interactionStrength * (0.6f + 0.2f * cosf(wavePhase + static_cast<float>(pair.distance)));
        const float deCompute = static_cast<float>(amouranth->computeDarkEnergy(pair.distance));

        const PushConstants iPush = {iModel, view, proj, iColor, strengthMod, 5.0f, wavePhase, cycleProgress,
                                     static_cast<float>(pair.strength), deCompute};
        vkCmdPushConstants(commandBuffer, pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(PushConstants), &iPush);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getSphereIndices().size()), 1, 0, 0, 0);
    });
}