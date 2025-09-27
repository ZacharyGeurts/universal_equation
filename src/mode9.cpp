// File: mode9.cpp (fixed: cast floats to int for % operator to match integer semantics in computeInteraction)
// Mode 9: Pulsating sphere for dimension 9, full integration with safeExp clamps and vertexMagnitude in permeation.
// Final rotations cycle through all axes with full term proxies. Heavy LOD and perspective.
// Uses simplified 128-byte PushConstants { mat4 model; mat4 view_proj; }

#include "core.hpp"
#include <iostream>
#include <vulkan/vulkan.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

void renderMode9(AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout) {

    // Enhanced local function for dim 9, with vertexMagnitude permeation and safeExp
    auto makeOscValue = [](const DimensionData& cacheEntry, float wavePhase) -> float {
        constexpr float omegaApprox = 0.33f;
        constexpr float weakMod = 0.7f;
        constexpr float meanFieldDamp = 0.65f;
        constexpr float asymCollapse = 0.4f;
        constexpr float alphaProxy = 2.0f;
        constexpr float betaProxy = 0.5f;
        constexpr float oneDPermProxy = 1.2f;
        constexpr float invMaxDim = 1.0f / 20.0f;
        constexpr float safeClampLow = -709.0f;
        constexpr float safeClampHigh = 709.0f;
        const int dimInt = 9;
        const int maxDInt = 20;
        const float denomProxy = std::pow(static_cast<float>(dimInt), (static_cast<int>(dimInt) % maxDInt + 1));  // Fixed: cast to int for %
        const float invDenom = 1.0f / std::max(1e-15f, static_cast<float>(denomProxy));
        // Vertex magnitude proxy: sqrt(sum v[i]^2 for i<dim), v[i]=±1, approx sqrt(dim/2)
        const float vertexMagProxy = std::sqrt(9.0f / 2.0f);
        const float permeationProxy = 1.0f + 0.5f * vertexMagProxy / 9.0f;  // beta * mag / dim
        const float twoDMod = std::cos(omegaApprox * 9.0f) * 0.8f * oneDPermProxy * invDenom * permeationProxy;
        const float deArg = std::clamp(9.0f * invMaxDim, safeClampLow, safeClampHigh);
        const float deMod = static_cast<float>(cacheEntry.darkEnergy) * std::exp(deArg) * 0.65f;
        const float dmMod = static_cast<float>(cacheEntry.darkMatter) * 0.45f;
        const float phase = 9.0f / 40.0f;
        const float osc = std::abs(std::cos(2.0f * M_PI * phase));
        const float vertexFactorProxy = 0.5f;
        const float asymTermArg = -alphaProxy * phase;
        const float asymTermProxy = asymCollapse * std::sin(M_PI * phase + osc + vertexFactorProxy) * std::exp(std::clamp(asymTermArg, safeClampLow, safeClampHigh)) * std::exp(-betaProxy * (9.0f - 1.0f));
        const float carrollMod = 1.0f - 0.5f * (1.0f - (9.0f / 20.0f));
        const float oscSinArg = wavePhase + deMod + dmMod + twoDMod + asymTermProxy;
        const float oscSin = std::sin(oscSinArg) * weakMod * invDenom * permeationProxy;
        const float oscCos = std::cos(wavePhase + dmMod);
        const float rawValue = static_cast<float>((cacheEntry.observable * oscSin + cacheEntry.potential * oscCos * twoDMod + asymTermProxy) * meanFieldDamp * carrollMod);
        return rawValue;
    };

    // Find the entry for dimension 9
    const DimensionData* dimData = nullptr;
    for (const auto& entry : cache) {
        if (entry.dimension == 9) {
            dimData = &entry;
            break;
        }
    }

    if (!dimData) {
        std::cerr << "Warning: No data found for dimension 9 in cache.\n";
        return;
    }

    const auto oscValue = makeOscValue(*dimData, wavePhase);

    constexpr float kScaleBias = 0.5f;
    constexpr float lodScaleProxy = 0.5f;  // Heavy LOD for d=9, lodFactor=1<<4=16
    const float avgScaleProxy = 10.0f / (10.0f + 9.0f / 10.0f);
    const float scaleFactor = (1.0f + std::abs(oscValue) * kScaleBias) * avgScaleProxy * lodScaleProxy;

    // Full axis cycle rotations for 9D
    glm::mat4 model = glm::mat4(1.0f);
    const float carrollMod = 1.0f - 0.5f * (1.0f - 9.0f / 20.0f);
    const float betaMod = std::exp(-0.5f * 8.0f);
    const float rotY = glm::radians(static_cast<float>(9) * 40.0f * carrollMod + wavePhase * 0.5f * betaMod);
    const float rotX = glm::radians(static_cast<float>(dimData->potential * 20.0f * carrollMod * betaMod));
    const float rotZ = glm::radians(static_cast<float>(dimData->darkMatter * 15.0f * carrollMod * betaMod));
    const float rotW = glm::radians(static_cast<float>(3.5f * wavePhase * carrollMod * betaMod));
    const float rotDiag = glm::radians(static_cast<float>(dimData->darkEnergy * 10.0f));  // de for extra twist
    glm::vec3 diagAxis(1.0f, 1.0f, 1.0f);
    model = glm::rotate(glm::rotate(glm::rotate(glm::rotate(glm::rotate(model, rotDiag, diagAxis), rotW, glm::vec3(0.0f, 1.0f, 0.0f)), rotZ, glm::vec3(0.0f, 0.0f, 1.0f)), rotX, glm::vec3(1.0f, 0.0f, 0.0f)), rotY, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(scaleFactor * zoomLevel));

    // Dynamic cam with full perspective
    const float perspTransProxy = 5.0f;
    glm::vec3 camPos = amouranth->isUserCamActive() ? amouranth->getUserCamPos() : glm::vec3(0.0f, 0.0f, -55.0f + static_cast<float>(dimData->darkEnergy) * -9.0f + perspTransProxy * 0.5f);

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

    // Draw the sphere for dimension 9
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getSphereIndices().size()), 1, 0, 0, 0);
}