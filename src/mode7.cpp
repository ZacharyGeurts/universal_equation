// File: mode7.cpp (new, based on UniversalEquation math for dimension 7)
// Mode 7: Pulsating sphere for dimension 7, integrating oneDPermeation proxy (though dim>1, for permeation cycle)
// and enhanced darkEnergy exp(d * invMaxDim). Rotations include diagonal axis for higher-D feel.
// Uses simplified 128-byte PushConstants { mat4 model; mat4 view_proj; }

#include "core.hpp"
#include <iostream>
#include <vulkan/vulkan.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

void renderMode7(AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout) {

    // Enhanced local function for dim 7, with permeation and darkEnergy enhancement
    auto makeOscValue = [](const DimensionData& cacheEntry, float wavePhase) -> float {
        constexpr float omegaApprox = 0.33f;
        constexpr float weakMod = 0.7f;
        constexpr float meanFieldDamp = 0.75f;
        constexpr float asymCollapse = 0.4f;
        constexpr float alphaProxy = 2.0f;
        constexpr float oneDPermProxy = 1.2f;  // Proxy for oneDPermeation_ in permeation for cycle
        constexpr float invMaxDim = 1.0f / 20.0f;
        const float twoDMod = std::cos(omegaApprox * 7.0f) * 0.8f * oneDPermProxy;
        const float deMod = static_cast<float>(cacheEntry.darkEnergy) * std::exp(7.0f * invMaxDim) * 0.65f;  // From computeDarkEnergy: deStrength * exp(d * invMaxDim)
        const float dmMod = static_cast<float>(cacheEntry.darkMatter) * 0.45f;
        const float phase = 7.0f / 40.0f;
        const float osc = std::abs(std::cos(2.0f * M_PI * phase));
        const float vertexFactorProxy = 0.5f;
        const float asymTermProxy = asymCollapse * std::sin(M_PI * phase + osc + vertexFactorProxy) * std::exp(-alphaProxy * phase);
        const float carrollMod = 1.0f - 0.5f * (1.0f - (7.0f / 20.0f));
        const float oscSin = std::sin(wavePhase + deMod + dmMod + twoDMod + asymTermProxy) * weakMod;
        const float oscCos = std::cos(wavePhase + dmMod);
        const float rawValue = static_cast<float>((cacheEntry.observable * oscSin + cacheEntry.potential * oscCos * twoDMod + asymTermProxy) * meanFieldDamp * carrollMod);
        return rawValue;
    };

    // Find the entry for dimension 7
    const DimensionData* dimData = nullptr;
    for (const auto& entry : cache) {
        if (entry.dimension == 7) {
            dimData = &entry;
            break;
        }
    }

    if (!dimData) {
        std::cerr << "Warning: No data found for dimension 7 in cache.\n";
        return;
    }

    const auto oscValue = makeOscValue(*dimData, wavePhase);

    constexpr float kScaleBias = 0.5f;
    constexpr float lodScaleProxy = 0.7f;  // Stronger LOD for d=7, lodFactor=1<<3=8
    const float avgScaleProxy = 10.0f / (10.0f + 7.0f / 10.0f);
    const float scaleFactor = (1.0f + std::abs(oscValue) * kScaleBias) * avgScaleProxy * lodScaleProxy;

    // Rotations with diagonal axis for 7D complexity
    glm::mat4 model = glm::mat4(1.0f);
    const float carrollMod = 1.0f - 0.5f * (1.0f - 7.0f / 20.0f);
    const float rotY = glm::radians(static_cast<float>(7) * 40.0f * carrollMod + wavePhase * 0.5f);
    const float rotX = glm::radians(static_cast<float>(dimData->potential * 20.0f * carrollMod));
    const float rotZ = glm::radians(static_cast<float>(dimData->darkMatter * 15.0f * carrollMod));
    const float rotDiag = glm::radians(static_cast<float>(2.5f * wavePhase * carrollMod));  // Diagonal axis
    glm::vec3 diagAxis(1.0f, 1.0f, 1.0f);
    model = glm::rotate(glm::rotate(glm::rotate(glm::rotate(model, rotDiag, diagAxis), rotZ, glm::vec3(0.0f, 0.0f, 1.0f)), rotX, glm::vec3(1.0f, 0.0f, 0.0f)), rotY, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(scaleFactor * zoomLevel));

    // Dynamic cam for 7D
    glm::vec3 camPos = amouranth->isUserCamActive() ? amouranth->getUserCamPos() : glm::vec3(0.0f, 0.0f, -45.0f + static_cast<float>(dimData->darkEnergy) * -7.0f);

    glm::mat4 view = glm::lookAt(camPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    glm::mat4 proj = glm::perspective(glm::radians(45.0f), static_cast<float>(width) / static_cast<float>(height), 0.1f, 100.0f);

    proj[1][1] *= -1; // Vulkan clip space adjustment

    glm::mat4 viewProj = proj * view;

    // Prepare simplified push constants
    PushConstants pc{};
    pc.model = model;
    pc.view_proj = viewProj;

    // Bind buffers
    VkDeviceSize offsets = 0;
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, &offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    // Push constants to shader
    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pc);

    // Draw the sphere for dimension 7
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getSphereIndices().size()), 1, 0, 0, 0);
}