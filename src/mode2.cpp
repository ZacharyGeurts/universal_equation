// File: mode2.cpp (new, based on UniversalEquation math for dimension 2)
// Mode 2: Pulsating sphere visualization for dimension 2, incorporating 2D oscillation term influence via cachedCos proxy
// (simulated via additional phase modulation). Uses interaction-like scaling and x-rotation from potential.
// Uses simplified 128-byte PushConstants { mat4 model; mat4 view_proj; }

#include "core.hpp"
#include <iostream>
#include <vulkan/vulkan.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>  // For sin/cos approximations of cachedCos term

void renderMode2(AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout) {

    // Enhanced local function to compute oscillation value for dim 2, incorporating twoD-like cos modulation
    // (proxied via cos(omega * dim) where omega ~ 2*pi/(2*maxD-1), assuming maxD~20, omega~0.33)
    auto makeOscValue = [](const DimensionData& cacheEntry, float wavePhase) -> float {
        constexpr float omegaApprox = 0.33f;  // Approximation from UniversalEquation::omega_ for maxDimensions=20
        const float twoDMod = std::cos(omegaApprox * 2.0f) * 0.8f;  // Proxy for twoD * cachedCos[2]
        const float deMod = static_cast<float>(cacheEntry.darkEnergy) * 0.65f;
        const float dmMod = static_cast<float>(cacheEntry.darkMatter) * 0.45f;
        const float oscSin = std::sin(wavePhase + deMod + dmMod + twoDMod);
        const float oscCos = std::cos(wavePhase + dmMod);
        const float rawValue = static_cast<float>(cacheEntry.observable * oscSin + cacheEntry.potential * oscCos * twoDMod);
        return rawValue;
    };

    // Find the entry for dimension 2
    const DimensionData* dimData = nullptr;
    for (const auto& entry : cache) {
        if (entry.dimension == 2) {
            dimData = &entry;
            break;
        }
    }

    if (!dimData) {
        std::cerr << "Warning: No data found for dimension 2 in cache.\n";
        return;
    }

    const auto oscValue = makeOscValue(*dimData, wavePhase);

    constexpr float kScaleBias = 0.5f;
    const float scaleFactor = 1.0f + std::abs(oscValue) * kScaleBias;

    // Setup transformation matrices with dual rotation: y for base, x incorporating potential (from observable - collapse)
    glm::mat4 model = glm::mat4(1.0f);
    const float rotY = glm::radians(static_cast<float>(2) * 40.0f + wavePhase * 0.5f);  // Dynamic y-rotation
    const float rotX = glm::radians(static_cast<float>(dimData->potential * 20.0f));  // Potential-driven x-rotation (modulo 2pi implicit in radians)
    model = glm::rotate(glm::rotate(model, rotX, glm::vec3(1.0f, 0.0f, 0.0f)), rotY, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(scaleFactor * zoomLevel));

    // Dynamic camera position with darkEnergy zoom and slight offset for 2D plane feel
    glm::vec3 camPos = amouranth->isUserCamActive() ? amouranth->getUserCamPos() : glm::vec3(0.0f, 0.0f, -20.0f + static_cast<float>(dimData->darkEnergy) * -2.0f);

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

    // Draw the sphere for dimension 2
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getSphereIndices().size()), 1, 0, 0, 0);
}