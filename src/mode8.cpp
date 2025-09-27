// File: mode8.cpp (fixed: cast floats to int for % operator to match integer semantics in computeInteraction)
// Mode 8: Pulsating sphere for dimension 8, with interaction denom proxy in oscSin (pow(dim, vertex%maxD+1))
// and beta modulation in exp terms. Increased LOD and perspective trans proxy in cam offset.
// Uses simplified 128-byte PushConstants { mat4 model; mat4 view_proj; }

#include "core.hpp"
#include <iostream>
#include <vulkan/vulkan.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

void renderMode8(AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout) {

    // Enhanced local function for dim 8, with interaction denom and beta exp
    auto makeOscValue = [](const DimensionData& cacheEntry, float wavePhase) -> float {
        constexpr float omegaApprox = 0.33f;
        constexpr float weakMod = 0.7f;
        constexpr float meanFieldDamp = 0.7f;
        constexpr float asymCollapse = 0.4f;
        constexpr float alphaProxy = 2.0f;
        constexpr float betaProxy = 0.5f;  // From exp(-beta*(dim-1)) in collapse
        constexpr float oneDPermProxy = 1.2f;
        constexpr float invMaxDim = 1.0f / 20.0f;
        const int dimInt = 8;
        const int maxDInt = 20;
        const float denomProxy = std::pow(static_cast<float>(dimInt), (static_cast<int>(dimInt) % maxDInt + 1));  // Fixed: cast to int for %
        const float invDenom = 1.0f / std::max(1e-15f, static_cast<float>(denomProxy));
        const float twoDMod = std::cos(omegaApprox * 8.0f) * 0.8f * oneDPermProxy * invDenom;
        const float deMod = static_cast<float>(cacheEntry.darkEnergy) * std::exp(8.0f * invMaxDim) * 0.65f;
        const float dmMod = static_cast<float>(cacheEntry.darkMatter) * 0.45f;
        const float phase = 8.0f / 40.0f;
        const float osc = std::abs(std::cos(2.0f * M_PI * phase));
        const float vertexFactorProxy = 0.5f;
        const float asymTermProxy = asymCollapse * std::sin(M_PI * phase + osc + vertexFactorProxy) * std::exp(-alphaProxy * phase) * std::exp(-betaProxy * (8.0f - 1.0f));
        const float carrollMod = 1.0f - 0.5f * (1.0f - (8.0f / 20.0f));
        const float oscSin = std::sin(wavePhase + deMod + dmMod + twoDMod + asymTermProxy) * weakMod * invDenom;
        const float oscCos = std::cos(wavePhase + dmMod);
        const float rawValue = static_cast<float>((cacheEntry.observable * oscSin + cacheEntry.potential * oscCos * twoDMod + asymTermProxy) * meanFieldDamp * carrollMod);
        return rawValue;
    };

    // Find the entry for dimension 8
    const DimensionData* dimData = nullptr;
    for (const auto& entry : cache) {
        if (entry.dimension == 8) {
            dimData = &entry;
            break;
        }
    }

    if (!dimData) {
        std::cerr << "Warning: No data found for dimension 8 in cache.\n";
        return;
    }

    const auto oscValue = makeOscValue(*dimData, wavePhase);

    constexpr float kScaleBias = 0.5f;
    constexpr float lodScaleProxy = 0.6f;  // LOD for d=8, lodFactor=1<<4=16
    const float avgScaleProxy = 10.0f / (10.0f + 8.0f / 10.0f);
    const float scaleFactor = (1.0f + std::abs(oscValue) * kScaleBias) * avgScaleProxy * lodScaleProxy;

    // Rotations with enhanced diagonal and beta-mod speeds
    glm::mat4 model = glm::mat4(1.0f);
    const float carrollMod = 1.0f - 0.5f * (1.0f - 8.0f / 20.0f);
    const float betaMod = std::exp(-0.5f * 7.0f);  // exp(-beta*(dim-1))
    const float rotY = glm::radians(static_cast<float>(8) * 40.0f * carrollMod + wavePhase * 0.5f * betaMod);
    const float rotX = glm::radians(static_cast<float>(dimData->potential * 20.0f * carrollMod * betaMod));
    const float rotZ = glm::radians(static_cast<float>(dimData->darkMatter * 15.0f * carrollMod));
    const float rotDiag = glm::radians(static_cast<float>(3.0f * wavePhase * carrollMod));
    glm::vec3 diagAxis(1.0f, 1.0f, 1.0f);
    model = glm::rotate(glm::rotate(glm::rotate(glm::rotate(model, rotDiag, diagAxis), rotZ, glm::vec3(0.0f, 0.0f, 1.0f)), rotX, glm::vec3(1.0f, 0.0f, 0.0f)), rotY, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(scaleFactor * zoomLevel));

    // Dynamic cam with perspective trans proxy offset (trans~5, added to depth)
    const float perspTransProxy = 5.0f;
    glm::vec3 camPos = amouranth->isUserCamActive() ? amouranth->getUserCamPos() : glm::vec3(0.0f, 0.0f, -50.0f + static_cast<float>(dimData->darkEnergy) * -8.0f + perspTransProxy);

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

    // Draw the sphere for dimension 8
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getSphereIndices().size()), 1, 0, 0, 0);
}