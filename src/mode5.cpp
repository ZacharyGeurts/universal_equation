// File: mode5.cpp (new, based on UniversalEquation math for dimension 5)
// Mode 5: Pulsating sphere for dimension 5, adding perspective modulation proxy to scaleFactor
// from avgScale in compute(). Enhanced collapse term influence in oscCos via symCollapse proxy.
// Uses simplified 128-byte PushConstants { mat4 model; mat4 view_proj; }

#include "core.hpp"
#include <iostream>
#include <vulkan/vulkan.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

void renderMode5(AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout) {

    // Enhanced local function for dim 5 oscillation, incorporating perspective avgScale proxy and symCollapse
    auto makeOscValue = [](const DimensionData& cacheEntry, float wavePhase) -> float {
        constexpr float omegaApprox = 0.33f;
        constexpr float weakMod = 0.7f;
        constexpr float meanFieldDamp = 0.85f;  // Slightly stronger damping for higher dim
        constexpr float asymProxy = 0.3f;
        constexpr float symCollapseProxy = 1.5f * std::exp(-0.2f * 4.0f) * 0.9f;  // Proxy from collapse: collapse * dim * exp(-beta*(dim-1)) * (0.8*osc+0.2), beta~0.2
        const float twoDMod = std::cos(omegaApprox * 5.0f) * 0.8f;
        const float deMod = static_cast<float>(cacheEntry.darkEnergy) * 0.65f;
        const float dmMod = static_cast<float>(cacheEntry.darkMatter) * 0.45f;
        const float phase = 5.0f / 40.0f;  // phase = dim / (2*maxD), maxD=20
        const float osc = std::abs(std::cos(2.0f * M_PI * phase));  // Proxy for cachedCos in collapse
        const float oscSin = std::sin(wavePhase + deMod + dmMod + twoDMod + asymProxy) * weakMod;
        const float oscCos = std::cos(wavePhase + dmMod) * symCollapseProxy * (0.8f * osc + 0.2f);
        const float rawValue = static_cast<float>((cacheEntry.observable * oscSin + cacheEntry.potential * oscCos * twoDMod) * meanFieldDamp);
        return rawValue;
    };

    // Find the entry for dimension 5
    const DimensionData* dimData = nullptr;
    for (const auto& entry : cache) {
        if (entry.dimension == 5) {
            dimData = &entry;
            break;
        }
    }

    if (!dimData) {
        std::cerr << "Warning: No data found for dimension 5 in cache.\n";
        return;
    }

    const auto oscValue = makeOscValue(*dimData, wavePhase);

    constexpr float kScaleBias = 0.5f;
    // Perspective modulation: proxy avgScale ~ focal / (focal + avgDist), assume avgDist ~ dim/10, focal~10
    constexpr float perspFocalProxy = 10.0f;
    constexpr float avgDistProxy = 5.0f / 10.0f;  // For dim5
    const float avgScaleProxy = perspFocalProxy / (perspFocalProxy + avgDistProxy);
    const float scaleFactor = (1.0f + std::abs(oscValue) * kScaleBias) * avgScaleProxy;

    // Enhanced rotations with additional modulation from collapse proxy
    glm::mat4 model = glm::mat4(1.0f);
    const float rotY = glm::radians(static_cast<float>(5) * 40.0f + wavePhase * 0.5f);
    const float rotX = glm::radians(static_cast<float>(dimData->potential * 20.0f));
    const float rotZ = glm::radians(static_cast<float>(dimData->darkMatter * 15.0f));
    const float rotW = glm::radians(static_cast<float>(1.5f * wavePhase));  // Increased asym roll
    model = glm::rotate(glm::rotate(glm::rotate(glm::rotate(model, rotW, glm::vec3(1.0f, 0.0f, 0.0f)), rotZ, glm::vec3(0.0f, 0.0f, 1.0f)), rotX, glm::vec3(1.0f, 0.0f, 0.0f)), rotY, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(scaleFactor * zoomLevel));

    // Dynamic cam with further zoom for 5D
    glm::vec3 camPos = amouranth->isUserCamActive() ? amouranth->getUserCamPos() : glm::vec3(0.0f, 0.0f, -35.0f + static_cast<float>(dimData->darkEnergy) * -5.0f);

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

    // Draw the sphere for dimension 5
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getSphereIndices().size()), 1, 0, 0, 0);
}